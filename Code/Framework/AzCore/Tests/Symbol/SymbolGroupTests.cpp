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
