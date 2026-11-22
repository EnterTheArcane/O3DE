/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzNetworking/DataStructures/FixedSizeVectorBitset.h>
#include <AzNetworking/Serialization/NetworkOutputSerializer.h>
#include <AzCore/UnitTest/TestTypes.h>

namespace UnitTest
{
    TEST(FixedSizeVectorBitsetTests, RejectsOversizedCountBeforeReadingBits)
    {
        constexpr uint8_t Capacity = 8;
        // This input ends after the count, so even a reader that attempts the first bit read stops there.
        const uint8_t count = Capacity + 1;
        AzNetworking::NetworkOutputSerializer serializer(&count, sizeof(count));
        AzNetworking::FixedSizeVectorBitset<Capacity> bitset;
        ASSERT_TRUE(bitset.PushBack(true));

        EXPECT_FALSE(bitset.Serialize(serializer));
        EXPECT_FALSE(serializer.IsValid());
        EXPECT_EQ(bitset.GetSize(), 1u);
        EXPECT_TRUE(bitset.GetBit(0));
    }
}
