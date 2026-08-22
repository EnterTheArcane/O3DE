/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/PlatformDef.h>

// ARM64EC also defines x64 compatibility macros, so detect it before the x86 family.
#if defined(_M_ARM64) || defined(_M_ARM64EC) || defined(__aarch64__)
    #define JOLT_ARCH_FAMILY_ARM 1
    #define JOLT_ARCH_FAMILY_X86 0
    #define JOLT_ARCH_ARM64 1
    #define JOLT_ARCH_X64 0
#elif defined(_M_AMD64) || defined(_M_X64) || defined(__x86_64__)
    #define JOLT_ARCH_FAMILY_ARM 0
    #define JOLT_ARCH_FAMILY_X86 1
    #define JOLT_ARCH_ARM64 0
    #define JOLT_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
    #define JOLT_ARCH_FAMILY_ARM 0
    #define JOLT_ARCH_FAMILY_X86 1
    #define JOLT_ARCH_ARM64 0
    #define JOLT_ARCH_X64 0
#else
    #define JOLT_ARCH_FAMILY_ARM 0
    #define JOLT_ARCH_FAMILY_X86 0
    #define JOLT_ARCH_ARM64 0
    #define JOLT_ARCH_X64 0
#endif

#ifdef _MSC_VER
    #define JOLT_ABI_MICROSOFT 1
#else
    #define JOLT_ABI_MICROSOFT 0
#endif
