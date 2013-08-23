//===-- SBTypeFormat.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SBTypeFormat_h_
#define LLDB_SBTypeFormat_h_

#include "lldb/API/SBDefines.h"

namespace lldb {

class SBTypeFormat
{
public:
    
    SBTypeFormat();
    
    SBTypeFormat (lldb::Format format,
                  uint32_t options = 0); // see lldb::eTypeOption values
    
    SBTypeFormat (const lldb::SBTypeFormat &rhs);
    
    ~SBTypeFormat ();
    
    bool
    IsValid() const;
    
    lldb::Format
    GetFormat ();
    
    uint32_t
    GetOptions();
    
    void
    SetFormat (lldb::Format);
    
    void
    SetOptions (uint32_t);
    
    bool
    GetDescription (lldb::SBStream &description, 
                    lldb::DescriptionLevel description_level);
    
    lldb::SBTypeFormat &
    operator = (const lldb::SBTypeFormat &rhs);
    
    bool
    IsEqualTo (lldb::SBTypeFormat &rhs);

    bool
    operator == (lldb::SBTypeFormat &rhs);
    
    bool
    operator != (lldb::SBTypeFormat &rhs);
    
protected:
    friend class SBDebugger;
    friend class SBTypeCategory;
    friend class SBValue;
    
    lldb::TypeFormatImplSP
    GetSP ();
    
    void
    SetSP (const lldb::TypeFormatImplSP &typeformat_impl_sp);    
    
    lldb::TypeFormatImplSP m_opaque_sp;
    
    SBTypeFormat (const lldb::TypeFormatImplSP &);
    
    bool
    CopyOnWrite_Impl();
    
};

    
} // namespace lldb

#endif // LLDB_SBTypeFormat_h_
