/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#if defined(__has_feature)
#   if __has_feature(memory_sanitizer)
#       define AZ_SIMD_NEON_MEMORY_SANITIZER
#   endif
#endif

namespace AZ
{
    namespace Simd
    {
        namespace Neon
        {
            constexpr uint32x4_t FirstThreeLanesMask{ 0, 0, 0, ~uint32_t{} };

            AZ_MATH_INLINE bool AreAllLanesTrue(uint32x2_t value)
            {
                return vminv_u32(value) != 0;
            }

            AZ_MATH_INLINE bool AreAllLanesTrue(uint32x4_t value)
            {
                return vminvq_u32(value) != 0;
            }

            AZ_MATH_INLINE bool AreFirstThreeLanesTrue(uint32x4_t value)
            {
                // Make every bit of the ignored fourth lane true so the result is
                // independent of that lane, which Vector3 may leave uninitialized.
#if defined(AZ_SIMD_NEON_MEMORY_SANITIZER)
                // Keep the data flow visible so MSan verifies that the all-ones
                // mask clears the ignored lane's shadow state.
                return AreAllLanesTrue(vorrq_u32(value, FirstThreeLanesMask));
#else
                // The barriers keep Clang from folding either side of this mask
                // into longer scalarized reductions; they emit no instructions.
                __asm__ volatile("" : "+w"(value));
                value = vorrq_u32(value, FirstThreeLanesMask);
                __asm__ volatile("" : "+w"(value));
                return AreAllLanesTrue(value);
#endif
            }
        }
    }
}

#undef AZ_SIMD_NEON_MEMORY_SANITIZER
