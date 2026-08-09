/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Material.h>
#include <Box3D/Queries.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>
#include <AzTest/AzTest.h>

#include <cfenv>

namespace Box3D::Tests
{
    namespace
    {
        struct QueryFixture final
        {
            System m_system;
            WorldHandle m_world = m_system.GetDefaultWorldHandle();
            BodyHandle m_staticBody;
            BodyHandle m_dynamicBody;
            BodyHandle m_sensorBody;

            QueryFixture()
            {
                m_staticBody = CreateSphere(BodyType::Static, 2.0f, false);
                m_dynamicBody = CreateSphere(BodyType::Dynamic, 4.0f, false);
                m_sensorBody = CreateSphere(BodyType::Static, 6.0f, true);
            }

            BodyHandle CreateSphere(BodyType bodyType, float x, bool sensor)
            {
                RigidBodyConfiguration bodyConfiguration;
                bodyConfiguration.m_bodyType = bodyType;
                bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(x));
                const BodyHandle body = m_system.CreateBody(m_world, bodyConfiguration);
                ShapeConfiguration shapeConfiguration;
                shapeConfiguration.m_geometry = SphereShapeConfiguration{};
                shapeConfiguration.m_properties.m_isSensor = sensor;
                EXPECT_TRUE(m_system.CreateShape(m_world, body, shapeConfiguration).IsValid());
                return body;
            }
        };

        bool RejectBody(const QueryHit& hit, void* userData)
        {
            return hit.m_bodyHandle != *static_cast<const BodyHandle*>(userData);
        }

        bool ChangeWorkerRoundingModes(AZ::JobContext& jobContext, size_t workerCount, int newRoundingMode, int expectedRoundingMode = -1)
        {
            AZStd::atomic_size_t startedWorkerCount{ 0 };
            AZStd::atomic_bool releaseWorkers{ false };
            AZStd::atomic_bool succeeded{ true };
            AZ::JobCompletion completion(&jobContext);
            completion.Reset(true);
            for (size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                AZ::Job* job = AZ::CreateJobFunction(
                    [&startedWorkerCount, &releaseWorkers, &succeeded, newRoundingMode, expectedRoundingMode]
                    {
                        if ((expectedRoundingMode != -1 && std::fegetround() != expectedRoundingMode) ||
                            std::fesetround(newRoundingMode) != 0)
                        {
                            succeeded.store(false);
                        }
                        startedWorkerCount.fetch_add(1);
                        while (!releaseWorkers.load())
                        {
                            AZStd::this_thread::yield();
                        }
                    },
                    true,
                    &jobContext);
                job->SetDependent(&completion);
                job->Start();
            }

            while (startedWorkerCount.load() != workerCount)
            {
                AZStd::this_thread::yield();
            }
            releaseWorkers.store(true);
            completion.StartAndWaitForCompletion();
            return succeeded.load();
        }
    } // namespace

    TEST(Box3DQueryTests, RaycastClosestAndFiltersReturnExpectedBodies)
    {
        QueryFixture fixture;
        RaycastRequest request;
        request.m_distance = 10.0f;
        QueryHit hit;
        ASSERT_TRUE(fixture.m_system.RaycastClosest(fixture.m_world, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, fixture.m_staticBody);
        EXPECT_NEAR(hit.m_distance, 1.5f, 0.01f);

        request.m_filter.m_bodyTypes = QueryBodyTypes::Dynamic;
        ASSERT_TRUE(fixture.m_system.RaycastClosest(fixture.m_world, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, fixture.m_dynamicBody);

        request.m_filter.m_bodyTypes = QueryBodyTypes::All;
        request.m_filter.m_callback = RejectBody;
        request.m_filter.m_userData = &fixture.m_staticBody;
        ASSERT_TRUE(fixture.m_system.RaycastClosest(fixture.m_world, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, fixture.m_dynamicBody);
    }

    TEST(Box3DQueryTests, SensorInclusionIsExplicit)
    {
        QueryFixture fixture;
        RaycastRequest request;
        request.m_start = AZ::Vector3::CreateAxisX(5.0f);
        request.m_distance = 2.0f;
        QueryHit hit;
        EXPECT_FALSE(fixture.m_system.RaycastClosest(fixture.m_world, request, hit));
        request.m_filter.m_includeSensors = true;
        ASSERT_TRUE(fixture.m_system.RaycastClosest(fixture.m_world, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, fixture.m_sensorBody);
    }

    TEST(Box3DQueryTests, ClosestRaycastBatchUsesCallerStorageAndReportsOverflow)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 4;
        System system(systemConfiguration);
        ASSERT_TRUE(system.DestroyWorld(system.GetDefaultWorldHandle()));
        WorldConfiguration worldConfiguration;
        worldConfiguration.m_name = AZ_NAME_LITERAL("BatchQueryTest");
        worldConfiguration.m_jobContext = &jobContext;
        const WorldHandle worldHandle = system.CreateWorld(worldConfiguration);
        ASSERT_TRUE(worldHandle.IsValid());

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(2.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyHandle, shapeConfiguration).IsValid());

        constexpr size_t batchQueryCount = 512;
        AZStd::array<RaycastRequest, batchQueryCount> requests;
        for (size_t requestIndex = 0; requestIndex < requests.size(); ++requestIndex)
        {
            if (requestIndex % 2 == 0)
            {
                const float offset = (static_cast<float>(requestIndex % 8) - 3.5f) * 0.01f;
                requests[requestIndex].m_start = AZ::Vector3(0.0f, offset, -0.5f * offset);
                requests[requestIndex].m_direction = AZ::Vector3(1.0f, 0.03125f, -0.015625f);
            }
            else
            {
                requests[requestIndex].m_start = 10.0f * AZ::Vector3::CreateAxisY();
            }
            requests[requestIndex].m_distance = 4.0f;
        }

        AZStd::array<ClosestQueryResult, batchQueryCount / 2> truncatedResults;
        BufferResult batchResult = system.RaycastClosestBatch(worldHandle, requests, truncatedResults);
        EXPECT_EQ(batchResult.m_count, truncatedResults.size());
        EXPECT_EQ(batchResult.m_requiredCount, requests.size());
        EXPECT_TRUE(batchResult.HasOverflow());

        AZStd::array<ClosestQueryResult, batchQueryCount> results;
        batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
        EXPECT_EQ(batchResult.m_count, requests.size());
        EXPECT_FALSE(batchResult.HasOverflow());
        for (size_t requestIndex = 0; requestIndex < results.size(); ++requestIndex)
        {
            SCOPED_TRACE(requestIndex);
            EXPECT_EQ(results[requestIndex].m_found, requestIndex % 2 == 0);
            if (results[requestIndex].m_found)
            {
                EXPECT_EQ(results[requestIndex].m_hit.m_bodyHandle, bodyHandle);
            }
        }

        const AZStd::array<ClosestQueryResult, batchQueryCount> baselineResults = results;
        const int callerRoundingMode = std::fegetround();
        const size_t workerCount = jobManager.GetNumWorkerThreads();
        const bool poisonedWorkers = ChangeWorkerRoundingModes(jobContext, workerCount, FE_UPWARD);
        AZStd::array<ClosestQueryResult, batchQueryCount> poisonedResults;
        batchResult = system.RaycastClosestBatch(worldHandle, requests, poisonedResults);
        const bool restoredWorkers = ChangeWorkerRoundingModes(jobContext, workerCount, FE_TONEAREST, FE_UPWARD);

        EXPECT_TRUE(poisonedWorkers);
        EXPECT_TRUE(restoredWorkers);
        EXPECT_EQ(std::fegetround(), callerRoundingMode);
        EXPECT_EQ(batchResult.m_count, requests.size());
        for (size_t requestIndex = 0; requestIndex < poisonedResults.size(); ++requestIndex)
        {
            SCOPED_TRACE(requestIndex);
            const ClosestQueryResult& baseline = baselineResults[requestIndex];
            const ClosestQueryResult& poisoned = poisonedResults[requestIndex];
            EXPECT_EQ(poisoned.m_found, baseline.m_found);
            if (!baseline.m_found)
            {
                continue;
            }

            EXPECT_EQ(poisoned.m_hit.m_bodyHandle, baseline.m_hit.m_bodyHandle);
            EXPECT_EQ(poisoned.m_hit.m_shapeHandle, baseline.m_hit.m_shapeHandle);
            EXPECT_EQ(poisoned.m_hit.m_materialHandle, baseline.m_hit.m_materialHandle);
            EXPECT_EQ(poisoned.m_hit.m_position.GetX(), baseline.m_hit.m_position.GetX());
            EXPECT_EQ(poisoned.m_hit.m_position.GetY(), baseline.m_hit.m_position.GetY());
            EXPECT_EQ(poisoned.m_hit.m_position.GetZ(), baseline.m_hit.m_position.GetZ());
            EXPECT_EQ(poisoned.m_hit.m_normal.GetX(), baseline.m_hit.m_normal.GetX());
            EXPECT_EQ(poisoned.m_hit.m_normal.GetY(), baseline.m_hit.m_normal.GetY());
            EXPECT_EQ(poisoned.m_hit.m_normal.GetZ(), baseline.m_hit.m_normal.GetZ());
            EXPECT_EQ(poisoned.m_hit.m_distance, baseline.m_hit.m_distance);
            EXPECT_EQ(poisoned.m_hit.m_fraction, baseline.m_hit.m_fraction);
            EXPECT_EQ(poisoned.m_hit.m_faceIndex, baseline.m_hit.m_faceIndex);
            EXPECT_EQ(poisoned.m_hit.m_childIndex, baseline.m_hit.m_childIndex);
        }

        ASSERT_TRUE(system.StartRecording(worldHandle, 4096));
        batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
        EXPECT_EQ(batchResult.m_count, requests.size());
        AZStd::vector<AZ::u8> recording;
        ASSERT_TRUE(system.StopRecording(worldHandle, recording));
        EXPECT_TRUE(system.ValidateRecording(recording, 4));
    }

    TEST(Box3DQueryTests, ClosestRaycastBatchPreservesPerRequestFiltering)
    {
        QueryFixture fixture;
        AZStd::array<RaycastRequest, 3> requests;
        requests[0].m_distance = 10.0f;
        requests[1] = requests[0];
        requests[1].m_filter.m_bodyTypes = QueryBodyTypes::Dynamic;
        requests[2] = requests[0];
        requests[2].m_filter.m_callback = RejectBody;
        requests[2].m_filter.m_userData = &fixture.m_staticBody;

        AZStd::array<ClosestQueryResult, 3> results;
        const BufferResult batchResult = fixture.m_system.RaycastClosestBatch(fixture.m_world, requests, results);
        ASSERT_EQ(batchResult.m_count, requests.size());
        ASSERT_TRUE(results[0].m_found);
        EXPECT_EQ(results[0].m_hit.m_bodyHandle, fixture.m_staticBody);
        ASSERT_TRUE(results[1].m_found);
        EXPECT_EQ(results[1].m_hit.m_bodyHandle, fixture.m_dynamicBody);
        ASSERT_TRUE(results[2].m_found);
        EXPECT_EQ(results[2].m_hit.m_bodyHandle, fixture.m_dynamicBody);
    }

    TEST(Box3DQueryTests, RaycastClosestIgnoresInitialOverlapOnEveryFilteringPath)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        const BodyHandle containingBody = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration containingShape;
        containingShape.m_geometry = SphereShapeConfiguration{ 1.0f };
        ASSERT_TRUE(system.CreateShape(worldHandle, containingBody, containingShape).IsValid());

        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(3.0f));
        const BodyHandle targetBody = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration targetShape;
        targetShape.m_geometry = SphereShapeConfiguration{ 0.5f };
        ASSERT_TRUE(system.CreateShape(worldHandle, targetBody, targetShape).IsValid());

        RaycastRequest request;
        request.m_distance = 5.0f;
        QueryHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, targetBody);
        EXPECT_NEAR(hit.m_distance, 2.5f, 0.01f);

        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(-3.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle sensorBody = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration sensorShape;
        sensorShape.m_geometry = SphereShapeConfiguration{ 0.5f };
        sensorShape.m_properties.m_isSensor = true;
        ASSERT_TRUE(system.CreateShape(worldHandle, sensorBody, sensorShape).IsValid());

        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, targetBody);
        EXPECT_NEAR(hit.m_distance, 2.5f, 0.01f);
    }

    TEST(Box3DQueryTests, AxisAlignedBoxRaycastsPreserveClosestQuerySemantics)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(3.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle boxBody = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() };
        ASSERT_TRUE(system.CreateShape(worldHandle, boxBody, shapeConfiguration).IsValid());

        RaycastRequest request;
        request.m_distance = 10.0f;
        QueryHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, boxBody);
        EXPECT_NEAR(hit.m_distance, 2.0f, 0.001f);
        EXPECT_TRUE(hit.m_normal.IsClose(-AZ::Vector3::CreateAxisX(), 0.001f));

        bodyConfiguration.m_transform = AZ::Transform::CreateIdentity();
        const BodyHandle containingBody = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(system.CreateShape(worldHandle, containingBody, shapeConfiguration).IsValid());
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, boxBody);

        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(-3.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle sensorBody = system.CreateBody(worldHandle, bodyConfiguration);
        shapeConfiguration.m_properties.m_isSensor = true;
        ASSERT_TRUE(system.CreateShape(worldHandle, sensorBody, shapeConfiguration).IsValid());
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, boxBody);

        request.m_start = 2.0f * AZ::Vector3::CreateAxisY();
        EXPECT_FALSE(system.RaycastClosest(worldHandle, request, hit));

        BodyRaycastRequest bodyRequest;
        bodyRequest.m_distance = 10.0f;
        bodyRequest.m_start = 3.0f * AZ::Vector3::CreateAxisX();
        EXPECT_FALSE(system.RaycastBody(worldHandle, boxBody, bodyRequest, hit));

        const AZ::Quaternion rotation = AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi);
        ASSERT_TRUE(system.SetBodyTransform(
            worldHandle, boxBody, AZ::Transform::CreateFromQuaternionAndTranslation(rotation, 3.0f * AZ::Vector3::CreateAxisX())));
        bodyRequest.m_start = AZ::Vector3::CreateZero();
        ASSERT_TRUE(system.RaycastBody(worldHandle, boxBody, bodyRequest, hit));
        constexpr float SquareRootTwo = 1.41421356237f;
        EXPECT_NEAR(hit.m_distance, 3.0f - SquareRootTwo, 0.001f);
        EXPECT_NEAR(hit.m_normal.GetLength(), 1.0f, 0.001f);
        EXPECT_LT(hit.m_normal.Dot(AZ::Vector3::CreateAxisX()), 0.0f);
    }

    TEST(Box3DQueryTests, AxisAlignedClosestRaycastsMatchAllHitsAcrossDirectionsAndFilters)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        constexpr AZ::u64 TargetCategory = AZ::u64{ 1 } << 1;
        const AZStd::array directions{ AZ::Vector3::CreateAxisX(),  -AZ::Vector3::CreateAxisX(), AZ::Vector3::CreateAxisY(),
                                       -AZ::Vector3::CreateAxisY(), AZ::Vector3::CreateAxisZ(),  -AZ::Vector3::CreateAxisZ() };

        AZStd::array<BodyHandle, directions.size()> targetBodies;
        for (size_t directionIndex = 0; directionIndex < directions.size(); ++directionIndex)
        {
            const AZ::Vector3& direction = directions[directionIndex];
            RigidBodyConfiguration bodyConfiguration;
            bodyConfiguration.m_bodyType = BodyType::Static;

            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(2.0f * direction);
            const BodyHandle excludedBody = system.CreateBody(worldHandle, bodyConfiguration);
            ShapeConfiguration excludedShape;
            excludedShape.m_geometry = BoxShapeConfiguration{ 0.5f * AZ::Vector3::CreateOne() };
            ASSERT_TRUE(system.CreateShape(worldHandle, excludedBody, excludedShape).IsValid());

            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(4.0f * direction);
            targetBodies[directionIndex] = system.CreateBody(worldHandle, bodyConfiguration);
            ShapeConfiguration targetShape = excludedShape;
            targetShape.m_properties.m_collisionFilter.m_categoryBits = TargetCategory;
            ASSERT_TRUE(system.CreateShape(worldHandle, targetBodies[directionIndex], targetShape).IsValid());
        }

        for (size_t directionIndex = 0; directionIndex < directions.size(); ++directionIndex)
        {
            SCOPED_TRACE(directionIndex);
            RaycastRequest request;
            request.m_direction = directions[directionIndex];
            request.m_distance = 8.0f;
            request.m_filter.m_collisionFilter.m_categoryBits = TargetCategory;
            request.m_filter.m_collisionFilter.m_maskBits = TargetCategory;

            QueryHit closestHit;
            ASSERT_TRUE(system.RaycastClosest(worldHandle, request, closestHit));
            EXPECT_EQ(closestHit.m_bodyHandle, targetBodies[directionIndex]);
            EXPECT_NEAR(closestHit.m_distance, 3.5f, 0.001f);
            EXPECT_TRUE(closestHit.m_normal.IsClose(-directions[directionIndex], 0.001f));

            AZStd::array<QueryHit, 8> allHits;
            const QueryResult result = system.Raycast(worldHandle, request, allHits);
            ASSERT_GE(result.m_hitCount, 1);
            EXPECT_EQ(allHits.front().m_bodyHandle, closestHit.m_bodyHandle);
            EXPECT_EQ(allHits.front().m_shapeHandle, closestHit.m_shapeHandle);
            EXPECT_NEAR(allHits.front().m_fraction, closestHit.m_fraction, 0.0001f);
        }
    }

    TEST(Box3DQueryTests, ClosestRaycastMapsReusedNativeShapeIdsToCurrentHandles)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(2.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
        const ShapeHandle firstShape = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(firstShape.IsValid());

        RaycastRequest request;
        request.m_distance = 4.0f;
        QueryHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_shapeHandle, firstShape);

        ASSERT_TRUE(system.DestroyShape(worldHandle, firstShape));
        const ShapeHandle replacementShape = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(replacementShape.IsValid());
        EXPECT_NE(replacementShape, firstShape);
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(hit.m_shapeHandle, replacementShape);

        ASSERT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(3.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle replacementBody = system.CreateBody(worldHandle, bodyConfiguration);
        const ShapeHandle bodyReplacementShape = system.CreateShape(worldHandle, replacementBody, shapeConfiguration);
        ASSERT_TRUE(bodyReplacementShape.IsValid());
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, replacementBody);
        EXPECT_EQ(hit.m_shapeHandle, bodyReplacementShape);
    }

    TEST(Box3DQueryTests, EveryConvexQueryGeometryCanSweep)
    {
        QueryFixture fixture;
        AZStd::array<QueryGeometry, 5> geometries{ SphereShapeConfiguration{ 0.1f },
                                                   CapsuleShapeConfiguration{ 0.4f, 0.1f },
                                                   BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.1f },
                                                   CylinderShapeConfiguration{ 0.2f, 0.1f, 8 },
                                                   ConvexHullShapeConfiguration{ { AZ::Vector3(-0.1f, -0.1f, -0.1f),
                                                                                   AZ::Vector3(0.1f, -0.1f, -0.1f),
                                                                                   AZ::Vector3(0.0f, 0.1f, -0.1f),
                                                                                   AZ::Vector3(0.0f, 0.0f, 0.1f) } } };
        for (const QueryGeometry& geometry : geometries)
        {
            ShapeCastRequest request;
            request.m_geometry = geometry;
            request.m_translation = 10.0f * AZ::Vector3::CreateAxisX();
            AZStd::array<QueryHit, 4> hits;
            const QueryResult result = fixture.m_system.ShapeCast(fixture.m_world, request, hits);
            EXPECT_GT(result.m_hitCount, 0);
            EXPECT_EQ(hits.front().m_bodyHandle, fixture.m_staticBody);
        }
    }

    TEST(Box3DQueryTests, QueryScaleAppliesToGeometry)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(1.4f));
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyHandle, shapeConfiguration).IsValid());

        OverlapRequest request;
        request.m_geometry = SphereShapeConfiguration{ 0.5f };
        AZStd::array<QueryHit, 1> hits;
        EXPECT_EQ(system.Overlap(worldHandle, request, hits).m_hitCount, 0);
        request.m_scale = AZ::Vector3::CreateOne() * 2.0f;
        const QueryResult result = system.Overlap(worldHandle, request, hits);
        ASSERT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(hits.front().m_bodyHandle, bodyHandle);

        request.m_scale = AZ::Vector3(2.0f, 1.0f, 1.0f);
        EXPECT_EQ(system.Overlap(worldHandle, request, hits).m_hitCount, 0);

        request.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.5f };
        EXPECT_EQ(system.Overlap(worldHandle, request, hits).m_hitCount, 1);

        request.m_transform.SetUniformScale(2.0f);
        EXPECT_EQ(system.Overlap(worldHandle, request, hits).m_hitCount, 0);
    }

    TEST(Box3DQueryTests, OverlapAndAabbOverlapReportCapacityAndStableOrder)
    {
        QueryFixture fixture;
        OverlapRequest overlap;
        overlap.m_geometry = SphereShapeConfiguration{ 3.0f };
        overlap.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(3.0f));
        AZStd::array<QueryHit, 1> overlapHits;
        const QueryResult overlapResult = fixture.m_system.Overlap(fixture.m_world, overlap, overlapHits);
        EXPECT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapResult.m_requiredHitCount, 2);
        EXPECT_TRUE(overlapResult.HasOverflow());
        EXPECT_EQ(overlapHits.front().m_bodyHandle, fixture.m_staticBody);

        AabbOverlapRequest aabbOverlap;
        aabbOverlap.m_aabb = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3::CreateAxisX(3.0f), AZ::Vector3(2.0f, 1.0f, 1.0f));
        AZStd::array<QueryHit, 4> aabbHits;
        const QueryResult aabbResult = fixture.m_system.OverlapAabb(fixture.m_world, aabbOverlap, aabbHits);
        ASSERT_EQ(aabbResult.m_hitCount, 2);
        EXPECT_LT(aabbHits[0].m_bodyHandle, aabbHits[1].m_bodyHandle);

        AZStd::array<OverlapHit, 4> compactAabbHits;
        const QueryResult compactAabbResult = fixture.m_system.OverlapAabb(fixture.m_world, aabbOverlap, compactAabbHits);
        ASSERT_EQ(compactAabbResult.m_hitCount, aabbResult.m_hitCount);
        for (size_t hitIndex = 0; hitIndex < compactAabbResult.m_hitCount; ++hitIndex)
        {
            EXPECT_EQ(compactAabbHits[hitIndex].m_bodyHandle, aabbHits[hitIndex].m_bodyHandle);
            EXPECT_EQ(compactAabbHits[hitIndex].m_shapeHandle, aabbHits[hitIndex].m_shapeHandle);
        }
    }

    TEST(Box3DQueryTests, CompactOverlapMatchesGeneralResultsAndFilters)
    {
        QueryFixture fixture;
        OverlapRequest request;
        request.m_geometry = SphereShapeConfiguration{ 3.0f };
        request.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(3.0f));

        AZStd::array<OverlapHit, 4> compactHits;
        AZStd::array<QueryHit, 4> generalHits;
        const QueryResult compactResult = fixture.m_system.Overlap(fixture.m_world, request, compactHits);
        const QueryResult generalResult = fixture.m_system.Overlap(fixture.m_world, request, generalHits);
        ASSERT_EQ(compactResult.m_hitCount, generalResult.m_hitCount);
        ASSERT_EQ(compactResult.m_requiredHitCount, generalResult.m_requiredHitCount);
        for (size_t hitIndex = 0; hitIndex < compactResult.m_hitCount; ++hitIndex)
        {
            EXPECT_EQ(compactHits[hitIndex].m_bodyHandle, generalHits[hitIndex].m_bodyHandle);
            EXPECT_EQ(compactHits[hitIndex].m_shapeHandle, generalHits[hitIndex].m_shapeHandle);
        }

        BodyHandle excludedBody = fixture.m_staticBody;
        request.m_filter.m_callback = [](const QueryHit& hit, void* userData)
        {
            return hit.m_bodyHandle != *static_cast<const BodyHandle*>(userData);
        };
        request.m_filter.m_userData = &excludedBody;
        const QueryResult filteredResult = fixture.m_system.Overlap(fixture.m_world, request, compactHits);
        ASSERT_EQ(filteredResult.m_hitCount, 1);
        EXPECT_NE(compactHits.front().m_bodyHandle, excludedBody);

        AZStd::array<OverlapHit, 1> limitedHits;
        request.m_filter = {};
        const QueryResult limitedResult = fixture.m_system.Overlap(fixture.m_world, request, limitedHits);
        EXPECT_EQ(limitedResult.m_hitCount, 1);
        EXPECT_EQ(limitedResult.m_requiredHitCount, 2);
        EXPECT_TRUE(limitedResult.HasOverflow());
        EXPECT_EQ(limitedHits.front().m_bodyHandle, fixture.m_staticBody);
    }

    TEST(Box3DQueryTests, OverlapSortsLargeResultSetsWithoutChangingDefaultHitData)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        constexpr size_t BodyCount = 40;
        AZStd::array<BodyHandle, BodyCount> bodies;
        for (size_t bodyIndex = 0; bodyIndex < BodyCount; ++bodyIndex)
        {
            RigidBodyConfiguration bodyConfiguration;
            bodyConfiguration.m_bodyType = BodyType::Static;
            const float x = static_cast<float>(bodyIndex % 8) - 3.5f;
            const float y = static_cast<float>(bodyIndex / 8) - 2.0f;
            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3(x, y, 0.0f));
            bodies[bodyIndex] = system.CreateBody(worldHandle, bodyConfiguration);

            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3(0.25f) };
            ASSERT_TRUE(system.CreateShape(worldHandle, bodies[bodyIndex], shapeConfiguration).IsValid());
        }

        OverlapRequest request;
        request.m_geometry = SphereShapeConfiguration{ 10.0f };
        AZStd::array<QueryHit, BodyCount> hits;
        const QueryResult result = system.Overlap(worldHandle, request, hits);
        ASSERT_EQ(result.m_hitCount, BodyCount);
        EXPECT_EQ(result.m_requiredHitCount, BodyCount);
        EXPECT_FALSE(result.HasOverflow());
        for (size_t hitIndex = 0; hitIndex < BodyCount; ++hitIndex)
        {
            EXPECT_EQ(hits[hitIndex].m_bodyHandle, bodies[hitIndex]);
            EXPECT_FALSE(hits[hitIndex].m_materialHandle.IsValid());
            EXPECT_TRUE(hits[hitIndex].m_position.IsZero());
            EXPECT_TRUE(hits[hitIndex].m_normal.IsZero());
            EXPECT_EQ(hits[hitIndex].m_distance, 0.0f);
            EXPECT_EQ(hits[hitIndex].m_fraction, 0.0f);
            EXPECT_EQ(hits[hitIndex].m_faceIndex, -1);
            EXPECT_EQ(hits[hitIndex].m_childIndex, -1);
        }
    }

    TEST(Box3DQueryTests, SphereOverlapHandlesRotatedBoxFacesEdgesAndCorners)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const AZ::Quaternion rotation = AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi);

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        bodyConfiguration.m_transform = AZ::Transform::CreateFromQuaternion(rotation);
        const BodyHandle bodyRotatedBox = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() };
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyRotatedBox, shapeConfiguration).IsValid());

        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(10.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle shapeRotatedBox = system.CreateBody(worldHandle, bodyConfiguration);
        shapeConfiguration.m_properties.m_localTransform = AZ::Transform::CreateFromQuaternion(rotation);
        ASSERT_TRUE(system.CreateShape(worldHandle, shapeRotatedBox, shapeConfiguration).IsValid());

        OverlapRequest request;
        request.m_geometry = SphereShapeConfiguration{ 0.5f };
        AZStd::array<QueryHit, 2> hits;
        const auto expectOverlap = [&](const AZ::Vector3& center, BodyHandle expectedBody, bool expected)
        {
            request.m_transform = AZ::Transform::CreateTranslation(center);
            const QueryResult result = system.Overlap(worldHandle, request, hits);
            EXPECT_EQ(result.m_hitCount, expected ? 1 : 0);
            if (expected && result.m_hitCount == 1)
            {
                EXPECT_EQ(hits.front().m_bodyHandle, expectedBody);
            }
        };

        expectOverlap(rotation.TransformVector(AZ::Vector3(1.49f, 0.0f, 0.0f)), bodyRotatedBox, true);
        expectOverlap(rotation.TransformVector(AZ::Vector3(1.51f, 0.0f, 0.0f)), bodyRotatedBox, false);
        expectOverlap(rotation.TransformVector(AZ::Vector3(1.3f, 1.3f, 0.0f)), bodyRotatedBox, true);
        expectOverlap(rotation.TransformVector(AZ::Vector3(1.4f, 1.4f, 0.0f)), bodyRotatedBox, false);

        const AZ::Vector3 secondBoxPosition = 10.0f * AZ::Vector3::CreateAxisX();
        expectOverlap(secondBoxPosition + rotation.TransformVector(AZ::Vector3(1.3f, 1.3f, 0.0f)), shapeRotatedBox, true);
        expectOverlap(secondBoxPosition + rotation.TransformVector(AZ::Vector3(1.4f, 1.4f, 0.0f)), shapeRotatedBox, false);
    }

    TEST(Box3DQueryTests, BodyQueriesOnlyTraverseTheRequestedBody)
    {
        QueryFixture fixture;

        BodyRaycastRequest raycast;
        raycast.m_distance = 10.0f;
        QueryHit hit;
        ASSERT_TRUE(fixture.m_system.RaycastBody(fixture.m_world, fixture.m_dynamicBody, raycast, hit));
        EXPECT_EQ(hit.m_bodyHandle, fixture.m_dynamicBody);
        EXPECT_NEAR(hit.m_distance, 3.5f, 0.01f);

        BodyShapeCastRequest shapeCast;
        shapeCast.m_geometry = SphereShapeConfiguration{ 0.25f };
        shapeCast.m_translation = 10.0f * AZ::Vector3::CreateAxisX();
        ASSERT_TRUE(fixture.m_system.ShapeCastBody(fixture.m_world, fixture.m_dynamicBody, shapeCast, hit));
        EXPECT_EQ(hit.m_bodyHandle, fixture.m_dynamicBody);
        EXPECT_NEAR(hit.m_distance, 3.25f, 0.01f);

        BodyOverlapRequest overlap;
        overlap.m_geometry = SphereShapeConfiguration{ 0.6f };
        overlap.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(4.0f));
        EXPECT_TRUE(fixture.m_system.OverlapBody(fixture.m_world, fixture.m_dynamicBody, overlap));
        overlap.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(8.0f));
        EXPECT_FALSE(fixture.m_system.OverlapBody(fixture.m_world, fixture.m_dynamicBody, overlap));
    }

    TEST(Box3DQueryTests, CompoundMeshFacesPreserveProviderMaterialIndices)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        AZStd::array<MaterialHandle, 3> materials;
        for (size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex)
        {
            MaterialConfiguration configuration;
            configuration.m_surfaceTypeId = materialIndex + 1;
            materials[materialIndex] = system.CreateMaterial(configuration);
            ASSERT_TRUE(materials[materialIndex].IsValid());
        }

        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle.IsValid());

        TriangleMeshShapeConfiguration mesh;
        mesh.m_vertices = {
            AZ::Vector3(-3.0f, -1.0f, 0.0f), AZ::Vector3(-1.0f, -1.0f, 0.0f), AZ::Vector3(-2.0f, 1.0f, 0.0f),
            AZ::Vector3(1.0f, -1.0f, 0.0f),  AZ::Vector3(3.0f, -1.0f, 0.0f),  AZ::Vector3(2.0f, 1.0f, 0.0f),
        };
        mesh.m_indices = { 0, 1, 2, 3, 4, 5 };
        mesh.m_materialIndices = { 2, 1 };

        CompoundShapeConfiguration compound;
        compound.m_children.push_back({ AZStd::move(mesh), AZ::Transform::CreateIdentity(), AZ::Vector3::CreateOne(), 0 });
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = AZStd::move(compound);
        shapeConfiguration.m_properties.m_materials.assign(materials.begin(), materials.end());
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle.IsValid());

        const auto raycastAt = [&](float x, QueryHit& hit)
        {
            RaycastRequest request;
            request.m_start = AZ::Vector3(x, 0.0f, 2.0f);
            request.m_direction = -AZ::Vector3::CreateAxisZ();
            request.m_distance = 4.0f;
            return system.RaycastClosest(worldHandle, request, hit);
        };

        QueryHit hit;
        ASSERT_TRUE(raycastAt(-2.0f, hit));
        EXPECT_EQ(hit.m_materialHandle, materials[2]);
        ASSERT_TRUE(raycastAt(2.0f, hit));
        EXPECT_EQ(hit.m_materialHandle, materials[1]);
        ASSERT_TRUE(system.RaycastShape(worldHandle, shapeHandle, AZ::Vector3(-2.0f, 0.0f, 2.0f), -AZ::Vector3::CreateAxisZ(), 4.0f, hit));
        EXPECT_EQ(hit.m_materialHandle, materials[2]);

        const MaterialHandle firstMaterial = materials[0];
        materials[0] = materials[2];
        materials[2] = firstMaterial;
        ASSERT_TRUE(system.SetShapeMaterials(worldHandle, shapeHandle, materials));
        ASSERT_TRUE(raycastAt(-2.0f, hit));
        EXPECT_EQ(hit.m_materialHandle, materials[2]);
        ASSERT_TRUE(system.RaycastShape(worldHandle, shapeHandle, AZ::Vector3(-2.0f, 0.0f, 2.0f), -AZ::Vector3::CreateAxisZ(), 4.0f, hit));
        EXPECT_EQ(hit.m_materialHandle, materials[2]);
    }

    TEST(Box3DQueryTests, InvalidRequestsReturnEmptyResults)
    {
        QueryFixture fixture;
        RaycastRequest raycast;
        raycast.m_direction = AZ::Vector3::CreateZero();
        AZStd::array<QueryHit, 1> hits;
        EXPECT_EQ(fixture.m_system.Raycast(fixture.m_world, raycast, hits).m_hitCount, 0);

        BodyRaycastRequest bodyRaycast;
        bodyRaycast.m_direction = AZ::Vector3::CreateZero();
        QueryHit hit;
        EXPECT_FALSE(fixture.m_system.RaycastBody(fixture.m_world, fixture.m_dynamicBody, bodyRaycast, hit));

        ShapeCastRequest shapeCast;
        shapeCast.m_translation = AZ::Vector3::CreateZero();
        EXPECT_EQ(fixture.m_system.ShapeCast(fixture.m_world, shapeCast, hits).m_hitCount, 0);

        AabbOverlapRequest overlap;
        EXPECT_EQ(fixture.m_system.OverlapAabb(fixture.m_world, overlap, hits).m_hitCount, 0);
    }
} // namespace Box3D::Tests
