//===--- SourceManager.cpp - Track and cache source files -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the SourceManager interface.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManagerInternals.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Capacity.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <sys/stat.h>

using namespace clang;
using namespace SrcMgr;
using llvm::MemoryBuffer;

//===----------------------------------------------------------------------===//
// SourceManager Helper Classes
//===----------------------------------------------------------------------===//

ContentCache::~ContentCache() {
  if (shouldFreeBuffer())
    delete Buffer.getPointer();
}

/// getSizeBytesMapped - Returns the number of bytes actually mapped for this
/// ContentCache. This can be 0 if the MemBuffer was not actually expanded.
unsigned ContentCache::getSizeBytesMapped() const {
  return Buffer.getPointer() ? Buffer.getPointer()->getBufferSize() : 0;
}

/// Returns the kind of memory used to back the memory buffer for
/// this content cache.  This is used for performance analysis.
llvm::MemoryBuffer::BufferKind ContentCache::getMemoryBufferKind() const {
  assert(Buffer.getPointer());

  // Should be unreachable, but keep for sanity.
  if (!Buffer.getPointer())
    return llvm::MemoryBuffer::MemoryBuffer_Malloc;
  
  const llvm::MemoryBuffer *buf = Buffer.getPointer();
  return buf->getBufferKind();
}

/// getSize - Returns the size of the content encapsulated by this ContentCache.
///  This can be the size of the source file or the size of an arbitrary
///  scratch buffer.  If the ContentCache encapsulates a source file, that
///  file is not lazily brought in from disk to satisfy this query.
unsigned ContentCache::getSize() const {
  return Buffer.getPointer() ? (unsigned) Buffer.getPointer()->getBufferSize()
                             : (unsigned) ContentsEntry->getSize();
}

void ContentCache::replaceBuffer(const llvm::MemoryBuffer *B,
                                 bool DoNotFree) {
  if (B && B == Buffer.getPointer()) {
    assert(0 && "Replacing with the same buffer");
    Buffer.setInt(DoNotFree? DoNotFreeFlag : 0);
    return;
  }
  
  if (shouldFreeBuffer())
    delete Buffer.getPointer();
  Buffer.setPointer(B);
  Buffer.setInt(DoNotFree? DoNotFreeFlag : 0);
}

const llvm::MemoryBuffer *ContentCache::getBuffer(DiagnosticsEngine &Diag,
                                                  const SourceManager &SM,
                                                  SourceLocation Loc,
                                                  bool *Invalid) const {
  // Lazily create the Buffer for ContentCaches that wrap files.  If we already
  // computed it, just return what we have.
  if (Buffer.getPointer() || ContentsEntry == 0) {
    if (Invalid)
      *Invalid = isBufferInvalid();
    
    return Buffer.getPointer();
  }    

  std::string ErrorStr;
  bool isVolatile = SM.userFilesAreVolatile() && !IsSystemFile;
  Buffer.setPointer(SM.getFileManager().getBufferForFile(ContentsEntry,
                                                         &ErrorStr,
                                                         isVolatile));

  // If we were unable to open the file, then we are in an inconsistent
  // situation where the content cache referenced a file which no longer
  // exists. Most likely, we were using a stat cache with an invalid entry but
  // the file could also have been removed during processing. Since we can't
  // really deal with this situation, just create an empty buffer.
  //
  // FIXME: This is definitely not ideal, but our immediate clients can't
  // currently handle returning a null entry here. Ideally we should detect
  // that we are in an inconsistent situation and error out as quickly as
  // possible.
  if (!Buffer.getPointer()) {
    const StringRef FillStr("<<<MISSING SOURCE FILE>>>\n");
    Buffer.setPointer(MemoryBuffer::getNewMemBuffer(ContentsEntry->getSize(), 
                                                    "<invalid>"));
    char *Ptr = const_cast<char*>(Buffer.getPointer()->getBufferStart());
    for (unsigned i = 0, e = ContentsEntry->getSize(); i != e; ++i)
      Ptr[i] = FillStr[i % FillStr.size()];

    if (Diag.isDiagnosticInFlight())
      Diag.SetDelayedDiagnostic(diag::err_cannot_open_file, 
                                ContentsEntry->getName(), ErrorStr);
    else 
      Diag.Report(Loc, diag::err_cannot_open_file)
        << ContentsEntry->getName() << ErrorStr;

    Buffer.setInt(Buffer.getInt() | InvalidFlag);
    
    if (Invalid) *Invalid = true;
    return Buffer.getPointer();
  }
  
  // Check that the file's size is the same as in the file entry (which may
  // have come from a stat cache).
  if (getRawBuffer()->getBufferSize() != (size_t)ContentsEntry->getSize()) {
    if (Diag.isDiagnosticInFlight())
      Diag.SetDelayedDiagnostic(diag::err_file_modified,
                                ContentsEntry->getName());
    else
      Diag.Report(Loc, diag::err_file_modified)
        << ContentsEntry->getName();

    Buffer.setInt(Buffer.getInt() | InvalidFlag);
    if (Invalid) *Invalid = true;
    return Buffer.getPointer();
  }

  // If the buffer is valid, check to see if it has a UTF Byte Order Mark
  // (BOM).  We only support UTF-8 with and without a BOM right now.  See
  // http://en.wikipedia.org/wiki/Byte_order_mark for more information.
  StringRef BufStr = Buffer.getPointer()->getBuffer();
  const char *InvalidBOM = llvm::StringSwitch<const char *>(BufStr)
    .StartsWith("\xFE\xFF", "UTF-16 (BE)")
    .StartsWith("\xFF\xFE", "UTF-16 (LE)")
    .StartsWith("\x00\x00\xFE\xFF", "UTF-32 (BE)")
    .StartsWith("\xFF\xFE\x00\x00", "UTF-32 (LE)")
    .StartsWith("\x2B\x2F\x76", "UTF-7")
    .StartsWith("\xF7\x64\x4C", "UTF-1")
    .StartsWith("\xDD\x73\x66\x73", "UTF-EBCDIC")
    .StartsWith("\x0E\xFE\xFF", "SDSU")
    .StartsWith("\xFB\xEE\x28", "BOCU-1")
    .StartsWith("\x84\x31\x95\x33", "GB-18030")
    .Default(0);

  if (InvalidBOM) {
    Diag.Report(Loc, diag::err_unsupported_bom)
      << InvalidBOM << ContentsEntry->getName();
    Buffer.setInt(Buffer.getInt() | InvalidFlag);
  }
  
  if (Invalid)
    *Invalid = isBufferInvalid();
  
  return Buffer.getPointer();
}

unsigned LineTableInfo::getLineTableFilenameID(StringRef Name) {
  // Look up the filename in the string table, returning the pre-existing value
  // if it exists.
  llvm::StringMapEntry<unsigned> &Entry =
    FilenameIDs.GetOrCreateValue(Name, ~0U);
  if (Entry.getValue() != ~0U)
    return Entry.getValue();

  // Otherwise, assign this the next available ID.
  Entry.setValue(FilenamesByID.size());
  FilenamesByID.push_back(&Entry);
  return FilenamesByID.size()-1;
}

/// AddLineNote - Add a line note to the line table that indicates that there
/// is a \#line at the specified FID/Offset location which changes the presumed
/// location to LineNo/FilenameID.
void LineTableInfo::AddLineNote(FileID FID, unsigned Offset,
                                unsigned LineNo, int FilenameID) {
  std::vector<LineEntry> &Entries = LineEntries[FID];

  assert((Entries.empty() || Entries.back().FileOffset < Offset) &&
         "Adding line entries out of order!");

  SrcMgr::CharacteristicKind Kind = SrcMgr::C_User;
  unsigned IncludeOffset = 0;

  if (!Entries.empty()) {
    // If this is a '#line 4' after '#line 42 "foo.h"', make sure to remember
    // that we are still in "foo.h".
    if (FilenameID == -1)
      FilenameID = Entries.back().FilenameID;

    // If we are after a line marker that switched us to system header mode, or
    // that set #include information, preserve it.
    Kind = Entries.back().FileKind;
    IncludeOffset = Entries.back().IncludeOffset;
  }

  Entries.push_back(LineEntry::get(Offset, LineNo, FilenameID, Kind,
                                   IncludeOffset));
}

/// AddLineNote This is the same as the previous version of AddLineNote, but is
/// used for GNU line markers.  If EntryExit is 0, then this doesn't change the
/// presumed \#include stack.  If it is 1, this is a file entry, if it is 2 then
/// this is a file exit.  FileKind specifies whether this is a system header or
/// extern C system header.
void LineTableInfo::AddLineNote(FileID FID, unsigned Offset,
                                unsigned LineNo, int FilenameID,
                                unsigned EntryExit,
                                SrcMgr::CharacteristicKind FileKind) {
  assert(FilenameID != -1 && "Unspecified filename should use other accessor");

  std::vector<LineEntry> &Entries = LineEntries[FID];

  assert((Entries.empty() || Entries.back().FileOffset < Offset) &&
         "Adding line entries out of order!");

  unsigned IncludeOffset = 0;
  if (EntryExit == 0) {  // No #include stack change.
    IncludeOffset = Entries.empty() ? 0 : Entries.back().IncludeOffset;
  } else if (EntryExit == 1) {
    IncludeOffset = Offset-1;
  } else if (EntryExit == 2) {
    assert(!Entries.empty() && Entries.back().IncludeOffset &&
       "PPDirectives should have caught case when popping empty include stack");

    // Get the include loc of the last entries' include loc as our include loc.
    IncludeOffset = 0;
    if (const LineEntry *PrevEntry =
          FindNearestLineEntry(FID, Entries.back().IncludeOffset))
      IncludeOffset = PrevEntry->IncludeOffset;
  }

  Entries.push_back(LineEntry::get(Offset, LineNo, FilenameID, FileKind,
                                   IncludeOffset));
}


/// FindNearestLineEntry - Find the line entry nearest to FID that is before
/// it.  If there is no line entry before Offset in FID, return null.
const LineEntry *LineTableInfo::FindNearestLineEntry(FileID FID,
                                                     unsigned Offset) {
  const std::vector<LineEntry> &Entries = LineEntries[FID];
  assert(!Entries.empty() && "No #line entries for this FID after all!");

  // It is very common for the query to be after the last #line, check this
  // first.
  if (Entries.back().FileOffset <= Offset)
    return &Entries.back();

  // Do a binary search to find the maximal element that is still before Offset.
  std::vector<LineEntry>::const_iterator I =
    std::upper_bound(Entries.begin(), Entries.end(), Offset);
  if (I == Entries.begin()) return 0;
  return &*--I;
}

/// \brief Add a new line entry that has already been encoded into
/// the internal representation of the line table.
void LineTableInfo::AddEntry(FileID FID,
                             const std::vector<LineEntry> &Entries) {
  LineEntries[FID] = Entries;
}

/// getLineTableFilenameID - Return the uniqued ID for the specified filename.
///
unsigned SourceManager::getLineTableFilenameID(StringRef Name) {
  if (LineTable == 0)
    LineTable = new LineTableInfo();
  return LineTable->getLineTableFilenameID(Name);
}


/// AddLineNote - Add a line note to the line table for the FileID and offset
/// specified by Loc.  If FilenameID is -1, it is considered to be
/// unspecified.
void SourceManager::AddLineNote(SourceLocation Loc, unsigned LineNo,
                                int FilenameID) {
  std::pair<FileID, unsigned> LocInfo = getDecomposedExpansionLoc(Loc);

  bool Invalid = false;
  const SLocEntry &Entry = getSLocEntry(LocInfo.first, &Invalid);
  if (!Entry.isFile() || Invalid)
    return;
  
  const SrcMgr::FileInfo &FileInfo = Entry.getFile();

  // Remember that this file has #line directives now if it doesn't already.
  const_cast<SrcMgr::FileInfo&>(FileInfo).setHasLineDirectives();

  if (LineTable == 0)
    LineTable = new LineTableInfo();
  LineTable->AddLineNote(LocInfo.first, LocInfo.second, LineNo, FilenameID);
}

/// AddLineNote - Add a GNU line marker to the line table.
void SourceManager::AddLineNote(SourceLocation Loc, unsigned LineNo,
                                int FilenameID, bool IsFileEntry,
                                bool IsFileExit, bool IsSystemHeader,
                                bool IsExternCHeader) {
  // If there is no filename and no flags, this is treated just like a #line,
  // which does not change the flags of the previous line marker.
  if (FilenameID == -1) {
    assert(!IsFileEntry && !IsFileExit && !IsSystemHeader && !IsExternCHeader &&
           "Can't set flags without setting the filename!");
    return AddLineNote(Loc, LineNo, FilenameID);
  }

  std::pair<FileID, unsigned> LocInfo = getDecomposedExpansionLoc(Loc);

  bool Invalid = false;
  const SLocEntry &Entry = getSLocEntry(LocInfo.first, &Invalid);
  if (!Entry.isFile() || Invalid)
    return;
  
  const SrcMgr::FileInfo &FileInfo = Entry.getFile();

  // Remember that this file has #line directives now if it doesn't already.
  const_cast<SrcMgr::FileInfo&>(FileInfo).setHasLineDirectives();

  if (LineTable == 0)
    LineTable = new LineTableInfo();

  SrcMgr::CharacteristicKind FileKind;
  if (IsExternCHeader)
    FileKind = SrcMgr::C_ExternCSystem;
  else if (IsSystemHeader)
    FileKind = SrcMgr::C_System;
  else
    FileKind = SrcMgr::C_User;

  unsigned EntryExit = 0;
  if (IsFileEntry)
    EntryExit = 1;
  else if (IsFileExit)
    EntryExit = 2;

  LineTable->AddLineNote(LocInfo.first, LocInfo.second, LineNo, FilenameID,
                         EntryExit, FileKind);
}

LineTableInfo &SourceManager::getLineTable() {
  if (LineTable == 0)
    LineTable = new LineTableInfo();
  return *LineTable;
}

//===----------------------------------------------------------------------===//
// Private 'Create' methods.
//===----------------------------------------------------------------------===//

SourceManager::SourceManager(DiagnosticsEngine &Diag, FileManager &FileMgr,
                             bool UserFilesAreVolatile)
  : Diag(Diag), FileMgr(FileMgr), OverridenFilesKeepOriginalName(true),
    UserFilesAreVolatile(UserFilesAreVolatile),
    ExternalSLocEntries(0), LineTable(0), NumLinearScans(0),
    NumBinaryProbes(0), FakeBufferForRecovery(0),
    FakeContentCacheForRecovery(0) {
  clearIDTables();
  Diag.setSourceManager(this);
}

SourceManager::~SourceManager() {
  delete LineTable;

  // Delete FileEntry objects corresponding to content caches.  Since the actual
  // content cache objects are bump pointer allocated, we just have to run the
  // dtors, but we call the deallocate method for completeness.
  for (unsigned i = 0, e = MemBufferInfos.size(); i != e; ++i) {
    if (MemBufferInfos[i]) {
      MemBufferInfos[i]->~ContentCache();
      ContentCacheAlloc.Deallocate(MemBufferInfos[i]);
    }
  }
  for (llvm::DenseMap<const FileEntry*, SrcMgr::ContentCache*>::iterator
       I = FileInfos.begin(), E = FileInfos.end(); I != E; ++I) {
    if (I->second) {
      I->second->~ContentCache();
      ContentCacheAlloc.Deallocate(I->second);
    }
  }
  
  delete FakeBufferForRecovery;
  delete FakeContentCacheForRecovery;

  for (llvm::DenseMap<FileID, MacroArgsMap *>::iterator
         I = MacroArgsCacheMap.begin(),E = MacroArgsCacheMap.end(); I!=E; ++I) {
    delete I->second;
  }
}

void SourceManager::clearIDTables() {
  MainFileID = FileID();
  LocalSLocEntryTable.clear();
  LoadedSLocEntryTable.clear();
  SLocEntryLoaded.clear();
  LastLineNoFileIDQuery = FileID();
  LastLineNoContentCache = 0;
  LastFileIDLookup = FileID();

  if (LineTable)
    LineTable->clear();

  // Use up FileID #0 as an invalid expansion.
  NextLocalOffset = 0;
  CurrentLoadedOffset = MaxLoadedOffset;
  createExpansionLoc(SourceLocation(),SourceLocation(),SourceLocation(), 1);
}

/// getOrCreateContentCache - Create or return a cached ContentCache for the
/// specified file.
const ContentCache *
SourceManager::getOrCreateContentCache(const FileEntry *FileEnt,
                                       bool isSystemFile) {
  assert(FileEnt && "Didn't specify a file entry to use?");

  // Do we already have information about this file?
  ContentCache *&Entry = FileInfos[FileEnt];
  if (Entry) return Entry;

  // Nope, create a new Cache entry.  Make sure it is at least 8-byte aligned
  // so that FileInfo can use the low 3 bits of the pointer for its own
  // nefarious purposes.
  unsigned EntryAlign = llvm::AlignOf<ContentCache>::Alignment;
  EntryAlign = std::max(8U, EntryAlign);
  Entry = ContentCacheAlloc.Allocate<ContentCache>(1, EntryAlign);

  if (OverriddenFilesInfo) {
    // If the file contents are overridden with contents from another file,
    // pass that file to ContentCache.
    llvm::DenseMap<const FileEntry *, const FileEntry *>::iterator
        overI = OverriddenFilesInfo->OverriddenFiles.find(FileEnt);
    if (overI == OverriddenFilesInfo->OverriddenFiles.end())
      new (Entry) ContentCache(FileEnt);
    else
      new (Entry) ContentCache(OverridenFilesKeepOriginalName ? FileEnt
                                                              : overI->second,
                               overI->second);
  } else {
    new (Entry) ContentCache(FileEnt);
  }

  Entry->IsSystemFile = isSystemFile;

  return Entry;
}


/// createMemBufferContentCache - Create a new ContentCache for the specified
///  memory buffer.  This does no caching.
const ContentCache*
SourceManager::createMemBufferContentCache(const MemoryBuffer *Buffer) {
  // Add a new ContentCache to the MemBufferInfos list and return it.  Make sure
  // it is at least 8-byte aligned so that FileInfo can use the low 3 bits of
  // the pointer for its own nefarious purposes.
  unsigned EntryAlign = llvm::AlignOf<ContentCache>::Alignment;
  EntryAlign = std::max(8U, EntryAlign);
  ContentCache *Entry = ContentCacheAlloc.Allocate<ContentCache>(1, EntryAlign);
  new (Entry) ContentCache();
  MemBufferInfos.push_back(Entry);
  Entry->setBuffer(Buffer);
  return Entry;
}

const SrcMgr::SLocEntry &SourceManager::loadSLocEntry(unsigned Index,
                                                      bool *Invalid) const {
  assert(!SLocEntryLoaded[Index]);
  if (ExternalSLocEntries->ReadSLocEntry(-(static_cast<int>(Index) + 2))) {
    if (Invalid)
      *Invalid = true;
    // If the file of the SLocEntry changed we could still have loaded it.
    if (!SLocEntryLoaded[Index]) {
      // Try to recover; create a SLocEntry so the rest of clang can handle it.
      LoadedSLocEntryTable[Index] = SLocEntry::get(0,
                                 FileInfo::get(SourceLocation(),
                                               getFakeContentCacheForRecovery(),
                                               SrcMgr::C_User));
    }
  }

  return LoadedSLocEntryTable[Index];
}

std::pair<int, unsigned>
SourceManager::AllocateLoadedSLocEntries(unsigned NumSLocEntries,
                                         unsigned TotalSize) {
  assert(ExternalSLocEntries && "Don't have an external sloc source");
  LoadedSLocEntryTable.resize(LoadedSLocEntryTable.size() + NumSLocEntries);
  SLocEntryLoaded.resize(LoadedSLocEntryTable.size());
  CurrentLoadedOffset -= TotalSize;
  assert(CurrentLoadedOffset >= NextLocalOffset && "Out of source locations");
  int ID = LoadedSLocEntryTable.size();
  return std::make_pair(-ID - 1, CurrentLoadedOffset);
}

/// \brief As part of recovering from missing or changed content, produce a
/// fake, non-empty buffer.
const llvm::MemoryBuffer *SourceManager::getFakeBufferForRecovery() const {
  if (!FakeBufferForRecovery)
    FakeBufferForRecovery
      = llvm::MemoryBuffer::getMemBuffer("<<<INVALID BUFFER>>");
  
  return FakeBufferForRecovery;
}

/// \brief As part of recovering from missing or changed content, produce a
/// fake content cache.
const SrcMgr::ContentCache *
SourceManager::getFakeContentCacheForRecovery() const {
  if (!FakeContentCacheForRecovery) {
    FakeContentCacheForRecovery = new ContentCache();
    FakeContentCacheForRecovery->replaceBuffer(getFakeBufferForRecovery(),
                                               /*DoNotFree=*/true);
  }
  return FakeContentCacheForRecovery;
}

//===----------------------------------------------------------------------===//
// Methods to create new FileID's and macro expansions.
//===----------------------------------------------------------------------===//

/// createFileID - Create a new FileID for the specified ContentCache and
/// include position.  This works regardless of whether the ContentCache
/// corresponds to a file or some other input source.
FileID SourceManager::createFileID(const ContentCache *File,
                                   SourceLocation IncludePos,
                                   SrcMgr::CharacteristicKind FileCharacter,
                                   int LoadedID, unsigned LoadedOffset) {
  if (LoadedID < 0) {
    assert(LoadedID != -1 && "Loading sentinel FileID");
    unsigned Index = unsigned(-LoadedID) - 2;
    assert(Index < LoadedSLocEntryTable.size() && "FileID out of range");
    assert(!SLocEntryLoaded[Index] && "FileID already loaded");
    LoadedSLocEntryTable[Index] = SLocEntry::get(LoadedOffset,
        FileInfo::get(IncludePos, File, FileCharacter));
    SLocEntryLoaded[Index] = true;
    return FileID::get(LoadedID);
  }
  LocalSLocEntryTable.push_back(SLocEntry::get(NextLocalOffset,
                                               FileInfo::get(IncludePos, File,
                                                             FileCharacter)));
  unsigned FileSize = File->getSize();
  assert(NextLocalOffset + FileSize + 1 > NextLocalOffset &&
         NextLocalOffset + FileSize + 1 <= CurrentLoadedOffset &&
         "Ran out of source locations!");
  // We do a +1 here because we want a SourceLocation that means "the end of the
  // file", e.g. for the "no newline at the end of the file" diagnostic.
  NextLocalOffset += FileSize + 1;

  // Set LastFileIDLookup to the newly created file.  The next getFileID call is
  // almost guaranteed to be from that file.
  FileID FID = FileID::get(LocalSLocEntryTable.size()-1);
  return LastFileIDLookup = FID;
}

SourceLocation
SourceManager::createMacroArgExpansionLoc(SourceLocation SpellingLoc,
                                          SourceLocation ExpansionLoc,
                                          unsigned TokLength) {
  ExpansionInfo Info = ExpansionInfo::createForMacroArg(SpellingLoc,
                                                        ExpansionLoc);
  return createExpansionLocImpl(Info, TokLength);
}

SourceLocation
SourceManager::createExpansionLoc(SourceLocation SpellingLoc,
                                  SourceLocation ExpansionLocStart,
                                  SourceLocation ExpansionLocEnd,
                                  unsigned TokLength,
                                  int LoadedID,
                                  unsigned LoadedOffset) {
  ExpansionInfo Info = ExpansionInfo::create(SpellingLoc, ExpansionLocStart,
                                             ExpansionLocEnd);
  return createExpansionLocImpl(Info, TokLength, LoadedID, LoadedOffset);
}

SourceLocation
SourceManager::createExpansionLocImpl(const ExpansionInfo &Info,
                                      unsigned TokLength,
                                      int LoadedID,
                                      unsigned LoadedOffset) {
  if (LoadedID < 0) {
    assert(LoadedID != -1 && "Loading sentinel FileID");
    unsigned Index = unsigned(-LoadedID) - 2;
    assert(Index < LoadedSLocEntryTable.size() && "FileID out of range");
    assert(!SLocEntryLoaded[Index] && "FileID already loaded");
    LoadedSLocEntryTable[Index] = SLocEntry::get(LoadedOffset, Info);
    SLocEntryLoaded[Index] = true;
    return SourceLocation::getMacroLoc(LoadedOffset);
  }
  LocalSLocEntryTable.push_back(SLocEntry::get(NextLocalOffset, Info));
  assert(NextLocalOffset + TokLength + 1 > NextLocalOffset &&
         NextLocalOffset + TokLength + 1 <= CurrentLoadedOffset &&
         "Ran out of source locations!");
  // See createFileID for that +1.
  NextLocalOffset += TokLength + 1;
  return SourceLocation::getMacroLoc(NextLocalOffset - (TokLength + 1));
}

const llvm::MemoryBuffer *
SourceManager::getMemoryBufferForFile(const FileEntry *File,
                                      bool *Invalid) {
  const SrcMgr::ContentCache *IR = getOrCreateContentCache(File);
  assert(IR && "getOrCreateContentCache() cannot return NULL");
  return IR->getBuffer(Diag, *this, SourceLocation(), Invalid);
}

void SourceManager::overrideFileContents(const FileEntry *SourceFile,
                                         const llvm::MemoryBuffer *Buffer,
                                         bool DoNotFree) {
  const SrcMgr::ContentCache *IR = getOrCreateContentCache(SourceFile);
  assert(IR && "getOrCreateContentCache() cannot return NULL");

  const_cast<SrcMgr::ContentCache *>(IR)->replaceBuffer(Buffer, DoNotFree);
  const_cast<SrcMgr::ContentCache *>(IR)->BufferOverridden = true;

  getOverriddenFilesInfo().OverriddenFilesWithBuffer.insert(SourceFile);
}

void SourceManager::overrideFileContents(const FileEntry *SourceFile,
                                         const FileEntry *NewFile) {
  assert(SourceFile->getSize() == NewFile->getSize() &&
         "Different sizes, use the FileManager to create a virtual file with "
         "the correct size");
  assert(FileInfos.count(SourceFile) == 0 &&
         "This function should be called at the initialization stage, before "
         "any parsing occurs.");
  getOverriddenFilesInfo().OverriddenFiles[SourceFile] = NewFile;
}

void SourceManager::disableFileContentsOverride(const FileEntry *File) {
  if (!isFileOverridden(File))
    return;

  const SrcMgr::ContentCache *IR = getOrCreateContentCache(File);
  const_cast<SrcMgr::ContentCache *>(IR)->replaceBuffer(0);
  const_cast<SrcMgr::ContentCache *>(IR)->ContentsEntry = IR->OrigEntry;

  assert(OverriddenFilesInfo);
  OverriddenFilesInfo->OverriddenFiles.erase(File);
  OverriddenFilesInfo->OverriddenFilesWithBuffer.erase(File);
}

StringRef SourceManager::getBufferData(FileID FID, bool *Invalid) const {
  bool MyInvalid = false;
  const SLocEntry &SLoc = getSLocEntry(FID, &MyInvalid);
  if (!SLoc.isFile() || MyInvalid) {
    if (Invalid) 
      *Invalid = true;
    return "<<<<<INVALID SOURCE LOCATION>>>>>";
  }
  
  const llvm::MemoryBuffer *Buf
    = SLoc.getFile().getContentCache()->getBuffer(Diag, *this, SourceLocation(), 
                                                  &MyInvalid);
  if (Invalid)
    *Invalid = MyInvalid;

  if (MyInvalid)
    return "<<<<<INVALID SOURCE LOCATION>>>>>";
  
  return Buf->getBuffer();
}

//===----------------------------------------------------------------------===//
// SourceLocation manipulation methods.
//===----------------------------------------------------------------------===//

/// \brief Return the FileID for a SourceLocation.
///
/// This is the cache-miss path of getFileID. Not as hot as that function, but
/// still very important. It is responsible for finding the entry in the
/// SLocEntry tables that contains the specified location.
FileID SourceManager::getFileIDSlow(unsigned SLocOffset) const {
  if (!SLocOffset)
    return FileID::get(0);

  // Now it is time to search for the correct file. See where the SLocOffset
  // sits in the global view and consult local or loaded buffers for it.
  if (SLocOffset < NextLocalOffset)
    return getFileIDLocal(SLocOffset);
  return getFileIDLoaded(SLocOffset);
}

/// \brief Return the FileID for a SourceLocation with a low offset.
///
/// This function knows that the SourceLocation is in a local buffer, not a
/// loaded one.
FileID SourceManager::getFileIDLocal(unsigned SLocOffset) const {
  assert(SLocOffset < NextLocalOffset && "Bad function choice");

  // After the first and second level caches, I see two common sorts of
  // behavior: 1) a lot of searched FileID's are "near" the cached file
  // location or are "near" the cached expansion location. 2) others are just
  // completely random and may be a very long way away.
  //
  // To handle this, we do a linear search for up to 8 steps to catch #1 quickly
  // then we fall back to a less cache efficient, but more scalable, binary
  // search to find the location.

  // See if this is near the file point - worst case we start scanning from the
  // most newly created FileID.
  const SrcMgr::SLocEntry *I;

  if (LastFileIDLookup.ID < 0 ||
      LocalSLocEntryTable[LastFileIDLookup.ID].getOffset() < SLocOffset) {
    // Neither loc prunes our search.
    I = LocalSLocEntryTable.end();
  } else {
    // Perhaps it is near the file point.
    I = LocalSLocEntryTable.begin()+LastFileIDLookup.ID;
  }

  // Find the FileID that contains this.  "I" is an iterator that points to a
  // FileID whose offset is known to be larger than SLocOffset.
  unsigned NumProbes = 0;
  while (1) {
    --I;
    if (I->getOffset() <= SLocOffset) {
      FileID Res = FileID::get(int(I - LocalSLocEntryTable.begin()));

      // If this isn't an expansion, remember it.  We have good locality across
      // FileID lookups.
      if (!I->isExpansion())
        LastFileIDLookup = Res;
      NumLinearScans += NumProbes+1;
      return Res;
    }
    if (++NumProbes == 8)
      break;
  }

  // Convert "I" back into an index.  We know that it is an entry whose index is
  // larger than the offset we are looking for.
  unsigned GreaterIndex = I - LocalSLocEntryTable.begin();
  // LessIndex - This is the lower bound of the range that we're searching.
  // We know that the offset corresponding to the FileID is is less than
  // SLocOffset.
  unsigned LessIndex = 0;
  NumProbes = 0;
  while (1) {
    bool Invalid = false;
    unsigned MiddleIndex = (GreaterIndex-LessIndex)/2+LessIndex;
    unsigned MidOffset = getLocalSLocEntry(MiddleIndex, &Invalid).getOffset();
    if (Invalid)
      return FileID::get(0);
    
    ++NumProbes;

    // If the offset of the midpoint is too large, chop the high side of the
    // range to the midpoint.
    if (MidOffset > SLocOffset) {
      GreaterIndex = MiddleIndex;
      continue;
    }

    // If the middle index contains the value, succeed and return.
    // FIXME: This could be made faster by using a function that's aware of
    // being in the local area.
    if (isOffsetInFileID(FileID::get(MiddleIndex), SLocOffset)) {
      FileID Res = FileID::get(MiddleIndex);

      // If this isn't a macro expansion, remember it.  We have good locality
      // across FileID lookups.
      if (!LocalSLocEntryTable[MiddleIndex].isExpansion())
        LastFileIDLookup = Res;
      NumBinaryProbes += NumProbes;
      return Res;
    }

    // Otherwise, move the low-side up to the middle index.
    LessIndex = MiddleIndex;
  }
}

/// \brief Return the FileID for a SourceLocation with a high offset.
///
/// This function knows that the SourceLocation is in a loaded buffer, not a
/// local one.
FileID SourceManager::getFileIDLoaded(unsigned SLocOffset) const {
  // Sanity checking, otherwise a bug may lead to hanging in release build.
  if (SLocOffset < CurrentLoadedOffset) {
    assert(0 && "Invalid SLocOffset or bad function choice");
    return FileID();
  }

  // Essentially the same as the local case, but the loaded array is sorted
  // in the other direction.

  // First do a linear scan from the last lookup position, if possible.
  unsigned I;
  int LastID = LastFileIDLookup.ID;
  if (LastID >= 0 || getLoadedSLocEntryByID(LastID).getOffset() < SLocOffset)
    I = 0;
  else
    I = (-LastID - 2) + 1;

  unsigned NumProbes;
  for (NumProbes = 0; NumProbes < 8; ++NumProbes, ++I) {
    // Make sure the entry is loaded!
    const SrcMgr::SLocEntry &E = getLoadedSLocEntry(I);
    if (E.getOffset() <= SLocOffset) {
      FileID Res = FileID::get(-int(I) - 2);

      if (!E.isExpansion())
        LastFileIDLookup = Res;
      NumLinearScans += NumProbes + 1;
      return Res;
    }
  }

  // Linear scan failed. Do the binary search. Note the reverse sorting of the
  // table: GreaterIndex is the one where the offset is greater, which is
  // actually a lower index!
  unsigned GreaterIndex = I;
  unsigned LessIndex = LoadedSLocEntryTable.size();
  NumProbes = 0;
  while (1) {
    ++NumProbes;
    unsigned MiddleIndex = (LessIndex - GreaterIndex) / 2 + GreaterIndex;
    const SrcMgr::SLocEntry &E = getLoadedSLocEntry(MiddleIndex);
    if (E.getOffset() == 0)
      return FileID(); // invalid entry.

    ++NumProbes;

    if (E.getOffset() > SLocOffset) {
      // Sanity checking, otherwise a bug may lead to hanging in release build.
      if (GreaterIndex == MiddleIndex) {
        assert(0 && "binary search missed the entry");
        return FileID();
      }
      GreaterIndex = MiddleIndex;
      continue;
    }

    if (isOffsetInFileID(FileID::get(-int(MiddleIndex) - 2), SLocOffset)) {
      FileID Res = FileID::get(-int(MiddleIndex) - 2);
      if (!E.isExpansion())
        LastFileIDLookup = Res;
      NumBinaryProbes += NumProbes;
      return Res;
    }

    // Sanity checking, otherwise a bug may lead to hanging in release build.
    if (LessIndex == MiddleIndex) {
      assert(0 && "binary search missed the entry");
      return FileID();
    }
    LessIndex = MiddleIndex;
  }
}

SourceLocation SourceManager::
getExpansionLocSlowCase(SourceLocation Loc) const {
  do {
    // Note: If Loc indicates an offset into a token that came from a macro
    // expansion (e.g. the 5th character of the token) we do not want to add
    // this offset when going to the expansion location.  The expansion
    // location is the macro invocation, which the offset has nothing to do
    // with.  This is unlike when we get the spelling loc, because the offset
    // directly correspond to the token whose spelling we're inspecting.
    Loc = getSLocEntry(getFileID(Loc)).getExpansion().getExpansionLocStart();
  } while (!Loc.isFileID());

  return Loc;
}

SourceLocation SourceManager::getSpellingLocSlowCase(SourceLocation Loc) const {
  do {
    std::pair<FileID, unsigned> LocInfo = getDecomposedLoc(Loc);
    Loc = getSLocEntry(LocInfo.first).getExpansion().getSpellingLoc();
    Loc = Loc.getLocWithOffset(LocInfo.second);
  } while (!Loc.isFileID());
  return Loc;
}

SourceLocation SourceManager::getFileLocSlowCase(SourceLocation Loc) const {
  do {
    if (isMacroArgExpansion(Loc))
      Loc = getImmediateSpellingLoc(Loc);
    else
      Loc = getImmediateExpansionRange(Loc).first;
  } while (!Loc.isFileID());
  return Loc;
}


std::pair<FileID, unsigned>
SourceManager::getDecomposedExpansionLocSlowCase(
                                             const SrcMgr::SLocEntry *E) const {
  // If this is an expansion record, walk through all the expansion points.
  FileID FID;
  SourceLocation Loc;
  unsigned Offset;
  do {
    Loc = E->getExpansion().getExpansionLocStart();

    FID = getFileID(Loc);
    E = &getSLocEntry(FID);
    Offset = Loc.getOffset()-E->getOffset();
  } while (!Loc.isFileID());

  return std::make_pair(FID, Offset);
}

std::pair<FileID, unsigned>
SourceManager::getDecomposedSpellingLocSlowCase(const SrcMgr::SLocEntry *E,
                                                unsigned Offset) const {
  // If this is an expansion record, walk through all the expansion points.
  FileID FID;
  SourceLocation Loc;
  do {
    Loc = E->getExpansion().getSpellingLoc();
    Loc = Loc.getLocWithOffset(Offset);

    FID = getFileID(Loc);
    E = &getSLocEntry(FID);
    Offset = Loc.getOffset()-E->getOffset();
  } while (!Loc.isFileID());

  return std::make_pair(FID, Offset);
}

/// getImmediateSpellingLoc - Given a SourceLocation object, return the
/// spelling location referenced by the ID.  This is the first level down
/// towards the place where the characters that make up the lexed token can be
/// found.  This should not generally be used by clients.
SourceLocation SourceManager::getImmediateSpellingLoc(SourceLocation Loc) const{
  if (Loc.isFileID()) return Loc;
  std::pair<FileID, unsigned> LocInfo = getDecomposedLoc(Loc);
  Loc = getSLocEntry(LocInfo.first).getExpansion().getSpellingLoc();
  return Loc.getLocWithOffset(LocInfo.second);
}


/// getImmediateExpansionRange - Loc is required to be an expansion location.
/// Return the start/end of the expansion information.
std::pair<SourceLocation,SourceLocation>
SourceManager::getImmediateExpansionRange(SourceLocation Loc) const {
  assert(Loc.isMacroID() && "Not a macro expansion loc!");
  const ExpansionInfo &Expansion = getSLocEntry(getFileID(Loc)).getExpansion();
  return Expansion.getExpansionLocRange();
}

/// getExpansionRange - Given a SourceLocation object, return the range of
/// tokens covered by the expansion in the ultimate file.
std::pair<SourceLocation,SourceLocation>
SourceManager::getExpansionRange(SourceLocation Loc) const {
  if (Loc.isFileID()) return std::make_pair(Loc, Loc);

  std::pair<SourceLocation,SourceLocation> Res =
    getImmediateExpansionRange(Loc);

  // Fully resolve the start and end locations to their ultimate expansion
  // points.
  while (!Res.first.isFileID())
    Res.first = getImmediateExpansionRange(Res.first).first;
  while (!Res.second.isFileID())
    Res.second = getImmediateExpansionRange(Res.second).second;
  return Res;
}

bool SourceManager::isMacroArgExpansion(SourceLocation Loc) const {
  if (!Loc.isMacroID()) return false;

  FileID FID = getFileID(Loc);
  const SrcMgr::ExpansionInfo &Expansion = getSLocEntry(FID).getExpansion();
  return Expansion.isMacroArgExpansion();
}

bool SourceManager::isMacroBodyExpansion(SourceLocation Loc) const {
  if (!Loc.isMacroID()) return false;

  FileID FID = getFileID(Loc);
  const SrcMgr::ExpansionInfo &Expansion = getSLocEntry(FID).getExpansion();
  return Expansion.isMacroBodyExpansion();
}


//===----------------------------------------------------------------------===//
// Queries about the code at a SourceLocation.
//===----------------------------------------------------------------------===//

/// getCharacterData - Return a pointer to the start of the specified location
/// in the appropriate MemoryBuffer.
const char *SourceManager::getCharacterData(SourceLocation SL,
                                            bool *Invalid) const {
  // Note that this is a hot function in the getSpelling() path, which is
  // heavily used by -E mode.
  std::pair<FileID, unsigned> LocInfo = getDecomposedSpellingLoc(SL);

  // Note that calling 'getBuffer()' may lazily page in a source file.
  bool CharDataInvalid = false;
  const SLocEntry &Entry = getSLocEntry(LocInfo.first, &CharDataInvalid);
  if (CharDataInvalid || !Entry.isFile()) {
    if (Invalid)
      *Invalid = true;
    
    return "<<<<INVALID BUFFER>>>>";
  }
  const llvm::MemoryBuffer *Buffer
    = Entry.getFile().getContentCache()
                  ->getBuffer(Diag, *this, SourceLocation(), &CharDataInvalid);
  if (Invalid)
    *Invalid = CharDataInvalid;
  return Buffer->getBufferStart() + (CharDataInvalid? 0 : LocInfo.second);
}


/// getColumnNumber - Return the column # for the specified file position.
/// this is significantly cheaper to compute than the line number.
unsigned SourceManager::getColumnNumber(FileID FID, unsigned FilePos,
                                        bool *Invalid) const {
  bool MyInvalid = false;
  const llvm::MemoryBuffer *MemBuf = getBuffer(FID, &MyInvalid);
  if (Invalid)
    *Invalid = MyInvalid;

  if (MyInvalid)
    return 1;

  // It is okay to request a position just past the end of the buffer.
  if (FilePos > MemBuf->getBufferSize()) {
    if (Invalid)
      *Invalid = true;
    return 1;
  }

  // See if we just calculated the line number for this FilePos and can use
  // that to lookup the start of the line instead of searching for it.
  if (LastLineNoFileIDQuery == FID &&
      LastLineNoContentCache->SourceLineCache != 0 &&
      LastLineNoResult < LastLineNoContentCache->NumLines) {
    unsigned *SourceLineCache = LastLineNoContentCache->SourceLineCache;
    unsigned LineStart = SourceLineCache[LastLineNoResult - 1];
    unsigned LineEnd = SourceLineCache[LastLineNoResult];
    if (FilePos >= LineStart && FilePos < LineEnd)
      return FilePos - LineStart + 1;
  }

  const char *Buf = MemBuf->getBufferStart();
  unsigned LineStart = FilePos;
  while (LineStart && Buf[LineStart-1] != '\n' && Buf[LineStart-1] != '\r')
    --LineStart;
  return FilePos-LineStart+1;
}

// isInvalid - Return the result of calling loc.isInvalid(), and
// if Invalid is not null, set its value to same.
static bool isInvalid(SourceLocation Loc, bool *Invalid) {
  bool MyInvalid = Loc.isInvalid();
  if (Invalid)
    *Invalid = MyInvalid;
  return MyInvalid;
}

unsigned SourceManager::getSpellingColumnNumber(SourceLocation Loc,
                                                bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return 0;
  std::pair<FileID, unsigned> LocInfo = getDecomposedSpellingLoc(Loc);
  return getColumnNumber(LocInfo.first, LocInfo.second, Invalid);
}

unsigned SourceManager::getExpansionColumnNumber(SourceLocation Loc,
                                                 bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return 0;
  std::pair<FileID, unsigned> LocInfo = getDecomposedExpansionLoc(Loc);
  return getColumnNumber(LocInfo.first, LocInfo.second, Invalid);
}

unsigned SourceManager::getPresumedColumnNumber(SourceLocation Loc,
                                                bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return 0;
  return getPresumedLoc(Loc).getColumn();
}

#ifdef __SSE2__
#include <emmintrin.h>
#endif

static LLVM_ATTRIBUTE_NOINLINE void
ComputeLineNumbers(DiagnosticsEngine &Diag, ContentCache *FI,
                   llvm::BumpPtrAllocator &Alloc,
                   const SourceManager &SM, bool &Invalid);
static void ComputeLineNumbers(DiagnosticsEngine &Diag, ContentCache *FI,
                               llvm::BumpPtrAllocator &Alloc,
                               const SourceManager &SM, bool &Invalid) {
  // Note that calling 'getBuffer()' may lazily page in the file.
  const MemoryBuffer *Buffer = FI->getBuffer(Diag, SM, SourceLocation(),
                                             &Invalid);
  if (Invalid)
    return;

  // Find the file offsets of all of the *physical* source lines.  This does
  // not look at trigraphs, escaped newlines, or anything else tricky.
  SmallVector<unsigned, 256> LineOffsets;

  // Line #1 starts at char 0.
  LineOffsets.push_back(0);

  const unsigned char *Buf = (const unsigned char *)Buffer->getBufferStart();
  const unsigned char *End = (const unsigned char *)Buffer->getBufferEnd();
  unsigned Offs = 0;
  while (1) {
    // Skip over the contents of the line.
    const unsigned char *NextBuf = (const unsigned char *)Buf;

#ifdef __SSE2__
    // Try to skip to the next newline using SSE instructions. This is very
    // performance sensitive for programs with lots of diagnostics and in -E
    // mode.
    __m128i CRs = _mm_set1_epi8('\r');
    __m128i LFs = _mm_set1_epi8('\n');

    // First fix up the alignment to 16 bytes.
    while (((uintptr_t)NextBuf & 0xF) != 0) {
      if (*NextBuf == '\n' || *NextBuf == '\r' || *NextBuf == '\0')
        goto FoundSpecialChar;
      ++NextBuf;
    }

    // Scan 16 byte chunks for '\r' and '\n'. Ignore '\0'.
    while (NextBuf+16 <= End) {
      const __m128i Chunk = *(const __m128i*)NextBuf;
      __m128i Cmp = _mm_or_si128(_mm_cmpeq_epi8(Chunk, CRs),
                                 _mm_cmpeq_epi8(Chunk, LFs));
      unsigned Mask = _mm_movemask_epi8(Cmp);

      // If we found a newline, adjust the pointer and jump to the handling code.
      if (Mask != 0) {
        NextBuf += llvm::CountTrailingZeros_32(Mask);
        goto FoundSpecialChar;
      }
      NextBuf += 16;
    }
#endif

    while (*NextBuf != '\n' && *NextBuf != '\r' && *NextBuf != '\0')
      ++NextBuf;

#ifdef __SSE2__
FoundSpecialChar:
#endif
    Offs += NextBuf-Buf;
    Buf = NextBuf;

    if (Buf[0] == '\n' || Buf[0] == '\r') {
      // If this is \n\r or \r\n, skip both characters.
      if ((Buf[1] == '\n' || Buf[1] == '\r') && Buf[0] != Buf[1])
        ++Offs, ++Buf;
      ++Offs, ++Buf;
      LineOffsets.push_back(Offs);
    } else {
      // Otherwise, this is a null.  If end of file, exit.
      if (Buf == End) break;
      // Otherwise, skip the null.
      ++Offs, ++Buf;
    }
  }

  // Copy the offsets into the FileInfo structure.
  FI->NumLines = LineOffsets.size();
  FI->SourceLineCache = Alloc.Allocate<unsigned>(LineOffsets.size());
  std::copy(LineOffsets.begin(), LineOffsets.end(), FI->SourceLineCache);
}

/// getLineNumber - Given a SourceLocation, return the spelling line number
/// for the position indicated.  This requires building and caching a table of
/// line offsets for the MemoryBuffer, so this is not cheap: use only when
/// about to emit a diagnostic.
unsigned SourceManager::getLineNumber(FileID FID, unsigned FilePos, 
                                      bool *Invalid) const {
  if (FID.isInvalid()) {
    if (Invalid)
      *Invalid = true;
    return 1;
  }

  ContentCache *Content;
  if (LastLineNoFileIDQuery == FID)
    Content = LastLineNoContentCache;
  else {
    bool MyInvalid = false;
    const SLocEntry &Entry = getSLocEntry(FID, &MyInvalid);
    if (MyInvalid || !Entry.isFile()) {
      if (Invalid)
        *Invalid = true;
      return 1;
    }
    
    Content = const_cast<ContentCache*>(Entry.getFile().getContentCache());
  }
  
  // If this is the first use of line information for this buffer, compute the
  /// SourceLineCache for it on demand.
  if (Content->SourceLineCache == 0) {
    bool MyInvalid = false;
    ComputeLineNumbers(Diag, Content, ContentCacheAlloc, *this, MyInvalid);
    if (Invalid)
      *Invalid = MyInvalid;
    if (MyInvalid)
      return 1;
  } else if (Invalid)
    *Invalid = false;

  // Okay, we know we have a line number table.  Do a binary search to find the
  // line number that this character position lands on.
  unsigned *SourceLineCache = Content->SourceLineCache;
  unsigned *SourceLineCacheStart = SourceLineCache;
  unsigned *SourceLineCacheEnd = SourceLineCache + Content->NumLines;

  unsigned QueriedFilePos = FilePos+1;

  // FIXME: I would like to be convinced that this code is worth being as
  // complicated as it is, binary search isn't that slow.
  //
  // If it is worth being optimized, then in my opinion it could be more
  // performant, simpler, and more obviously correct by just "galloping" outward
  // from the queried file position. In fact, this could be incorporated into a
  // generic algorithm such as lower_bound_with_hint.
  //
  // If someone gives me a test case where this matters, and I will do it! - DWD

  // If the previous query was to the same file, we know both the file pos from
  // that query and the line number returned.  This allows us to narrow the
  // search space from the entire file to something near the match.
  if (LastLineNoFileIDQuery == FID) {
    if (QueriedFilePos >= LastLineNoFilePos) {
      // FIXME: Potential overflow?
      SourceLineCache = SourceLineCache+LastLineNoResult-1;

      // The query is likely to be nearby the previous one.  Here we check to
      // see if it is within 5, 10 or 20 lines.  It can be far away in cases
      // where big comment blocks and vertical whitespace eat up lines but
      // contribute no tokens.
      if (SourceLineCache+5 < SourceLineCacheEnd) {
        if (SourceLineCache[5] > QueriedFilePos)
          SourceLineCacheEnd = SourceLineCache+5;
        else if (SourceLineCache+10 < SourceLineCacheEnd) {
          if (SourceLineCache[10] > QueriedFilePos)
            SourceLineCacheEnd = SourceLineCache+10;
          else if (SourceLineCache+20 < SourceLineCacheEnd) {
            if (SourceLineCache[20] > QueriedFilePos)
              SourceLineCacheEnd = SourceLineCache+20;
          }
        }
      }
    } else {
      if (LastLineNoResult < Content->NumLines)
        SourceLineCacheEnd = SourceLineCache+LastLineNoResult+1;
    }
  }

  // If the spread is large, do a "radix" test as our initial guess, based on
  // the assumption that lines average to approximately the same length.
  // NOTE: This is currently disabled, as it does not appear to be profitable in
  // initial measurements.
  if (0 && SourceLineCacheEnd-SourceLineCache > 20) {
    unsigned FileLen = Content->SourceLineCache[Content->NumLines-1];

    // Take a stab at guessing where it is.
    unsigned ApproxPos = Content->NumLines*QueriedFilePos / FileLen;

    // Check for -10 and +10 lines.
    unsigned LowerBound = std::max(int(ApproxPos-10), 0);
    unsigned UpperBound = std::min(ApproxPos+10, FileLen);

    // If the computed lower bound is less than the query location, move it in.
    if (SourceLineCache < SourceLineCacheStart+LowerBound &&
        SourceLineCacheStart[LowerBound] < QueriedFilePos)
      SourceLineCache = SourceLineCacheStart+LowerBound;

    // If the computed upper bound is greater than the query location, move it.
    if (SourceLineCacheEnd > SourceLineCacheStart+UpperBound &&
        SourceLineCacheStart[UpperBound] >= QueriedFilePos)
      SourceLineCacheEnd = SourceLineCacheStart+UpperBound;
  }

  unsigned *Pos
    = std::lower_bound(SourceLineCache, SourceLineCacheEnd, QueriedFilePos);
  unsigned LineNo = Pos-SourceLineCacheStart;

  LastLineNoFileIDQuery = FID;
  LastLineNoContentCache = Content;
  LastLineNoFilePos = QueriedFilePos;
  LastLineNoResult = LineNo;
  return LineNo;
}

unsigned SourceManager::getSpellingLineNumber(SourceLocation Loc, 
                                              bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return 0;
  std::pair<FileID, unsigned> LocInfo = getDecomposedSpellingLoc(Loc);
  return getLineNumber(LocInfo.first, LocInfo.second);
}
unsigned SourceManager::getExpansionLineNumber(SourceLocation Loc,
                                               bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return 0;
  std::pair<FileID, unsigned> LocInfo = getDecomposedExpansionLoc(Loc);
  return getLineNumber(LocInfo.first, LocInfo.second);
}
unsigned SourceManager::getPresumedLineNumber(SourceLocation Loc,
                                              bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return 0;
  return getPresumedLoc(Loc).getLine();
}

/// getFileCharacteristic - return the file characteristic of the specified
/// source location, indicating whether this is a normal file, a system
/// header, or an "implicit extern C" system header.
///
/// This state can be modified with flags on GNU linemarker directives like:
///   # 4 "foo.h" 3
/// which changes all source locations in the current file after that to be
/// considered to be from a system header.
SrcMgr::CharacteristicKind
SourceManager::getFileCharacteristic(SourceLocation Loc) const {
  assert(!Loc.isInvalid() && "Can't get file characteristic of invalid loc!");
  std::pair<FileID, unsigned> LocInfo = getDecomposedExpansionLoc(Loc);
  bool Invalid = false;
  const SLocEntry &SEntry = getSLocEntry(LocInfo.first, &Invalid);
  if (Invalid || !SEntry.isFile())
    return C_User;
  
  const SrcMgr::FileInfo &FI = SEntry.getFile();

  // If there are no #line directives in this file, just return the whole-file
  // state.
  if (!FI.hasLineDirectives())
    return FI.getFileCharacteristic();

  assert(LineTable && "Can't have linetable entries without a LineTable!");
  // See if there is a #line directive before the location.
  const LineEntry *Entry =
    LineTable->FindNearestLineEntry(LocInfo.first, LocInfo.second);

  // If this is before the first line marker, use the file characteristic.
  if (!Entry)
    return FI.getFileCharacteristic();

  return Entry->FileKind;
}

/// Return the filename or buffer identifier of the buffer the location is in.
/// Note that this name does not respect \#line directives.  Use getPresumedLoc
/// for normal clients.
const char *SourceManager::getBufferName(SourceLocation Loc, 
                                         bool *Invalid) const {
  if (isInvalid(Loc, Invalid)) return "<invalid loc>";

  return getBuffer(getFileID(Loc), Invalid)->getBufferIdentifier();
}


/// getPresumedLoc - This method returns the "presumed" location of a
/// SourceLocation specifies.  A "presumed location" can be modified by \#line
/// or GNU line marker directives.  This provides a view on the data that a
/// user should see in diagnostics, for example.
///
/// Note that a presumed location is always given as the expansion point of an
/// expansion location, not at the spelling location.
PresumedLoc SourceManager::getPresumedLoc(SourceLocation Loc,
                                          bool UseLineDirectives) const {
  if (Loc.isInvalid()) return PresumedLoc();

  // Presumed locations are always for expansion points.
  std::pair<FileID, unsigned> LocInfo = getDecomposedExpansionLoc(Loc);

  bool Invalid = false;
  const SLocEntry &Entry = getSLocEntry(LocInfo.first, &Invalid);
  if (Invalid || !Entry.isFile())
    return PresumedLoc();
  
  const SrcMgr::FileInfo &FI = Entry.getFile();
  const SrcMgr::ContentCache *C = FI.getContentCache();

  // To get the source name, first consult the FileEntry (if one exists)
  // before the MemBuffer as this will avoid unnecessarily paging in the
  // MemBuffer.
  const char *Filename;
  if (C->OrigEntry)
    Filename = C->OrigEntry->getName();
  else
    Filename = C->getBuffer(Diag, *this)->getBufferIdentifier();

  unsigned LineNo = getLineNumber(LocInfo.first, LocInfo.second, &Invalid);
  if (Invalid)
    return PresumedLoc();
  unsigned ColNo  = getColumnNumber(LocInfo.first, LocInfo.second, &Invalid);
  if (Invalid)
    return PresumedLoc();
  
  SourceLocation IncludeLoc = FI.getIncludeLoc();

  // If we have #line directives in this file, update and overwrite the physical
  // location info if appropriate.
  if (UseLineDirectives && FI.hasLineDirectives()) {
    assert(LineTable && "Can't have linetable entries without a LineTable!");
    // See if there is a #line directive before this.  If so, get it.
    if (const LineEntry *Entry =
          LineTable->FindNearestLineEntry(LocInfo.first, LocInfo.second)) {
      // If the LineEntry indicates a filename, use it.
      if (Entry->FilenameID != -1)
        Filename = LineTable->getFilename(Entry->FilenameID);

      // Use the line number specified by the LineEntry.  This line number may
      // be multiple lines down from the line entry.  Add the difference in
      // physical line numbers from the query point and the line marker to the
      // total.
      unsigned MarkerLineNo = getLineNumber(LocInfo.first, Entry->FileOffset);
      LineNo = Entry->LineNo + (LineNo-MarkerLineNo-1);

      // Note that column numbers are not molested by line markers.

      // Handle virtual #include manipulation.
      if (Entry->IncludeOffset) {
        IncludeLoc = getLocForStartOfFile(LocInfo.first);
        IncludeLoc = IncludeLoc.getLocWithOffset(Entry->IncludeOffset);
      }
    }
  }

  return PresumedLoc(Filename, LineNo, ColNo, IncludeLoc);
}

/// \brief The size of the SLocEnty that \arg FID represents.
unsigned SourceManager::getFileIDSize(FileID FID) const {
  bool Invalid = false;
  const SrcMgr::SLocEntry &Entry = getSLocEntry(FID, &Invalid);
  if (Invalid)
    return 0;

  int ID = FID.ID;
  unsigned NextOffset;
  if ((ID > 0 && unsigned(ID+1) == local_sloc_entry_size()))
    NextOffset = getNextLocalOffset();
  else if (ID+1 == -1)
    NextOffset = MaxLoadedOffset;
  else
    NextOffset = getSLocEntry(FileID::get(ID+1)).getOffset();

  return NextOffset - Entry.getOffset() - 1;
}

//===----------------------------------------------------------------------===//
// Other miscellaneous methods.
//===----------------------------------------------------------------------===//

/// \brief Retrieve the inode for the given file entry, if possible.
///
/// This routine involves a system call, and therefore should only be used
/// in non-performance-critical code.
static Optional<ino_t> getActualFileInode(const FileEntry *File) {
  if (!File)
    return None;
  
  struct stat StatBuf;
  if (::stat(File->getName(), &StatBuf))
    return None;
    
  return StatBuf.st_ino;
}

/// \brief Get the source location for the given file:line:col triplet.
///
/// If the source file is included multiple times, the source location will
/// be based upon an arbitrary inclusion.
SourceLocation SourceManager::translateFileLineCol(const FileEntry *SourceFile,
                                                  unsigned Line,
                                                  unsigned Col) const {
  assert(SourceFile && "Null source file!");
  assert(Line && Col && "Line and column should start from 1!");

  FileID FirstFID = translateFile(SourceFile);
  return translateLineCol(FirstFID, Line, Col);
}

/// \brief Get the FileID for the given file.
///
/// If the source file is included multiple times, the FileID will be the
/// first inclusion.
FileID SourceManager::translateFile(const FileEntry *SourceFile) const {
  assert(SourceFile && "Null source file!");

  // Find the first file ID that corresponds to the given file.
  FileID FirstFID;

  // First, check the main file ID, since it is common to look for a
  // location in the main file.
  Optional<ino_t> SourceFileInode;
  Optional<StringRef> SourceFileName;
  if (!MainFileID.isInvalid()) {
    bool Invalid = false;
    const SLocEntry &MainSLoc = getSLocEntry(MainFileID, &Invalid);
    if (Invalid)
      return FileID();
    
    if (MainSLoc.isFile()) {
      const ContentCache *MainContentCache
        = MainSLoc.getFile().getContentCache();
      if (!MainContentCache) {
        // Can't do anything
      } else if (MainContentCache->OrigEntry == SourceFile) {
        FirstFID = MainFileID;
      } else {
        // Fall back: check whether we have the same base name and inode
        // as the main file.
        const FileEntry *MainFile = MainContentCache->OrigEntry;
        SourceFileName = llvm::sys::path::filename(SourceFile->getName());
        if (*SourceFileName == llvm::sys::path::filename(MainFile->getName())) {
          SourceFileInode = getActualFileInode(SourceFile);
          if (SourceFileInode) {
            if (Optional<ino_t> MainFileInode = getActualFileInode(MainFile)) {
              if (*SourceFileInode == *MainFileInode) {
                FirstFID = MainFileID;
                SourceFile = MainFile;
              }
            }
          }
        }
      }
    }
  }

  if (FirstFID.isInvalid()) {
    // The location we're looking for isn't in the main file; look
    // through all of the local source locations.
    for (unsigned I = 0, N = local_sloc_entry_size(); I != N; ++I) {
      bool Invalid = false;
      const SLocEntry &SLoc = getLocalSLocEntry(I, &Invalid);
      if (Invalid)
        return FileID();
      
      if (SLoc.isFile() && 
          SLoc.getFile().getContentCache() &&
          SLoc.getFile().getContentCache()->OrigEntry == SourceFile) {
        FirstFID = FileID::get(I);
        break;
      }
    }
    // If that still didn't help, try the modules.
    if (FirstFID.isInvalid()) {
      for (unsigned I = 0, N = loaded_sloc_entry_size(); I != N; ++I) {
        const SLocEntry &SLoc = getLoadedSLocEntry(I);
        if (SLoc.isFile() && 
            SLoc.getFile().getContentCache() &&
            SLoc.getFile().getContentCache()->OrigEntry == SourceFile) {
          FirstFID = FileID::get(-int(I) - 2);
          break;
        }
      }
    }
  }

  // If we haven't found what we want yet, try again, but this time stat()
  // each of the files in case the files have changed since we originally 
  // parsed the file. 
  if (FirstFID.isInvalid() &&
      (SourceFileName || 
       (SourceFileName = llvm::sys::path::filename(SourceFile->getName()))) &&
      (SourceFileInode ||
       (SourceFileInode = getActualFileInode(SourceFile)))) {
    bool Invalid = false;
    for (unsigned I = 0, N = local_sloc_entry_size(); I != N; ++I) {
      FileID IFileID;
      IFileID.ID = I;
      const SLocEntry &SLoc = getSLocEntry(IFileID, &Invalid);
      if (Invalid)
        return FileID();
      
      if (SLoc.isFile()) { 
        const ContentCache *FileContentCache 
          = SLoc.getFile().getContentCache();
      const FileEntry *Entry =FileContentCache? FileContentCache->OrigEntry : 0;
        if (Entry && 
            *SourceFileName == llvm::sys::path::filename(Entry->getName())) {
          if (Optional<ino_t> EntryInode = getActualFileInode(Entry)) {
            if (*SourceFileInode == *EntryInode) {
              FirstFID = FileID::get(I);
              SourceFile = Entry;
              break;
            }
          }
        }
      }
    }      
  }
  
  (void) SourceFile;
  return FirstFID;
}

/// \brief Get the source location in \arg FID for the given line:col.
/// Returns null location if \arg FID is not a file SLocEntry.
SourceLocation SourceManager::translateLineCol(FileID FID,
                                               unsigned Line,
                                               unsigned Col) const {
  if (FID.isInvalid())
    return SourceLocation();

  bool Invalid = false;
  const SLocEntry &Entry = getSLocEntry(FID, &Invalid);
  if (Invalid)
    return SourceLocation();
  
  if (!Entry.isFile())
    return SourceLocation();

  SourceLocation FileLoc = SourceLocation::getFileLoc(Entry.getOffset());

  if (Line == 1 && Col == 1)
    return FileLoc;

  ContentCache *Content
    = const_cast<ContentCache *>(Entry.getFile().getContentCache());
  if (!Content)
    return SourceLocation();
    
  // If this is the first use of line information for this buffer, compute the
  // SourceLineCache for it on demand.
  if (Content->SourceLineCache == 0) {
    bool MyInvalid = false;
    ComputeLineNumbers(Diag, Content, ContentCacheAlloc, *this, MyInvalid);
    if (MyInvalid)
      return SourceLocation();
  }

  if (Line > Content->NumLines) {
    unsigned Size = Content->getBuffer(Diag, *this)->getBufferSize();
    if (Size > 0)
      --Size;
    return FileLoc.getLocWithOffset(Size);
  }

  const llvm::MemoryBuffer *Buffer = Content->getBuffer(Diag, *this);
  unsigned FilePos = Content->SourceLineCache[Line - 1];
  const char *Buf = Buffer->getBufferStart() + FilePos;
  unsigned BufLength = Buffer->getBufferSize() - FilePos;
  if (BufLength == 0)
    return FileLoc.getLocWithOffset(FilePos);

  unsigned i = 0;

  // Check that the given column is valid.
  while (i < BufLength-1 && i < Col-1 && Buf[i] != '\n' && Buf[i] != '\r')
    ++i;
  if (i < Col-1)
    return FileLoc.getLocWithOffset(FilePos + i);

  return FileLoc.getLocWithOffset(FilePos + Col - 1);
}

/// \brief Compute a map of macro argument chunks to their expanded source
/// location. Chunks that are not part of a macro argument will map to an
/// invalid source location. e.g. if a file contains one macro argument at
/// offset 100 with length 10, this is how the map will be formed:
///     0   -> SourceLocation()
///     100 -> Expanded macro arg location
///     110 -> SourceLocation()
void SourceManager::computeMacroArgsCache(MacroArgsMap *&CachePtr,
                                          FileID FID) const {
  assert(!FID.isInvalid());
  assert(!CachePtr);

  CachePtr = new MacroArgsMap();
  MacroArgsMap &MacroArgsCache = *CachePtr;
  // Initially no macro argument chunk is present.
  MacroArgsCache.insert(std::make_pair(0, SourceLocation()));

  int ID = FID.ID;
  while (1) {
    ++ID;
    // Stop if there are no more FileIDs to check.
    if (ID > 0) {
      if (unsigned(ID) >= local_sloc_entry_size())
        return;
    } else if (ID == -1) {
      return;
    }

    const SrcMgr::SLocEntry &Entry = getSLocEntryByID(ID);
    if (Entry.isFile()) {
      SourceLocation IncludeLoc = Entry.getFile().getIncludeLoc();
      if (IncludeLoc.isInvalid())
        continue;
      if (!isInFileID(IncludeLoc, FID))
        return; // No more files/macros that may be "contained" in this file.

      // Skip the files/macros of the #include'd file, we only care about macros
      // that lexed macro arguments from our file.
      if (Entry.getFile().NumCreatedFIDs)
        ID += Entry.getFile().NumCreatedFIDs - 1/*because of next ++ID*/;
      continue;
    }

    const ExpansionInfo &ExpInfo = Entry.getExpansion();

    if (ExpInfo.getExpansionLocStart().isFileID()) {
      if (!isInFileID(ExpInfo.getExpansionLocStart(), FID))
        return; // No more files/macros that may be "contained" in this file.
    }

    if (!ExpInfo.isMacroArgExpansion())
      continue;

    associateFileChunkWithMacroArgExp(MacroArgsCache, FID,
                                 ExpInfo.getSpellingLoc(),
                                 SourceLocation::getMacroLoc(Entry.getOffset()),
                                 getFileIDSize(FileID::get(ID)));
  }
}

void SourceManager::associateFileChunkWithMacroArgExp(
                                         MacroArgsMap &MacroArgsCache,
                                         FileID FID,
                                         SourceLocation SpellLoc,
                                         SourceLocation ExpansionLoc,
                                         unsigned ExpansionLength) const {
  if (!SpellLoc.isFileID()) {
    unsigned SpellBeginOffs = SpellLoc.getOffset();
    unsigned SpellEndOffs = SpellBeginOffs + ExpansionLength;

    // The spelling range for this macro argument expansion can span multiple
    // consecutive FileID entries. Go through each entry contained in the
    // spelling range and if one is itself a macro argument expansion, recurse
    // and associate the file chunk that it represents.

    FileID SpellFID; // Current FileID in the spelling range.
    unsigned SpellRelativeOffs;
    llvm::tie(SpellFID, SpellRelativeOffs) = getDecomposedLoc(SpellLoc);
    while (1) {
      const SLocEntry &Entry = getSLocEntry(SpellFID);
      unsigned SpellFIDBeginOffs = Entry.getOffset();
      unsigned SpellFIDSize = getFileIDSize(SpellFID);
      unsigned SpellFIDEndOffs = SpellFIDBeginOffs + SpellFIDSize;
      const ExpansionInfo &Info = Entry.getExpansion();
      if (Info.isMacroArgExpansion()) {
        unsigned CurrSpellLength;
        if (SpellFIDEndOffs < SpellEndOffs)
          CurrSpellLength = SpellFIDSize - SpellRelativeOffs;
        else
          CurrSpellLength = ExpansionLength;
        associateFileChunkWithMacroArgExp(MacroArgsCache, FID,
                      Info.getSpellingLoc().getLocWithOffset(SpellRelativeOffs),
                      ExpansionLoc, CurrSpellLength);
      }

      if (SpellFIDEndOffs >= SpellEndOffs)
        return; // we covered all FileID entries in the spelling range.

      // Move to the next FileID entry in the spelling range.
      unsigned advance = SpellFIDSize - SpellRelativeOffs + 1;
      ExpansionLoc = ExpansionLoc.getLocWithOffset(advance);
      ExpansionLength -= advance;
      ++SpellFID.ID;
      SpellRelativeOffs = 0;
    }

  }

  assert(SpellLoc.isFileID());

  unsigned BeginOffs;
  if (!isInFileID(SpellLoc, FID, &BeginOffs))
    return;

  unsigned EndOffs = BeginOffs + ExpansionLength;

  // Add a new chunk for this macro argument. A previous macro argument chunk
  // may have been lexed again, so e.g. if the map is
  //     0   -> SourceLocation()
  //     100 -> Expanded loc #1
  //     110 -> SourceLocation()
  // and we found a new macro FileID that lexed from offet 105 with length 3,
  // the new map will be:
  //     0   -> SourceLocation()
  //     100 -> Expanded loc #1
  //     105 -> Expanded loc #2
  //     108 -> Expanded loc #1
  //     110 -> SourceLocation()
  //
  // Since re-lexed macro chunks will always be the same size or less of
  // previous chunks, we only need to find where the ending of the new macro
  // chunk is mapped to and update the map with new begin/end mappings.

  MacroArgsMap::iterator I = MacroArgsCache.upper_bound(EndOffs);
  --I;
  SourceLocation EndOffsMappedLoc = I->second;
  MacroArgsCache[BeginOffs] = ExpansionLoc;
  MacroArgsCache[EndOffs] = EndOffsMappedLoc;
}

/// \brief If \arg Loc points inside a function macro argument, the returned
/// location will be the macro location in which the argument was expanded.
/// If a macro argument is used multiple times, the expanded location will
/// be at the first expansion of the argument.
/// e.g.
///   MY_MACRO(foo);
///             ^
/// Passing a file location pointing at 'foo', will yield a macro location
/// where 'foo' was expanded into.
SourceLocation
SourceManager::getMacroArgExpandedLocation(SourceLocation Loc) const {
  if (Loc.isInvalid() || !Loc.isFileID())
    return Loc;

  FileID FID;
  unsigned Offset;
  llvm::tie(FID, Offset) = getDecomposedLoc(Loc);
  if (FID.isInvalid())
    return Loc;

  MacroArgsMap *&MacroArgsCache = MacroArgsCacheMap[FID];
  if (!MacroArgsCache)
    computeMacroArgsCache(MacroArgsCache, FID);

  assert(!MacroArgsCache->empty());
  MacroArgsMap::iterator I = MacroArgsCache->upper_bound(Offset);
  --I;
  
  unsigned MacroArgBeginOffs = I->first;
  SourceLocation MacroArgExpandedLoc = I->second;
  if (MacroArgExpandedLoc.isValid())
    return MacroArgExpandedLoc.getLocWithOffset(Offset - MacroArgBeginOffs);

  return Loc;
}

/// Given a decomposed source location, move it up the include/expansion stack
/// to the parent source location.  If this is possible, return the decomposed
/// version of the parent in Loc and return false.  If Loc is the top-level
/// entry, return true and don't modify it.
static bool MoveUpIncludeHierarchy(std::pair<FileID, unsigned> &Loc,
                                   const SourceManager &SM) {
  SourceLocation UpperLoc;
  const SrcMgr::SLocEntry &Entry = SM.getSLocEntry(Loc.first);
  if (Entry.isExpansion())
    UpperLoc = Entry.getExpansion().getExpansionLocStart();
  else
    UpperLoc = Entry.getFile().getIncludeLoc();
  
  if (UpperLoc.isInvalid())
    return true; // We reached the top.
  
  Loc = SM.getDecomposedLoc(UpperLoc);
  return false;
}

/// Return the cache entry for comparing the given file IDs
/// for isBeforeInTranslationUnit.
InBeforeInTUCacheEntry &SourceManager::getInBeforeInTUCache(FileID LFID,
                                                            FileID RFID) const {
  // This is a magic number for limiting the cache size.  It was experimentally
  // derived from a small Objective-C project (where the cache filled
  // out to ~250 items).  We can make it larger if necessary.
  enum { MagicCacheSize = 300 };
  IsBeforeInTUCacheKey Key(LFID, RFID);

  // If the cache size isn't too large, do a lookup and if necessary default
  // construct an entry.  We can then return it to the caller for direct
  // use.  When they update the value, the cache will get automatically
  // updated as well.
  if (IBTUCache.size() < MagicCacheSize)
    return IBTUCache[Key];

  // Otherwise, do a lookup that will not construct a new value.
  InBeforeInTUCache::iterator I = IBTUCache.find(Key);
  if (I != IBTUCache.end())
    return I->second;

  // Fall back to the overflow value.
  return IBTUCacheOverflow;
}

/// \brief Determines the order of 2 source locations in the translation unit.
///
/// \returns true if LHS source location comes before RHS, false otherwise.
bool SourceManager::isBeforeInTranslationUnit(SourceLocation LHS,
                                              SourceLocation RHS) const {
  assert(LHS.isValid() && RHS.isValid() && "Passed invalid source location!");
  if (LHS == RHS)
    return false;

  std::pair<FileID, unsigned> LOffs = getDecomposedLoc(LHS);
  std::pair<FileID, unsigned> ROffs = getDecomposedLoc(RHS);

  // If the source locations are in the same file, just compare offsets.
  if (LOffs.first == ROffs.first)
    return LOffs.second < ROffs.second;

  // If we are comparing a source location with multiple locations in the same
  // file, we get a big win by caching the result.
  InBeforeInTUCacheEntry &IsBeforeInTUCache =
    getInBeforeInTUCache(LOffs.first, ROffs.first);

  // If we are comparing a source location with multiple locations in the same
  // file, we get a big win by caching the result.
  if (IsBeforeInTUCache.isCacheValid(LOffs.first, ROffs.first))
    return IsBeforeInTUCache.getCachedResult(LOffs.second, ROffs.second);

  // Okay, we missed in the cache, start updating the cache for this query.
  IsBeforeInTUCache.setQueryFIDs(LOffs.first, ROffs.first,
                          /*isLFIDBeforeRFID=*/LOffs.first.ID < ROffs.first.ID);

  // We need to find the common ancestor. The only way of doing this is to
  // build the complete include chain for one and then walking up the chain
  // of the other looking for a match.
  // We use a map from FileID to Offset to store the chain. Easier than writing
  // a custom set hash info that only depends on the first part of a pair.
  typedef llvm::DenseMap<FileID, unsigned> LocSet;
  LocSet LChain;
  do {
    LChain.insert(LOffs);
    // We catch the case where LOffs is in a file included by ROffs and
    // quit early. The other way round unfortunately remains suboptimal.
  } while (LOffs.first != ROffs.first && !MoveUpIncludeHierarchy(LOffs, *this));
  LocSet::iterator I;
  while((I = LChain.find(ROffs.first)) == LChain.end()) {
    if (MoveUpIncludeHierarchy(ROffs, *this))
      break; // Met at topmost file.
  }
  if (I != LChain.end())
    LOffs = *I;

  // If we exited because we found a nearest common ancestor, compare the
  // locations within the common file and cache them.
  if (LOffs.first == ROffs.first) {
    IsBeforeInTUCache.setCommonLoc(LOffs.first, LOffs.second, ROffs.second);
    return IsBeforeInTUCache.getCachedResult(LOffs.second, ROffs.second);
  }

  // This can happen if a location is in a built-ins buffer.
  // But see PR5662.
  // Clear the lookup cache, it depends on a common location.
  IsBeforeInTUCache.clear();
  bool LIsBuiltins = strcmp("<built-in>",
                            getBuffer(LOffs.first)->getBufferIdentifier()) == 0;
  bool RIsBuiltins = strcmp("<built-in>",
                            getBuffer(ROffs.first)->getBufferIdentifier()) == 0;
  // built-in is before non-built-in
  if (LIsBuiltins != RIsBuiltins)
    return LIsBuiltins;
  assert(LIsBuiltins && RIsBuiltins &&
         "Non-built-in locations must be rooted in the main file");
  // Both are in built-in buffers, but from different files. We just claim that
  // lower IDs come first.
  return LOffs.first < ROffs.first;
}

void SourceManager::PrintStats() const {
  llvm::errs() << "\n*** Source Manager Stats:\n";
  llvm::errs() << FileInfos.size() << " files mapped, " << MemBufferInfos.size()
               << " mem buffers mapped.\n";
  llvm::errs() << LocalSLocEntryTable.size() << " local SLocEntry's allocated ("
               << llvm::capacity_in_bytes(LocalSLocEntryTable)
               << " bytes of capacity), "
               << NextLocalOffset << "B of Sloc address space used.\n";
  llvm::errs() << LoadedSLocEntryTable.size()
               << " loaded SLocEntries allocated, "
               << MaxLoadedOffset - CurrentLoadedOffset
               << "B of Sloc address space used.\n";
  
  unsigned NumLineNumsComputed = 0;
  unsigned NumFileBytesMapped = 0;
  for (fileinfo_iterator I = fileinfo_begin(), E = fileinfo_end(); I != E; ++I){
    NumLineNumsComputed += I->second->SourceLineCache != 0;
    NumFileBytesMapped  += I->second->getSizeBytesMapped();
  }
  unsigned NumMacroArgsComputed = MacroArgsCacheMap.size();

  llvm::errs() << NumFileBytesMapped << " bytes of files mapped, "
               << NumLineNumsComputed << " files with line #'s computed, "
               << NumMacroArgsComputed << " files with macro args computed.\n";
  llvm::errs() << "FileID scans: " << NumLinearScans << " linear, "
               << NumBinaryProbes << " binary.\n";
}

ExternalSLocEntrySource::~ExternalSLocEntrySource() { }

/// Return the amount of memory used by memory buffers, breaking down
/// by heap-backed versus mmap'ed memory.
SourceManager::MemoryBufferSizes SourceManager::getMemoryBufferSizes() const {
  size_t malloc_bytes = 0;
  size_t mmap_bytes = 0;
  
  for (unsigned i = 0, e = MemBufferInfos.size(); i != e; ++i)
    if (size_t sized_mapped = MemBufferInfos[i]->getSizeBytesMapped())
      switch (MemBufferInfos[i]->getMemoryBufferKind()) {
        case llvm::MemoryBuffer::MemoryBuffer_MMap:
          mmap_bytes += sized_mapped;
          break;
        case llvm::MemoryBuffer::MemoryBuffer_Malloc:
          malloc_bytes += sized_mapped;
          break;
      }
  
  return MemoryBufferSizes(malloc_bytes, mmap_bytes);
}

size_t SourceManager::getDataStructureSizes() const {
  size_t size = llvm::capacity_in_bytes(MemBufferInfos)
    + llvm::capacity_in_bytes(LocalSLocEntryTable)
    + llvm::capacity_in_bytes(LoadedSLocEntryTable)
    + llvm::capacity_in_bytes(SLocEntryLoaded)
    + llvm::capacity_in_bytes(FileInfos);
  
  if (OverriddenFilesInfo)
    size += llvm::capacity_in_bytes(OverriddenFilesInfo->OverriddenFiles);

  return size;
}
