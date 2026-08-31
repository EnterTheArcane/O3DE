/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Dependency/Version.h>
#include <AzCore/UnitTest/TestTypes.h>

namespace UnitTest
{
    TEST(VersionTests, ParseFromString_ValidThreePartVersion_ReturnsAllParts)
    {
        auto parsedVersion = AZ::Version<3>::ParseFromString("1.2.3");

        ASSERT_TRUE(parsedVersion.IsSuccess());
        EXPECT_EQ(parsedVersion.GetValue().m_parts, (AZStd::array<AZ::u64, 3>{ 1, 2, 3 }));
    }

    TEST(VersionTests, ParseFromString_NonnumericPart_ReturnsFailure)
    {
        EXPECT_FALSE(AZ::Version<3>::ParseFromString("1.x.3").IsSuccess());
    }

    TEST(VersionTests, Compare_MaximumComponentValues_ReturnsOrderedResultWithoutNarrowing)
    {
        const AZ::Version<1> minimumVersion{ 0 };
        const AZ::Version<1> maximumVersion{ ~AZ::u64{} };

        EXPECT_LT(AZ::Version<1>::Compare(minimumVersion, maximumVersion), 0);
        EXPECT_GT(AZ::Version<1>::Compare(maximumVersion, minimumVersion), 0);
        EXPECT_EQ(AZ::Version<1>::Compare(maximumVersion, maximumVersion), 0);
    }
} // namespace UnitTest
