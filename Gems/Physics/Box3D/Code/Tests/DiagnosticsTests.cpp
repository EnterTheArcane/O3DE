/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Diagnostics.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        class CountingRenderer final
            : public IDebugRenderer
        {
        public:
            void DrawBounds(
                const AZ::Aabb&,
                const AZ::Color&) override
            {
                ++m_drawCallCount;
            }

            void DrawBox(
                const AZ::Vector3&,
                const AZ::Transform&,
                const AZ::Color&) override
            {
                ++m_drawCallCount;
            }

            void DrawCapsule(
                const AZ::Vector3&,
                const AZ::Vector3&,
                float,
                const AZ::Color&,
                float) override
            {
                ++m_drawCallCount;
            }

            void DrawLine(
                const AZ::Vector3&,
                const AZ::Vector3&,
                const AZ::Color&) override
            {
                ++m_drawCallCount;
            }

            void DrawPoint(
                const AZ::Vector3&,
                float,
                const AZ::Color&) override
            {
                ++m_drawCallCount;
            }

            void DrawSphere(
                const AZ::Vector3&,
                float,
                const AZ::Color&,
                float) override
            {
                ++m_drawCallCount;
            }

            void DrawText(
                const AZ::Vector3&,
                AZStd::string_view,
                const AZ::Color&) override
            {
                ++m_drawCallCount;
            }

            void DrawTransform(
                const AZ::Transform&) override
            {
                ++m_drawCallCount;
            }

            bool DrawTriangles(
                AZStd::span<const AZ::Vector3> vertices,
                AZStd::span<const AZ::u32> indices,
                const AZ::Transform& transform,
                const AZ::Color& color,
                DebugMaterialPreset) override
            {
                ++m_drawCallCount;
                return !vertices.empty() && !indices.empty() && transform.IsFinite() && color.IsFinite();
            }

            [[nodiscard]]
            AZ::u32 GetDrawCallCount() const
            {
                return m_drawCallCount;
            }

        private:
            AZ::u32 m_drawCallCount = 0;
        };

        BodyHandle CreateBody(
            System& system,
            WorldHandle worldHandle,
            BodyType bodyType,
            const AZ::Vector3& position)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = bodyType;
            configuration.m_transform = AZ::Transform::CreateTranslation(position);
            return system.CreateBody(worldHandle, configuration);
        }
    } // namespace

    TEST(
        Box3DDiagnosticsTests,
        ReportsStatisticsDrawsAndRebuildsStaticTree)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyHandle, shapeConfiguration).IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, StatisticsFlags::All, statistics));
        EXPECT_EQ(statistics.m_simulationTick, 1);
        EXPECT_EQ(statistics.m_counters.m_bodyCount, 1);
        EXPECT_EQ(statistics.m_counters.m_shapeCount, 1);
        EXPECT_GE(statistics.m_lastStep.m_total.count(), 0.0f);
        EXPECT_TRUE(statistics.m_worldBounds.IsValid());

        CountingRenderer renderer;
        EXPECT_TRUE(system.DrawWorld(worldHandle, DebugDrawSettings{}, renderer));
        EXPECT_GT(renderer.GetDrawCallCount(), 0);
        EXPECT_TRUE(system.RebuildStaticTree(worldHandle));
    }

    TEST(
        Box3DDiagnosticsTests,
        RecordingReplaySupportsSeekingQueriesBodiesAndDrawing)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.StartRecording(worldHandle, 4096));

        const BodyHandle ground = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateAxisZ(-0.5f));
        ShapeConfiguration groundShape;
        groundShape.m_geometry = BoxShapeConfiguration{AZ::Vector3(4.0f, 4.0f, 0.5f)};
        ASSERT_TRUE(system.CreateShape(worldHandle, ground, groundShape).IsValid());
        const BodyHandle fallingBody = CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisZ(2.0f));
        ShapeConfiguration sphereShape;
        sphereShape.m_geometry = SphereShapeConfiguration{};
        ASSERT_TRUE(system.CreateShape(worldHandle, fallingBody, sphereShape).IsValid());

        RaycastRequest raycast;
        raycast.m_start = AZ::Vector3::CreateAxisZ(4.0f);
        raycast.m_direction = -AZ::Vector3::CreateAxisZ();
        raycast.m_distance = 10.0f;
        for (AZ::u32 frame = 0; frame < 12; ++frame)
        {
            AZStd::array<QueryHit, 4> hits;
            EXPECT_GT(system.Raycast(worldHandle, raycast, hits).m_hitCount, 0);
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        AZStd::vector<AZ::u8> recording;
        ASSERT_TRUE(system.StopRecording(worldHandle, recording));
        ASSERT_FALSE(recording.empty());
        EXPECT_TRUE(system.ValidateRecording(recording, 1));

        AZStd::unique_ptr<IReplay> replay = system.CreateReplay(recording, 1);
        ASSERT_NE(replay, nullptr);
        const ReplayInfo info = replay->GetInfo();
        EXPECT_GE(info.m_frameCount, 12);
        EXPECT_EQ(info.m_workerCount, 1);
        EXPECT_NEAR(info.m_timeStep, 1.0f / 60.0f, 0.00001f);
        EXPECT_FLOAT_EQ(info.m_lengthScale, 1.0f);

        replay->SetKeyframePolicy(64 * 1024, 2);
        EXPECT_EQ(replay->GetKeyframeBudget(), 64 * 1024);
        EXPECT_EQ(replay->GetMinimumKeyframeInterval(), 2);

        bool sawRecordedQuery = false;
        ReplayShapeId firstHitShapeId;
        for (AZ::u32 frame = 0; frame < info.m_frameCount && replay->Step(); ++frame)
        {
            ReplayBody body;
            ASSERT_TRUE(replay->GetBody(0, body));
            for (AZ::u32 queryIndex = 0; queryIndex < replay->GetFrameQueryCount(); ++queryIndex)
            {
                ReplayQuery query;
                ASSERT_TRUE(replay->GetFrameQuery(queryIndex, query));
                sawRecordedQuery = true;
                if (query.m_hitCount > 0)
                {
                    ReplayQueryHit hit;
                    EXPECT_TRUE(replay->GetFrameQueryHit(queryIndex, 0, hit));
                    EXPECT_TRUE(hit.m_shapeId.IsValid());
                    if (!firstHitShapeId.IsValid())
                    {
                        firstHitShapeId = hit.m_shapeId;
                    }
                }
            }
        }
        EXPECT_TRUE(sawRecordedQuery);
        EXPECT_FALSE(replay->Step());
        EXPECT_TRUE(replay->IsAtEnd());
        EXPECT_FALSE(replay->HasDiverged());

        replay->Seek(info.m_frameCount / 2);
        EXPECT_EQ(replay->GetFrame(), info.m_frameCount / 2);
        replay->Restart();
        EXPECT_EQ(replay->GetFrame(), 0);
        ASSERT_TRUE(replay->Step());
        bool comparedRestartedHit = false;
        for (AZ::u32 queryIndex = 0; queryIndex < replay->GetFrameQueryCount(); ++queryIndex)
        {
            ReplayQuery query;
            ASSERT_TRUE(replay->GetFrameQuery(queryIndex, query));
            if (query.m_hitCount > 0)
            {
                ReplayQueryHit hit;
                ASSERT_TRUE(replay->GetFrameQueryHit(queryIndex, 0, hit));
                EXPECT_EQ(hit.m_shapeId, firstHitShapeId);
                comparedRestartedHit = true;
                break;
            }
        }
        EXPECT_TRUE(comparedRestartedHit);

        CountingRenderer renderer;
        EXPECT_TRUE(replay->Draw(DebugDrawSettings{}, renderer));
        EXPECT_GT(renderer.GetDrawCallCount(), 0);
        ReplayBody invalidBody;
        EXPECT_FALSE(replay->GetBody(replay->GetBodyCount(), invalidBody));
    }

    TEST(
        Box3DDiagnosticsTests,
        RecordingCapturesConfiguredLengthScale)
    {
        SystemConfiguration configuration;
        configuration.m_lengthUnitsPerMeter = 100.0f;
        System system(configuration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        ASSERT_TRUE(system.StartRecording(worldHandle, 0));
        configuration.m_lengthUnitsPerMeter = 50.0f;
        system.UpdateConfiguration(configuration);
        EXPECT_FLOAT_EQ(system.GetConfiguration().m_lengthUnitsPerMeter, 100.0f);

        AZStd::vector<AZ::u8> recording;
        ASSERT_TRUE(system.StopRecording(worldHandle, recording));
        AZStd::unique_ptr<IReplay> replay = system.CreateReplay(recording, 1);
        ASSERT_NE(replay, nullptr);
        EXPECT_FLOAT_EQ(replay->GetInfo().m_lengthScale, 100.0f);

        configuration.m_lengthUnitsPerMeter = 1.0f;
        system.UpdateConfiguration(configuration);
        EXPECT_NE(system.GetCompatibilityFingerprint().find("length-units=1"), AZStd::string_view::npos);
    }
} // namespace Box3D::Tests
