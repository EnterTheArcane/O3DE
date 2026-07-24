/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Portions of this file independently reimplement the XXH3_64bits and XXH3_128bits algorithms from Yann Collet's xxHash.
// The xxHash reference implementation is distributed under the following BSD 2-Clause license:
//
// xxHash - Extremely Fast Hash algorithm
// Copyright (C) 2012-2023 Yann Collet
//
// BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following disclaimer
//       in the documentation and/or other materials provided with the
//       distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <AzCore/Hash/Xxh3.h>

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#if AZ_TRAIT_USE_PLATFORM_SIMD_SSE
#include <immintrin.h>
#elif AZ_TRAIT_USE_PLATFORM_SIMD_NEON
#include <arm_neon.h>
#endif

namespace AZ::Hash
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(Xxh3, "Xxh3", "{9C4D3E2F-1A6B-4C8D-B5E7-2F80A1B3C4D6}")

    void Xxh3::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Xxh3>()
                ->Field("Value", &Xxh3::m_value);
        }
    }

    AZ_TYPE_INFO_WITH_NAME_IMPL(Xxh128, "Xxh128", "{AD5E4F30-2B7C-4D9E-C6F8-3091B2C4D5E7}")

    void Xxh128::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            // TODO: Expose "Value" when we have a proper 128-bit type to reflect.
            serializeContext->Class<Xxh128>()
                ->Field("Low", &Xxh128::m_low)
                ->Field("High", &Xxh128::m_high);
        }
    }
} // namespace AZ::Hash

namespace AZ::Hash::Private::Xxh3
{
    namespace
    {
        // Long-path geometry shared by the vectorized backends (the scalar fallback reuses HashLongScalar's copy).
        [[maybe_unused]] constexpr AZStd::size_t StripesPerBlock = (Secret.size() - StripeLen) / 8; // 16
        [[maybe_unused]] constexpr AZStd::size_t BlockSize = StripeLen * StripesPerBlock; // 1024

#if AZ_TRAIT_USE_PLATFORM_SIMD_SSE

        void Accumulate512(__m256i* acc, const u8* stripe, const u8* secret) noexcept
        {
            for (int i = 0; i < 2; ++i)
            {
                const __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(stripe + 32 * i));
                const __m256i key = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(secret + 32 * i));
                const __m256i dataKey = _mm256_xor_si256(data, key);
                const __m256i product = _mm256_mul_epu32(dataKey, _mm256_srli_epi64(dataKey, 32));
                const __m256i dataSwap = _mm256_shuffle_epi32(data, _MM_SHUFFLE(1, 0, 3, 2));
                acc[i] = _mm256_add_epi64(product, _mm256_add_epi64(acc[i], dataSwap));
            }
        }

        void Scramble(__m256i* acc, const u8* scrambleSecret) noexcept
        {
            const __m256i prime = _mm256_set1_epi64x(static_cast<long long>(static_cast<u64>(Prime32_1)));
            for (int i = 0; i < 2; ++i)
            {
                __m256i value = acc[i];
                value = _mm256_xor_si256(value, _mm256_srli_epi64(value, 47));
                const __m256i dataKey =
                    _mm256_xor_si256(value, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(scrambleSecret + 32 * i)));
                const __m256i prodLo = _mm256_mul_epu32(dataKey, prime);
                const __m256i prodHi = _mm256_mul_epu32(_mm256_srli_epi64(dataKey, 32), prime);
                acc[i] = _mm256_add_epi64(prodLo, _mm256_slli_epi64(prodHi, 32));
            }
        }

        u128 HashLongVectorized(const u8* data, const AZStd::size_t len, const u64 seed, const bool want128) noexcept
        {
            const auto secretStore = InitCustomSecret(seed);
            const u8* secret = secretStore.data();

            alignas(32) u64 acc[8] = {
                InitAcc[0],
                InitAcc[1],
                InitAcc[2],
                InitAcc[3],
                InitAcc[4],
                InitAcc[5],
                InitAcc[6],
                InitAcc[7],
            };
            __m256i* accVec = reinterpret_cast<__m256i*>(acc);

            const AZStd::size_t nbBlocks = (len - 1) / BlockSize;
            for (AZStd::size_t block = 0; block < nbBlocks; ++block)
            {
                const u8* blockStart = data + block * BlockSize;
                for (AZStd::size_t stripe = 0; stripe < StripesPerBlock; ++stripe)
                {
                    Accumulate512(accVec, blockStart + stripe * StripeLen, secret + stripe * 8);
                }
                Scramble(accVec, secret + (Secret.size() - StripeLen));
            }
            const AZStd::size_t nbStripes = ((len - 1) - nbBlocks * BlockSize) / StripeLen;
            const u8* lastBlock = data + nbBlocks * BlockSize;
            for (AZStd::size_t stripe = 0; stripe < nbStripes; ++stripe)
            {
                Accumulate512(accVec, lastBlock + stripe * StripeLen, secret + stripe * 8);
            }
            Accumulate512(accVec, data + len - StripeLen, secret + (Secret.size() - 71));

            const u64 low = FinalMerge(acc, static_cast<u64>(len) * Prime64_1, secret, 11);
            u64 high = 0;
            if (want128)
            {
                high = FinalMerge(acc, ~(static_cast<u64>(len) * Prime64_2), secret, Secret.size() - 75);
            }
            return u128{.a = low, .b = high};
        }

#elif AZ_TRAIT_USE_PLATFORM_SIMD_NEON

        // NEON: four 128-bit registers hold the 8 accumulators (2 lanes each).
        // Accumulators load/store through vld1q/vst1q rather than aliasing a vector pointer.
        // The reinterpret_cast bridges u64 (unsigned long long) and the uint64_t the NEON intrinsics expect (they differ on LP64).
        void Accumulate512(uint64x2_t (&acc)[4], const u8* stripe, const u8* secret) noexcept
        {
            for (int i = 0; i < 4; ++i)
            {
                const uint64x2_t data = vreinterpretq_u64_u8(vld1q_u8(stripe + 16 * i));
                const uint64x2_t key = vreinterpretq_u64_u8(vld1q_u8(secret + 16 * i));
                const uint64x2_t dataKey = veorq_u64(data, key);
                // low32(dataKey) * high32(dataKey), widening 32x32 -> 64 per lane.
                const uint64x2_t product = vmull_u32(vmovn_u64(dataKey), vshrn_n_u64(dataKey, 32));
                // Swap the two 64-bit lanes: the canonical acc[i ^ 1] += stripe[i] cross-lane add.
                const uint64x2_t dataSwap = vextq_u64(data, data, 1);
                acc[i] = vaddq_u64(vaddq_u64(acc[i], dataSwap), product);
            }
        }

        void Scramble(uint64x2_t (&acc)[4], const u8* scrambleSecret) noexcept
        {
            const uint32x2_t prime = vdup_n_u32(Prime32_1);
            for (int i = 0; i < 4; ++i)
            {
                uint64x2_t value = acc[i];
                value = veorq_u64(value, vshrq_n_u64(value, 47));
                const uint64x2_t dataKey = veorq_u64(value, vreinterpretq_u64_u8(vld1q_u8(scrambleSecret + 16 * i)));
                // dataKey * Prime32_1 (mod 2^64) split as low32*prime + ((high32*prime) << 32).
                const uint64x2_t prodLo = vmull_u32(vmovn_u64(dataKey), prime);
                const uint64x2_t prodHi = vmull_u32(vshrn_n_u64(dataKey, 32), prime);
                acc[i] = vaddq_u64(prodLo, vshlq_n_u64(prodHi, 32));
            }
        }

        u128 HashLongVectorized(const u8* data, const AZStd::size_t len, const u64 seed, const bool want128) noexcept
        {
            const auto secretStore = InitCustomSecret(seed);
            const u8* secret = secretStore.data();

            uint64x2_t acc[4] = {
                vld1q_u64(reinterpret_cast<const uint64_t*>(InitAcc + 0)),
                vld1q_u64(reinterpret_cast<const uint64_t*>(InitAcc + 2)),
                vld1q_u64(reinterpret_cast<const uint64_t*>(InitAcc + 4)),
                vld1q_u64(reinterpret_cast<const uint64_t*>(InitAcc + 6)),
            };

            const AZStd::size_t nbBlocks = (len - 1) / BlockSize;
            for (AZStd::size_t block = 0; block < nbBlocks; ++block)
            {
                const u8* blockStart = data + block * BlockSize;
                for (AZStd::size_t stripe = 0; stripe < StripesPerBlock; ++stripe)
                {
                    Accumulate512(acc, blockStart + stripe * StripeLen, secret + stripe * 8);
                }
                Scramble(acc, secret + (Secret.size() - StripeLen));
            }
            const AZStd::size_t nbStripes = ((len - 1) - nbBlocks * BlockSize) / StripeLen;
            const u8* lastBlock = data + nbBlocks * BlockSize;
            for (AZStd::size_t stripe = 0; stripe < nbStripes; ++stripe)
            {
                Accumulate512(acc, lastBlock + stripe * StripeLen, secret + stripe * 8);
            }
            Accumulate512(acc, data + len - StripeLen, secret + (Secret.size() - 71));

            u64 accScalar[8];
            vst1q_u64(reinterpret_cast<uint64_t*>(accScalar + 0), acc[0]);
            vst1q_u64(reinterpret_cast<uint64_t*>(accScalar + 2), acc[1]);
            vst1q_u64(reinterpret_cast<uint64_t*>(accScalar + 4), acc[2]);
            vst1q_u64(reinterpret_cast<uint64_t*>(accScalar + 6), acc[3]);

            const u64 low = FinalMerge(accScalar, static_cast<u64>(len) * Prime64_1, secret, 11);
            u64 high = 0;
            if (want128)
            {
                high = FinalMerge(accScalar, ~(static_cast<u64>(len) * Prime64_2), secret, Secret.size() - 75);
            }
            return u128{.a = low, .b = high};
        }

#else // scalar (no SIMD backend): reuse the constexpr scalar long path directly.

        u128 HashLongVectorized(const u8* data, const AZStd::size_t len, const u64 seed, const bool want128) noexcept
        {
            return HashLongScalar(data, len, seed, want128);
        }

#endif
    } // namespace

    u64 Hash64(const AZStd::byte* data, const AZStd::size_t len, const u64 seed) noexcept
    {
        const u8* input = reinterpret_cast<const u8*>(data);
        if (len <= 240)
        {
            return Hash64Scalar(input, len, seed);
        }
        return HashLongVectorized(input, len, seed, false).a;
    }

    u128 Hash128(const AZStd::byte* data, const AZStd::size_t len, const u64 seed) noexcept
    {
        const u8* input = reinterpret_cast<const u8*>(data);
        if (len <= 240)
        {
            return Hash128Scalar(input, len, seed);
        }
        return HashLongVectorized(input, len, seed, true);
    }
} // namespace AZ::Hash::Private::Xxh3
