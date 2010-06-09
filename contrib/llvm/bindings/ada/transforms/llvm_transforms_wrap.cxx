/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.36
 * 
 * This file is not intended to be easily readable and contains a number of 
 * coding conventions designed to improve portability and efficiency. Do not make
 * changes to this file unless you know what you are doing--modify the SWIG 
 * interface file instead. 
 * ----------------------------------------------------------------------------- */


#ifdef __cplusplus
template<typename T> class SwigValueWrapper {
    T *tt;
public:
    SwigValueWrapper() : tt(0) { }
    SwigValueWrapper(const SwigValueWrapper<T>& rhs) : tt(new T(*rhs.tt)) { }
    SwigValueWrapper(const T& t) : tt(new T(t)) { }
    ~SwigValueWrapper() { delete tt; } 
    SwigValueWrapper& operator=(const T& t) { delete tt; tt = new T(t); return *this; }
    operator T&() const { return *tt; }
    T *operator&() { return tt; }
private:
    SwigValueWrapper& operator=(const SwigValueWrapper<T>& rhs);
};

template <typename T> T SwigValueInit() {
  return T();
}
#endif

/* -----------------------------------------------------------------------------
 *  This section contains generic SWIG labels for method/variable
 *  declarations/attributes, and other compiler dependent labels.
 * ----------------------------------------------------------------------------- */

/* template workaround for compilers that cannot correctly implement the C++ standard */
#ifndef SWIGTEMPLATEDISAMBIGUATOR
# if defined(__SUNPRO_CC) && (__SUNPRO_CC <= 0x560)
#  define SWIGTEMPLATEDISAMBIGUATOR template
# elif defined(__HP_aCC)
/* Needed even with `aCC -AA' when `aCC -V' reports HP ANSI C++ B3910B A.03.55 */
/* If we find a maximum version that requires this, the test would be __HP_aCC <= 35500 for A.03.55 */
#  define SWIGTEMPLATEDISAMBIGUATOR template
# else
#  define SWIGTEMPLATEDISAMBIGUATOR
# endif
#endif

/* inline attribute */
#ifndef SWIGINLINE
# if defined(__cplusplus) || (defined(__GNUC__) && !defined(__STRICT_ANSI__))
#   define SWIGINLINE inline
# else
#   define SWIGINLINE
# endif
#endif

/* attribute recognised by some compilers to avoid 'unused' warnings */
#ifndef SWIGUNUSED
# if defined(__GNUC__)
#   if !(defined(__cplusplus)) || (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
#     define SWIGUNUSED __attribute__ ((__unused__)) 
#   else
#     define SWIGUNUSED
#   endif
# elif defined(__ICC)
#   define SWIGUNUSED __attribute__ ((__unused__)) 
# else
#   define SWIGUNUSED 
# endif
#endif

#ifndef SWIGUNUSEDPARM
# ifdef __cplusplus
#   define SWIGUNUSEDPARM(p)
# else
#   define SWIGUNUSEDPARM(p) p SWIGUNUSED 
# endif
#endif

/* internal SWIG method */
#ifndef SWIGINTERN
# define SWIGINTERN static SWIGUNUSED
#endif

/* internal inline SWIG method */
#ifndef SWIGINTERNINLINE
# define SWIGINTERNINLINE SWIGINTERN SWIGINLINE
#endif

/* exporting methods */
#if (__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  ifndef GCC_HASCLASSVISIBILITY
#    define GCC_HASCLASSVISIBILITY
#  endif
#endif

#ifndef SWIGEXPORT
# if defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#   if defined(STATIC_LINKED)
#     define SWIGEXPORT
#   else
#     define SWIGEXPORT __declspec(dllexport)
#   endif
# else
#   if defined(__GNUC__) && defined(GCC_HASCLASSVISIBILITY)
#     define SWIGEXPORT __attribute__ ((visibility("default")))
#   else
#     define SWIGEXPORT
#   endif
# endif
#endif

/* calling conventions for Windows */
#ifndef SWIGSTDCALL
# if defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#   define SWIGSTDCALL __stdcall
# else
#   define SWIGSTDCALL
# endif 
#endif

/* Deal with Microsoft's attempt at deprecating C standard runtime functions */
#if !defined(SWIG_NO_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER) && !defined(_CRT_SECURE_NO_DEPRECATE)
# define _CRT_SECURE_NO_DEPRECATE
#endif

/* Deal with Microsoft's attempt at deprecating methods in the standard C++ library */
#if !defined(SWIG_NO_SCL_SECURE_NO_DEPRECATE) && defined(_MSC_VER) && !defined(_SCL_SECURE_NO_DEPRECATE)
# define _SCL_SECURE_NO_DEPRECATE
#endif



#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(_WIN32) || defined(__CYGWIN32__)
#  define DllExport   __declspec( dllexport )
#  define SWIGSTDCALL __stdcall
#else
#  define DllExport  
#  define SWIGSTDCALL
#endif 


#ifdef __cplusplus
#  include <new>
#endif




/* Support for throwing Ada exceptions from C/C++ */

typedef enum 
{
  SWIG_AdaException,
  SWIG_AdaOutOfMemoryException,
  SWIG_AdaIndexOutOfRangeException,
  SWIG_AdaDivideByZeroException,
  SWIG_AdaArgumentOutOfRangeException,
  SWIG_AdaNullReferenceException
} SWIG_AdaExceptionCodes;


typedef void (SWIGSTDCALL* SWIG_AdaExceptionCallback_t)(const char *);


typedef struct 
{
  SWIG_AdaExceptionCodes code;
  SWIG_AdaExceptionCallback_t callback;
} 
  SWIG_AdaExceptions_t;


static 
SWIG_AdaExceptions_t 
SWIG_ada_exceptions[] = 
{
  { SWIG_AdaException, NULL },
  { SWIG_AdaOutOfMemoryException, NULL },
  { SWIG_AdaIndexOutOfRangeException, NULL },
  { SWIG_AdaDivideByZeroException, NULL },
  { SWIG_AdaArgumentOutOfRangeException, NULL },
  { SWIG_AdaNullReferenceException, NULL } 
};


static 
void 
SWIG_AdaThrowException (SWIG_AdaExceptionCodes code, const char *msg) 
{
  SWIG_AdaExceptionCallback_t callback = SWIG_ada_exceptions[SWIG_AdaException].callback;
  if (code >=0 && (size_t)code < sizeof(SWIG_ada_exceptions)/sizeof(SWIG_AdaExceptions_t)) {
    callback = SWIG_ada_exceptions[code].callback;
  }
  callback(msg);
}



#ifdef __cplusplus
extern "C" 
#endif

DllExport void SWIGSTDCALL SWIGRegisterExceptionCallbacks_LLVM_Transforms (SWIG_AdaExceptionCallback_t systemException,
                                                                   SWIG_AdaExceptionCallback_t outOfMemory, 
                                                                   SWIG_AdaExceptionCallback_t indexOutOfRange, 
                                                                   SWIG_AdaExceptionCallback_t divideByZero, 
                                                                   SWIG_AdaExceptionCallback_t argumentOutOfRange,
                                                                   SWIG_AdaExceptionCallback_t nullReference) 
{
  SWIG_ada_exceptions [SWIG_AdaException].callback                   = systemException;
  SWIG_ada_exceptions [SWIG_AdaOutOfMemoryException].callback        = outOfMemory;
  SWIG_ada_exceptions [SWIG_AdaIndexOutOfRangeException].callback    = indexOutOfRange;
  SWIG_ada_exceptions [SWIG_AdaDivideByZeroException].callback       = divideByZero;
  SWIG_ada_exceptions [SWIG_AdaArgumentOutOfRangeException].callback = argumentOutOfRange;
  SWIG_ada_exceptions [SWIG_AdaNullReferenceException].callback      = nullReference;
}


/* Callback for returning strings to Ada without leaking memory */

typedef char * (SWIGSTDCALL* SWIG_AdaStringHelperCallback)(const char *);
static SWIG_AdaStringHelperCallback SWIG_ada_string_callback = NULL;



/* probably obsolete ...
#ifdef __cplusplus
extern "C" 
#endif
DllExport void SWIGSTDCALL SWIGRegisterStringCallback_LLVM_Transforms(SWIG_AdaStringHelperCallback callback) {
  SWIG_ada_string_callback = callback;
}
*/



/* Contract support */

#define SWIG_contract_assert(nullreturn, expr, msg) if (!(expr)) {SWIG_AdaThrowException(SWIG_AdaArgumentOutOfRangeException, msg); return nullreturn; } else


#define protected public
#define private   public

#include "llvm-c/Transforms/IPO.h"
#include "llvm-c/Transforms/Scalar.h"



//  struct LLVMCtxt;


#undef protected
#undef private
#ifdef __cplusplus 
extern "C" {
#endif
DllExport void SWIGSTDCALL Ada_LLVMAddArgumentPromotionPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddArgumentPromotionPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddConstantMergePass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddConstantMergePass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddDeadArgEliminationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddDeadArgEliminationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddDeadTypeEliminationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddDeadTypeEliminationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddFunctionAttrsPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddFunctionAttrsPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddFunctionInliningPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddFunctionInliningPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddGlobalDCEPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddGlobalDCEPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddGlobalOptimizerPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddGlobalOptimizerPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddIPConstantPropagationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddIPConstantPropagationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLowerSetJmpPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLowerSetJmpPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddPruneEHPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddPruneEHPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddRaiseAllocationsPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddRaiseAllocationsPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddStripDeadPrototypesPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddStripDeadPrototypesPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddStripSymbolsPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddStripSymbolsPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddAggressiveDCEPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddAggressiveDCEPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddCFGSimplificationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddCFGSimplificationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddCondPropagationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddCondPropagationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddDeadStoreEliminationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddDeadStoreEliminationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddGVNPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddGVNPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddIndVarSimplifyPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddIndVarSimplifyPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddInstructionCombiningPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddInstructionCombiningPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddJumpThreadingPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddJumpThreadingPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLICMPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLICMPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLoopDeletionPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLoopDeletionPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLoopIndexSplitPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLoopIndexSplitPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLoopRotatePass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLoopRotatePass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLoopUnrollPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLoopUnrollPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddLoopUnswitchPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddLoopUnswitchPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddMemCpyOptPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddMemCpyOptPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddPromoteMemoryToRegisterPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddPromoteMemoryToRegisterPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddReassociatePass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddReassociatePass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddSCCPPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddSCCPPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddScalarReplAggregatesPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddScalarReplAggregatesPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddSimplifyLibCallsPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddSimplifyLibCallsPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddTailCallEliminationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddTailCallEliminationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddConstantPropagationPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddConstantPropagationPass(arg1);
  
  
}



DllExport void SWIGSTDCALL Ada_LLVMAddDemoteMemoryToRegisterPass (
  void * jarg1
  )
{
  LLVMPassManagerRef arg1 = (LLVMPassManagerRef) 0 ;
  
  arg1 = (LLVMPassManagerRef)jarg1; 
  
  LLVMAddDemoteMemoryToRegisterPass(arg1);
  
  
}



#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif

