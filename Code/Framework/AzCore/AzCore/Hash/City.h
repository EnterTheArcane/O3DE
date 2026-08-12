/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Portions of this file are derived from Google's CityHash reference implementation
// (https://github.com/google/cityhash), used under the following license:
//
// Copyright (c) 2011 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// CityHash, by Geoff Pike and Jyrki Alakuijala.

#pragma once

#include <AzCore/std/bit.h>
#include <AzCore/std/concepts/concepts.h>
#include <AzCore/std/hash.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/RTTI/TypeInfoSimple.h>

namespace AZ
{
    class ReflectContext;
}

/**
 * CityHash, a family of fast, non-cryptographic hash functions designed by Google.
 * They produce high-quality hashes of byte arrays, passing standard tests such as Austin Appleby's SMHasher,
 * and remain among the fastest known hashes of comparable quality on modern 64-bit hardware.
 *
 * This header provides three type-safe wrappers (City32, City64, City128) that can stand in for raw integer hash values.
 * Each implicitly converts from string literals, string views, and byte spans, so hashing happens at the point of construction.
 * The 32- and 64-bit variants also convert implicitly to their underlying integer (u32 / u64).
 * The string-literal and string-view constructors are fully constexpr: hashes computed at compile time match those computed at runtime.
 * At runtime the inner unaligned-load helpers fall back to std::memcpy, which modern compilers lower to a single load instruction.
 * Hashes in this family are not suitable for cryptography.
 *
 * - <a href="https://github.com/google/cityhash">Reference implementation</a>
 */
namespace AZ::Hash
{
    struct City32;
    struct City64;
    struct City128;
} // namespace AZ::Hash

namespace AZ::Hash::Private::City
{
    // Some primes between 2^63 and 2^64 used by the 64- and 128-bit variants.
    inline constexpr u64 K0 = 0xC3A5C85C97CB3127ull;
    inline constexpr u64 K1 = 0xB492B66FBE98F273ull;
    inline constexpr u64 K2 = 0x9AE16A3B2F90404Full;

    // Magic numbers from Murmur3 used by the 32-bit variant.
    inline constexpr u32 C1 = 0xCC9E2D51u;
    inline constexpr u32 C2 = 0x1B873593u;

    // Cyclic permutation: (a, b, c) -> (c, a, b).
    template<typename T>
    constexpr void Permute3(T& a, T& b, T& c) noexcept
    {
        AZStd::swap(a, b);
        AZStd::swap(a, c);
    }

    // Read a little-endian unsigned integer of size sizeof(T) from a byte-addressable source.
    // Constexpr-safe: at compile time the bytes are assembled one at a time.
    // At runtime a single std::memcpy and optional byteswap is used so the compiler can lower it to a native unaligned load.
    template<AZStd::unsigned_integral T>
    [[nodiscard]]
    constexpr T Fetch(const char* p) noexcept
    {
        if (az_builtin_is_constant_evaluated())
        {
            T r = 0;
            for (AZStd::size_t i = 0; i < sizeof(T); ++i)
            {
                r |= static_cast<T>(static_cast<u8>(p[i])) << (i * 8);
            }
            return r;
        }
        T r{};
        std::memcpy(&r, p, sizeof(T));
        if constexpr (AZStd::endian::native == AZStd::endian::big)
        {
            r = AZStd::byteswap(r);
        }
        return r;
    }

    [[nodiscard]]
    constexpr u64 Fetch64(const char* p) noexcept
    {
        return Fetch<u64>(p);
    }

    [[nodiscard]]
    constexpr u32 Fetch32(const char* p) noexcept
    {
        return Fetch<u32>(p);
    }

    [[nodiscard]]
    constexpr u64 ShiftMix(const u64 v) noexcept
    {
        return v ^ (v >> 47);
    }

    // 128-bit -> 64-bit reduction inspired by Murmur.
    [[nodiscard]]
    constexpr u64 Hash128to64(const u64 low, const u64 high) noexcept
    {
        constexpr u64 Mul = 0x9DDFEA08EB382D69ull;
        u64 a = (low ^ high) * Mul;
        a ^= (a >> 47);
        u64 b = (high ^ a) * Mul;
        b ^= (b >> 47);
        b *= Mul;
        return b;
    }

    [[nodiscard]]
    constexpr u64 HashLen16(const u64 u, const u64 v) noexcept
    {
        return Hash128to64(u, v);
    }

    [[nodiscard]]
    constexpr u64 HashLen16(const u64 u, const u64 v, const u64 mul) noexcept
    {
        u64 a = (u ^ v) * mul;
        a ^= (a >> 47);
        u64 b = (v ^ a) * mul;
        b ^= (b >> 47);
        b *= mul;
        return b;
    }

    // Murmur3 helper used by the 32-bit variant: a 32-bit-to-32-bit finalizer mix.
    [[nodiscard]]
    constexpr u32 Fmix(u32 h) noexcept
    {
        h ^= h >> 16;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13;
        h *= 0xC2B2AE35u;
        h ^= h >> 16;
        return h;
    }

    // Murmur3 mixing helper: combine a value into the running hash.
    [[nodiscard]]
    constexpr u32 Mur(u32 a, u32 h) noexcept
    {
        a *= C1;
        a = AZStd::rotr(a, 17);
        a *= C2;
        h ^= a;
        h = AZStd::rotr(h, 19);
        return h * 5u + 0xE6546B64u;
    }

    [[nodiscard]]
    constexpr u32 Hash32Len0to4(const char* s, const AZStd::size_t len) noexcept
    {
        u32 b = 0;
        u32 c = 9;
        for (AZStd::size_t i = 0; i < len; ++i)
        {
            // Sign-extend through signed char to match the reference algorithm exactly.
            const auto v = static_cast<s8>(s[i]);
            b = b * C1 + static_cast<u32>(static_cast<s32>(v));
            c ^= b;
        }
        return Fmix(Mur(b, Mur(static_cast<u32>(len), c)));
    }

    [[nodiscard]]
    constexpr u32 Hash32Len5to12(const char* s, const AZStd::size_t len) noexcept
    {
        u32 a = static_cast<u32>(len);
        u32 b = a * 5;
        u32 c = 9;
        const u32 d = b;
        a += Fetch32(s);
        b += Fetch32(s + len - 4);
        c += Fetch32(s + ((len >> 1) & 4));
        return Fmix(Mur(c, Mur(b, Mur(a, d))));
    }

    [[nodiscard]]
    constexpr u32 Hash32Len13to24(const char* s, const AZStd::size_t len) noexcept
    {
        const u32 a = Fetch32(s + (len >> 1) - 4);
        const u32 b = Fetch32(s + 4);
        const u32 c = Fetch32(s + len - 8);
        const u32 d = Fetch32(s + (len >> 1));
        const u32 e = Fetch32(s);
        const u32 f = Fetch32(s + len - 4);
        const u32 h = static_cast<u32>(len);
        return Fmix(Mur(f, Mur(e, Mur(d, Mur(c, Mur(b, Mur(a, h)))))));
    }

    [[nodiscard]]
    constexpr u32 Compute32(const char* s, AZStd::size_t len) noexcept
    {
        if (len <= 24)
        {
            if (len <= 12)
            {
                return len <= 4 ? Hash32Len0to4(s, len) : Hash32Len5to12(s, len);
            }
            return Hash32Len13to24(s, len);
        }

        // len > 24 - main loop processes 20 bytes per iteration.
        u32 h = static_cast<u32>(len);
        u32 g = C1 * h;
        u32 f = g;
        {
            const u32 a0 = AZStd::rotr(Fetch32(s + len - 4) * C1, 17) * C2;
            const u32 a1 = AZStd::rotr(Fetch32(s + len - 8) * C1, 17) * C2;
            const u32 a2 = AZStd::rotr(Fetch32(s + len - 16) * C1, 17) * C2;
            const u32 a3 = AZStd::rotr(Fetch32(s + len - 12) * C1, 17) * C2;
            const u32 a4 = AZStd::rotr(Fetch32(s + len - 20) * C1, 17) * C2;
            h ^= a0;
            h = AZStd::rotr(h, 19);
            h = h * 5u + 0xE6546B64u;
            h ^= a2;
            h = AZStd::rotr(h, 19);
            h = h * 5u + 0xE6546B64u;
            g ^= a1;
            g = AZStd::rotr(g, 19);
            g = g * 5u + 0xE6546B64u;
            g ^= a3;
            g = AZStd::rotr(g, 19);
            g = g * 5u + 0xE6546B64u;
            f += a4;
            f = AZStd::rotr(f, 19);
            f = f * 5u + 0xE6546B64u;
        }
        AZStd::size_t iters = (len - 1) / 20;
        do
        {
            const u32 a0 = AZStd::rotr(Fetch32(s) * C1, 17) * C2;
            const u32 a1 = Fetch32(s + 4);
            const u32 a2 = AZStd::rotr(Fetch32(s + 8) * C1, 17) * C2;
            const u32 a3 = AZStd::rotr(Fetch32(s + 12) * C1, 17) * C2;
            const u32 a4 = Fetch32(s + 16);
            h ^= a0;
            h = AZStd::rotr(h, 18);
            h = h * 5u + 0xE6546B64u;
            f += a1;
            f = AZStd::rotr(f, 19);
            f = f * C1;
            g += a2;
            g = AZStd::rotr(g, 18);
            g = g * 5u + 0xE6546B64u;
            h ^= a3 + a1;
            h = AZStd::rotr(h, 19);
            h = h * 5u + 0xE6546B64u;
            g ^= a4;
            g = AZStd::byteswap(g) * 5u;
            h += a4 * 5u;
            h = AZStd::byteswap(h);
            f += a0;
            Permute3(f, h, g);
            s += 20;
        }
        while (--iters != 0);
        g = AZStd::rotr(g, 11) * C1;
        g = AZStd::rotr(g, 17) * C1;
        f = AZStd::rotr(f, 11) * C1;
        f = AZStd::rotr(f, 17) * C1;
        h = AZStd::rotr(h + g, 19);
        h = h * 5u + 0xE6546B64u;
        h = AZStd::rotr(h, 17) * C1;
        h = AZStd::rotr(h + f, 19);
        h = h * 5u + 0xE6546B64u;
        h = AZStd::rotr(h, 17) * C1;
        return h;
    }

    [[nodiscard]]
    constexpr u64 HashLen0to16(const char* s, const AZStd::size_t len) noexcept
    {
        if (len >= 8)
        {
            const u64 mul = K2 + len * 2;
            const u64 a = Fetch64(s) + K2;
            const u64 b = Fetch64(s + len - 8);
            const u64 c = AZStd::rotr(b, 37) * mul + a;
            const u64 d = (AZStd::rotr(a, 25) + b) * mul;
            return HashLen16(c, d, mul);
        }
        if (len >= 4)
        {
            const u64 mul = K2 + len * 2;
            const u64 a = Fetch32(s);
            return HashLen16(len + (a << 3), Fetch32(s + len - 4), mul);
        }
        if (len > 0)
        {
            const auto a = static_cast<u8>(s[0]);
            const auto b = static_cast<u8>(s[len >> 1]);
            const auto c = static_cast<u8>(s[len - 1]);
            const u32 y = static_cast<u32>(a) + (static_cast<u32>(b) << 8);
            const u32 z = static_cast<u32>(len) + (static_cast<u32>(c) << 2);
            return ShiftMix(y * K2 ^ z * K0) * K2;
        }
        return K2;
    }

    [[nodiscard]]
    constexpr u64 HashLen17to32(const char* s, const AZStd::size_t len) noexcept
    {
        const u64 mul = K2 + len * 2;
        const u64 a = Fetch64(s) * K1;
        const u64 b = Fetch64(s + 8);
        const u64 c = Fetch64(s + len - 8) * mul;
        const u64 d = Fetch64(s + len - 16) * K2;
        return HashLen16(
            AZStd::rotr(a + b, 43) + AZStd::rotr(c, 30) + d,
            a + AZStd::rotr(b + K2, 18) + c,
            mul);
    }

    // 16-byte hash for 48 bytes, quick and dirty. Callers do best to use "random-looking" a, b.
    [[nodiscard]]
    constexpr u128 WeakHashLen32WithSeeds(
        const u64 w,
        const u64 x,
        const u64 y,
        const u64 z,
        u64 a,
        u64 b) noexcept
    {
        a += w;
        b = AZStd::rotr(b + a + z, 21);
        const u64 c = a;
        a += x;
        a += y;
        b += AZStd::rotr(a, 44);
        return u128{.a = a + z, .b = b + c};
    }

    // 16-byte hash for s[0..31], a, b.
    [[nodiscard]]
    constexpr u128 WeakHashLen32WithSeeds(
        const char* s,
        const u64 a,
        const u64 b) noexcept
    {
        return WeakHashLen32WithSeeds(Fetch64(s), Fetch64(s + 8), Fetch64(s + 16), Fetch64(s + 24), a, b);
    }

    [[nodiscard]]
    constexpr u64 HashLen33to64(const char* s, const AZStd::size_t len) noexcept
    {
        const u64 mul = K2 + len * 2;
        u64 a = Fetch64(s) * K2;
        const u64 b = Fetch64(s + 8);
        const u64 c = Fetch64(s + len - 24);
        const u64 d = Fetch64(s + len - 32);
        const u64 e = Fetch64(s + 16) * K2;
        const u64 f = Fetch64(s + 24) * 9;
        const u64 g = Fetch64(s + len - 8);
        const u64 h = Fetch64(s + len - 16) * mul;
        const u64 u = AZStd::rotr(a + g, 43) + (AZStd::rotr(b, 30) + c) * 9;
        const u64 v = ((a + g) ^ d) + f + 1;
        const u64 w = AZStd::byteswap((u + v) * mul) + h;
        const u64 x = AZStd::rotr(e + f, 42) + c;
        const u64 y = (AZStd::byteswap((v + w) * mul) + g) * mul;
        const u64 z = e + f + c;
        a = AZStd::byteswap((x + z) * mul + y) + b;
        const u64 b2 = ShiftMix((z + a) * mul + d + h) * mul;
        return b2 + x;
    }

    [[nodiscard]]
    constexpr u64 Compute64(const char* s, AZStd::size_t len) noexcept
    {
        if (len <= 32)
        {
            if (len <= 16)
            {
                return HashLen0to16(s, len);
            }
            return HashLen17to32(s, len);
        }
        if (len <= 64)
        {
            return HashLen33to64(s, len);
        }

        // For strings over 64 bytes hash the end first, then loop over 64-byte chunks.
        u64 x = Fetch64(s + len - 40);
        u64 y = Fetch64(s + len - 16) + Fetch64(s + len - 56);
        u64 z = HashLen16(Fetch64(s + len - 48) + len, Fetch64(s + len - 24));
        u128 v = WeakHashLen32WithSeeds(s + len - 64, len, z);
        u128 w = WeakHashLen32WithSeeds(s + len - 32, y + K1, x);
        x = x * K1 + Fetch64(s);

        // Decrease len to the nearest multiple of 64 and operate on 64-byte chunks.
        len = (len - 1) & ~static_cast<AZStd::size_t>(63);
        do
        {
            x = AZStd::rotr(x + y + v.a + Fetch64(s + 8), 37) * K1;
            y = AZStd::rotr(y + v.b + Fetch64(s + 48), 42) * K1;
            x ^= w.b;
            y += v.a + Fetch64(s + 40);
            z = AZStd::rotr(z + w.a, 33) * K1;
            v = WeakHashLen32WithSeeds(s, v.b * K1, x + w.a);
            w = WeakHashLen32WithSeeds(s + 32, z + w.b, y + Fetch64(s + 16));
            AZStd::swap(z, x);
            s += 64;
            len -= 64;
        }
        while (len != 0);
        return HashLen16(
            HashLen16(v.a, w.a) + ShiftMix(y) * K1 + z,
            HashLen16(v.b, w.b) + x);
    }

    [[nodiscard]]
    constexpr u64 Compute64WithSeeds(
        const char* s,
        const AZStd::size_t len,
        const u64 seed0,
        const u64 seed1) noexcept
    {
        return HashLen16(Compute64(s, len) - seed0, seed1);
    }

    [[nodiscard]]
    constexpr u64 Compute64WithSeed(const char* s, const AZStd::size_t len, const u64 seed) noexcept
    {
        return Compute64WithSeeds(s, len, K2, seed);
    }

    // A subroutine for Compute128(): a decent 128-bit hash for any length, based on City and Murmur.
    [[nodiscard]]
    constexpr u128 CityMurmur(const char* s, AZStd::size_t len, const u128 seed) noexcept
    {
        u64 a = seed.a;
        u64 b = seed.b;
        u64 c;
        u64 d;
        if (len <= 16)
        {
            a = ShiftMix(a * K1) * K1;
            c = b * K1 + HashLen0to16(s, len);
            d = ShiftMix(a + (len >= 8 ? Fetch64(s) : c));
        }
        else
        {
            c = HashLen16(Fetch64(s + len - 8) + K1, a);
            d = HashLen16(b + len, c + Fetch64(s + len - 16));
            a += d;
            // len > 16 here, so the do-while is safe.
            do
            {
                a ^= ShiftMix(Fetch64(s) * K1) * K1;
                a *= K1;
                b ^= a;
                c ^= ShiftMix(Fetch64(s + 8) * K1) * K1;
                c *= K1;
                d ^= c;
                s += 16;
                len -= 16;
            }
            while (len > 16);
        }
        a = HashLen16(a, c);
        b = HashLen16(d, b);
        return u128{.a = a ^ b, .b = HashLen16(b, a)};
    }

    [[nodiscard]]
    constexpr u128 Compute128WithSeed(const char* s, AZStd::size_t len, const u128 seed) noexcept
    {
        if (len < 128)
        {
            return CityMurmur(s, len, seed);
        }

        // We expect len >= 128 to be the common case. Keep 56 bytes of state: v, w, x, y, z.
        u128 v;
        u128 w;
        u64 x = seed.a;
        u64 y = seed.b;
        u64 z = len * K1;
        v.a = AZStd::rotr(y ^ K1, 49) * K1 + Fetch64(s);
        v.b = AZStd::rotr(v.a, 42) * K1 + Fetch64(s + 8);
        w.a = AZStd::rotr(y + z, 35) * K1 + x;
        w.b = AZStd::rotr(x + Fetch64(s + 88), 53) * K1;

        // Same inner loop as Compute64(), manually unrolled.
        do
        {
            x = AZStd::rotr(x + y + v.a + Fetch64(s + 8), 37) * K1;
            y = AZStd::rotr(y + v.b + Fetch64(s + 48), 42) * K1;
            x ^= w.b;
            y += v.a + Fetch64(s + 40);
            z = AZStd::rotr(z + w.a, 33) * K1;
            v = WeakHashLen32WithSeeds(s, v.b * K1, x + w.a);
            w = WeakHashLen32WithSeeds(s + 32, z + w.b, y + Fetch64(s + 16));
            AZStd::swap(z, x);
            s += 64;
            x = AZStd::rotr(x + y + v.a + Fetch64(s + 8), 37) * K1;
            y = AZStd::rotr(y + v.b + Fetch64(s + 48), 42) * K1;
            x ^= w.b;
            y += v.a + Fetch64(s + 40);
            z = AZStd::rotr(z + w.a, 33) * K1;
            v = WeakHashLen32WithSeeds(s, v.b * K1, x + w.a);
            w = WeakHashLen32WithSeeds(s + 32, z + w.b, y + Fetch64(s + 16));
            AZStd::swap(z, x);
            s += 64;
            len -= 128;
        }
        while (len >= 128);
        x += AZStd::rotr(v.a + z, 49) * K0;
        y = y * K0 + AZStd::rotr(w.b, 37);
        z = z * K0 + AZStd::rotr(w.a, 27);
        w.a *= 9;
        v.a *= K0;
        // If 0 < len < 128, hash up to 4 chunks of 32 bytes each from the end of s.
        for (AZStd::size_t tail_done = 0; tail_done < len;)
        {
            tail_done += 32;
            y = AZStd::rotr(x + y, 42) * K0 + v.b;
            w.a += Fetch64(s + len - tail_done + 16);
            x = x * K0 + w.a;
            z += w.b + Fetch64(s + len - tail_done);
            w.b += v.a;
            v = WeakHashLen32WithSeeds(s + len - tail_done, v.a + z, v.b);
            v.a *= K0;
        }
        // The 56 bytes of state contain enough information for a strong 128-bit hash.
        // Use two different 56-byte-to-8-byte hashes to get a 16-byte final result.
        x = HashLen16(x, v.a);
        y = HashLen16(y + z, w.a);
        return u128{HashLen16(x + v.b, w.b) + y, HashLen16(x + w.b, y + v.b)};
    }

    [[nodiscard]]
    constexpr u128 Compute128(const char* s, const AZStd::size_t len) noexcept
    {
        return len >= 16
            ? Compute128WithSeed(s + 16, len - 16, u128{.a = Fetch64(s), .b = Fetch64(s + 8) + K0})
            : Compute128WithSeed(s, len, u128{.a = K0, .b = K1});
    }
} // namespace AZ::Hash::Private::City

namespace AZ::Hash
{
    /**
     * CityHash 32-bit variant. Best for short-lived, in-memory keys where a 32-bit width is sufficient.
     *
     * - <a href="https://github.com/google/cityhash">Reference implementation</a>
     */
    struct AZCORE_API City32 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, City32);

        constexpr City32() = default;

        /* Wraps a precomputed value. Does not hash. */
        explicit constexpr City32(const u32 value)
            : m_value{value}
        {
        }

        template<AZStd::size_t N>
        constexpr City32(const char (&str)[N])
            : m_value{Private::City::Compute32(str, N - 1)}
        {
        }

        constexpr City32(const AZStd::string_view view)
            : m_value{Private::City::Compute32(view.data(), view.size())}
        {
        }

        City32(const AZStd::span<const AZStd::byte> data)
            : m_value{Private::City::Compute32(reinterpret_cast<const char*>(data.data()), data.size())}
        {
        }

        City32(const AZStd::span<const u8> data)
            : m_value{Private::City::Compute32(reinterpret_cast<const char*>(data.data()), data.size())}
        {
        }

        [[nodiscard]]
        constexpr u32 GetValue() const noexcept
        {
            return m_value;
        }

        constexpr operator u32() const
        {
            return m_value;
        }

        constexpr bool operator==(const City32&) const = default;

        constexpr std::strong_ordering operator<=>(const City32&) const = default;

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        u32 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, City32);

    /**
     * CityHash 64-bit variant, the general-purpose recommendation.
     * Very fast on 64-bit CPUs and well suited to hash tables and other non-security-sensitive indexing.
     * Accepts optional one or two seed values for seeded hashing.
     *
     * - <a href="https://github.com/google/cityhash">Reference implementation</a>
     */
    struct AZCORE_API City64 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, City64);

        constexpr City64() = default;

        /* Wraps a precomputed value. Does not hash. */
        explicit constexpr City64(const u64 value)
            : m_value{value}
        {
        }

        template<AZStd::size_t N>
        constexpr City64(const char (&str)[N])
            : m_value{Private::City::Compute64(str, N - 1)}
        {
        }

        constexpr City64(const AZStd::string_view view)
            : m_value{Private::City::Compute64(view.data(), view.size())}
        {
        }

        City64(const AZStd::span<const AZStd::byte> data)
            : m_value{Private::City::Compute64(reinterpret_cast<const char*>(data.data()), data.size())}
        {
        }

        City64(const AZStd::span<const u8> data)
            : m_value{Private::City::Compute64(reinterpret_cast<const char*>(data.data()), data.size())}
        {
        }

        template<AZStd::size_t N>
        constexpr City64(const char (&str)[N], const u64 seed)
            : m_value{Private::City::Compute64WithSeed(str, N - 1, seed)}
        {
        }

        constexpr City64(const AZStd::string_view view, const u64 seed)
            : m_value{Private::City::Compute64WithSeed(view.data(), view.size(), seed)}
        {
        }

        City64(const AZStd::span<const AZStd::byte> data, const u64 seed)
            : m_value{Private::City::Compute64WithSeed(reinterpret_cast<const char*>(data.data()), data.size(), seed)}
        {
        }

        City64(const AZStd::span<const u8> data, const u64 seed)
            : m_value{Private::City::Compute64WithSeed(reinterpret_cast<const char*>(data.data()), data.size(), seed)}
        {
        }

        template<AZStd::size_t N>
        constexpr City64(const char (&str)[N], const u64 seed0, const u64 seed1)
            : m_value{Private::City::Compute64WithSeeds(str, N - 1, seed0, seed1)}
        {
        }

        constexpr City64(const AZStd::string_view view, const u64 seed0, const u64 seed1)
            : m_value{Private::City::Compute64WithSeeds(view.data(), view.size(), seed0, seed1)}
        {
        }

        City64(const AZStd::span<const AZStd::byte> data, const u64 seed0, const u64 seed1)
            : m_value{Private::City::Compute64WithSeeds(reinterpret_cast<const char*>(data.data()), data.size(), seed0, seed1)}
        {
        }

        City64(const AZStd::span<const u8> data, const u64 seed0, const u64 seed1)
            : m_value{Private::City::Compute64WithSeeds(reinterpret_cast<const char*>(data.data()), data.size(), seed0, seed1)}
        {
        }

        [[nodiscard]]
        constexpr u64 GetValue() const noexcept
        {
            return m_value;
        }

        constexpr operator u64() const
        {
            return m_value;
        }

        constexpr bool operator==(const City64&) const = default;

        constexpr std::strong_ordering operator<=>(const City64&) const = default;

        explicit constexpr operator bool() const
        {
            return m_value != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        u64 m_value = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, City64);

    /**
     * CityHash 128-bit variant, exposed as a (low, high) pair of 64-bit values.
     * Provides a wider result for non-security-sensitive uses where 64 bits are insufficient.
     *
     * - <a href="https://github.com/google/cityhash">Reference implementation</a>
     */
    struct AZCORE_API City128 final
    {
        AZ_TYPE_INFO_WITH_NAME_DECL_API(AZCORE_API, City128);

        constexpr City128() = default;

        /* Wraps precomputed halves; does not hash. */
        constexpr City128(const u64 low, const u64 high)
            : m_low{low}
            , m_high{high}
        {
        }

        template<AZStd::size_t N>
        constexpr City128(const char (&str)[N])
        {
            const auto h = Private::City::Compute128(str, N - 1);
            m_low = h.a;
            m_high = h.b;
        }

        constexpr City128(const AZStd::string_view view)
        {
            const auto h = Private::City::Compute128(view.data(), view.size());
            m_low = h.a;
            m_high = h.b;
        }

        City128(const AZStd::span<const AZStd::byte> data)
        {
            const auto h = Private::City::Compute128(reinterpret_cast<const char*>(data.data()), data.size());
            m_low = h.a;
            m_high = h.b;
        }

        City128(const AZStd::span<const u8> data)
        {
            const auto h = Private::City::Compute128(reinterpret_cast<const char*>(data.data()), data.size());
            m_low = h.a;
            m_high = h.b;
        }

        template<AZStd::size_t N>
        constexpr City128(const char (&str)[N], const City128 seed)
        {
            const auto h = Private::City::Compute128WithSeed(str, N - 1, u128{.a = seed.m_low, .b = seed.m_high});
            m_low = h.a;
            m_high = h.b;
        }

        constexpr City128(const AZStd::string_view view, const City128 seed)
        {
            const auto h = Private::City::Compute128WithSeed(view.data(), view.size(), u128{.a = seed.m_low, .b = seed.m_high});
            m_low = h.a;
            m_high = h.b;
        }

        City128(const AZStd::span<const AZStd::byte> data, const City128 seed)
        {
            const auto h = Private::City::Compute128WithSeed(
                reinterpret_cast<const char*>(data.data()), data.size(), u128{.a = seed.m_low, .b = seed.m_high});
            m_low = h.a;
            m_high = h.b;
        }

        City128(const AZStd::span<const u8> data, const City128 seed)
        {
            const auto h = Private::City::Compute128WithSeed(
                reinterpret_cast<const char*>(data.data()), data.size(), u128{.a = seed.m_low, .b = seed.m_high});
            m_low = h.a;
            m_high = h.b;
        }

        [[nodiscard]]
        constexpr u64 GetLow() const noexcept
        {
            return m_low;
        }

        [[nodiscard]]
        constexpr u64 GetHigh() const noexcept
        {
            return m_high;
        }

        // Reduce the 128-bit value down to a 64-bit hash suitable for use as a regular hash key.
        [[nodiscard]]
        constexpr u64 GetValue() const noexcept
        {
            return Private::City::Hash128to64(m_low, m_high);
        }

        constexpr bool operator==(const City128&) const = default;

        constexpr std::strong_ordering operator<=>(const City128&) const = default;

        explicit constexpr operator bool() const
        {
            return m_low != 0 || m_high != 0;
        }

        static void Reflect(AZ::ReflectContext* context);

    private:
        u64 m_low = 0;
        u64 m_high = 0;
    };

    AZ_TYPE_INFO_WITH_NAME_DECL_EXT_API(AZCORE_API, City128);
} // namespace AZ::Hash

consteval AZ::Hash::City32 operator""_city32(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::City32{AZStd::string_view{str, count}};
}

consteval AZ::Hash::City64 operator""_city64(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::City64{AZStd::string_view{str, count}};
}

consteval AZ::Hash::City128 operator""_city128(const char* str, const AZStd::size_t count)
{
    return AZ::Hash::City128{AZStd::string_view{str, count}};
}

template<>
struct AZStd::hash<AZ::Hash::City32>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::City32 input) const noexcept
    {
        return input.GetValue();
    }
};

template<>
struct AZStd::hash<AZ::Hash::City64>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::City64 input) const noexcept
    {
        return input.GetValue();
    }
};

template<>
struct AZStd::hash<AZ::Hash::City128>
{
    constexpr AZStd::size_t operator()(const AZ::Hash::City128 input) const noexcept
    {
        return input.GetValue();
    }
};
