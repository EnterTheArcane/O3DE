/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolGroup.h>

#if AZ_TRAIT_USE_PLATFORM_SIMD_SSE
#include <emmintrin.h>
#elif AZ_TRAIT_USE_PLATFORM_SIMD_NEON
#include <arm_neon.h>
#endif

namespace AZ::Internal
{
    namespace
    {
#if AZ_TRAIT_USE_PLATFORM_SIMD_NEON
        [[nodiscard]]
        u16 PackMask(const uint8x16_t lanes)
        {
            alignas(16) constexpr u8 LaneWeights[SymbolGroup::Width] = {
                1, 2, 4, 8, 16, 32, 64, 128,
                1, 2, 4, 8, 16, 32, 64, 128,
            };

            const uint8x16_t bits = vandq_u8(lanes, vdupq_n_u8(1));
            const uint8x16_t weightedBits = vmulq_u8(bits, vld1q_u8(LaneWeights));
            const uint16x8_t pairs = vpaddlq_u8(weightedBits);
            const uint32x4_t quads = vpaddlq_u16(pairs);
            const uint64x2_t halves = vpaddlq_u32(quads);
            return static_cast<u16>(vgetq_lane_u64(halves, 0) | (vgetq_lane_u64(halves, 1) << 8));
        }
#endif
    }

    SymbolGroupMasks SymbolGroup::Match(
        const u8* controls,
        const u8 fingerprint)
    {
#if AZ_TRAIT_USE_PLATFORM_SIMD_SSE
        const __m128i controlValues = _mm_loadu_si128(reinterpret_cast<const __m128i*>(controls));
        const __m128i fingerprints = _mm_set1_epi8(static_cast<char>(fingerprint));
        return SymbolGroupMasks{
            .m_matches = static_cast<u16>(_mm_movemask_epi8(_mm_cmpeq_epi8(controlValues, fingerprints))),
            .m_empty = static_cast<u16>(_mm_movemask_epi8(controlValues)),
        };
#elif AZ_TRAIT_USE_PLATFORM_SIMD_NEON
        const uint8x16_t controlValues = vld1q_u8(controls);
        return SymbolGroupMasks{
            .m_matches = PackMask(vceqq_u8(controlValues, vdupq_n_u8(fingerprint))),
            .m_empty = PackMask(vceqq_u8(controlValues, vdupq_n_u8(SymbolGroupEmptyControl))),
        };
#else
        return MatchScalar(controls, fingerprint);
#endif
    }

    SymbolGroupMasks SymbolGroup::MatchScalar(
        const u8* controls,
        const u8 fingerprint)
    {
        SymbolGroupMasks masks{};
        for (size_t lane = 0; lane < Width; ++lane)
        {
            const u16 laneBit = static_cast<u16>(1u << lane);
            if (controls[lane] == fingerprint)
            {
                masks.m_matches |= laneBit;
            }
            if (controls[lane] == SymbolGroupEmptyControl)
            {
                masks.m_empty |= laneBit;
            }
        }
        return masks;
    }
} // namespace AZ::Internal
