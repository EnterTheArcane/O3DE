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
#include <AzTest/Benchmark/BenchmarkEnvironment.h>

namespace Box3D::Benchmarks
{
    namespace
    {
        constexpr AZ::u32 ValidationFrameCount = 600;
        constexpr float MatchedGridSpacing = 1.1f;
        constexpr AZ::u32 WarmupFrameCount = 600;

        void AddCpuAffinityCounters(
            benchmark::State& state,
            const AZ::Test::ScopedBenchmarkCpuAffinity& cpuAffinity)
        {
            state.counters["AffinityConstrained"] = 0;
            if (cpuAffinity.IsConstrained())
            {
                state.counters["AffinityConstrained"] = 1;
            }
            state.counters["AffinityProcessors"] = cpuAffinity.GetProcessorCount();
        }

        class JobContextScope final
        {
        public:
            explicit JobContextScope(
                AZ::u32 workerCount)
                : m_cpuAffinity(AZ::Test::GetBenchmarkCpuAffinity())
                , m_jobManager(
                    [workerCount]
                    {
                        AZ::JobManagerDesc descriptor;
                        AZ::Test::GetBenchmarkCpuAffinity().ConfigureJobManagerThreads(
                            descriptor,
                            workerCount);
                        return descriptor;
                    }())
                , m_jobContext(m_jobManager)
            {
            }

            [[nodiscard]]
            AZ::JobContext& Get()
            {
                return m_jobContext;
            }

            void AddCounters(
                benchmark::State& state) const
            {
                AddCpuAffinityCounters(state, m_cpuAffinity);
            }

        private:
            const AZ::Test::ScopedBenchmarkCpuAffinity& m_cpuAffinity;
            AZ::JobManager m_jobManager;
            AZ::JobContext m_jobContext;
        };

        [[nodiscard]]
        BodyHandle CreateBody(
            System& system,
            WorldHandle worldHandle,
            BodyType bodyType,
            const AZ::Vector3& position,
            const float damping = 0.0f)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = bodyType;
            configuration.m_transform = AZ::Transform::CreateTranslation(position);
            configuration.m_angularDamping = damping;
            configuration.m_enableSleep = false;
            configuration.m_linearDamping = damping;
            return system.CreateBody(worldHandle, configuration);
        }

        [[nodiscard]]
        ShapeHandle CreateSphere(
            System& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float radius = 0.5f,
            MaterialHandle materialHandle = MaterialHandle::Invalid)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = SphereShapeConfiguration{radius};
            if (materialHandle)
            {
                configuration.m_properties.m_materials.push_back(materialHandle);
            }
            configuration.m_properties.m_enableContactEvents = false;
            configuration.m_properties.m_enableHitEvents = false;
            return system.CreateShape(worldHandle, bodyHandle, configuration);
        }

        [[nodiscard]]
        ShapeHandle CreateBox(
            System& system,
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const AZ::Vector3& halfExtents,
            MaterialHandle materialHandle = MaterialHandle::Invalid)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = BoxShapeConfiguration{halfExtents};
            if (materialHandle)
            {
                configuration.m_properties.m_materials.push_back(materialHandle);
            }
            configuration.m_properties.m_enableContactEvents = false;
            configuration.m_properties.m_enableHitEvents = false;
            return system.CreateShape(worldHandle, bodyHandle, configuration);
        }

        void AddStepCounters(
            benchmark::State& state,
            System& system,
            WorldHandle worldHandle)
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

    void StepFallingBoxes(
        benchmark::State& state)
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

        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_friction = 0.0f;
        materialConfiguration.m_restitution = 0.0f;
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        if (!materialHandle)
        {
            state.SkipWithError("Failed to create the benchmark material.");
            return;
        }

        const BodyHandle ground = CreateBody(system, worldHandle, BodyType::Static, AZ::Vector3(0.0f, 0.0f, -0.5f));
        if (!ground
            || !CreateBox(
                system,
                worldHandle,
                ground,
                AZ::Vector3(64.0f, 64.0f, 0.5f),
                materialHandle))
        {
            state.SkipWithError("Failed to create the benchmark ground.");
            return;
        }

        AZ::u32 rowWidth = 16;
        if (bodyCount > rowWidth * rowWidth)
        {
            rowWidth = 32;
        }
        AZStd::vector<BodyHandle> bodies;
        bodies.reserve(bodyCount);
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            const AZ::u32 layerIndex = bodyIndex / (rowWidth * rowWidth);
            const AZ::u32 rowIndex = (bodyIndex / rowWidth) % rowWidth;
            const AZ::u32 columnIndex = bodyIndex % rowWidth;
            const AZ::Vector3 position(
                MatchedGridSpacing
                    * (static_cast<float>(columnIndex) - 0.5f * static_cast<float>(rowWidth)),
                MatchedGridSpacing
                    * (static_cast<float>(rowIndex) - 0.5f * static_cast<float>(rowWidth)),
                0.6f + static_cast<float>(layerIndex) * MatchedGridSpacing);
            const BodyHandle bodyHandle = CreateBody(
                system,
                worldHandle,
                BodyType::Dynamic,
                position,
                1.0f);
            if (!bodyHandle
                || !CreateBox(
                    system,
                    worldHandle,
                    bodyHandle,
                    AZ::Vector3(0.5f),
                    materialHandle))
            {
                state.SkipWithError("Failed to create a benchmark body.");
                return;
            }
            bodies.push_back(bodyHandle);
        }

        for (AZ::u32 warmupTick = 0; warmupTick < WarmupFrameCount; ++warmupTick)
        {
            if (!system.StepWorld(worldHandle, worldConfiguration.m_fixedTimeStep))
            {
                state.SkipWithError("Failed to warm the benchmark world.");
                return;
            }
        }

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

        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(system.StepWorld(worldHandle, worldConfiguration.m_fixedTimeStep));
        }

        benchmark::DoNotOptimize(system.GetStateDigest(worldHandle));
        float minimumHeight = AZStd::numeric_limits<float>::max();
        float maximumDisplacement = 0.0f;
        bool qualityValid = statesValid;
        for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
        {
            BodyState bodyState;
            qualityValid = system.GetBodyState(worldHandle, bodies[bodyIndex], bodyState)
                && bodyState.m_transform.IsFinite()
                && positions[bodyIndex].IsFinite()
                && qualityValid;
            minimumHeight = AZStd::min(minimumHeight, bodyState.m_transform.GetTranslation().GetZ());
            maximumDisplacement = AZStd::max(maximumDisplacement, bodyState.m_transform.GetTranslation().GetDistance(positions[bodyIndex]));
        }
        qualityValid = qualityValid && minimumHeight >= 0.45f && maximumDisplacement <= 0.02f;
        AddStepCounters(state, system, worldHandle);
        jobContextScope.AddCounters(state);
        state.counters["AngularDamping"] = 1;
        state.counters["DynamicBodies"] = bodyCount;
        state.counters["Friction"] = 0;
        state.counters["Ccd"] = 0;
        state.counters["MaximumDisplacement"] = maximumDisplacement;
        state.counters["MinimumHeight"] = minimumHeight;
        state.counters["Layers"] = (bodyCount - 1) / (rowWidth * rowWidth) + 1;
        state.counters["LinearDamping"] = 1;
        state.counters["Notifications"] = 0;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Sleep"] = 0;
        state.counters["GridSpacingMillimeters"] = 1100;
        state.counters["RowWidth"] = rowWidth;
        state.counters["Restitution"] = 0;
        state.counters["ValidationFrames"] = ValidationFrameCount;
        state.counters["WarmupTicks"] = WarmupFrameCount;
        state.counters["WarmupCompleted"] = 1;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void CreateDestroyBodies(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::Test::ScopedBenchmarkCpuAffinity& cpuAffinity = AZ::Test::GetBenchmarkCpuAffinity();
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
        AddCpuAffinityCounters(state, cpuAffinity);
        state.counters["Notifications"] = 0;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void RaycastGrid(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        const AZ::Test::ScopedBenchmarkCpuAffinity& cpuAffinity = AZ::Test::GetBenchmarkCpuAffinity();
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = workerCount;
        systemConfiguration.m_staticBodyCapacity = obstacleCount;
        systemConfiguration.m_staticShapeCapacity = obstacleCount;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (!worldQueries)
        {
            state.SkipWithError("Failed to acquire the query benchmark world view.");
            return;
        }

        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
        {
            const AZ::Vector3 position(
                2.0f * static_cast<float>(obstacleIndex % rowWidth),
                2.0f * static_cast<float>(obstacleIndex / rowWidth),
                -0.5f);
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
        bool qualityValid = false;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                request.m_start = AZ::Vector3(
                    2.0f * static_cast<float>(obstacleIndex % rowWidth),
                    2.0f * static_cast<float>(obstacleIndex / rowWidth),
                    10.0f);
                qualityValid = worldQueries->RaycastClosest(request, hit);
                benchmark::DoNotOptimize(qualityValid);
            }
        }

        state.counters["Obstacles"] = obstacleCount;
        AddCpuAffinityCounters(state, cpuAffinity);
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void RaycastClosestBatchGrid(
        benchmark::State& state)
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
                2.0f * static_cast<float>(obstacleIndex % rowWidth),
                2.0f * static_cast<float>(obstacleIndex / rowWidth),
                -0.5f);
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
                2.0f * static_cast<float>(obstacleIndex % rowWidth),
                2.0f * static_cast<float>(obstacleIndex / rowWidth),
                10.0f);
            requests[rayIndex].m_direction = -AZ::Vector3::CreateAxisZ();
            requests[rayIndex].m_distance = 20.0f;
        }

        constexpr auto warmupDuration = AZStd::chrono::milliseconds(100);
        const auto warmupDeadline = AZStd::chrono::steady_clock::now() + warmupDuration;
        BufferResult batchResult;
        do
        {
            batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
            benchmark::DoNotOptimize(batchResult);
        } while (AZStd::chrono::steady_clock::now() < warmupDeadline);

        for ([[maybe_unused]] auto iteration : state)
        {
            batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
            benchmark::DoNotOptimize(batchResult);
        }

        bool qualityValid = batchResult.m_count == rayCount
            && batchResult.m_requiredCount == rayCount;
        for (const ClosestQueryResult& result : results)
        {
            qualityValid = result.m_found
                && result.m_hit.m_bodyHandle
                && result.m_hit.m_shapeHandle
                && qualityValid;
        }
        state.counters["Obstacles"] = obstacleCount;
        jobContextScope.AddCounters(state);
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["WarmupMs"] = static_cast<double>(warmupDuration.count());
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void OverlapGrid(
        benchmark::State& state)
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
                2.0f * static_cast<float>(obstacleIndex % rowWidth),
                2.0f * static_cast<float>(obstacleIndex / rowWidth),
                0.0f);
            const BodyHandle bodyHandle = CreateBody(system, worldHandle, BodyType::Static, position);
            if (!bodyHandle || !CreateSphere(system, worldHandle, bodyHandle))
            {
                state.SkipWithError("Failed to create an overlap benchmark obstacle.");
                return;
            }
        }

        AabbOverlapRequest request;
        request.m_aabb = AZ::Aabb::CreateCenterHalfExtents(
            AZ::Vector3(static_cast<float>(rowWidth), static_cast<float>(rowWidth), 0.0f),
            AZ::Vector3(8.0f, 8.0f, 1.0f));
        AZStd::array<QueryHit, 256> hits;
        for ([[maybe_unused]] auto iteration : state)
        {
            benchmark::DoNotOptimize(system.OverlapAabb(worldHandle, request, hits));
        }

        state.counters["Obstacles"] = obstacleCount;
        state.SetItemsProcessed(state.iterations());
    }

    template<typename Hit>
    void OverlapSphereGridImpl(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 queryCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        const AZ::Test::ScopedBenchmarkCpuAffinity& cpuAffinity = AZ::Test::GetBenchmarkCpuAffinity();
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
                2.0f * static_cast<float>(obstacleIndex % rowWidth),
                2.0f * static_cast<float>(obstacleIndex / rowWidth),
                0.0f);
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
        request.m_geometry = SphereShapeConfiguration{5.0f};
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
        AddCpuAffinityCounters(state, cpuAffinity);
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = 0;
        if (result.m_requiredHitCount == expectedHitCount && !result.HasOverflow())
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * queryCount);
    }

    void OverlapSphereGrid(
        benchmark::State& state)
    {
        OverlapSphereGridImpl<OverlapHit>(state);
    }

    void OverlapSphereGridQueryHit(
        benchmark::State& state)
    {
        OverlapSphereGridImpl<QueryHit>(state);
    }

    void RaycastValidationFloor(
        benchmark::State& state)
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

    void RaycastEmptyWorld(
        benchmark::State& state)
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

    void NativeRaycastGrid(
        benchmark::State& state)
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
                    2.0f * static_cast<float>(obstacleIndex % rowWidth),
                    2.0f * static_cast<float>(obstacleIndex / rowWidth),
                    -0.5f));
            const BodyId bodyId = CreateBody(worldId, bodyConfiguration);
            NativeShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_enableContactEvents = false;
            shapeConfiguration.m_enableHitEvents = false;
            if (!IsValid(bodyId)
                || !IsValid(CreateBoxShape(bodyId, shapeConfiguration, AZ::Vector3(0.5f), AZ::Transform::CreateIdentity())))
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
                    2.0f * static_cast<float>(obstacleIndex % rowWidth),
                    2.0f * static_cast<float>(obstacleIndex / rowWidth),
                    10.0f);
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

    void NativeOverlapSphereGrid(
        benchmark::State& state)
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
                    2.0f * static_cast<float>(obstacleIndex % rowWidth),
                    2.0f * static_cast<float>(obstacleIndex / rowWidth),
                    -0.5f));
            const BodyId bodyId = CreateBody(worldId, bodyConfiguration);
            NativeShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_enableContactEvents = false;
            shapeConfiguration.m_enableHitEvents = false;
            if (!IsValid(bodyId)
                || !IsValid(CreateBoxShape(bodyId, shapeConfiguration, AZ::Vector3(0.5f), AZ::Transform::CreateIdentity())))
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

        const AZStd::array points{AZ::Vector3::CreateZero()};
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
                worldId,
                AZ::Vector3(32.0f, 32.0f, 0.0f),
                points,
                5.0f,
                1,
                AZStd::numeric_limits<AZ::u64>::max(),
                countHit,
                &hitCount);
            benchmark::DoNotOptimize(hitCount);
        }

        constexpr size_t expectedHitCount = 25;
        state.counters["ActualHits"] = aznumeric_cast<double>(hitCount);
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["LeafVisits"] = treeStatistics.m_leafVisits;
        state.counters["NodeVisits"] = treeStatistics.m_nodeVisits;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = 0;
        if (hitCount == expectedHitCount)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = 1;
        state.SetItemsProcessed(state.iterations());
        DestroyWorld(worldId);
    }

    BENCHMARK(StepFallingBoxes)
        ->Name("Box3D/Step/FallingBoxes")
        ->Args({128, 1})
        ->Args({128, 4})
        ->Args({128, 8})
        ->Args({1024, 1})
        ->Args({1024, 4})
        ->Args({1024, 8})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(ValidationFrameCount);

    BENCHMARK(CreateDestroyBodies)
        ->Name("Box3D/Lifecycle/CreateDestroyBodies")
        ->Args({128, 1})
        ->Args({1024, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastGrid)->Name("Box3D/Query/RaycastGrid")->Args({1024, 128, 1})->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(RaycastClosestBatchGrid)
        ->Name("Box3D/Query/RaycastClosestBatchGrid")
        ->Args({1024, 128, 4})
        ->Args({1024, 1024, 4})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(OverlapGrid)->Name("Box3D/Query/OverlapGrid")->Arg(1024)->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(OverlapSphereGrid)->Name("Box3D/Query/OverlapSphereGrid")->Args({1024, 1, 1})->Unit(benchmark::kMicrosecond)->UseRealTime();

    BENCHMARK(OverlapSphereGridQueryHit)
        ->Name("Box3D/Diagnostic/OverlapSphereGridQueryHit")
        ->Args({1024, 1, 1})
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
        ->Args({1024, 128})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(NativeOverlapSphereGrid)
        ->Name("Box3D/Diagnostic/NativeOverlapSphereGrid")
        ->Arg(1024)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();
} // namespace Box3D::Benchmarks

#endif // HAVE_BENCHMARK
