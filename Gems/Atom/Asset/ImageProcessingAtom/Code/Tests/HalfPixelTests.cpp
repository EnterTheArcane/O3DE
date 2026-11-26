/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/UnitTest/TestTypes.h>
#include <Converters/PixelOperation.h>
#include <cmath>
#include <cstring>

namespace UnitTest
{
    using HalfPixelTests = LeakDetectionFixture;

    TEST_F(HalfPixelTests, DecodeExceptionalChannels)
    {
        const AZ::u16 encoded[] = {0x7c00, 0xfc00, 0x7e00, 0x8000};
        AZ::u8 bytes[sizeof(encoded) + 1]{};
        memcpy(bytes + 1, encoded, sizeof(encoded));
        auto operation = ImageProcessingAtom::CreatePixelOperation(ImageProcessingAtom::ePixelFormat_R16G16B16A16F);
        float r, g, b, a;
        operation->GetRGBA(bytes + 1, r, g, b, a);
        EXPECT_TRUE(std::isinf(r));
        EXPECT_GT(r, 0.0f);
        EXPECT_TRUE(std::isinf(g));
        EXPECT_LT(g, 0.0f);
        EXPECT_TRUE(std::isnan(b));
        EXPECT_EQ(a, 0.0f);
        EXPECT_TRUE(std::signbit(a));
    }

    TEST_F(HalfPixelTests, EncodeOverflowAndPreserveUnalignedPixelBoundaries)
    {
        AZ::u8 bytes[10];
        memset(bytes, 0x55, sizeof(bytes));
        auto operation = ImageProcessingAtom::CreatePixelOperation(ImageProcessingAtom::ePixelFormat_R16G16B16A16F);
        operation->SetRGBA(bytes + 1, 65520.0f, -65520.0f, 1.0f, -0.0f);
        AZ::u16 encoded[4];
        memcpy(encoded, bytes + 1, sizeof(encoded));
        EXPECT_EQ(encoded[0], 0x7c00);
        EXPECT_EQ(encoded[1], 0xfc00);
        EXPECT_EQ(encoded[2], 0x3c00);
        EXPECT_EQ(encoded[3], 0x8000);
        EXPECT_EQ(bytes[0], 0x55);
        EXPECT_EQ(bytes[9], 0x55);
    }

    TEST_F(HalfPixelTests, ChannelDefaultsAndHdrValuesAcrossHalfFormats)
    {
        using namespace ImageProcessingAtom;
        const EPixelFormat formats[] = {ePixelFormat_R16F, ePixelFormat_R16G16F, ePixelFormat_R16G16B16A16F};
        const size_t channelCounts[] = {1, 2, 4};
        const AZ::u16 expectedBits[] = {0xc000, 0x0001, 0x5000, 0x8000};
        for (size_t formatIndex = 0; formatIndex < 3; ++formatIndex)
        {
            const size_t channels = channelCounts[formatIndex];
            AZ::u8 bytes[18];
            memset(bytes, 0x55, sizeof(bytes));
            auto operation = CreatePixelOperation(formats[formatIndex]);
            operation->SetRGBA(bytes + 1 + channels * 2, -2.0f, 0x1p-24f, 32.0f, -0.0f);
            EXPECT_EQ(bytes[channels * 2], 0x55);
            EXPECT_EQ(bytes[1 + channels * 4], 0x55);
            AZ::u16 encoded[4]{};
            memcpy(encoded, bytes + 1 + channels * 2, channels * 2);
            for (size_t channel = 0; channel < channels; ++channel)
            {
                EXPECT_EQ(encoded[channel], expectedBits[channel]);
            }
            float r, g, b, a;
            operation->GetRGBA(bytes + 1 + channels * 2, r, g, b, a);
            EXPECT_EQ(r, -2.0f);
            if (channels == 1)
            {
                EXPECT_EQ(g, r);
                EXPECT_EQ(b, r);
            }
            else
            {
                EXPECT_EQ(g, 0x1p-24f);
                if (channels == 2)
                {
                    EXPECT_EQ(b, 0.0f);
                }
            }
            if (channels == 4)
            {
                EXPECT_EQ(b, 32.0f);
                EXPECT_EQ(a, 0.0f);
                EXPECT_TRUE(std::signbit(a));
            }
            else
            {
                EXPECT_EQ(a, 1.0f);
            }
        }
    }
}
