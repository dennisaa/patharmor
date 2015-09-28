/* **********************************************************
 * Copyright (c) 2010-2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DR_DEFINES_H_
#define _DR_DEFINES_H_ 1

/****************************************************************************
 * GENERAL TYPEDEFS AND DEFINES
 */

/**
 * @file dr_defines.h
 * @brief Basic defines and type definitions.
 */

/* A client's target operating system and architecture must be specified. */
#if !defined(LINUX) && !defined(WINDOWS) && !defined(MACOS)
# error Target operating system unspecified: must define WINDOWS, LINUX, or MACOS
#endif

#if (!defined(X86_64) && !defined(X86_32)) || (defined(X86_64) && defined(X86_32))
# error Target architecture unspecified: must define either X86_64 xor X86_32
#endif

#if defined(X86_64) && !defined(X64)
# define X64
#endif

#if (defined(LINUX) || defined(MACOS)) && !defined(UNIX)
# define UNIX
#endif

#ifdef WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winbase.h>
#else
#  include <stdio.h>
#  include <stdlib.h>
#endif
#include <stdarg.h> /* for varargs */

#ifdef WINDOWS
/* allow nameless struct/union */
#  pragma warning(disable: 4201)
/* VS2005 warns about comparison operator results being cast to bool (i#523) */
#  if _MSC_VER >= 1400 && _MSC_VER < 1500
#    pragma warning(disable: 4244)
#  endif
#endif

#ifdef WINDOWS
#  define DR_EXPORT __declspec(dllexport)
#  define LINK_ONCE __declspec(selectany)
#  define ALIGN_VAR(x) __declspec(align(x))
#  ifndef __cplusplus
#   define inline __inline
#  endif
#  define INLINE_FORCED __forceinline
#else
/* We assume gcc is being used.  If the client is using -fvisibility
 * (in gcc >= 3.4) to not export symbols by default, setting
 * USE_VISIBILITY_ATTRIBUTES will properly export.
 */
#  ifdef USE_VISIBILITY_ATTRIBUTES
#    define DR_EXPORT __attribute__ ((visibility ("default")))
#  else
#    define DR_EXPORT
#  endif
#  define LINK_ONCE __attribute__ ((weak))
#  define ALIGN_VAR(x) __attribute__ ((aligned (x)))
#  ifndef __cplusplus
#   define inline __inline__
#  endif
#  define INLINE_FORCED inline
#endif


/** Cross-platform maximum file path length. */
#define MAXIMUM_PATH      260


#ifndef NULL
#  define NULL (0)
#endif

#ifndef __cplusplus
#  ifndef DR_DO_NOT_DEFINE_bool
#    ifdef DR__Bool_EXISTS
/* prefer _Bool as it avoids truncation casting non-zero to zero */
typedef _Bool bool;
#    else
/* char-sized for compatibility with C++ */
typedef char bool;
#    endif
#  endif
#  ifndef true
#    define true  (1)
#  endif
#  ifndef false
#    define false (0)
#  endif
#endif

/* on Windows where bool is char casting can truncate non-zero to zero
 * so we use this macro
 */
#define CAST_TO_bool(x) (!!(x))

#ifndef DR_DO_NOT_DEFINE_uint
typedef unsigned int uint;
#endif
#ifndef DR_DO_NOT_DEFINE_ushort
typedef unsigned short ushort;
#endif
#ifndef DR_DO_NOT_DEFINE_byte
typedef unsigned char byte;
#endif
#ifndef DR_DO_NOT_DEFINE_sbyte
typedef signed char sbyte;
#endif
typedef byte * app_pc;

typedef void (*generic_func_t) ();

#ifdef WINDOWS
#  ifndef DR_DO_NOT_DEFINE_uint64
typedef unsigned __int64 uint64;
#  endif
#  ifndef DR_DO_NOT_DEFINE_int64
typedef __int64 int64;
#  endif
#  ifdef X64
typedef __int64 ssize_t;
#  else
typedef int ssize_t;
#  endif
#  define INT64_FORMAT "I64"
#else /* Linux */
#  ifdef X64
#    ifndef DR_DO_NOT_DEFINE_uint64
typedef unsigned long int uint64;
#    endif
#    ifndef DR_DO_NOT_DEFINE_int64
typedef long int int64;
#    endif
#    define INT64_FORMAT "l"
#  else
#    ifndef DR_DO_NOT_DEFINE_uint64
typedef unsigned long long int uint64;
#    endif
#    ifndef DR_DO_NOT_DEFINE_int64
typedef long long int int64;
#    endif
#    define INT64_FORMAT "ll"
#  endif
#endif

/* a register value: could be of any type; size is what matters. */
#ifdef X64
typedef uint64 reg_t;
#else
typedef uint reg_t;
#endif
/* integer whose size is based on pointers: ptr diff, mask, etc. */
typedef reg_t ptr_uint_t;
#ifdef X64
typedef int64 ptr_int_t;
#else
typedef int ptr_int_t;
#endif
/* for memory region sizes, use size_t */

/**
 * Application offset from module base.
 * PE32+ modules are limited to 2GB, but not ELF x64 med/large code model.
 */
typedef size_t app_rva_t;

#define PTR_UINT_0       ((ptr_uint_t)0U)
#define PTR_UINT_1       ((ptr_uint_t)1U)
#define PTR_UINT_MINUS_1 ((ptr_uint_t)-1)

#ifdef WINDOWS
typedef ptr_uint_t thread_id_t;
typedef ptr_uint_t process_id_t;
#elif defined(MACOS)
typedef uint64 thread_id_t;
typedef pid_t process_id_t;
#else /* Linux */
typedef pid_t thread_id_t;
typedef pid_t process_id_t;
#endif

#define INVALID_PROCESS_ID PTR_UINT_MINUS_1

#ifdef WINDOWS
/* since a FILE cannot be used outside of the DLL it was created in,
 * we have to use HANDLE on Windows
 * we hide the distinction behind the file_t type
 */
typedef HANDLE file_t;
/** The sentinel value for an invalid file_t. */
#  define INVALID_FILE INVALID_HANDLE_VALUE
/* dr_get_stdout_file and dr_get_stderr_file return errors as
 * INVALID_HANDLE_VALUE.  We leave INVALID_HANDLE_VALUE as is,
 * since it equals INVALID_FILE
 */
/** The file_t value for standard output. */
#  define STDOUT (dr_get_stdout_file())
/** The file_t value for standard error. */
#  define STDERR (dr_get_stderr_file())
/** The file_t value for standard input. */
#  define STDIN  (dr_get_stdin_file())
#endif

#ifdef UNIX
typedef int file_t;
/** The sentinel value for an invalid file_t. */
#  define INVALID_FILE -1
/** Allow use of stdout after the application closes it. */
extern file_t our_stdout;
/** Allow use of stderr after the application closes it. */
extern file_t our_stderr;
/** Allow use of stdin after the application closes it. */
extern file_t our_stdin;
/** The file_t value for standard output. */
#  define STDOUT our_stdout
/** The file_t value for standard error. */
#  define STDERR our_stderr
/** The file_t value for standard error. */
#  define STDIN  our_stdin
#endif

/**
 * ID used to uniquely identify a client.  This value is set at
 * client registration and passed to the client in dr_init().
 */
typedef uint client_id_t;

#ifndef DR_FAST_IR
/**
 * Internal structure of opnd_t is below abstraction layer.
 * But compiler needs to know field sizes to copy it around
 */
typedef struct {
# ifdef X64
    uint black_box_uint;
    uint64 black_box_uint64;
# else
    uint black_box_uint[3];
# endif
} opnd_t;

/**
 * Internal structure of instr_t is below abstraction layer, but we
 * provide its size so that it can be used in stack variables
 * instead of always allocated on the heap.
 */
typedef struct {
# ifdef X64
    uint black_box_uint[26];
# else
    uint black_box_uint[16];
# endif
} instr_t;
#else
struct _opnd_t;
typedef struct _opnd_t opnd_t;
struct _instr_t;
typedef struct _instr_t instr_t;
#endif /* API_EXPORT_ONLY */

# define IN /* marks input param */
# define OUT /* marks output param */
# define INOUT /* marks input+output param */


#ifdef X64
# define IF_X64(x) x
# define IF_X64_ELSE(x, y) x
# define IF_X64_(x) x,
# define _IF_X64(x) , x
# define IF_NOT_X64(x)
# define _IF_NOT_X64(x)
#else
# define IF_X64(x)
# define IF_X64_ELSE(x, y) y
# define IF_X64_(x)
# define _IF_X64(x)
# define IF_NOT_X64(x) x
# define _IF_NOT_X64(x) , x
#endif

#define UINT64_FORMAT_CODE INT64_FORMAT "u"
#define INT64_FORMAT_CODE INT64_FORMAT "d"
#define UINT64_FORMAT_STRING "%" UINT64_FORMAT_CODE
#define INT64_FORMAT_STRING "%" INT64_FORMAT_CODE
#define HEX64_FORMAT_STRING "%" INT64_FORMAT "x"
#define ZHEX64_FORMAT_STRING "%016" INT64_FORMAT "x"
#define ZHEX32_FORMAT_STRING "%08x"
#define HEX32_FORMAT_STRING "%x"
/* Convenience defines for cross-platform printing */
#ifdef X64
# define PFMT ZHEX64_FORMAT_STRING
# define PIFMT HEX64_FORMAT_STRING
# define SZFMT INT64_FORMAT_STRING
#else
# define PFMT ZHEX32_FORMAT_STRING
# define PIFMT HEX32_FORMAT_STRING
# define SZFMT "%d"
#endif

#define PFX "0x" PFMT      /**< printf format code for pointers */
#define PIFX "0x" PIFMT    /**< printf format code for pointer-sized integers */

# define INFINITE            0xFFFFFFFF

/* printf codes for {thread,process}_id_t */
#ifdef WINDOWS
# define PIDFMT SZFMT /**< printf format code for process_id_t */
# define TIDFMT SZFMT /**< printf format code for thread_id_t */
#else
# define PIDFMT "%d" /**< printf format code for process_id_t */
# ifdef MACOS
#  define TIDFMT UINT64_FORMAT_STRING /**< printf format code for thread_id_t */
# else
#  define TIDFMT "%d" /**< printf format code for thread_id_t */
# endif
#endif

/** 128-bit XMM register. */
typedef union _dr_xmm_t {
#ifdef X64
    uint64 u64[2]; /**< Representation as 2 64-bit integers. */
#endif
    uint   u32[4]; /**< Representation as 4 32-bit integers. */
    byte   u8[16]; /**< Representation as 16 8-bit integers. */
    reg_t  reg[IF_X64_ELSE(2,4)]; /**< Representation as 2 or 4 registers. */
} dr_xmm_t;

/** 256-bit YMM register. */
typedef union _dr_ymm_t {
#ifdef X64
    uint64 u64[4]; /**< Representation as 4 64-bit integers. */
#endif
    uint   u32[8]; /**< Representation as 8 32-bit integers. */
    byte   u8[32]; /**< Representation as 32 8-bit integers. */
    reg_t  reg[IF_X64_ELSE(4,8)]; /**< Representation as 4 or 8 registers. */
} dr_ymm_t;

#ifdef X64
# ifdef WINDOWS
#  define NUM_XMM_SLOTS 6 /**< Number of [xy]mm reg slots in dr_mcontext_t */ /*xmm0-5*/
# else
#  define NUM_XMM_SLOTS 16 /**< Number of [xy]mm reg slots in dr_mcontext_t */ /*xmm0-15*/
# endif
# define PRE_XMM_PADDING 16 /**< Bytes of padding before xmm/ymm dr_mcontext_t slots */
#else
# define NUM_XMM_SLOTS 8 /**< Number of [xy]mm reg slots in dr_mcontext_t */ /*xmm0-7*/
# define PRE_XMM_PADDING 24 /**< Bytes of padding before xmm/ymm dr_mcontext_t slots */
#endif

/** Values for the flags field of dr_mcontext_t */
typedef enum {
    /**
     * Selects the xdi, xsi, xbp, xbx, xdx, xcx, xax, and r8-r15 fields (i.e.,
     * all of the general-purpose registers excluding xsp, xip, and xflags).
     */
    DR_MC_INTEGER    = 0x01,
    /**
     * Selects the xsp, xflags, and xip fields.
     * \note: The xip field is only honored as an input for
     * dr_redirect_execution(), and as an output for system call
     * events.
     */
    DR_MC_CONTROL    = 0x02,
    /**
     * Selects the ymm (and xmm) fields.  This flag is ignored unless
     * dr_mcontext_xmm_fields_valid() returns true.  If
     * dr_mcontext_xmm_fields_valid() returns false, the application values of
     * the multimedia registers remain in the registers themselves.
     */
    DR_MC_MULTIMEDIA = 0x04,
    /** Selects all fields */
    DR_MC_ALL        = (DR_MC_INTEGER | DR_MC_CONTROL | DR_MC_MULTIMEDIA),
} dr_mcontext_flags_t;

/**
 * Machine context structure.
 */
typedef struct _dr_mcontext_t {
    /**
     * The size of this structure.  This field must be set prior to filling
     * in the fields to support forward compatibility.
     */
    size_t size;
    /**
     * The valid fields of this structure.  This field must be set prior to
     * filling in the fields.  For input requests (dr_get_mcontext()), this
     * indicates which fields should be written.  Writing the multimedia fields
     * frequently can incur a performance hit.  For output requests
     * (dr_set_mcontext() and dr_redirect_execution()), this indicates which
     * fields will be copied to the actual context.
     */
    dr_mcontext_flags_t flags;

    union {
        reg_t xdi; /**< platform-independent name for full rdi/edi register */
        reg_t IF_X64_ELSE(rdi, edi); /**< platform-dependent name for rdi/edi register */
    }; /**< anonymous union of alternative names for rdi/edi register */
    union {
        reg_t xsi; /**< platform-independent name for full rsi/esi register */
        reg_t IF_X64_ELSE(rsi, esi); /**< platform-dependent name for rsi/esi register */
    }; /**< anonymous union of alternative names for rsi/esi register */
    union {
        reg_t xbp; /**< platform-independent name for full rbp/ebp register */
        reg_t IF_X64_ELSE(rbp, ebp); /**< platform-dependent name for rbp/ebp register */
    }; /**< anonymous union of alternative names for rbp/ebp register */
    union {
        reg_t xsp; /**< platform-independent name for full rsp/esp register */
        reg_t IF_X64_ELSE(rsp, esp); /**< platform-dependent name for rsp/esp register */
    }; /**< anonymous union of alternative names for rsp/esp register */
    union {
        reg_t xbx; /**< platform-independent name for full rbx/ebx register */
        reg_t IF_X64_ELSE(rbx, ebx); /**< platform-dependent name for rbx/ebx register */
    }; /**< anonymous union of alternative names for rbx/ebx register */
    union {
        reg_t xdx; /**< platform-independent name for full rdx/edx register */
        reg_t IF_X64_ELSE(rdx, edx); /**< platform-dependent name for rdx/edx register */
    }; /**< anonymous union of alternative names for rdx/edx register */
    union {
        reg_t xcx; /**< platform-independent name for full rcx/ecx register */
        reg_t IF_X64_ELSE(rcx, ecx); /**< platform-dependent name for rcx/ecx register */
    }; /**< anonymous union of alternative names for rcx/ecx register */
    union {
        reg_t xax; /**< platform-independent name for full rax/eax register */
        reg_t IF_X64_ELSE(rax, eax); /**< platform-dependent name for rax/eax register */
    }; /**< anonymous union of alternative names for rax/eax register */
#ifdef X64
    reg_t r8;  /**< r8 register. \note For 64-bit DR builds only. */
    reg_t r9;  /**< r9 register. \note For 64-bit DR builds only. */
    reg_t r10; /**< r10 register. \note For 64-bit DR builds only. */
    reg_t r11; /**< r11 register. \note For 64-bit DR builds only. */
    reg_t r12; /**< r12 register. \note For 64-bit DR builds only. */
    reg_t r13; /**< r13 register. \note For 64-bit DR builds only. */
    reg_t r14; /**< r14 register. \note For 64-bit DR builds only. */
    reg_t r15; /**< r15 register. \note For 64-bit DR builds only. */
#endif
    union {
        reg_t xflags; /**< platform-independent name for full rflags/eflags register */
        reg_t IF_X64_ELSE(rflags, eflags); /**< platform-dependent name for
                                                rflags/eflags register */
    }; /**< anonymous union of alternative names for rflags/eflags register */
    /**
     * Anonymous union of alternative names for the program counter /
     * instruction pointer (eip/rip).  This field is not always set or
     * read by all API routines.
     */
    union {
        byte *xip; /**< platform-independent name for full rip/eip register */
        byte *pc; /**< platform-independent alt name for full rip/eip register */
        byte *IF_X64_ELSE(rip, eip); /**< platform-dependent name for rip/eip register */
    };
    byte padding[PRE_XMM_PADDING]; /**< padding to get ymm field 32-byte aligned */
    /**
     * The SSE registers xmm0-xmm5 (-xmm15 on Linux) are volatile
     * (caller-saved) for 64-bit and WOW64, and are actually zeroed out on
     * Windows system calls.  These fields are ignored for 32-bit processes
     * that are not WOW64, or if the underlying processor does not support
     * SSE.  Use dr_mcontext_xmm_fields_valid() to determine whether the
     * fields are valid.
     *
     * When the fields are valid, on processors with AVX enabled (i.e.,
     * proc_has_feature(FEATURE_AVX) returns true), these fields will
     * contain the full ymm register values; otherwise, the top 128
     * bits of each slot will be undefined.
     */
    dr_ymm_t ymm[NUM_XMM_SLOTS];
} dr_mcontext_t;


typedef struct _instr_list_t instrlist_t;
typedef struct _module_data_t module_data_t;


#ifdef X64
/**
 * Upper note values are reserved for core DR.
 */
# define DR_NOTE_FIRST_RESERVED 0xfffffffffffffff0ULL
#else
/**
 * Upper note values are reserved for core DR.
 */
# define DR_NOTE_FIRST_RESERVED 0xfffffff0UL
#endif
#define DR_NOTE_ANNOTATION (DR_NOTE_FIRST_RESERVED + 1)

/**
 * Structure written by dr_get_time() to specify the current time.
 */
typedef struct {
    uint year;         /**< */
    uint month;        /**< */
    uint day_of_week;  /**< */
    uint day;          /**< */
    uint hour;         /**< */
    uint minute;       /**< */
    uint second;       /**< */
    uint milliseconds; /**< */
} dr_time_t;



#endif /* _DR_DEFINES_H_ */
