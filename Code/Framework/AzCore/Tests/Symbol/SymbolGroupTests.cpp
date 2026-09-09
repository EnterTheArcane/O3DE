/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/Internal/SymbolGroup.h>
#include <AzCore/UnitTest/TestTypes.h>

namespace UnitTest
{
    class SymbolGroupTests
        : public LeakDetectionFixture
    {
    };

    TEST_F(SymbolGroupTests, EveryLaneMaskMatchesAndFindsEmptyControls)
    {
        AZ::u8 controls[AZ::Internal::SymbolGroup::Width];
        for (AZ::u32 mask = 0; mask <= 0xFFFF; ++mask)
        {
            for (size_t lane = 0; lane < AZ::Internal::SymbolGroup::Width; ++lane)
            {
                controls[lane] = AZ::Internal::SymbolGroupEmptyControl;
                if ((mask & (1u << lane)) != 0)
                {
                    controls[lane] = 0x7F;
                }
            }
            const auto native = AZ::Internal::SymbolGroup::Match(controls, 0x7F);
            const auto scalar = AZ::Internal::SymbolGroup::MatchScalar(controls, 0x7F);
            EXPECT_EQ(native.m_matches, mask);
            EXPECT_EQ(native.m_empty, static_cast<AZ::u16>(~mask));
            EXPECT_EQ(native.m_matches, scalar.m_matches);
            EXPECT_EQ(native.m_empty, scalar.m_empty);
        }
    }

    TEST_F(SymbolGroupTests, MatchingSupportsUnalignedControlsAndEveryFingerprint)
    {
        AZ::u8 storage[AZ::Internal::SymbolGroup::Width * 2];
        for (size_t offset = 0; offset < AZ::Internal::SymbolGroup::Width; ++offset)
        {
            AZ::u8* controls = storage + offset;
            for (AZ::u16 fingerprint = 0; fingerprint < 0x80; ++fingerprint)
            {
                for (size_t lane = 0; lane < AZ::Internal::SymbolGroup::Width; ++lane)
                {
                    controls[lane] = static_cast<AZ::u8>(fingerprint);
                }
                const auto native = AZ::Internal::SymbolGroup::Match(controls, static_cast<AZ::u8>(fingerprint));
                EXPECT_EQ(native.m_matches, 0xFFFF);
                EXPECT_EQ(native.m_empty, 0);
            }
        }
    }

    TEST_F(SymbolGroupTests, NativeMatch_EqualsScalarOracle)
    {
        alignas(16) AZ::u8 controls[AZ::Internal::SymbolGroup::Width];
        AZ::u32 generator = 0x9E3779B9u;

        for (size_t testIndex = 0; testIndex < 4096; ++testIndex)
        {
            for (size_t lane = 0; lane < AZ::Internal::SymbolGroup::Width; ++lane)
            {
                generator = generator * 1664525u + 1013904223u;
                controls[lane] = static_cast<AZ::u8>(generator >> 24);
                if ((generator & 7u) == 0)
                {
                    controls[lane] = AZ::Internal::SymbolGroupEmptyControl;
                }
                else
                {
                    controls[lane] &= 0x7F;
                }
            }

            const AZ::u8 fingerprint = static_cast<AZ::u8>((generator >> 8) & 0x7F);
            const AZ::Internal::SymbolGroupMasks scalar =
                AZ::Internal::SymbolGroup::MatchScalar(controls, fingerprint);
            const AZ::Internal::SymbolGroupMasks native =
                AZ::Internal::SymbolGroup::Match(controls, fingerprint);
            EXPECT_EQ(native.m_matches, scalar.m_matches);
            EXPECT_EQ(native.m_empty, scalar.m_empty);
        }
    }
} // namespace UnitTest
