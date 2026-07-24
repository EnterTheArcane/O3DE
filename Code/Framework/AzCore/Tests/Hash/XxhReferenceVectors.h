/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// The known-answer vectors and the pseudo-random data-fill in this file are derived from Yann Collet's
// xxHash test suite (https://github.com/Cyan4973/xxHash, v0.8.3, cli/xsum_sanity_check.c).
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

// Each row hashes the first `m_len` bytes of the reference pseudo-random buffer with the given seed.
// MakeReferenceBuffer below generates the input data.
// Generated from the reference tables. Do not edit by hand.

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

namespace UnitTest
{
    inline constexpr u32 XxhSeedPrime32 = 2654435761u;
    inline constexpr u64 XxhSeedPrime64 = 11400714785074694797ull;

    // Reproduces the reference pseudo-random buffer used by xxHash's own sanity checks.
    // inline (shared by XxhTests.cpp and Xxh3Tests.cpp) so it does not collide in unity builds.
    inline AZStd::vector<AZStd::byte> MakeReferenceBuffer(const u32 len)
    {
        AZStd::vector<AZStd::byte> buffer(len);
        u64 byteGen = XxhSeedPrime32;
        for (u32 i = 0; i < len; ++i)
        {
            buffer[i] = static_cast<AZStd::byte>(static_cast<u8>(byteGen >> 56));
            byteGen *= XxhSeedPrime64;
        }
        return buffer;
    }

    struct Xxh32Vector final
    {
        u32 m_len;
        u32 m_seed;
        u32 m_expected;
    };

    inline constexpr Xxh32Vector Xxh32Vectors[] = {
        {0u, 0u, 0x02CC5D05u},
        {0u, 2654435761u, 0x36B78AE7u},
        {1u, 0u, 0xCF65B03Eu},
        {1u, 2654435761u, 0xB4545AA4u},
        {14u, 0u, 0x1208E7E2u},
        {14u, 2654435761u, 0x6AF1D1FEu},
        {222u, 0u, 0x5BD11DBDu},
        {222u, 2654435761u, 0x58803C5Fu},
    };

    struct Xxh64Vector final
    {
        u32 m_len;
        u64 m_seed;
        u64 m_expected;
    };

    inline constexpr Xxh64Vector Xxh64Vectors[] = {
        {0u, 0ull, 0xEF46DB3751D8E999ull},
        {0u, 2654435761ull, 0xAC75FDA2929B17EFull},
        {1u, 0ull, 0xE934A84ADB052768ull},
        {1u, 2654435761ull, 0x5014607643A9B4C3ull},
        {4u, 0ull, 0x9136A0DCA57457EEull},
        {14u, 0ull, 0x8282DCC4994E35C8ull},
        {14u, 2654435761ull, 0xC3BD6BF63DEB6DF0ull},
        {222u, 0ull, 0xB641AE8CB691C174ull},
        {222u, 2654435761ull, 0x20CB8AB7AE10C14Aull},
    };

    struct Xxh3Vector final
    {
        u32 m_len;
        u64 m_seed;
        u64 m_expected;
    };

    inline constexpr Xxh3Vector Xxh3Vectors[] = {
        {0u, 0ull, 0x2D06800538D394C2ull},
        {0u, 11400714785074694797ull, 0xA8A6B918B2F0364Aull},
        {1u, 0ull, 0xC44BDFF4074EECDBull},
        {1u, 11400714785074694797ull, 0x032BE332DD766EF8ull},
        {6u, 0ull, 0x27B56A84CD2D7325ull},
        {6u, 11400714785074694797ull, 0x84589C116AB59AB9ull},
        {12u, 0ull, 0xA713DAF0DFBB77E7ull},
        {12u, 11400714785074694797ull, 0xE7303E1B2336DE0Eull},
        {24u, 0ull, 0xA3FE70BF9D3510EBull},
        {24u, 11400714785074694797ull, 0x850E80FC35BDD690ull},
        {48u, 0ull, 0x397DA259ECBA1F11ull},
        {48u, 11400714785074694797ull, 0xADC2CBAA44ACC616ull},
        {80u, 0ull, 0xBCDEFBBB2C47C90Aull},
        {80u, 11400714785074694797ull, 0xC6DD0CB699532E73ull},
        {195u, 0ull, 0xCD94217EE362EC3Aull},
        {195u, 11400714785074694797ull, 0xBA68003D370CB3D9ull},
        {403u, 0ull, 0xCDEB804D65C6DEA4ull},
        {403u, 11400714785074694797ull, 0x6259F6ECFD6443FDull},
        {512u, 0ull, 0x617E49599013CB6Bull},
        {512u, 11400714785074694797ull, 0x3CE457DE14C27708ull},
        {2048u, 0ull, 0xDD59E2C3A5F038E0ull},
        {2048u, 11400714785074694797ull, 0x66F81670669ABABCull},
        {2099u, 0ull, 0xC6B9D9B3FC9AC765ull},
        {2099u, 11400714785074694797ull, 0x184F316843663974ull},
        {2240u, 0ull, 0x6E73A90539CF2948ull},
        {2240u, 11400714785074694797ull, 0x757BA8487D1B5247ull},
        {2367u, 0ull, 0xCB37AEB9E5D361EDull},
        {2367u, 11400714785074694797ull, 0xD2DB3415B942B42Aull},
    };

    struct Xxh128Vector final
    {
        u32 m_len;
        u64 m_seed;
        u64 m_low;
        u64 m_high;
    };

    inline constexpr Xxh128Vector Xxh128Vectors[] = {
        {0u, 0ull, 0x6001C324468D497Full, 0x99AA06D3014798D8ull},
        {0u, 2654435761ull, 0x5444F7869C671AB0ull, 0x92220AE55E14AB50ull},
        {1u, 0ull, 0xC44BDFF4074EECDBull, 0xA6CD5E9392000F6Aull},
        {1u, 2654435761ull, 0xB53D5557E7F76F8Dull, 0x89B99554BA22467Cull},
        {6u, 0ull, 0x3E7039BDDA43CFC6ull, 0x082AFE0B8162D12Aull},
        {6u, 2654435761ull, 0x269D8F70BE98856Eull, 0x5A865B5389ABD2B1ull},
        {12u, 0ull, 0x061A192713F69AD9ull, 0x6E3EFD8FC7802B18ull},
        {12u, 2654435761ull, 0x9BE9F9A67F3C7DFBull, 0xD7E09D518A3405D3ull},
        {24u, 0ull, 0x1E7044D28B1B901Dull, 0x0CE966E4678D3761ull},
        {24u, 2654435761ull, 0xD7304C54EBAD40A9ull, 0x3162026714A6A243ull},
        {48u, 0ull, 0xF942219AED80F67Bull, 0xA002AC4E5478227Eull},
        {48u, 2654435761ull, 0x7BA3C3E453A1934Eull, 0x163ADDE36C072295ull},
        {81u, 0ull, 0x5E8BAFB9F95FB803ull, 0x4952F58181AB0042ull},
        {81u, 2654435761ull, 0x703FBB3D7A5F755Cull, 0x2724EC7ADC750FB6ull},
        {222u, 0ull, 0xF1AEBD597CEC6B3Aull, 0x337E09641B948717ull},
        {222u, 2654435761ull, 0xAE995BB8AF917A8Dull, 0x91820016621E97F1ull},
        {403u, 0ull, 0xCDEB804D65C6DEA4ull, 0x1B6DE21E332DD73Dull},
        {403u, 11400714785074694797ull, 0x6259F6ECFD6443FDull, 0xBED311971E0BE8F2ull},
        {512u, 0ull, 0x617E49599013CB6Bull, 0x18D2D110DCC9BCA1ull},
        {512u, 11400714785074694797ull, 0x3CE457DE14C27708ull, 0x925D06B8EC5B8040ull},
        {2048u, 0ull, 0xDD59E2C3A5F038E0ull, 0xF736557FD47073A5ull},
        {2048u, 2654435761ull, 0x230D43F30206260Bull, 0x7FB03F7E7186C3EAull},
        {2240u, 0ull, 0x6E73A90539CF2948ull, 0xCCB134FBFA7CE49Dull},
        {2240u, 2654435761ull, 0xED385111126FBA6Full, 0x50A1FE17B338995Full},
        {2367u, 0ull, 0xCB37AEB9E5D361EDull, 0xE89C0F6FF369B427ull},
        {2367u, 2654435761ull, 0x6F5360AE69C2F406ull, 0xD23AAE4B76C31ECBull},
    };
} // namespace UnitTest
