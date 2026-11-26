/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Color.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RHI.Reflect/ImageDescriptor.h>
#include <cstring>
#include <cmath>

namespace UnitTest
{
    TEST(HalfImageTests, DecodeDistinctChannelsAtNonzeroCoordinates)
    {
        const AZ::RHI::Format formats[] = {
            AZ::RHI::Format::R16_FLOAT, AZ::RHI::Format::R16G16_FLOAT, AZ::RHI::Format::R16G16B16A16_FLOAT};
        const AZ::u16 channelBits[6][4] = {
            {0x3c00, 0x4000, 0x4200, 0x4400}, {0x4500, 0x4600, 0x4700, 0x4800},
            {0x4880, 0x4900, 0x4980, 0x4a00}, {0x4a80, 0x4b00, 0x4b80, 0x4c00},
            {0x4c40, 0x4c80, 0x4cc0, 0x4d00}, {0x4d40, 0x4d80, 0x4dc0, 0x4e00}};
        for (const auto format : formats)
        {
            const AZ::u32 channels = AZ::RHI::GetFormatComponentCount(format);
            const auto descriptor = AZ::RHI::ImageDescriptor::Create2D(AZ::RHI::ImageBindFlags::ShaderRead, 3, 2, format);
            AZ::u8 bytes[6 * 4 * sizeof(AZ::u16) + 1]{};
            for (AZ::u32 pixel = 0; pixel < 6; ++pixel)
            {
                for (AZ::u32 channel = 0; channel < channels; ++channel)
                {
                    memcpy(bytes + 1 + (pixel * channels + channel) * sizeof(AZ::u16), &channelBits[pixel][channel], sizeof(AZ::u16));
                }
            }
            const AZStd::span<const AZ::u8> image(bytes + 1, 6 * channels * sizeof(AZ::u16));
            for (AZ::u32 channel = 0; channel < channels; ++channel)
            {
                EXPECT_EQ(AZ::RPI::GetImageDataPixelValue<float>(image, descriptor, 2, 1, channel), static_cast<float>(channel + 21));
            }
            const auto color = AZ::RPI::GetImageDataPixelValue<AZ::Color>(image, descriptor, 2, 1);
            EXPECT_EQ(color.GetR(), 21.0f);
            if (channels >= 2)
            {
                EXPECT_EQ(color.GetG(), 22.0f);
            }
            else
            {
                EXPECT_EQ(color.GetG(), 0.0f);
            }
            if (channels == 4)
            {
                EXPECT_EQ(color.GetB(), 23.0f);
                EXPECT_EQ(color.GetA(), 24.0f);
            }
            else
            {
                EXPECT_EQ(color.GetB(), 0.0f);
                EXPECT_EQ(color.GetA(), 1.0f);
            }
        }
    }

    TEST(HalfImageTests, DecodeExceptionalValuesAndSubnormalAtBufferEnd)
    {
        const AZ::u16 encoded[] = {0x7c00, 0xfc00, 0x7d01, 0x8000, 0x0001};
        AZ::u8 bytes[sizeof(encoded) + 1]{};
        memcpy(bytes + 1, encoded, sizeof(encoded));
        const auto descriptor = AZ::RHI::ImageDescriptor::Create2D(AZ::RHI::ImageBindFlags::ShaderRead, 5, 1, AZ::RHI::Format::R16_FLOAT);
        const AZStd::span<const AZ::u8> image(bytes + 1, sizeof(encoded));
        EXPECT_EQ(AZ::RPI::GetImageDataPixelValue<float>(image, descriptor, 0, 0), INFINITY);
        EXPECT_EQ(AZ::RPI::GetImageDataPixelValue<float>(image, descriptor, 1, 0), -INFINITY);
        EXPECT_TRUE(std::isnan(AZ::RPI::GetImageDataPixelValue<float>(image, descriptor, 2, 0)));
        EXPECT_TRUE(std::signbit(AZ::RPI::GetImageDataPixelValue<float>(image, descriptor, 3, 0)));
        EXPECT_EQ(AZ::RPI::GetImageDataPixelValue<float>(image, descriptor, 4, 0), 0x1p-24f);
        EXPECT_EQ(AZ::RPI::GetImageDataPixelValue<AZ::Color>(image, descriptor, 4, 0).GetR(), 0x1p-24f);
    }
}
