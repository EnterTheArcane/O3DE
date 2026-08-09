/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#ifdef HAVE_BENCHMARK

#include <Box3D/Diagnostics.h>
#include <Box3D/NativeApi.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzTest/AzTest.h>

namespace Box3D::Benchmarks
{
    namespace
    {
        class JobContextScope final
        {
        public:
            explicit JobContextScope(AZ::u32 workerCount)
                : m_jobManager(
                      [workerCount]
                      {
                          AZ::JobManagerDesc descriptor;
                          descriptor.m_workerThreads.resize(workerCount);
                          return descriptor;
                      }())
                , m_jobContext(m_jobManager)
            {
            }

            [[nodiscard]] AZ::JobContext& Get()
            {
                return m_jobContext;
            }

        private:
            AZ::JobManager m_jobManager;
            AZ::JobContext m_jobContext;
        };

        [[nodiscard]] BodyHandle CreateBody(System& system, WorldHandle worldHandle, BodyType bodyType, const AZ::Vector3& position)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = bodyType;
            configuration.m_transform = AZ::Transform::CreateTranslation(position);
            configuration.m_enableSleep = false;
            return system.CreateBody(worldHandle, configuration);
        }

        [[nodiscard]] ShapeHandle CreateSphere(System& system, WorldHandle worldHandle, BodyHandle bodyHandle, float radius = 0.5f)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = SphereShapeConfiguration{ radius };
            configuration.m_properties.m_enableContactEvents = false;
            configuration.m_properties.m_enableHitEvents = false;
            return system.CreateShape(worldHandle, bodyHandle, configuration);
        }

        [[nodiscard]] ShapeHandle CreateBox(System& system, WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& halfExtents)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = BoxShapeConfiguration{ halfExtents };
            configuration.m_properties.m_enableContactEvents = false;
            configuration.m_properties.m_enableHitEvents = false;
            return system.CreateShape(worldHandle, bodyHandle, configuration);
        }

        void AddStepCounters(benchmark::State& state, System& system, WorldHandle worldHandle)
        {
            WorldStatistics statistics;
            if (!system.GetWorldStatistics(worldHandle, StatisticsFlags::All, statistics))
            {
                return;
            }

            state.counters["AwakeBodies"] = statistics.m_counters.m_awakeBodyCount;
            state.counters["Bodies"] = statistics.m_counters.m_bodyCount;
            state.counters["Contacts"] = statistics.m_counters.m_contactCount;
            state.counters["NativeBytes"] = aznumeric_cast<double>(statistics.m_counters.m_globalAllocatedBytes);
            state.counters["StepMs"] = statistics.m_lastStep.m_total.count();
            state.counters["PairsMs"] = statistics.m_lastStep.m_pairGeneration.count();
            state.counters["CollisionMs"] = statistics.m_lastStep.m_collision.count();
            state.counters["SolveMs"] = statistics.m_lastStep.m_solver.count();
            state.counters["Tasks"] = statistics.m_counters.m_taskCount;
            state.counters["Workers"] = statistics.m_counters.m_workerCount;
        }
    } // namespace

    void StepFallingSpheres(benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        JobContextScope jobContextScope(workerCount);

        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = workerCount;
        systemConfiguration.m_dynamicBodyCapacity = bodyCount;
        systemConfiguration.m_dynamicShapeCapacity = bodyCount;
        systemConfiguration.m_staticBodyCapacity = 1;
        systemConfiguration.m_staticShapeCapacity = 1;
        systemConfiguration.m_contactCapacity = bodyCount * 8;
        systemConfiguration.m_enableSleep = false;
        systemConfiguration.m_enableContinuous = false;
        systemConfiguration.m_enableSpeculative = false;
        System system(systemConfiguration);

        EXPECT_TRUE(system.DestroyWorld(system.GetDefaultWorldHandle()));
        WorldConfiguration worldConfiguration;
        worldConfiguration.m_name = AZ_NAME_LITERAL("StepBenchmark");
        worldConfiguration.m_jobContext = &jobContextScope.Get();
        worldConfiguration.m_collectedEventTypes = StepEventTypes::None;
        const WorldHandle worldHandle = system.CreateWorld(worldConfiguration);
        if (!worldHandle)
        {
            state.SkipWithError("Failed to create the benchmark world.");
            return;
        }

        const BodyHandle ground = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3(0.0f, 0.0f, -0.5f));
        if (!ground || !CreateBox(system, worldHandle, ground, AZ::Vector3(64.0f, 64.0f, 0.5f)))
        {
            state.SkipWithError("Failed to create the benchmark ground.");
            return;
        }

        constexpr AZ::u32 rowWidth = 16;
        AZStd::vector<BodyHandle> bodies;
        bodies.reserve(bodyCount);
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            const AZ::u32 layerIndex = bodyIndex / (rowWidth * rowWidth);
            const AZ::u32 rowIndex = (bodyIndex / rowWidth) % rowWidth;
            const AZ::u32 columnIndex = bodyIndex % rowWidth;
            const AZ::Vector3 position(
                static_cast<float>(columnIndex) - 0.5f * static_cast<float>(rowWidth),
                static_cast<float>(rowIndex) - 0.5f * static_cast<float>(rowWidth),
                0.6f + static_cast<float>(layerIndex) * 1.1f);
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Dynamic, position);
            if (!bodyHandle || !CreateSphere(system, worldHandle, bodyHandle))
            {
                state.SkipWithError("Failed to create a benchmark body.");
                return;
            }
            bodies.push_back(bodyHandle);
        }

        constexpr AZ::u32 minimumWarmupTickCount = 30;
        constexpr AZ::u32 maximumWarmupTickCount = 600;
        constexpr AZ::u32 requiredStableTickCount = 5;
        constexpr float maximumStableDisplacement = 0.02f;
        AZStd::vector<AZ::Vector3> previousPositions(bodyCount);
        AZ::u32 stableTickCount = 0;
        AZ::u32 warmupTickCount = 0;
        for (; warmupTickCount < maximumWarmupTickCount && stableTickCount < requiredStableTickCount; ++warmupTickCount)
        {
            bool statesValid = true;
            for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
            {
                BodyState bodyState;
                statesValid = system.GetBodyState(worldHandle, bodies[bodyIndex], bodyState) && statesValid;
                previousPositions[bodyIndex] = bodyState.m_transform.GetTranslation();
            }
            if (!statesValid || !system.StepWorld(worldHandle, worldConfiguration.m_fixedTimeStep))
            {
                state.SkipWithError("Failed to warm the benchmark world.");
                return;
            }

            float maximumDisplacement = 0.0f;
            for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
            {
                BodyState bodyState;
                statesValid = system.GetBodyState(worldHandle, bodies[bodyIndex], bodyState) && statesValid;
                maximumDisplacement =
                    AZStd::max(maximumDisplacement, bodyState.m_transform.GetTranslation().GetDistance(previousPositions[bodyIndex]));
            }
            stableTickCount =
                warmupTickCount + 1 >= minimumWarmupTickCount && statesValid && maximumDisplacement <= maximumStableDisplacement
                ? stableTickCount + 1
                : 0;
        }
        if (stableTickCount < requiredStableTickCount)
        {
            state.SkipWithError("The benchmark world did not reach the required steady state.");
            return;
        }

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(system.StepWorld(worldHandle, worldConfiguration.m_fixedTimeStep));
        }

        benchmark::DoNotOptimize(system.GetStateDigest(worldHandle));
        AZStd::vector<AZ::Vector3> positions;
        positions.reserve(bodies.size());
        bool statesValid = true;
        for (BodyHandle bodyHandle : bodies)
        {
            BodyState bodyState;
            if (!system.GetBodyState(worldHandle, bodyHandle, bodyState))
            {
                positions.push_back(AZ::Vector3::CreateZero());
                statesValid = false;
                continue;
            }
            positions.push_back(bodyState.m_transform.GetTranslation());
        }
        const bool validationStepCompleted = system.StepWorld(worldHandle, worldConfiguration.m_fixedTimeStep);
        float minimumHeight = AZStd::numeric_limits<float>::max();
        float maximumDisplacement = 0.0f;
        bool qualityValid = validationStepCompleted && statesValid;
        for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
        {
            BodyState bodyState;
            qualityValid = system.GetBodyState(worldHandle, bodies[bodyIndex], bodyState) && bodyState.m_transform.IsFinite() &&
                positions[bodyIndex].IsFinite() && qualityValid;
            minimumHeight = AZStd::min(minimumHeight, bodyState.m_transform.GetTranslation().GetZ());
            maximumDisplacement = AZStd::max(maximumDisplacement, bodyState.m_transform.GetTranslation().GetDistance(positions[bodyIndex]));
        }
        qualityValid = qualityValid && minimumHeight >= 0.45f && maximumDisplacement <= 0.02f;
        AddStepCounters(state, system, worldHandle);
        state.counters["DynamicBodies"] = bodyCount;
        state.counters["Ccd"] = 0;
        state.counters["MaximumDisplacement"] = maximumDisplacement;
        state.counters["MinimumHeight"] = minimumHeight;
        state.counters["Notifications"] = 0;
        state.counters["QualityValid"] = qualityValid ? 1 : 0;
        state.counters["Sleep"] = 0;
        state.counters["WarmupTicks"] = warmupTickCount;
        state.counters["WarmupStable"] = 1;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void CreateDestroyBodies(benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = workerCount;
        systemConfiguration.m_dynamicBodyCapacity = bodyCount;
        systemConfiguration.m_dynamicShapeCapacity = bodyCount;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        AZStd::vector<BodyHandle> bodies;
        bodies.reserve(bodyCount);

        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
            {
                const BodyHandle bodyHandle =
                    CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(bodyIndex)));
                benchmark::DoNotOptimize(CreateSphere(system, worldHandle, bodyHandle));
                bodies.push_back(bodyHandle);
            }
            for (BodyHandle bodyHandle : bodies)
            {
                benchmark::DoNotOptimize(system.DestroyBody(worldHandle, bodyHandle));
            }
            bodies.clear();
        }

        state.counters["DynamicBodies"] = bodyCount;
        state.counters["Notifications"] = 0;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void RaycastGrid(benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = workerCount;
        systemConfiguration.m_staticBodyCapacity = obstacleCount;
        systemConfiguration.m_staticShapeCapacity = obstacleCount;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (worldQueries == nullptr)
        {
            state.SkipWithError("Failed to acquire the query benchmark world view.");
            return;
        }

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            const AZ::Vector3 position(
                2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), -0.5f);
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, position);
            if (!bodyHandle || !CreateBox(system, worldHandle, bodyHandle, AZ::Vector3(0.5f)))
            {
                state.SkipWithError("Failed to create a query benchmark obstacle.");
                return;
            }
        }
        if (!system.RebuildStaticTree(worldHandle))
        {
            state.SkipWithError("Failed to optimize the query benchmark static tree.");
            return;
        }

        RaycastRequest request;
        request.m_direction = -AZ::Vector3::CreateAxisZ();
        request.m_distance = 20.0f;
        QueryHit hit;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                request.m_start = AZ::Vector3(
                    2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 10.0f);
                benchmark::DoNotOptimize(worldQueries->RaycastClosest(request, hit));
            }
        }

        state.counters["Obstacles"] = obstacleCount;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void RaycastClosestBatchGrid(benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContextScope(workerCount);

        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = workerCount;
        systemConfiguration.m_staticBodyCapacity = obstacleCount;
        systemConfiguration.m_staticShapeCapacity = obstacleCount;
        System system(systemConfiguration);
        EXPECT_TRUE(system.DestroyWorld(system.GetDefaultWorldHandle()));

        WorldConfiguration worldConfiguration;
        worldConfiguration.m_name = AZ_NAME_LITERAL("BatchRaycastBenchmark");
        worldConfiguration.m_jobContext = &jobContextScope.Get();
        const WorldHandle worldHandle = system.CreateWorld(worldConfiguration);
        if (!worldHandle)
        {
            state.SkipWithError("Failed to create the batch query benchmark world.");
            return;
        }

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            const AZ::Vector3 position(
                2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), -0.5f);
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, position);
            if (!bodyHandle || !CreateBox(system, worldHandle, bodyHandle, AZ::Vector3(0.5f)))
            {
                state.SkipWithError("Failed to create a batch query benchmark obstacle.");
                return;
            }
        }
        if (!system.RebuildStaticTree(worldHandle))
        {
            state.SkipWithError("Failed to optimize the batch query benchmark static tree.");
            return;
        }

        AZStd::vector<RaycastRequest> requests(rayCount);
        AZStd::vector<ClosestQueryResult> results(rayCount);
        for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
        {
            const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
            requests[rayIndex].m_start = AZ::Vector3(
                2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 10.0f);
            requests[rayIndex].m_direction = -AZ::Vector3::CreateAxisZ();
            requests[rayIndex].m_distance = 20.0f;
        }

        constexpr auto warmupDuration = AZStd::chrono::milliseconds(100);
        const auto warmupDeadline = AZStd::chrono::steady_clock::now() + warmupDuration;
        do
        {
            benchmark::DoNotOptimize(system.RaycastClosestBatch(worldHandle, requests, results));
        } while (AZStd::chrono::steady_clock::now() < warmupDeadline);

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(system.RaycastClosestBatch(worldHandle, requests, results));
        }

        state.counters["Obstacles"] = obstacleCount;
        state.counters["WarmupMs"] = static_cast<double>(warmupDuration.count());
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void OverlapGrid(benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        systemConfiguration.m_staticBodyCapacity = obstacleCount;
        systemConfiguration.m_staticShapeCapacity = obstacleCount;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            const AZ::Vector3 position(
                2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 0.0f);
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, position);
            if (!bodyHandle || !CreateSphere(system, worldHandle, bodyHandle))
            {
                state.SkipWithError("Failed to create an overlap benchmark obstacle.");
                return;
            }
        }

        AabbOverlapRequest request;
        request.m_aabb = AZ::Aabb::CreateCenterHalfExtents(
            AZ::Vector3(static_cast<float>(rowWidth), static_cast<float>(rowWidth), 0.0f), AZ::Vector3(8.0f, 8.0f, 1.0f));
        AZStd::array<QueryHit, 256> hits;
        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(system.OverlapAabb(worldHandle, request, hits));
        }

        state.counters["Obstacles"] = obstacleCount;
        state.SetItemsProcessed(state.iterations());
    }

    template<class Hit>
    void OverlapSphereGridImpl(benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 queryCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = workerCount;
        systemConfiguration.m_staticBodyCapacity = obstacleCount;
        systemConfiguration.m_staticShapeCapacity = obstacleCount;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            const AZ::Vector3 position(
                2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 0.0f);
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, position);
            if (!bodyHandle || !CreateBox(system, worldHandle, bodyHandle, AZ::Vector3(0.5f)))
            {
                state.SkipWithError("Failed to create a matched overlap benchmark obstacle.");
                return;
            }
        }
        if (!system.RebuildStaticTree(worldHandle))
        {
            state.SkipWithError("Failed to optimize the matched overlap benchmark static tree.");
            return;
        }

        OverlapRequest request;
        request.m_geometry = SphereShapeConfiguration{ 5.0f };
        request.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3(32.0f, 32.0f, 0.0f));
        AZStd::array<Hit, 256> hits;
        QueryResult result;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 queryIndex = 0; queryIndex < queryCount; ++queryIndex)
            {
                result = system.Overlap(worldHandle, request, hits);
                benchmark::DoNotOptimize(result);
            }
        }

        constexpr size_t expectedHitCount = 25;
        state.counters["ActualHits"] = aznumeric_cast<double>(result.m_requiredHitCount);
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = result.m_requiredHitCount == expectedHitCount && !result.HasOverflow() ? 1 : 0;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * queryCount);
    }

    void OverlapSphereGrid(benchmark::State& state)
    {
        OverlapSphereGridImpl<OverlapHit>(state);
    }

    void OverlapSphereGridQueryHit(benchmark::State& state)
    {
        OverlapSphereGridImpl<QueryHit>(state);
    }

    void RaycastValidationFloor(benchmark::State& state)
    {
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(0));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RaycastRequest request;
        request.m_distance = 0.0f;
        QueryHit hit;

        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                benchmark::DoNotOptimize(system.RaycastClosest(worldHandle, request, hit));
            }
        }

        state.counters["Workers"] = 1;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void RaycastEmptyWorld(benchmark::State& state)
    {
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(0));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RaycastRequest request;
        request.m_distance = 20.0f;
        QueryHit hit;

        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                benchmark::DoNotOptimize(system.RaycastClosest(worldHandle, request, hit));
            }
        }

        state.counters["Workers"] = 1;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void NativeRaycastGrid(benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
        NativeWorldConfiguration worldConfiguration;
        worldConfiguration.m_staticBodyCapacity = obstacleCount;
        worldConfiguration.m_staticShapeCapacity = obstacleCount;
        const WorldId worldId = CreateWorld(worldConfiguration);
        if (!IsValid(worldId))
        {
            state.SkipWithError("Failed to create the native diagnostic world.");
            return;
        }

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(
                AZ::Vector3(
                    2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), -0.5f));
            const BodyId bodyId = CreateBody(worldId, bodyConfiguration);
            NativeShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_enableContactEvents = false;
            shapeConfiguration.m_enableHitEvents = false;
            if (!IsValid(bodyId) ||
                !IsValid(CreateBoxShape(bodyId, shapeConfiguration, AZ::Vector3(0.5f), AZ::Transform::CreateIdentity())))
            {
                state.SkipWithError("Failed to create a native query benchmark obstacle.");
                DestroyWorld(worldId);
                return;
            }
        }
        if (!RebuildStaticTree(worldId))
        {
            state.SkipWithError("Failed to optimize the native query benchmark static tree.");
            DestroyWorld(worldId);
            return;
        }

        ClosestCastHit hit;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                const AZ::Vector3 start(
                    2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 10.0f);
                benchmark::DoNotOptimize(
                    CastRayClosest(worldId, start, -20.0f * AZ::Vector3::CreateAxisZ(), 1, AZStd::numeric_limits<AZ::u64>::max(), hit));
            }
        }

        state.counters["Obstacles"] = obstacleCount;
        state.counters["Workers"] = 1;
        state.counters["NodeVisits"] = hit.m_nodeVisits;
        state.counters["LeafVisits"] = hit.m_leafVisits;
        state.SetItemsProcessed(state.iterations() * rayCount);
        DestroyWorld(worldId);
    }

    void NativeOverlapSphereGrid(benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        NativeWorldConfiguration worldConfiguration;
        worldConfiguration.m_staticBodyCapacity = obstacleCount;
        worldConfiguration.m_staticShapeCapacity = obstacleCount;
        const WorldId worldId = CreateWorld(worldConfiguration);
        if (!IsValid(worldId))
        {
            state.SkipWithError("Failed to create the native overlap diagnostic world.");
            return;
        }

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(
                AZ::Vector3(
                    2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), -0.5f));
            const BodyId bodyId = CreateBody(worldId, bodyConfiguration);
            NativeShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_enableContactEvents = false;
            shapeConfiguration.m_enableHitEvents = false;
            if (!IsValid(bodyId) ||
                !IsValid(CreateBoxShape(bodyId, shapeConfiguration, AZ::Vector3(0.5f), AZ::Transform::CreateIdentity())))
            {
                state.SkipWithError("Failed to create a native overlap diagnostic obstacle.");
                DestroyWorld(worldId);
                return;
            }
        }
        if (!RebuildStaticTree(worldId))
        {
            state.SkipWithError("Failed to optimize the native overlap diagnostic tree.");
            DestroyWorld(worldId);
            return;
        }

        const AZStd::array points{ AZ::Vector3::CreateZero() };
        size_t hitCount = 0;
        const auto countHit = []([[maybe_unused]] const OverlapCandidate& candidate, void* context)
        {
            ++*static_cast<size_t*>(context);
            return true;
        };
        TreeVisitStatistics treeStatistics;
        for ([[maybe_unused]] auto iteration : state)
        {
            hitCount = 0;
            treeStatistics = OverlapShape(
                worldId, AZ::Vector3(32.0f, 32.0f, 0.0f), points, 5.0f, 1, AZStd::numeric_limits<AZ::u64>::max(), countHit, &hitCount);
            benchmark::DoNotOptimize(hitCount);
        }

        constexpr size_t expectedHitCount = 25;
        state.counters["ActualHits"] = aznumeric_cast<double>(hitCount);
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["LeafVisits"] = treeStatistics.m_leafVisits;
        state.counters["NodeVisits"] = treeStatistics.m_nodeVisits;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = hitCount == expectedHitCount ? 1 : 0;
        state.counters["Workers"] = 1;
        state.SetItemsProcessed(state.iterations());
        DestroyWorld(worldId);
    }

    BENCHMARK(StepFallingSpheres)
        ->Name("Box3D/Step/FallingSpheres")
        ->Args({ 128, 1 })
        ->Args({ 128, 4 })
        ->Args({ 128, 8 })
        ->Args({ 1024, 1 })
        ->Args({ 1024, 4 })
        ->Args({ 1024, 8 })
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(CreateDestroyBodies)
        ->Name("Box3D/Lifecycle/CreateDestroyBodies")
        ->Args({ 128, 1 })
        ->Args({ 1024, 1 })
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastGrid)->Name("Box3D/Query/RaycastGrid")->Args({ 1024, 128, 1 })->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(RaycastClosestBatchGrid)
        ->Name("Box3D/Query/RaycastClosestBatchGrid")
        ->Args({ 1024, 128, 4 })
        ->Args({ 1024, 1024, 4 })
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(OverlapGrid)->Name("Box3D/Query/OverlapGrid")->Arg(1024)->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(OverlapSphereGrid)->Name("Box3D/Query/OverlapSphereGrid")->Args({ 1024, 1, 1 })->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(OverlapSphereGridQueryHit)
        ->Name("Box3D/Diagnostic/OverlapSphereGridQueryHit")
        ->Args({ 1024, 1, 1 })
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastValidationFloor)
        ->Name("Box3D/Diagnostic/RaycastValidationFloor")
        ->Arg(128)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastEmptyWorld)->Name("Box3D/Diagnostic/RaycastEmptyWorld")->Arg(128)->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(NativeRaycastGrid)
        ->Name("Box3D/Diagnostic/NativeRaycastGrid")
        ->Args({ 1024, 128 })
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(NativeOverlapSphereGrid)
        ->Name("Box3D/Diagnostic/NativeOverlapSphereGrid")
        ->Arg(1024)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();
} // namespace Box3D::Benchmarks

#endif // HAVE_BENCHMARK
