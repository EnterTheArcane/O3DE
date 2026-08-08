/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <VertexFormats.h>

#include <array>
#include <cstring>

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

TEST(AtomFontSanityTest, Sanity)
{
    EXPECT_EQ(1, 1);
}

TEST(AtomFontVertexFormatsTest, PackedVertexBytesMatchDeclaredGpuLayout)
{
    SVF_P3F_C4B_T2F vertex{};
    vertex.xyz = AZ::PackedVector3f(1.0f, 2.0f, 3.0f);
    vertex.color.dcolor = 0x44332211;
    vertex.st = AZ::PackedVector2f(4.0f, 5.0f);

    std::array<unsigned char, sizeof(vertex)> bytes{};
    std::memcpy(bytes.data(), &vertex, sizeof(vertex));

    float xyz[3]{};
    std::memcpy(xyz, bytes.data(), sizeof(xyz));
    EXPECT_FLOAT_EQ(xyz[0], 1.0f);
    EXPECT_FLOAT_EQ(xyz[1], 2.0f);
    EXPECT_FLOAT_EQ(xyz[2], 3.0f);

    uint32 color = 0;
    std::memcpy(&color, bytes.data() + 12, sizeof(color));
    EXPECT_EQ(color, 0x44332211);

    float uv[2]{};
    std::memcpy(uv, bytes.data() + 16, sizeof(uv));
    EXPECT_FLOAT_EQ(uv[0], 4.0f);
    EXPECT_FLOAT_EQ(uv[1], 5.0f);
}

TEST(AtomFontVertexFormatsTest, PositionColorVertexBytesMatchDeclaredGpuLayout)
{
    SVF_P3F_C4B vertex{};
    vertex.xyz = AZ::PackedVector3f(1.0f, 2.0f, 3.0f);
    vertex.color.dcolor = 0x44332211;

    std::array<unsigned char, sizeof(vertex)> bytes{};
    std::memcpy(bytes.data(), &vertex, sizeof(vertex));

    float xyz[3]{};
    std::memcpy(xyz, bytes.data(), sizeof(xyz));
    EXPECT_FLOAT_EQ(xyz[0], 1.0f);
    EXPECT_FLOAT_EQ(xyz[1], 2.0f);
    EXPECT_FLOAT_EQ(xyz[2], 3.0f);

    uint32 color = 0;
    std::memcpy(&color, bytes.data() + 12, sizeof(color));
    EXPECT_EQ(color, 0x44332211);
}

TEST(AtomFontVertexFormatsTest, PositionVertexBytesMatchDeclaredGpuLayout)
{
    SVF_P3F vertex{};
    vertex.xyz = AZ::PackedVector3f(1.0f, 2.0f, 3.0f);

    std::array<unsigned char, sizeof(vertex)> bytes{};
    std::memcpy(bytes.data(), &vertex, sizeof(vertex));

    float xyz[3]{};
    std::memcpy(xyz, bytes.data(), sizeof(xyz));
    EXPECT_FLOAT_EQ(xyz[0], 1.0f);
    EXPECT_FLOAT_EQ(xyz[1], 2.0f);
    EXPECT_FLOAT_EQ(xyz[2], 3.0f);
}

TEST(AtomFontVertexFormatsTest, UiFontVertexBytesMatchDeclaredGpuLayout)
{
    SVF_P2F_C4B_T2F_F4B vertex{};
    vertex.xy = AZ::PackedVector2f(1.0f, 2.0f);
    vertex.color.dcolor = 0x44332211;
    vertex.st = AZ::PackedVector2f(3.0f, 4.0f);
    vertex.texIndex = 5;
    vertex.texHasColorChannel = 6;
    vertex.texIndex2 = 7;
    vertex.pad = 8;

    std::array<unsigned char, sizeof(vertex)> bytes{};
    std::memcpy(bytes.data(), &vertex, sizeof(vertex));

    float position[2]{};
    std::memcpy(position, bytes.data(), sizeof(position));
    EXPECT_FLOAT_EQ(position[0], 1.0f);
    EXPECT_FLOAT_EQ(position[1], 2.0f);

    uint32 color = 0;
    std::memcpy(&color, bytes.data() + 8, sizeof(color));
    EXPECT_EQ(color, 0x44332211);

    float uv[2]{};
    std::memcpy(uv, bytes.data() + 12, sizeof(uv));
    EXPECT_FLOAT_EQ(uv[0], 3.0f);
    EXPECT_FLOAT_EQ(uv[1], 4.0f);
    EXPECT_EQ(bytes[20], 5);
    EXPECT_EQ(bytes[21], 6);
    EXPECT_EQ(bytes[22], 7);
    EXPECT_EQ(bytes[23], 8);
}
