/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/DebugRenderer.h>

#include <AzTest/AzTest.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace Jolt
{
    namespace
    {
        class CaptureRenderer final
            : public IDebugRenderer
        {
        public:
            void DrawGeometry(
                [[maybe_unused]] AZStd::span<const DebugVertex> vertices,
                [[maybe_unused]] AZStd::span<const AZ::u32> indices,
                [[maybe_unused]] const DebugGeometryConfiguration& configuration) override
            {
                ++m_geometryCount;
            }

            void DrawLine(
                const WorldPosition& start,
                [[maybe_unused]] const WorldPosition& end,
                [[maybe_unused]] const AZ::Color& color) override
            {
                m_lineStartPositions.push_back(start);
            }

            void DrawText(
                const WorldPosition& position,
                const AZStd::string_view text,
                [[maybe_unused]] const AZ::Color& color,
                [[maybe_unused]] const float height) override
            {
                m_textPositions.push_back(position);
                m_text.emplace_back(text);
            }

            void DrawTriangle(
                const WorldPosition& first,
                [[maybe_unused]] const WorldPosition& second,
                [[maybe_unused]] const WorldPosition& third,
                [[maybe_unused]] const AZ::Color& color,
                [[maybe_unused]] const bool castsShadow) override
            {
                m_triangleFirstPositions.push_back(first);
            }

            AZStd::vector<WorldPosition> m_lineStartPositions;
            AZStd::vector<WorldPosition> m_textPositions;
            AZStd::vector<WorldPosition> m_triangleFirstPositions;
            AZStd::vector<AZStd::string> m_text;
            AZ::u32 m_geometryCount = 0;
        };
    } // namespace

    TEST(DebugRendererTests, CaptureBoundsEveryPrimitiveAndHandlesEmptyText)
    {
        DebugCaptureConfiguration configuration;
        configuration.m_geometryCapacity = 0;
        configuration.m_lineCapacity = 0;
        configuration.m_textCapacity = 3;
        configuration.m_textByteCapacity = 3;
        configuration.m_triangleCapacity = 0;

        DebugCapture capture;
        capture.Configure(configuration);
        capture.Begin();

        DebugBatch batch;
        capture.RecordGeometry(batch, {});
        capture.RecordLine({}, {}, 0);
        capture.RecordText({}, {}, 0, 1.0f);
        capture.RecordText({}, "abc", 0, 1.0f);
        capture.RecordText({}, "d", 0, 1.0f);
        capture.RecordTriangle({}, {}, {}, 0, false);
        capture.End();

        const DebugCaptureStatistics statistics = capture.GetStatistics();
        EXPECT_EQ(statistics.m_geometryCount, 0);
        EXPECT_EQ(statistics.m_droppedGeometryCount, 1);
        EXPECT_EQ(statistics.m_lineCount, 0);
        EXPECT_EQ(statistics.m_droppedLineCount, 1);
        EXPECT_EQ(statistics.m_textCount, 2);
        EXPECT_EQ(statistics.m_textByteCount, 3);
        EXPECT_EQ(statistics.m_droppedTextCount, 1);
        EXPECT_EQ(statistics.m_triangleCount, 0);
        EXPECT_EQ(statistics.m_droppedTriangleCount, 1);

        CaptureRenderer renderer;
        capture.Replay(renderer);
        ASSERT_EQ(renderer.m_text.size(), 2);
        EXPECT_TRUE(renderer.m_text[0].empty());
        EXPECT_EQ(renderer.m_text[1], "abc");
    }

    TEST(DebugRendererTests, CaptureReplayUsesCanonicalPrimitiveOrder)
    {
        DebugCaptureConfiguration configuration;
        configuration.m_geometryCapacity = 0;
        configuration.m_lineCapacity = 2;
        configuration.m_textCapacity = 2;
        configuration.m_textByteCapacity = 2;
        configuration.m_triangleCapacity = 2;

        DebugCapture capture;
        capture.Configure(configuration);
        capture.Begin();

        capture.RecordLine({.m_x = 2.0}, {}, 0);
        capture.RecordLine({.m_x = 1.0}, {}, 0);
        capture.RecordText({.m_x = 2.0}, "z", 0, 1.0f);
        capture.RecordText({.m_x = 1.0}, "a", 0, 1.0f);
        capture.RecordTriangle({.m_x = 2.0}, {}, {}, 0, false);
        capture.RecordTriangle({.m_x = 1.0}, {}, {}, 0, false);
        capture.End();

        CaptureRenderer renderer;
        capture.Replay(renderer);
        ASSERT_EQ(renderer.m_lineStartPositions.size(), 2);
        EXPECT_DOUBLE_EQ(renderer.m_lineStartPositions[0].m_x, 1.0);
        EXPECT_DOUBLE_EQ(renderer.m_lineStartPositions[1].m_x, 2.0);
        ASSERT_EQ(renderer.m_textPositions.size(), 2);
        EXPECT_DOUBLE_EQ(renderer.m_textPositions[0].m_x, 1.0);
        EXPECT_DOUBLE_EQ(renderer.m_textPositions[1].m_x, 2.0);
        ASSERT_EQ(renderer.m_triangleFirstPositions.size(), 2);
        EXPECT_DOUBLE_EQ(renderer.m_triangleFirstPositions[0].m_x, 1.0);
        EXPECT_DOUBLE_EQ(renderer.m_triangleFirstPositions[1].m_x, 2.0);
    }
} // namespace Jolt
