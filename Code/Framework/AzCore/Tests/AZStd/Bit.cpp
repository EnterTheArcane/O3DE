/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/base.h>
#include <AzCore/std/bit.h>
#include <AzCore/UnitTest/TestTypes.h>

namespace UnitTest
{
    TEST(BitTests, BitFunctions)
    {
        static_assert(AZStd::bit_cast<AZ::u32>(1.0f) == 0x3F800000u);
        static_assert(AZStd::bit_ceil(AZ::u32{5}) == 8u);
        static_assert(AZStd::bit_floor(AZ::u32{5}) == 4u);
        static_assert(AZStd::bit_width(AZ::u32{16}) == 5);
        static_assert(AZStd::countl_one(AZ::u32{0xFFFFFFFFu}) == 32);
        static_assert(AZStd::countl_zero(AZ::u32{1}) == 31);
        static_assert(AZStd::countr_one(AZ::u32{0xFu}) == 4);
        static_assert(AZStd::countr_zero(AZ::u32{0x10u}) == 4);
        static_assert(AZStd::endian::little != AZStd::endian::big);
        static_assert(AZStd::has_single_bit(AZ::u32{8}));
        static_assert(AZStd::popcount(AZ::u32{0xF0F0u}) == 8);
        static_assert(AZStd::rotl(AZ::u32{0x12345678u}, 8) == 0x34567812u);
        static_assert(AZStd::rotr(AZ::u32{0x12345678u}, 8) == 0x78123456u);

        SUCCEED();
}

    TEST(BitTests, Byteswap)
    {
        static_assert(AZStd::byteswap(true));
        static_assert(AZStd::byteswap(AZ::u8{0x12u}) == 0x12u);
        static_assert(AZStd::byteswap(AZ::u16{0x1234u}) == 0x3412u);
        static_assert(AZStd::byteswap(AZ::u32{0x12345678u}) == 0x78563412u);
        static_assert(AZStd::byteswap(AZ::u64{0x0123456789ABCDEFull}) == 0xEFCDAB8967452301ull);
        static_assert(AZStd::byteswap(AZ::s32{0x12345678}) == 0x78563412);
        static_assert(noexcept(AZStd::byteswap(AZ::u64{})));

        AZ::u64 value = 0x0123456789ABCDEFull;
        EXPECT_EQ(AZStd::byteswap(value), 0xEFCDAB8967452301ull);
        EXPECT_EQ(AZStd::byteswap(AZStd::byteswap(value)), value);
}
} // namespace UnitTest
