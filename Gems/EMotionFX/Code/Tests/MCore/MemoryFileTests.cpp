/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <MCore/Source/MemoryFile.h>
#include <Tests/MCoreSystemFixture.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>

namespace EMotionFX
{
    using MemoryFileTests = MCoreSystemFixture;

    TEST_F(MemoryFileTests, EmptyFileWritesAndReadsSevenBytes)
    {
        MCore::MemoryFile file;
        ASSERT_TRUE(file.Open());
        EXPECT_EQ(file.GetPos(), 0);
        EXPECT_TRUE(file.GetIsEOF());
        const AZStd::array<AZ::u8, 7> data = { 1, 2, 3, 4, 5, 6, 7 };
        ASSERT_EQ(file.Write(data.data(), data.size()), data.size());
        EXPECT_EQ(file.GetPos(), data.size());
        EXPECT_EQ(file.GetFileSize(), data.size());
        ASSERT_TRUE(file.Seek(0));
        AZStd::array<AZ::u8, 7> read{};
        ASSERT_EQ(file.Read(read.data(), read.size()), read.size());
        EXPECT_EQ(read, data);
        EXPECT_TRUE(file.Seek(data.size() + file.GetPreAllocSize()));
        EXPECT_FALSE(file.Seek(data.size() + file.GetPreAllocSize() + 1));
    }

    TEST_F(MemoryFileTests, ZeroLengthOperationsDoNotAllocate)
    {
        MCore::MemoryFile file;
        ASSERT_TRUE(file.Open());
        EXPECT_EQ(file.Write(nullptr, 0), 0);
        EXPECT_EQ(file.Read(nullptr, 0), 0);
        EXPECT_TRUE(file.Seek(0));
        EXPECT_TRUE(file.Forward(0));
        EXPECT_EQ(file.GetMemoryStart(), nullptr);
        EXPECT_EQ(file.GetPos(), 0);
        EXPECT_EQ(file.GetFileSize(), 0);
    }

    TEST_F(MemoryFileTests, GrowthAndOverwritePreserveDataAndFileSize)
    {
        MCore::MemoryFile file;
        ASSERT_TRUE(file.Open(nullptr, 4));
        file.SetPreAllocSize(0);
        const AZStd::array<AZ::u8, 4> data = { 1, 2, 3, 4 };
        ASSERT_EQ(file.Write(data.data(), data.size()), data.size());
        ASSERT_EQ(file.Write(data.data(), data.size()), data.size());
        ASSERT_TRUE(file.Seek(2));
        const AZStd::array<AZ::u8, 2> overwrite = { 9, 10 };
        ASSERT_EQ(file.Write(overwrite.data(), overwrite.size()), overwrite.size());
        EXPECT_EQ(file.GetFileSize(), 8);
        ASSERT_TRUE(file.Seek(0));
        AZStd::array<AZ::u8, 8> read{};
        ASSERT_EQ(file.Read(read.data(), read.size()), read.size());
        const AZStd::array<AZ::u8, 8> expected = { 1, 2, 9, 10, 1, 2, 3, 4 };
        EXPECT_EQ(read, expected);
        EXPECT_TRUE(file.Seek(12));
        EXPECT_FALSE(file.Seek(13));
    }

    TEST_F(MemoryFileTests, FixedBufferRejectsOutOfRangeWritesAndSeeks)
    {
        AZStd::array<AZ::u8, 4> buffer{};
        MCore::MemoryFile file;
        ASSERT_TRUE(file.Open(buffer.data(), buffer.size()));
        const AZStd::array<AZ::u8, 4> data = { 1, 2, 3, 4 };
        ASSERT_EQ(file.Write(data.data(), data.size()), data.size());
        EXPECT_EQ(file.Write(data.data(), 1), 0);
        EXPECT_FALSE(file.WriteByte(5));
        EXPECT_EQ(buffer, data);
        ASSERT_TRUE(file.Seek(0));
        EXPECT_FALSE(file.Forward(AZStd::numeric_limits<size_t>::max()));
        EXPECT_FALSE(file.Seek(AZStd::numeric_limits<size_t>::max()));
        EXPECT_EQ(file.GetPos(), 0);
        EXPECT_TRUE(file.Forward(buffer.size()));
        EXPECT_FALSE(file.Forward(1));
    }

    TEST_F(MemoryFileTests, ReadPastEndReturnsOnlyAvailableBytes)
    {
        AZStd::array<AZ::u8, 4> buffer = { 1, 2, 3, 4 };
        MCore::MemoryFile file;
        ASSERT_TRUE(file.Open(buffer.data(), buffer.size()));
        ASSERT_TRUE(file.Seek(2));
        AZStd::array<AZ::u8, 4> read{};
        EXPECT_EQ(file.Read(read.data(), AZStd::numeric_limits<size_t>::max()), 2);
        EXPECT_EQ(read[0], 3);
        EXPECT_EQ(read[1], 4);
        EXPECT_EQ(read[2], 0);
        EXPECT_EQ(read[3], 0);
        EXPECT_EQ(file.GetPos(), 4);
    }

    TEST_F(MemoryFileTests, GrowthOverflowLeavesFileUntouched)
    {
        MCore::MemoryFile file;
        ASSERT_TRUE(file.Open());
        const AZ::u8 data = 1;
        EXPECT_EQ(file.Write(&data, AZStd::numeric_limits<size_t>::max()), 0);
        EXPECT_EQ(file.GetMemoryStart(), nullptr);
        EXPECT_EQ(file.GetPos(), 0);
        EXPECT_EQ(file.GetFileSize(), 0);
        file.SetPreAllocSize(AZStd::numeric_limits<size_t>::max());
        EXPECT_EQ(file.Write(&data, 1), 0);
        EXPECT_EQ(file.GetMemoryStart(), nullptr);
    }
} // namespace EMotionFX
