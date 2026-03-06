/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

//////////////////////////////////////////////////////////////////////////
// Platforms

#include <AzCore/variadic.h>

#include "PlatformRestrictedFileDef.h"

#if defined(__clang__)
    #define AZ_COMPILER_CLANG   __clang_major__
#elif defined(__GNUC__)
    //  Assign AZ_COMPILER_GCC to a number that represents the major+minor (2 digits) + path level (2 digits)  i.e. 3.2.0 == 30200
    #define AZ_COMPILER_GCC     (__GNUC__ * 10000 \
                               + __GNUC_MINOR__ * 100 \
                               + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
    #define AZ_COMPILER_MSVC    _MSC_VER
    // DLL exporting and extern as mutually exclusive for MSVC compilers but needed for clang/gcc, so omit the keyword when declaring an exported template
    // (see https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-1-c4910?view=msvc-170)
#else
#   error This compiler is not supported
#endif

#include <AzCore/AzCore_Traits_Platform.h>

//////////////////////////////////////////////////////////////////////////

#define AZ_INLINE                       inline
#define AZ_THREAD_LOCAL                 thread_local
#define AZ_DYNAMIC_LIBRARY_PREFIX       AZ_TRAIT_OS_DYNAMIC_LIBRARY_PREFIX
#define AZ_DYNAMIC_LIBRARY_EXTENSION    AZ_TRAIT_OS_DYNAMIC_LIBRARY_EXTENSION

#if defined(AZ_COMPILER_CLANG) || defined(AZ_COMPILER_GCC)
    #define AZ_DLL_EXPORT               AZ_TRAIT_OS_DLL_EXPORT_CLANG
    #define AZ_DLL_IMPORT               AZ_TRAIT_OS_DLL_IMPORT_CLANG
    #define AZ_DLL_EXPORT_EXTERN        AZ_TRAIT_OS_DLL_EXPORT_EXTERN_CLANG
    #define AZ_DLL_IMPORT_EXTERN        AZ_TRAIT_OS_DLL_IMPORT_EXTERN_CLANG
#elif defined(AZ_COMPILER_MSVC)
    #define AZ_DLL_EXPORT               __declspec(dllexport)
    #define AZ_DLL_IMPORT               __declspec(dllimport)
    #define AZ_DLL_EXPORT_EXTERN
    #define AZ_DLL_IMPORT_EXTERN
#endif

// AZ_DEPRECATED and AZ_DEPRECATED_MESSAGE are convenience wrappers for the
// standard [[deprecated("message")]] attribute. Prefer using the attribute directly.
#define AZ_DEPRECATED(_decl, _message) [[deprecated(_message)]] _decl
#define AZ_DEPRECATED_MESSAGE(_message) [[deprecated(_message)]]

#define AZ_STRINGIZE_I(text) #text

#if defined(AZ_COMPILER_MSVC)
#    define AZ_STRINGIZE(text) AZ_STRINGIZE_A((text))
#    define AZ_STRINGIZE_A(arg) AZ_STRINGIZE_I arg
#else
#    define AZ_STRINGIZE(text) AZ_STRINGIZE_I(text)
#endif

#if defined(AZ_COMPILER_MSVC)

/// Disables a warning using push style. For use matched with an AZ_POP_WARNING

// Compiler specific AZ_PUSH_DISABLE_WARNING
#define AZ_PUSH_DISABLE_WARNING_MSVC(_msvcOption)       \
    __pragma(warning(push))                             \
    __pragma(warning(disable : _msvcOption))
#define AZ_PUSH_DISABLE_WARNING_CLANG(_clangOption)
#define AZ_PUSH_DISABLE_WARNING_GCC(_gccOption)

/// Compiler specific AZ_POP_DISABLE_WARNING. This needs to be matched with the compiler specific AZ_PUSH_DISABLE_WARNINGs
#define AZ_POP_DISABLE_WARNING_CLANG
#define AZ_POP_DISABLE_WARNING_MSVC                     \
    __pragma(warning(pop))
#define AZ_POP_DISABLE_WARNING_GCC


// Variadic definitions for AZ_PUSH_DISABLE_WARNING for the current compiler
#define AZ_PUSH_DISABLE_WARNING_1(_msvcOption)          \
    __pragma(warning(push))                             \
    __pragma(warning(disable : _msvcOption))

#define AZ_PUSH_DISABLE_WARNING_2(_msvcOption, _2)      \
    __pragma(warning(push))                             \
    __pragma(warning(disable : _msvcOption))

#define AZ_PUSH_DISABLE_WARNING_3(_msvcOption, _2, _3)  \
    __pragma(warning(push))                             \
    __pragma(warning(disable : _msvcOption))

/// Pops the warning stack. For use matched with an AZ_PUSH_DISABLE_WARNING
#define AZ_POP_DISABLE_WARNING                          \
    __pragma(warning(pop))

#   define AZ_FORCE_INLINE  __forceinline

/// Pointer will be aliased.
#   define AZ_MAY_ALIAS
/// Function signature macro
#   define AZ_FUNCTION_SIGNATURE    __FUNCSIG__

//////////////////////////////////////////////////////////////////////////
#elif defined(AZ_COMPILER_CLANG) || defined(AZ_COMPILER_GCC)

#if defined(AZ_COMPILER_CLANG)

/// Disables a single warning using push style. For use matched with an AZ_POP_WARNING

// Compiler specific AZ_PUSH_DISABLE_WARNING
#define AZ_PUSH_DISABLE_WARNING_CLANG(_clangOption)  \
    _Pragma("clang diagnostic push")                 \
    _Pragma(AZ_STRINGIZE(clang diagnostic ignored _clangOption))
#define AZ_PUSH_DISABLE_WARNING_MSVC(_msvcOption)
#define AZ_PUSH_DISABLE_WARNING_GCC(_gccOption)

/// Compiler specific AZ_POP_DISABLE_WARNING. This needs to be matched with the compiler specific AZ_PUSH_DISABLE_WARNINGs
#define AZ_POP_DISABLE_WARNING_CLANG                       \
    _Pragma("clang diagnostic pop")
#define AZ_POP_DISABLE_WARNING_MSVC
#define AZ_POP_DISABLE_WARNING_GCC

// Variadic definitions for AZ_PUSH_DISABLE_WARNING for the current compiler
#define AZ_PUSH_DISABLE_WARNING_1(_1)
#define AZ_PUSH_DISABLE_WARNING_2(_1, _clangOption)     AZ_PUSH_DISABLE_WARNING_CLANG(_clangOption)
#define AZ_PUSH_DISABLE_WARNING_3(_1, _clangOption, _2) AZ_PUSH_DISABLE_WARNING_CLANG(_clangOption)

/// Pops the warning stack. For use matched with an AZ_PUSH_DISABLE_WARNING
#define AZ_POP_DISABLE_WARNING                              \
    _Pragma("clang diagnostic pop")

#else

/// Disables a single warning using push style. For use matched with an AZ_POP_WARNING

// Compiler specific AZ_PUSH_DISABLE_WARNING
#define AZ_PUSH_DISABLE_WARNING_GCC(_gccOption)             \
    _Pragma("GCC diagnostic push")                          \
    _Pragma(AZ_STRINGIZE(GCC diagnostic ignored _gccOption))
#define AZ_PUSH_DISABLE_WARNING_CLANG(_clangOption)
#define AZ_PUSH_DISABLE_WARNING_MSVC(_msvcOption)

/// Compiler specific AZ_POP_DISABLE_WARNING. This needs to be matched with the compiler specific AZ_PUSH_DISABLE_WARNINGs
#define AZ_POP_DISABLE_WARNING_CLANG
#define AZ_POP_DISABLE_WARNING_MSVC
#define AZ_POP_DISABLE_WARNING_GCC                          \
    _Pragma("GCC diagnostic pop")

// Variadic definitions for AZ_PUSH_DISABLE_WARNING for the current compiler
#define AZ_PUSH_DISABLE_WARNING_1(_1)
#define AZ_PUSH_DISABLE_WARNING_2(_1, _2)
#define AZ_PUSH_DISABLE_WARNING_3(_1, _2, _gccOption)   AZ_PUSH_DISABLE_WARNING_GCC(_gccOption)

/// Pops the warning stack. For use matched with an AZ_PUSH_DISABLE_WARNING
#define AZ_POP_DISABLE_WARNING
    _Pragma("GCC diagnostic pop")

#endif // defined(AZ_COMPILER_CLANG)

#   define AZ_FORCE_INLINE  inline

/// Pointer will be aliased.
#   define AZ_MAY_ALIAS __attribute__((__may_alias__))
/// Function signature macro
#   define AZ_FUNCTION_SIGNATURE    __PRETTY_FUNCTION__

#else
    #error Compiler not supported
#endif

#define AZ_PUSH_DISABLE_WARNING(...) AZ_MACRO_SPECIALIZE(AZ_PUSH_DISABLE_WARNING_, AZ_VA_NUM_ARGS(__VA_ARGS__), (__VA_ARGS__))

// We need to define AZ_DEBUG_BUILD in debug mode. We can also define it in debug optimized mode (left up to the user).
// note that _DEBUG is not in fact always defined on all platforms, and only AZ_DEBUG_BUILD should be relied on.
#if !defined(AZ_DEBUG_BUILD) && defined(_DEBUG)
#   define AZ_DEBUG_BUILD
#endif

#if !defined(AZ_PROFILE_BUILD) && defined(_PROFILE)
#   define AZ_PROFILE_BUILD
#endif

#if !defined(AZ_RELEASE_BUILD) && defined(_RELEASE)
#   define AZ_RELEASE_BUILD
#endif

// note that many include ONLY PlatformDef.h and not base.h, so flags such as below need to be here.
// AZ_ENABLE_DEBUG_TOOLS - turns on and off interaction with the debugger.
// Things like being able to check whether the current process is being debugged, to issue a "debug break" command, etc.
#if defined(AZ_DEBUG_BUILD) && !defined(AZ_ENABLE_DEBUG_TOOLS)
#   define AZ_ENABLE_DEBUG_TOOLS
#endif

// AZ_ENABLE_TRACE_ASSERTS - toggles display of native UI assert dialogs with ignore/break options
#define AZ_ENABLE_TRACE_ASSERTS 1

// AZ_ENABLE_TRACING - turns on and off the availability of AZ_TracePrintf / AZ_Assert / AZ_Error / AZ_Warning
#if (defined(AZ_DEBUG_BUILD) || defined(AZ_PROFILE_BUILD)) && !defined(AZ_ENABLE_TRACING)
#   define AZ_ENABLE_TRACING
#endif

#if !defined(AZ_COMMAND_LINE_LEN)
#   define AZ_COMMAND_LINE_LEN 2048
#endif

#include <type_traits>
#include <utility>
#include <memory>
#include <cstdint>
#include <cstring>

// Compiler builtin detection for constexpr-capable memory operations.
// The list of builtins can be found on the clang and GCC documentation pages at
// https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
// https://clang.llvm.org/docs/LanguageExtensions.html#builtin-functions
// NOTE: __builtin_memcpy and __builtin_memmove for GCC is not usable in a compile time context currently.
// NOTE: MSVC supports __has_builtin since 19.29 (VS 2019 16.10), but does not report all builtins
// through it. The || defined(AZ_COMPILER_MSVC) fallbacks handle builtins that MSVC supports
// for constexpr evaluation via its own mechanism.
#if defined(__has_builtin)
    #if __has_builtin(__builtin_memcpy) && (!defined(AZ_COMPILER_GCC))
        #define az_has_builtin_memcpy true
    #endif
    #if __has_builtin(__builtin_wmemcpy)
        #define az_has_builtin_wmemcpy true
    #endif
    #if __has_builtin(__builtin_memmove) && (!defined(AZ_COMPILER_GCC))
        #define az_has_builtin_memmove true
    #endif
    #if __has_builtin(__builtin_wmemmove)
        #define az_has_builtin_wmemmove true
    #endif
    #if __has_builtin(__builtin_strlen)
        #define az_has_builtin_strlen true
    #endif
    #if __has_builtin(__builtin_wcslen)
        #define az_has_builtin_wcslen true
    #endif
    #if __has_builtin(__builtin_char_memchr)
        #define az_has_builtin_char_memchr true
    #endif
    #if __has_builtin(__builtin_wmemchr)
        #define az_has_builtin_wmemchr true
    #endif
    #if __has_builtin(__builtin_memcmp) && (!defined(AZ_COMPILER_GCC))
        #define az_has_builtin_memcmp true
    #endif
    #if __has_builtin(__builtin_wmemcmp)
        #define az_has_builtin_wmemcmp true
    #endif
#endif
// MSVC constexpr support for these operations works without __has_builtin reporting them.
#if defined(AZ_COMPILER_MSVC)
    #if !defined(az_has_builtin_strlen)
        #define az_has_builtin_strlen true
    #endif
    #if !defined(az_has_builtin_wcslen)
        #define az_has_builtin_wcslen true
    #endif
    #if !defined(az_has_builtin_char_memchr)
        #define az_has_builtin_char_memchr true
    #endif
    #if !defined(az_has_builtin_wmemchr)
        #define az_has_builtin_wmemchr true
    #endif
    #if !defined(az_has_builtin_memcmp)
        #define az_has_builtin_memcmp true
    #endif
    #if !defined(az_has_builtin_wmemcmp)
        #define az_has_builtin_wmemcmp true
    #endif
#endif

#if !defined(az_has_builtin_memcpy)
    #define az_has_builtin_memcpy false
#endif
#if !defined(az_has_builtin_wmemcpy)
    #define az_has_builtin_wmemcpy false
#endif
#if !defined(az_has_builtin_memmove)
    #define az_has_builtin_memmove false
#endif
#if !defined(az_has_builtin_wmemmove)
    #define az_has_builtin_wmemmove false
#endif
#if !defined(az_has_builtin_strlen)
    #define az_has_builtin_strlen false
#endif
#if !defined(az_has_builtin_wcslen)
    #define az_has_builtin_wcslen false
#endif
#if !defined(az_has_builtin_char_memchr)
    #define az_has_builtin_char_memchr false
#endif
#if !defined(az_has_builtin_wmemchr)
    #define az_has_builtin_wmemchr false
#endif
#if !defined(az_has_builtin_memcmp)
    #define az_has_builtin_memcmp false
#endif
#if !defined(az_has_builtin_wmemcmp)
    #define az_has_builtin_wmemcmp false
#endif

#define AZ_NO_UNIQUE_ADDRESS [[no_unique_address]]
