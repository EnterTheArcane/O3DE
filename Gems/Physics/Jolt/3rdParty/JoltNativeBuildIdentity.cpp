/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <cstdint>

#include <Jolt/Core/Core.h>

#if JOLT_ARCH_FAMILY_X86
    #ifndef JPH_USE_SSE4_1
        #error "The private Jolt x86 objects require SSE4.1."
    #endif

    #if defined(JPH_USE_AVX) \
        || defined(JPH_USE_AVX2) \
        || defined(JPH_USE_AVX512) \
        || defined(JPH_USE_F16C) \
        || defined(JPH_USE_FMADD) \
        || defined(JPH_USE_LZCNT) \
        || defined(JPH_USE_SSE4_2) \
        || defined(JPH_USE_TZCNT)
        #error "The private Jolt x86 objects must remain exactly SSE4.1."
    #endif
#elif JOLT_ARCH_FAMILY_ARM
    #if defined(JPH_USE_AVX) \
        || defined(JPH_USE_AVX2) \
        || defined(JPH_USE_AVX512) \
        || defined(JPH_USE_F16C) \
        || defined(JPH_USE_FMADD) \
        || defined(JPH_USE_LZCNT) \
        || defined(JPH_USE_SSE4_1) \
        || defined(JPH_USE_SSE4_2) \
        || defined(JPH_USE_TZCNT)
        #error "The private Jolt ARM objects cannot use x86 instruction-set features."
    #endif
#endif

extern "C" std::uint64_t O3DEJoltNativeBuildFingerprint()
{
    return JOLT_NATIVE_BUILD_FINGERPRINT;
}
