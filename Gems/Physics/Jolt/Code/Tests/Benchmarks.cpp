/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#ifdef HAVE_BENCHMARK

#include <Jolt/FloatEnvironment.h>
#include <Jolt/SystemInternal.h>

#include <ProviderModuleApi.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/string.h>
#include <AzTest/AzTest.h>
#include <AzTest/Benchmark/BenchmarkEnvironment.h>

namespace Jolt::Benchmarks
{
    namespace
    {
        constexpr float MatchedTimeStep = 1.0f / 60.0f;
        constexpr float MatchedGridSpacing = 1.1f;
        constexpr AZ::u32 StatefulBenchmarkFrameCount = 120;
        constexpr size_t TailSampleCount = 4096;
        constexpr AZ::u32 VehicleBenchmarkFrameCount = 30;
        constexpr AZ::u32 WarmupFrameCount = 600;
        constexpr AZ::u32 ValidationFrameCount = 600;

        class NameDictionaryScope final
        {
        public:
            NameDictionaryScope()
            {
                if (!AZ::Interface<AZ::NameDictionary>::Get())
                {
                    AZ::NameDictionary::Create();
                    m_ownsDictionary = true;
                }
            }

            ~NameDictionaryScope()
            {
                if (m_ownsDictionary)
                {
                    AZ::NameDictionary::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(NameDictionaryScope);

        private:
            bool m_ownsDictionary = false;
        };

        void EnsureNameDictionary()
        {
            static NameDictionaryScope nameDictionary;
        }

        class JobContextScope final
        {
        public:
            explicit JobContextScope(
                const AZ::u32 workerCount)
                : m_cpuAffinity(AZ::Test::GetBenchmarkCpuAffinity())
                , m_jobManager(
                    [workerCount]
                    {
                        AZ::u32 backgroundWorkerCount = 0;
                        if (workerCount > 1)
                        {
                            backgroundWorkerCount = workerCount - 1;
                        }
                        AZ::JobManagerDesc descriptor;
                        AZ::Test::GetBenchmarkCpuAffinity().ConfigureJobManagerThreads(
                            descriptor,
                            backgroundWorkerCount);
                        return descriptor;
                    }())
                , m_jobContext(m_jobManager)
            {
                if (workerCount > 1)
                {
                    m_backgroundWorkerCount = workerCount - 1;
                }
            }

            [[nodiscard]]
            AZ::JobContext& Get()
            {
                return m_jobContext;
            }

            void AddCounters(
                benchmark::State& state) const
            {
                state.counters["AffinityConstrained"] = 0;
                if (m_cpuAffinity.IsConstrained())
                {
                    state.counters["AffinityConstrained"] = 1;
                }
                state.counters["AffinityProcessors"] = m_cpuAffinity.GetProcessorCount();
                state.counters["BackgroundWorkers"] = m_backgroundWorkerCount;
                state.counters["CallerParticipates"] = 1;
            }

        private:
            const AZ::Test::ScopedBenchmarkCpuAffinity& m_cpuAffinity;
            AZ::u32 m_backgroundWorkerCount = 0;
            AZ::JobManager m_jobManager;
            AZ::JobContext m_jobContext;
        };

        [[nodiscard]]
        SystemConfiguration CreateSystemConfiguration(
            const AZ::u32 workerCount,
            const AZ::u32 maximumBodyCount,
            const bool useMatchedSolverPolicy = false)
        {
            EnsureNameDictionary();

            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_autoSimulate = false;
            configuration.m_defaultWorld.m_collectActivationEvents = false;
            configuration.m_defaultWorld.m_collectContactEvents = false;
            configuration.m_defaultWorld.m_workerCount = workerCount;
            configuration.m_defaultWorld.m_capacity.m_maxBodies = maximumBodyCount;
            configuration.m_defaultWorld.m_capacity.m_maxBodyPairs = maximumBodyCount * 16;
            configuration.m_defaultWorld.m_capacity.m_maxContactConstraints = maximumBodyCount * 16;
            configuration.m_defaultWorld.m_simulation.m_allowSleeping = false;
            if (useMatchedSolverPolicy)
            {
                configuration.m_defaultWorld.m_simulation.m_penetrationSlop = 0.001f;
                configuration.m_defaultWorld.m_simulation.m_positionStepCount = 4;
                configuration.m_defaultWorld.m_simulation.m_velocityStepCount = 2;
                if (workerCount == 1)
                {
                    configuration.m_defaultWorld.m_simulation.m_useLargeIslandSplitter = false;
                }
            }
            return configuration;
        }

        [[nodiscard]]
        AZ::Vector3 ToVector3(
            const WorldPosition& position)
        {
            return AZ::Vector3(
                aznumeric_cast<float>(position.m_x),
                aznumeric_cast<float>(position.m_y),
                aznumeric_cast<float>(position.m_z));
        }

        [[nodiscard]]
        ShapeHandle CreateBox(
            Runtime& system,
            const WorldHandle worldHandle,
            const AZ::Vector3& dimensions)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = BoxShapeConfiguration{.m_dimensions = dimensions};
            return system.CreateShape(worldHandle, configuration);
        }

        [[nodiscard]]
        ShapeHandle CreateSphere(
            Runtime& system,
            const WorldHandle worldHandle,
            const float radius)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = SphereShapeConfiguration{.m_radius = radius};
            return system.CreateShape(worldHandle, configuration);
        }

        struct HairBenchmarkScenario final
        {
            HairDefinitionHandle m_definitionHandle;
            HairHandle m_hairHandle;
            HairDefinitionState m_definitionState;
        };

        [[nodiscard]]
        bool CreateHairBenchmarkScenario(
            Runtime& system,
            const WorldHandle worldHandle,
            const AZ::u32 strandCount,
            const AZ::u32 verticesPerStrand,
            const AZ::u32 gridSize,
            HairBenchmarkScenario& scenario)
        {
            HairDefinitionConfiguration definitionConfiguration;
            definitionConfiguration.m_vertices.reserve(
                static_cast<size_t>(strandCount) * verticesPerStrand);
            definitionConfiguration.m_strands.reserve(strandCount);
            constexpr AZ::u32 strandRowWidth = 32;
            constexpr float strandSpacing = 0.01f;
            constexpr float vertexSpacing = 0.01f;
            for (AZ::u32 strandIndex = 0; strandIndex < strandCount; ++strandIndex)
            {
                const AZ::u32 beginVertex = aznumeric_cast<AZ::u32>(definitionConfiguration.m_vertices.size());
                const float rootX = strandSpacing * aznumeric_cast<float>(strandIndex % strandRowWidth);
                const float rootY = strandSpacing * aznumeric_cast<float>(strandIndex / strandRowWidth);
                for (AZ::u32 vertexIndex = 0; vertexIndex < verticesPerStrand; ++vertexIndex)
                {
                    definitionConfiguration.m_vertices.push_back({
                        .m_position = AZ::Vector3(
                            rootX,
                            rootY,
                            1.0f - vertexSpacing * aznumeric_cast<float>(vertexIndex)),
                        .m_inverseMass = 1.0f,
                    });
                }
                definitionConfiguration.m_vertices[beginVertex].m_inverseMass = 0.0f;
                definitionConfiguration.m_strands.push_back({
                    .m_beginVertex = beginVertex,
                    .m_endVertex = aznumeric_cast<AZ::u32>(definitionConfiguration.m_vertices.size()),
                    .m_materialIndex = 0,
                });
            }
            definitionConfiguration.m_materials.resize(1);
            definitionConfiguration.m_materials[0].m_simulationStrandFraction = 1.0f;
            definitionConfiguration.m_gridSizeX = gridSize;
            definitionConfiguration.m_gridSizeY = gridSize;
            definitionConfiguration.m_gridSizeZ = gridSize;

            scenario.m_definitionHandle = system.CreateHairDefinition(definitionConfiguration);
            if (!scenario.m_definitionHandle
                || !system.GetHairDefinitionState(scenario.m_definitionHandle, scenario.m_definitionState))
            {
                return false;
            }

            HairConfiguration hairConfiguration;
            hairConfiguration.m_definitionHandle = scenario.m_definitionHandle;
            hairConfiguration.m_objectLayer = DefaultLayers::Moving;
            scenario.m_hairHandle = system.CreateHair(worldHandle, hairConfiguration);
            return static_cast<bool>(scenario.m_hairHandle);
        }

        [[nodiscard]]
        BodyHandle CreateBody(
            Runtime& system,
            const WorldHandle worldHandle,
            const ShapeHandle shapeHandle,
            const MotionType motionType,
            const AZ::Vector3& position,
            const float damping = 0.0f,
            const bool useMatchedMaterial = false)
        {
            BodyConfiguration configuration;
            configuration.m_shapeHandle = shapeHandle;
            configuration.m_transform.m_position = {
                .m_x = position.GetX(),
                .m_y = position.GetY(),
                .m_z = position.GetZ(),
            };
            configuration.m_motionType = motionType;
            configuration.m_allowSleeping = false;
            configuration.m_angularDamping = damping;
            configuration.m_linearDamping = damping;
            if (useMatchedMaterial)
            {
                configuration.m_friction = 0.0f;
                configuration.m_restitution = 0.0f;
            }
            if (motionType == MotionType::Static)
            {
                configuration.m_activate = false;
                configuration.m_objectLayer = DefaultLayers::NonMoving;
            }
            return system.CreateBody(worldHandle, configuration);
        }

        void AddWorldCounters(
            benchmark::State& state,
            Runtime& system,
            const WorldHandle worldHandle)
        {
            WorldStatistics statistics;
            if (!system.GetWorldStatistics(worldHandle, statistics))
            {
                return;
            }

            state.counters["ActiveDynamicBodies"] = statistics.m_activeDynamicBodyCount;
            state.counters["Bodies"] = statistics.m_bodyCount;
            state.counters["Constraints"] = statistics.m_constraintCount;
            state.counters["Jobs"] = statistics.m_lastUpdateJobCount;
            state.counters["MaximumTasks"] = statistics.m_lastUpdateMaximumTaskCount;
            state.counters["ShapeBytes"] = aznumeric_cast<double>(statistics.m_shapeBytes);
            state.counters["Tasks"] = statistics.m_lastUpdateTaskCount;
            state.counters["TempAllocatorBytes"] = aznumeric_cast<double>(statistics.m_tempAllocatorUsageBytes);
            state.counters["UpdateNs"] = aznumeric_cast<double>(statistics.m_lastUpdateNanoseconds);
        }

        void AddPerformanceCounters(
            benchmark::State& state,
            Runtime& system,
            const WorldHandle worldHandle)
        {
            WorldPerformanceStatistics statistics;
            if (!system.GetPerformanceStatistics(worldHandle, statistics, false))
            {
                return;
            }

            state.counters["NativeAllocationCount"] = aznumeric_cast<double>(
                statistics.m_processNativeAllocationCount);
            state.counters["NativeAllocatedBytes"] = aznumeric_cast<double>(
                statistics.m_processNativeAllocatedBytes);
            state.counters["NativeFreeCount"] = aznumeric_cast<double>(statistics.m_processNativeFreeCount);
            state.counters["NativePeakAllocatedBytes"] = aznumeric_cast<double>(
                statistics.m_processNativePeakAllocatedBytes);
            state.counters["NativeReallocationCount"] = aznumeric_cast<double>(
                statistics.m_processNativeReallocationCount);
            state.counters["SnapshotBytes"] = aznumeric_cast<double>(statistics.m_snapshotBytes);
            state.counters["SnapshotFailures"] = aznumeric_cast<double>(statistics.m_snapshotFailureCount);
            state.counters["SnapshotPeakBytes"] = aznumeric_cast<double>(statistics.m_snapshotPeakBytes);
            state.counters["TempAllocatorCurrentBytes"] = aznumeric_cast<double>(
                statistics.m_tempAllocatorCurrentBytes);
            state.counters["TempAllocatorPeakBytes"] = aznumeric_cast<double>(
                statistics.m_tempAllocatorPeakBytes);
            state.counters["WrapperRetainedBytes"] = aznumeric_cast<double>(
                statistics.m_wrapperRetainedBytes);
        }

        [[nodiscard]]
        bool CreateQueryGrid(
            Runtime& system,
            const WorldHandle worldHandle,
            const AZ::u32 obstacleCount)
        {
            const ShapeHandle obstacleShape = CreateBox(
                system,
                worldHandle,
                AZ::Vector3::CreateOne());
            if (!obstacleShape)
            {
                return false;
            }

            constexpr AZ::u32 rowWidth = 32;
            for (AZ::u32 obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
            {
                const AZ::Vector3 position(
                    2.0f * aznumeric_cast<float>(obstacleIndex % rowWidth),
                    2.0f * aznumeric_cast<float>(obstacleIndex / rowWidth),
                    -0.5f);
                if (!CreateBody(
                    system,
                    worldHandle,
                    obstacleShape,
                    MotionType::Static,
                    position))
                {
                    return false;
                }
            }

            return system.OptimizeBroadPhase(worldHandle);
        }

        [[nodiscard]]
        bool CreateBodiesForDestruction(
            Runtime& system,
            const WorldHandle worldHandle,
            const ShapeHandle shapeHandle,
            const AZ::u32 bodyCount,
            AZStd::vector<BodyHandle>& bodyHandles)
        {
            bodyHandles.clear();
            BodyConfiguration configuration;
            configuration.m_shapeHandle = shapeHandle;
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
            {
                configuration.m_transform.m_position.m_z = bodyIndex;
                const BodyHandle bodyHandle = system.CreateBody(worldHandle, configuration);
                if (!bodyHandle)
                {
                    return false;
                }
                bodyHandles.push_back(bodyHandle);
            }
            return true;
        }

        [[nodiscard]]
        bool CreateConstraintsForMembership(
            Runtime& system,
            const WorldHandle worldHandle,
            const ShapeHandle shapeHandle,
            const AZ::u32 constraintCount,
            AZStd::vector<ConstraintHandle>& constraintHandles)
        {
            const BodyHandle firstBodyHandle = CreateBody(
                system,
                worldHandle,
                shapeHandle,
                MotionType::Dynamic,
                AZ::Vector3::CreateZero());
            if (!firstBodyHandle)
            {
                return false;
            }

            constraintHandles.clear();
            constraintHandles.reserve(constraintCount);
            ConstraintConfiguration configuration;
            configuration.m_firstBodyHandle = firstBodyHandle;
            configuration.m_geometry = PointConstraintConfiguration{};
            configuration.m_startInSimulation = false;
            for (AZ::u32 constraintIndex = 0; constraintIndex < constraintCount; ++constraintIndex)
            {
                configuration.m_secondBodyHandle = CreateBody(
                    system,
                    worldHandle,
                    shapeHandle,
                    MotionType::Dynamic,
                    AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(constraintIndex + 1)));
                if (!configuration.m_secondBodyHandle)
                {
                    return false;
                }

                const ConstraintHandle constraintHandle =
                    system.CreateConstraint(worldHandle, configuration);
                if (!constraintHandle)
                {
                    return false;
                }
                constraintHandles.push_back(constraintHandle);
            }
            return true;
        }

        bool CreateBodiesForConstraintDestruction(
            Runtime& system,
            const WorldHandle worldHandle,
            const ShapeHandle shapeHandle,
            const AZ::u32 constraintCount,
            AZStd::vector<BodyHandle>& bodyHandles)
        {
            bodyHandles.clear();
            bodyHandles.reserve(constraintCount + 1);
            for (AZ::u32 bodyIndex = 0; bodyIndex <= constraintCount; ++bodyIndex)
            {
                const BodyHandle bodyHandle = CreateBody(
                    system,
                    worldHandle,
                    shapeHandle,
                    MotionType::Dynamic,
                    AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(bodyIndex)));
                if (!bodyHandle)
                {
                    return false;
                }
                bodyHandles.push_back(bodyHandle);
            }
            return true;
        }

        bool CreateConstraintsForDestruction(
            Runtime& system,
            const WorldHandle worldHandle,
            const AZStd::span<const BodyHandle> bodyHandles,
            AZStd::vector<ConstraintHandle>& constraintHandles)
        {
            if (bodyHandles.size() < 2)
            {
                return false;
            }

            constraintHandles.clear();
            constraintHandles.reserve(bodyHandles.size() - 1);
            ConstraintConfiguration configuration;
            configuration.m_firstBodyHandle = bodyHandles.front();
            configuration.m_geometry = PointConstraintConfiguration{};
            for (const BodyHandle bodyHandle : bodyHandles.subspan(1))
            {
                configuration.m_secondBodyHandle = bodyHandle;
                const ConstraintHandle constraintHandle =
                    system.CreateConstraint(worldHandle, configuration);
                if (!constraintHandle)
                {
                    return false;
                }
                constraintHandles.push_back(constraintHandle);
            }
            return true;
        }

        [[nodiscard]]
        SoftBodyDefinitionConfiguration CreateSoftBodyBenchmarkDefinition(
            const AZ::u32 gridSize)
        {
            SoftBodyDefinitionConfiguration configuration;
            const size_t vertexCount = static_cast<size_t>(gridSize) * gridSize;
            configuration.m_vertices.reserve(vertexCount);
            configuration.m_faces.reserve(static_cast<size_t>(gridSize - 1) * (gridSize - 1) * 2);
            configuration.m_edgeConstraints.reserve(static_cast<size_t>(gridSize - 1) * gridSize * 2);
            for (AZ::u32 row = 0; row < gridSize; ++row)
            {
                for (AZ::u32 column = 0; column < gridSize; ++column)
                {
                    configuration.m_vertices.push_back({
                        .m_position = AZ::Vector3(
                            aznumeric_cast<float>(column) * 0.1f,
                            aznumeric_cast<float>(row) * 0.1f,
                            0.0f),
                    });
                    const AZ::u32 vertexIndex = row * gridSize + column;
                    if (column > 0)
                    {
                        configuration.m_edgeConstraints.push_back({
                            .m_firstVertex = vertexIndex - 1,
                            .m_secondVertex = vertexIndex,
                        });
                    }
                    if (row > 0)
                    {
                        configuration.m_edgeConstraints.push_back({
                            .m_firstVertex = vertexIndex - gridSize,
                            .m_secondVertex = vertexIndex,
                        });
                    }
                    if (row == 0 || column == 0)
                    {
                        continue;
                    }

                    const AZ::u32 lowerRight = vertexIndex;
                    const AZ::u32 lowerLeft = vertexIndex - 1;
                    const AZ::u32 upperRight = vertexIndex - gridSize;
                    const AZ::u32 upperLeft = upperRight - 1;
                    configuration.m_faces.push_back({
                        .m_firstVertex = upperLeft,
                        .m_secondVertex = lowerLeft,
                        .m_thirdVertex = lowerRight,
                    });
                    configuration.m_faces.push_back({
                        .m_firstVertex = upperLeft,
                        .m_secondVertex = lowerRight,
                        .m_thirdVertex = upperRight,
                    });
                }
            }
            return configuration;
        }

        struct RagdollBenchmarkScenario final
        {
            SkeletonDefinitionHandle m_skeletonHandle;
            ShapeHandle m_shapeHandle;
            RagdollDefinitionHandle m_definitionHandle;
            RagdollConfiguration m_configuration;
            RagdollHandle m_ragdollHandle;
        };

        bool CreateRagdollBenchmarkScenario(
            Runtime& system,
            const WorldHandle worldHandle,
            const AZ::u32 partCount,
            RagdollBenchmarkScenario& scenario)
        {
            SkeletonDefinitionConfiguration skeletonConfiguration;
            skeletonConfiguration.m_joints.reserve(partCount);
            for (AZ::u32 partIndex = 0; partIndex < partCount; ++partIndex)
            {
                AZ::s32 parentIndex = -1;
                if (partIndex > 0)
                {
                    parentIndex = aznumeric_cast<AZ::s32>(partIndex - 1);
                }
                skeletonConfiguration.m_joints.push_back({
                    .m_name = AZ::Name(AZStd::string::format("part_%u", partIndex)),
                    .m_parentIndex = parentIndex,
                });
            }
            scenario.m_skeletonHandle = system.CreateSkeletonDefinition(skeletonConfiguration);

            scenario.m_shapeHandle = CreateSphere(system, worldHandle, 0.25f);
            if (!scenario.m_skeletonHandle || !scenario.m_shapeHandle)
            {
                return false;
            }

            RagdollDefinitionConfiguration definitionConfiguration;
            definitionConfiguration.m_skeletonHandle = scenario.m_skeletonHandle;
            definitionConfiguration.m_parts.resize(partCount);
            for (AZ::u32 partIndex = 0; partIndex < partCount; ++partIndex)
            {
                RagdollPartConfiguration& part = definitionConfiguration.m_parts[partIndex];
                part.m_body.m_shapeHandle = scenario.m_shapeHandle;
                part.m_body.m_transform.m_position.m_z = aznumeric_cast<double>(partIndex) * 0.5;
                if (partIndex > 0)
                {
                    HingeConstraintConfiguration hinge;
                    hinge.m_firstPoint.m_z = 0.25;
                    hinge.m_secondPoint.m_z = -0.25;
                    part.m_toParent = hinge;
                }
            }
            scenario.m_definitionHandle = system.CreateRagdollDefinition(
                worldHandle,
                definitionConfiguration);
            if (!scenario.m_definitionHandle)
            {
                return false;
            }

            scenario.m_configuration.m_definitionHandle = scenario.m_definitionHandle;
            scenario.m_configuration.m_name = AZ::Name::FromStringLiteral("benchmark_ragdoll", nullptr);
            scenario.m_ragdollHandle = system.CreateRagdoll(worldHandle, scenario.m_configuration);
            if (!scenario.m_ragdollHandle)
            {
                return false;
            }

            return true;
        }

        void DestroyRagdollBenchmarkScenario(
            Runtime& system,
            const WorldHandle worldHandle,
            RagdollBenchmarkScenario& scenario)
        {
            if (scenario.m_ragdollHandle)
            {
                [[maybe_unused]] const bool destroyed =
                    system.DestroyRagdoll(worldHandle, scenario.m_ragdollHandle);
            }
            if (scenario.m_definitionHandle)
            {
                [[maybe_unused]] const bool destroyed =
                    system.DestroyRagdollDefinition(worldHandle, scenario.m_definitionHandle);
            }
            if (scenario.m_shapeHandle)
            {
                [[maybe_unused]] const bool destroyed =
                    system.DestroyShape(worldHandle, scenario.m_shapeHandle);
            }
            if (scenario.m_skeletonHandle)
            {
                [[maybe_unused]] const bool destroyed =
                    system.DestroySkeletonDefinition(scenario.m_skeletonHandle);
            }
            scenario = {};
        }
    } // namespace

    template<bool CaptureTailSamples>
    void StepSettledBoxesImpl(
        benchmark::State& state,
        const bool useMatchedSolverPolicy)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(
                workerCount,
                bodyCount + 1,
                useMatchedSolverPolicy),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        const ShapeHandle floorShape = CreateBox(
            system,
            worldHandle,
            AZ::Vector3(128.0f, 128.0f, 1.0f));
        const BodyHandle floorBody = CreateBody(
            system,
            worldHandle,
            floorShape,
            MotionType::Static,
            AZ::Vector3(0.0f, 0.0f, -0.5f));
        const ShapeHandle bodyShape = CreateBox(
            system,
            worldHandle,
            AZ::Vector3::CreateOne());
        if (!system || !worldHandle || !floorShape || !floorBody || !bodyShape)
        {
            state.SkipWithError("Failed to create the Jolt box benchmark world.");
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
                    * (aznumeric_cast<float>(columnIndex) - 0.5f * aznumeric_cast<float>(rowWidth)),
                MatchedGridSpacing
                    * (aznumeric_cast<float>(rowIndex) - 0.5f * aznumeric_cast<float>(rowWidth)),
                0.6f + aznumeric_cast<float>(layerIndex) * MatchedGridSpacing);
            const BodyHandle bodyHandle = CreateBody(
                system,
                worldHandle,
                bodyShape,
                MotionType::Dynamic,
                position,
                1.0f,
                true);
            if (!bodyHandle)
            {
                state.SkipWithError("Failed to create a Jolt benchmark body.");
                return;
            }
            bodies.push_back(bodyHandle);
        }

        constexpr float maximumAllowedDisplacement = 0.02f;
        for (AZ::u32 warmupTick = 0; warmupTick < WarmupFrameCount; ++warmupTick)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Failed to warm the Jolt benchmark world.");
                return;
            }
        }

        AZStd::vector<AZ::Vector3> positions;
        positions.reserve(bodies.size());
        bool qualityValid = true;
        for (const BodyHandle bodyHandle : bodies)
        {
            BodyState bodyState;
            qualityValid = system.GetBodyState(worldHandle, bodyHandle, bodyState)
                && qualityValid;
            positions.push_back(ToVector3(bodyState.m_transform.m_position));
        }

        bool allStepsSucceeded = true;
        AZ::u64 validationFrameCount = ValidationFrameCount;
        if constexpr (CaptureTailSamples)
        {
            AZStd::array<double, TailSampleCount> tailSamples;
            for ([[maybe_unused]] auto iteration : state)
            {
                for (double& sample : tailSamples)
                {
                    const auto start = AZStd::chrono::steady_clock::now();
                    const bool stepSucceeded = system.StepWorld(worldHandle, MatchedTimeStep);
                    const auto end = AZStd::chrono::steady_clock::now();
                    sample = AZStd::chrono::duration<double, AZStd::nano>(end - start).count();
                    allStepsSucceeded = stepSucceeded && allStepsSucceeded;
                }

                for (size_t sampleIndex = 0; sampleIndex < tailSamples.size(); ++sampleIndex)
                {
                    const AZStd::string sampleName = AZStd::string::format(
                        "Frame%uNs",
                        aznumeric_cast<unsigned int>(sampleIndex));
                    state.counters[sampleName.c_str()] = tailSamples[sampleIndex];
                }

                AZStd::sort(tailSamples.begin(), tailSamples.end());
                constexpr size_t p50Index = (TailSampleCount * 50 + 99) / 100 - 1;
                constexpr size_t p95Index = (TailSampleCount * 95 + 99) / 100 - 1;
                constexpr size_t p99Index = (TailSampleCount * 99 + 99) / 100 - 1;
                state.SetIterationTime(tailSamples[p95Index] * 1.0e-9);
                state.counters["TailMaximumNs"] = tailSamples.back();
                state.counters["TailP50Ns"] = tailSamples[p50Index];
                state.counters["TailP95Ns"] = tailSamples[p95Index];
                state.counters["TailP99Ns"] = tailSamples[p99Index];
                state.counters["TailSampleCount"] = TailSampleCount;
            }
            validationFrameCount = TailSampleCount;
        }
        else
        {
            for ([[maybe_unused]] auto iteration : state)
            {
                const bool stepSucceeded = system.StepWorld(worldHandle, MatchedTimeStep);
                allStepsSucceeded = stepSucceeded && allStepsSucceeded;
                benchmark::DoNotOptimize(stepSucceeded);
            }
        }

        float maximumDisplacement = 0.0f;
        float maximumHorizontalDisplacement = 0.0f;
        float maximumVerticalDisplacement = 0.0f;
        float minimumHeight = AZStd::numeric_limits<float>::max();
        for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
        {
            BodyState bodyState;
            qualityValid = system.GetBodyState(worldHandle, bodies[bodyIndex], bodyState)
                && qualityValid;
            const AZ::Vector3 position = ToVector3(bodyState.m_transform.m_position);
            qualityValid = position.IsFinite() && qualityValid;
            const AZ::Vector3 displacement = position - positions[bodyIndex];
            minimumHeight = AZStd::min(minimumHeight, position.GetZ());
            maximumDisplacement = AZStd::max(
                maximumDisplacement,
                displacement.GetLength());
            maximumHorizontalDisplacement = AZStd::max(
                maximumHorizontalDisplacement,
                AZ::Vector2(displacement.GetX(), displacement.GetY()).GetLength());
            maximumVerticalDisplacement = AZStd::max(
                maximumVerticalDisplacement,
                std::abs(displacement.GetZ()));
        }
        qualityValid = minimumHeight >= 0.45f
            && maximumDisplacement <= maximumAllowedDisplacement
            && allStepsSucceeded
            && qualityValid;

        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["Ccd"] = 0;
        state.counters["DynamicBodies"] = bodyCount;
        state.counters["Friction"] = 0;
        state.counters["AngularDamping"] = 1;
        state.counters["LargeIslandSplitter"] = 1;
        state.counters["Layers"] = (bodyCount - 1) / (rowWidth * rowWidth) + 1;
        state.counters["LinearDamping"] = 1;
        state.counters["MaximumDisplacement"] = maximumDisplacement;
        state.counters["MaximumHorizontalDisplacement"] = maximumHorizontalDisplacement;
        state.counters["MaximumVerticalDisplacement"] = maximumVerticalDisplacement;
        state.counters["MinimumHeight"] = minimumHeight;
        state.counters["Notifications"] = 0;
        state.counters["PenetrationSlopMillimeters"] = 20;
        state.counters["PositionSteps"] = 2;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Sleep"] = 0;
        state.counters["GridSpacingMillimeters"] = 1100;
        state.counters["RowWidth"] = rowWidth;
        state.counters["Restitution"] = 0;
        state.counters["ValidationFrames"] = aznumeric_cast<double>(validationFrameCount);
        state.counters["VelocitySteps"] = 10;
        if (useMatchedSolverPolicy)
        {
            if (workerCount == 1)
            {
                state.counters["LargeIslandSplitter"] = 0;
            }
            state.counters["PenetrationSlopMillimeters"] = 1;
            state.counters["PositionSteps"] = 4;
            state.counters["VelocitySteps"] = 2;
        }
        state.counters["WarmupCompleted"] = 1;
        state.counters["WarmupTicks"] = WarmupFrameCount;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(validationFrameCount * bodyCount);
    }

    void StepSettledBoxes(
        benchmark::State& state)
    {
        StepSettledBoxesImpl<false>(state, true);
    }

    void StepSettledBoxesTail(
        benchmark::State& state)
    {
        StepSettledBoxesImpl<true>(state, true);
    }

    void StepSettledBoxesDefaultQuality(
        benchmark::State& state)
    {
        StepSettledBoxesImpl<false>(state, false);
    }

    void StepAutomaticWorlds(
        benchmark::State& state)
    {
        const AZ::u32 worldCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 bodiesPerWorld = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workersPerWorld = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContext(AZStd::max(worldCount, workersPerWorld));

        SystemConfiguration systemConfiguration = CreateSystemConfiguration(
            workersPerWorld,
            bodiesPerWorld + 1);
        systemConfiguration.m_createDefaultWorld = false;
        systemConfiguration.m_defaultWorld.m_autoSimulate = true;
        Runtime system(systemConfiguration, &jobContext.Get());
        if (!system)
        {
            state.SkipWithError("Failed to create the Jolt multi-world benchmark system.");
            return;
        }

        AZStd::vector<WorldHandle> worldHandles;
        worldHandles.reserve(worldCount);
        for (AZ::u32 worldIndex = 0; worldIndex < worldCount; ++worldIndex)
        {
            const WorldHandle worldHandle = system.CreateWorld(systemConfiguration.m_defaultWorld);
            const ShapeHandle floorShape = CreateBox(
                system,
                worldHandle,
                AZ::Vector3(64.0f, 64.0f, 1.0f));
            const BodyHandle floorBody = CreateBody(
                system,
                worldHandle,
                floorShape,
                MotionType::Static,
                AZ::Vector3(0.0f, 0.0f, -0.5f));
            const ShapeHandle bodyShape = CreateBox(
                system,
                worldHandle,
                AZ::Vector3::CreateOne());
            if (!worldHandle || !floorShape || !floorBody || !bodyShape)
            {
                state.SkipWithError("Failed to create a Jolt multi-world benchmark world.");
                return;
            }

            constexpr AZ::u32 rowWidth = 16;
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodiesPerWorld; ++bodyIndex)
            {
                const AZ::u32 layerIndex = bodyIndex / (rowWidth * rowWidth);
                const AZ::u32 rowIndex = (bodyIndex / rowWidth) % rowWidth;
                const AZ::u32 columnIndex = bodyIndex % rowWidth;
                const AZ::Vector3 position(
                    MatchedGridSpacing
                        * (aznumeric_cast<float>(columnIndex) - 0.5f * rowWidth),
                    MatchedGridSpacing
                        * (aznumeric_cast<float>(rowIndex) - 0.5f * rowWidth),
                    0.6f + aznumeric_cast<float>(layerIndex) * MatchedGridSpacing);
                if (!CreateBody(
                    system,
                    worldHandle,
                    bodyShape,
                    MotionType::Dynamic,
                    position))
                {
                    state.SkipWithError("Failed to create a Jolt multi-world benchmark body.");
                    return;
                }
            }
            worldHandles.push_back(worldHandle);
        }

        constexpr AZ::u32 warmupFrameCount = 120;
        for (AZ::u32 frameIndex = 0; frameIndex < warmupFrameCount; ++frameIndex)
        {
            if (!system.StepAutoSimulatedWorlds(MatchedTimeStep))
            {
                state.SkipWithError("Failed to warm the Jolt multi-world benchmark.");
                return;
            }
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            qualityValid = system.StepAutoSimulatedWorlds(MatchedTimeStep)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        jobContext.AddCounters(state);
        state.counters["BodiesPerWorld"] = bodiesPerWorld;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["TotalBodies"] = bodiesPerWorld * worldCount;
        state.counters["WorkersPerWorld"] = workersPerWorld;
        state.counters["Worlds"] = worldCount;
        state.SetItemsProcessed(state.iterations() * bodiesPerWorld * worldCount);
    }

    void CreateDestroyBodiesImpl(
        benchmark::State& state,
        const bool collectStatistics)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system || !worldHandle)
        {
            state.SkipWithError("Failed to create the Jolt lifecycle benchmark world.");
            return;
        }
        if (collectStatistics
            && !system.ConfigurePerformanceStatistics(
                worldHandle,
                PerformanceStatisticsFlags::Memory | PerformanceStatisticsFlags::Resources))
        {
            state.SkipWithError("Failed to enable Jolt lifecycle allocation statistics.");
            return;
        }
        AZStd::vector<BodyHandle> bodies;
        bodies.reserve(bodyCount);
        AZStd::vector<ShapeHandle> shapes;
        shapes.reserve(bodyCount);
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
            {
                const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
                const BodyHandle bodyHandle = CreateBody(
                    system,
                    worldHandle,
                    shapeHandle,
                    MotionType::Dynamic,
                    AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(bodyIndex)));
                if (!shapeHandle || !bodyHandle)
                {
                    state.SkipWithError("Failed to create a Jolt lifecycle benchmark body.");
                    return;
                }
                benchmark::DoNotOptimize(bodyHandle);
                bodies.push_back(bodyHandle);
                shapes.push_back(shapeHandle);
            }
            for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
            {
                const bool bodyDestroyed = system.DestroyBody(worldHandle, bodies[bodyIndex]);
                const bool shapeDestroyed = system.DestroyShape(worldHandle, shapes[bodyIndex]);
                benchmark::DoNotOptimize(bodyDestroyed);
                benchmark::DoNotOptimize(shapeDestroyed);
                if (!bodyDestroyed || !shapeDestroyed)
                {
                    state.SkipWithError("Failed to destroy a Jolt lifecycle benchmark body.");
                    return;
                }
            }
            bodies.clear();
            shapes.clear();
        }

        state.counters["DynamicBodies"] = bodyCount;
        if (collectStatistics)
        {
            AddPerformanceCounters(state, system, worldHandle);
        }
        state.counters["InstrumentationEnabled"] = 0;
        if (collectStatistics)
        {
            state.counters["InstrumentationEnabled"] = 1;
        }
        jobContext.AddCounters(state);
        state.counters["Notifications"] = 0;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void CreateDestroyBodies(
        benchmark::State& state)
    {
        CreateDestroyBodiesImpl(state, false);
    }

    void CreateDestroyBodiesInstrumented(
        benchmark::State& state)
    {
        CreateDestroyBodiesImpl(state, true);
    }

    void ChangeRagdollMembership(
        benchmark::State& state)
    {
        const AZ::u32 partCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, partCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RagdollBenchmarkScenario scenario;
        if (!system
            || !worldHandle
            || !CreateRagdollBenchmarkScenario(system, worldHandle, partCount, scenario)
            || !system.RemoveRagdollFromSimulation(worldHandle, scenario.m_ragdollHandle)
            || !system.AddRagdollToSimulation(worldHandle, scenario.m_ragdollHandle, true))
        {
            state.SkipWithError("Failed to prepare the Jolt ragdoll membership benchmark.");
            return;
        }

        bool changedMembership = false;
        for ([[maybe_unused]] auto iteration : state)
        {
            changedMembership = system.RemoveRagdollFromSimulation(worldHandle, scenario.m_ragdollHandle)
                && system.AddRagdollToSimulation(worldHandle, scenario.m_ragdollHandle, true);
            benchmark::DoNotOptimize(changedMembership);
            if (!changedMembership)
            {
                state.SkipWithError("Failed to change Jolt ragdoll simulation membership.");
                break;
            }
        }

        DestroyRagdollBenchmarkScenario(system, worldHandle, scenario);
        jobContext.AddCounters(state);
        state.counters["Parts"] = partCount;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * partCount * 2);
    }

    void RecreateRagdoll(
        benchmark::State& state)
    {
        const AZ::u32 partCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, partCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RagdollBenchmarkScenario scenario;
        if (!system
            || !worldHandle
            || !CreateRagdollBenchmarkScenario(system, worldHandle, partCount, scenario)
            || !system.DestroyRagdoll(worldHandle, scenario.m_ragdollHandle))
        {
            state.SkipWithError("Failed to prepare the Jolt ragdoll recreation benchmark.");
            return;
        }
        scenario.m_ragdollHandle = {};

        bool recreated = false;
        for ([[maybe_unused]] auto iteration : state)
        {
            scenario.m_ragdollHandle = system.CreateRagdoll(worldHandle, scenario.m_configuration);
            recreated = scenario.m_ragdollHandle
                && system.DestroyRagdoll(worldHandle, scenario.m_ragdollHandle);
            benchmark::DoNotOptimize(recreated);
            scenario.m_ragdollHandle = {};
            if (!recreated)
            {
                state.SkipWithError("Failed to recreate a Jolt ragdoll.");
                break;
            }
        }

        DestroyRagdollBenchmarkScenario(system, worldHandle, scenario);
        jobContext.AddCounters(state);
        state.counters["Parts"] = partCount;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * partCount * 2);
    }

    void CreateSoftBodyDefinition(
        benchmark::State& state)
    {
        const AZ::u32 gridSize = aznumeric_cast<AZ::u32>(state.range(0));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_createDefaultWorld = false;
        Runtime system(systemConfiguration, nullptr);
        const SoftBodyDefinitionConfiguration configuration =
            CreateSoftBodyBenchmarkDefinition(gridSize);
        for ([[maybe_unused]] auto iteration : state)
        {
            const SoftBodyDefinitionHandle definitionHandle =
                system.CreateSoftBodyDefinition(configuration);
            benchmark::DoNotOptimize(definitionHandle);
            if (!definitionHandle || !system.DestroySoftBodyDefinition(definitionHandle))
            {
                state.SkipWithError("Failed to create or destroy a soft body definition.");
                return;
            }
        }

        state.counters["Edges"] = aznumeric_cast<double>(configuration.m_edgeConstraints.size());
        state.counters["Faces"] = aznumeric_cast<double>(configuration.m_faces.size());
        state.counters["Vertices"] = aznumeric_cast<double>(configuration.m_vertices.size());
        state.SetItemsProcessed(state.iterations() * configuration.m_vertices.size());
    }

    void ImportSoftBodyDefinition(
        benchmark::State& state)
    {
        const AZ::u32 gridSize = aznumeric_cast<AZ::u32>(state.range(0));
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_createDefaultWorld = false;
        Runtime system(systemConfiguration, nullptr);
        const SoftBodyDefinitionConfiguration configuration =
            CreateSoftBodyBenchmarkDefinition(gridSize);
        const SoftBodyDefinitionHandle sourceHandle = system.CreateSoftBodyDefinition(configuration);
        SoftBodyDefinitionArchive archive;
        AZStd::vector<MaterialHandle> materialHandles;
        if (!sourceHandle
            || !system.ExportSoftBodyDefinition(sourceHandle, archive, materialHandles)
            || !system.DestroySoftBodyDefinition(sourceHandle))
        {
            state.SkipWithError("Failed to prepare a cooked soft body definition.");
            return;
        }

        for ([[maybe_unused]] auto iteration : state)
        {
            const SoftBodyDefinitionHandle definitionHandle =
                system.ImportSoftBodyDefinition(archive, materialHandles);
            benchmark::DoNotOptimize(definitionHandle);
            if (!definitionHandle || !system.DestroySoftBodyDefinition(definitionHandle))
            {
                state.SkipWithError("Failed to import or destroy a soft body definition.");
                return;
            }
        }

        state.counters["ArchiveBytes"] = aznumeric_cast<double>(archive.m_binaryState.size());
        state.counters["Edges"] = aznumeric_cast<double>(configuration.m_edgeConstraints.size());
        state.counters["Faces"] = aznumeric_cast<double>(configuration.m_faces.size());
        state.counters["Vertices"] = aznumeric_cast<double>(configuration.m_vertices.size());
        state.SetBytesProcessed(state.iterations() * archive.m_binaryState.size());
        state.SetItemsProcessed(state.iterations() * configuration.m_vertices.size());
    }

    void BenchmarkFilteredRollbackState(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const bool restoreState = state.range(1) != 0;
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the rollback recapture benchmark world.");
            return;
        }

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            const BodyHandle bodyHandle = CreateBody(
                system,
                worldHandle,
                shapeHandle,
                MotionType::Dynamic,
                AZ::Vector3(aznumeric_cast<float>(bodyIndex) * 2.0f, 0.0f, 0.0f));
            if (!bodyHandle)
            {
                state.SkipWithError("Failed to create a rollback recapture benchmark body.");
                return;
            }
            bodyHandles.push_back(bodyHandle);
        }

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_geometry = PointConstraintConfiguration{};
        for (size_t bodyIndex = 1; bodyIndex < bodyHandles.size(); ++bodyIndex)
        {
            constraintConfiguration.m_firstBodyHandle = bodyHandles[bodyIndex - 1];
            constraintConfiguration.m_secondBodyHandle = bodyHandles[bodyIndex];
            if (!system.CreateConstraint(worldHandle, constraintConfiguration))
            {
                state.SkipWithError("Failed to create a rollback recapture benchmark constraint.");
                return;
            }
        }

        StateSnapshotConfiguration snapshotConfiguration{
            .m_flags = StateSnapshotFlags::Bodies | StateSnapshotFlags::Constraints,
            .m_filterBodies = true,
        };
        if (state.range(1) == 2)
        {
            snapshotConfiguration.m_restoreSafety = RestoreSafety::Validated;
        }
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(
            worldHandle,
            snapshotConfiguration,
            bodyHandles);
        if (!snapshotHandle
            || !system.CaptureWorldState(
                worldHandle,
                snapshotHandle,
                snapshotConfiguration,
                bodyHandles)
            || !system.CaptureWorldState(
                worldHandle,
                snapshotHandle,
                snapshotConfiguration,
                bodyHandles))
        {
            state.SkipWithError("Failed to warm the rollback recapture benchmark.");
            return;
        }
        if (restoreState)
        {
            constexpr AZ::u32 restoreWarmupCount = 2;
            for (AZ::u32 warmupIndex = 0; warmupIndex < restoreWarmupCount; ++warmupIndex)
            {
                if (!system.RestoreWorldState(worldHandle, snapshotHandle))
                {
                    state.SkipWithError("Failed to warm the rollback restore benchmark.");
                    return;
                }
            }
        }
        if (!system.ConfigurePerformanceStatistics(
                worldHandle,
                PerformanceStatisticsFlags::Memory
                    | PerformanceStatisticsFlags::Resources
                    | PerformanceStatisticsFlags::Snapshots))
        {
            state.SkipWithError("Failed to enable rollback allocation statistics.");
            return;
        }
        WorldPerformanceStatistics warmupStatistics;
        if (!system.GetPerformanceStatistics(worldHandle, warmupStatistics, true))
        {
            state.SkipWithError("Failed to reset rollback allocation statistics.");
            return;
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            if (restoreState)
            {
                qualityValid = static_cast<bool>(system.RestoreWorldState(worldHandle, snapshotHandle)) && qualityValid;
            }
            else
            {
                qualityValid = system.CaptureWorldState(
                    worldHandle,
                    snapshotHandle,
                    snapshotConfiguration,
                    bodyHandles)
                    && qualityValid;
            }
            benchmark::DoNotOptimize(qualityValid);
        }

        WorldPerformanceStatistics finalStatistics;
        if (!system.GetPerformanceStatistics(worldHandle, finalStatistics, false))
        {
            state.SkipWithError("Failed to read rollback allocation statistics.");
            return;
        }
        AZ::u64 nativeAllocatedGrowthBytes = 0;
        if (finalStatistics.m_processNativeAllocatedBytes > warmupStatistics.m_processNativeAllocatedBytes)
        {
            nativeAllocatedGrowthBytes =
                finalStatistics.m_processNativeAllocatedBytes - warmupStatistics.m_processNativeAllocatedBytes;
        }
        AZ::u64 tempAllocatorCapacityGrowthBytes = 0;
        if (finalStatistics.m_tempAllocatorCapacityBytes > warmupStatistics.m_tempAllocatorCapacityBytes)
        {
            tempAllocatorCapacityGrowthBytes =
                finalStatistics.m_tempAllocatorCapacityBytes - warmupStatistics.m_tempAllocatorCapacityBytes;
        }
        AZ::u64 wrapperRetainedGrowthBytes = 0;
        if (finalStatistics.m_wrapperRetainedBytes > warmupStatistics.m_wrapperRetainedBytes)
        {
            wrapperRetainedGrowthBytes = finalStatistics.m_wrapperRetainedBytes - warmupStatistics.m_wrapperRetainedBytes;
        }
        qualityValid = finalStatistics.m_processNativeAllocationCount == 0
            && finalStatistics.m_processNativeFreeCount == 0
            && finalStatistics.m_processNativeReallocationCount == 0
            && nativeAllocatedGrowthBytes == 0
            && finalStatistics.m_snapshotFailureCount == 0
            && tempAllocatorCapacityGrowthBytes == 0
            && wrapperRetainedGrowthBytes == 0
            && qualityValid;

        state.counters["Bodies"] = bodyCount;
        state.counters["Constraints"] = bodyCount - 1;
        AddPerformanceCounters(state, system, worldHandle);
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["NativeAllocatedGrowthBytes"] = aznumeric_cast<double>(nativeAllocatedGrowthBytes);
        state.counters["TempAllocatorCapacityGrowthBytes"] = aznumeric_cast<double>(
            tempAllocatorCapacityGrowthBytes);
        state.counters["WrapperRetainedGrowthBytes"] = aznumeric_cast<double>(wrapperRetainedGrowthBytes);
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * (bodyCount * 2 - 1));
    }

    void CreateDestroyCookedCompounds(
        benchmark::State& state)
    {
        const AZ::u32 compoundCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system || !worldHandle)
        {
            state.SkipWithError("Failed to create the Jolt cooked compound lifecycle benchmark world.");
            return;
        }

        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        const CookedShapeHandle boxHandle = system.CookShape(boxConfiguration);

        CookedDecoratedShapeConfiguration scaledConfiguration;
        scaledConfiguration.m_geometry = CookedScaledShapeConfiguration{
            .m_shapeHandle = boxHandle,
            .m_scale = AZ::Vector3(2.0f, 1.0f, 0.5f),
        };
        const CookedShapeHandle scaledHandle = system.CookShape(scaledConfiguration);

        CookedCompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_children = {
            {
                .m_position = -AZ::Vector3::CreateAxisX(2.0f),
                .m_shapeHandle = scaledHandle,
            },
            {
                .m_position = AZ::Vector3::CreateAxisX(2.0f),
                .m_shapeHandle = boxHandle,
            },
            {
                .m_position = AZ::Vector3::CreateAxisY(2.0f),
                .m_shapeHandle = boxHandle,
            },
        };
        const CookedShapeHandle compoundHandle = system.CookShape(compoundConfiguration);
        if (!boxHandle || !scaledHandle || !compoundHandle)
        {
            state.SkipWithError("Failed to cook the Jolt compound lifecycle benchmark shapes.");
            return;
        }

        AZStd::vector<ShapeHandle> compounds;
        compounds.reserve(compoundCount);
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 compoundIndex = 0; compoundIndex < compoundCount; ++compoundIndex)
            {
                const ShapeHandle shapeHandle = system.CreateShape(worldHandle, compoundHandle);
                if (!shapeHandle)
                {
                    state.SkipWithError("Failed to instantiate a Jolt cooked compound benchmark shape.");
                    return;
                }
                benchmark::DoNotOptimize(shapeHandle);
                compounds.push_back(shapeHandle);
            }

            for (const ShapeHandle shapeHandle : compounds)
            {
                const bool destroyed = system.DestroyShape(worldHandle, shapeHandle);
                benchmark::DoNotOptimize(destroyed);
                if (!destroyed)
                {
                    state.SkipWithError("Failed to destroy a Jolt cooked compound benchmark shape.");
                    return;
                }
            }
            compounds.clear();
        }

        if (!system.DestroyCookedShape(compoundHandle)
            || !system.DestroyCookedShape(scaledHandle)
            || !system.DestroyCookedShape(boxHandle))
        {
            state.SkipWithError("Failed to release the Jolt cooked compound benchmark shapes.");
            return;
        }

        state.counters["ChildPlacements"] = 3;
        state.counters["Compounds"] = compoundCount;
        state.counters["UniqueChildren"] = 2;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * compoundCount);
    }

    void ChangeBodyMembershipIndividually(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the individual-membership benchmark world.");
            return;
        }

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        BodyConfiguration configuration;
        configuration.m_shapeHandle = shapeHandle;
        configuration.m_startInSimulation = false;
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            configuration.m_transform.m_position.m_z = bodyIndex;
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, configuration);
            if (!bodyHandle)
            {
                state.SkipWithError("Failed to create an individual-membership benchmark body.");
                return;
            }
            bodyHandles.push_back(bodyHandle);
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (const BodyHandle bodyHandle : bodyHandles)
            {
                qualityValid = system.AddBodyToSimulation(worldHandle, bodyHandle, false)
                    && qualityValid;
            }
            for (const BodyHandle bodyHandle : bodyHandles)
            {
                qualityValid = system.RemoveBodyFromSimulation(worldHandle, bodyHandle)
                    && qualityValid;
            }
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Bodies"] = bodyCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount * 2);
    }

    void ReadBodyVelocities(
        benchmark::State& state)
    {
        const AZ::u32 readCount = aznumeric_cast<AZ::u32>(state.range(0));
        const bool combined = state.range(1) != 0;
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        const BodyHandle bodyHandle = CreateBody(
            system,
            worldHandle,
            shapeHandle,
            MotionType::Dynamic,
            AZ::Vector3::CreateZero());
        if (!system || !worldHandle || !shapeHandle || !bodyHandle)
        {
            state.SkipWithError("Failed to create the body-velocity benchmark world.");
            return;
        }

        const AZ::Vector3 expectedLinearVelocity(1.0f, 2.0f, 3.0f);
        const AZ::Vector3 expectedAngularVelocity(4.0f, 5.0f, 6.0f);
        if (!system.SetBodyVelocities(
                worldHandle,
                bodyHandle,
                expectedLinearVelocity,
                expectedAngularVelocity))
        {
            state.SkipWithError("Failed to initialize the body-velocity benchmark.");
            return;
        }

        AZ::Vector3 linearVelocity;
        AZ::Vector3 angularVelocity;
        bool qualityValid = true;
        if (combined)
        {
            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 readIndex = 0; readIndex < readCount; ++readIndex)
                {
                    qualityValid = system.GetBodyVelocities(
                        worldHandle,
                        bodyHandle,
                        linearVelocity,
                        angularVelocity)
                        && qualityValid;
                    benchmark::DoNotOptimize(linearVelocity);
                    benchmark::DoNotOptimize(angularVelocity);
                }
                benchmark::DoNotOptimize(qualityValid);
            }
        }
        else
        {
            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 readIndex = 0; readIndex < readCount; ++readIndex)
                {
                    qualityValid = system.GetBodyLinearVelocity(
                        worldHandle,
                        bodyHandle,
                        linearVelocity)
                        && system.GetBodyAngularVelocity(
                            worldHandle,
                            bodyHandle,
                            angularVelocity)
                        && qualityValid;
                    benchmark::DoNotOptimize(linearVelocity);
                    benchmark::DoNotOptimize(angularVelocity);
                }
                benchmark::DoNotOptimize(qualityValid);
            }
        }

        qualityValid = qualityValid
            && linearVelocity.IsClose(expectedLinearVelocity)
            && angularVelocity.IsClose(expectedAngularVelocity);
        state.counters["Combined"] = 0;
        if (combined)
        {
            state.counters["Combined"] = 1;
        }
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Reads"] = readCount;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * readCount);
    }

    void ChangeBodyMembershipInBulk(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the bulk-membership benchmark world.");
            return;
        }

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        BodyConfiguration configuration;
        configuration.m_shapeHandle = shapeHandle;
        configuration.m_startInSimulation = false;
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            configuration.m_transform.m_position.m_z = bodyIndex;
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, configuration);
            if (!bodyHandle)
            {
                state.SkipWithError("Failed to create a bulk-membership benchmark body.");
                return;
            }
            bodyHandles.push_back(bodyHandle);
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            qualityValid = system.AddBodiesToSimulation(worldHandle, bodyHandles, false)
                && qualityValid;
            qualityValid = system.RemoveBodiesFromSimulation(worldHandle, bodyHandles)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Bodies"] = bodyCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount * 2);
    }

    void DestroyBodiesIndividually(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the individual-destruction benchmark world.");
            return;
        }

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            state.PauseTiming();
            const bool created = CreateBodiesForDestruction(
                system,
                worldHandle,
                shapeHandle,
                bodyCount,
                bodyHandles);
            state.ResumeTiming();
            if (!created)
            {
                state.SkipWithError("Failed to create individual-destruction benchmark bodies.");
                return;
            }

            for (const BodyHandle bodyHandle : bodyHandles)
            {
                qualityValid = system.DestroyBody(worldHandle, bodyHandle)
                    && qualityValid;
            }
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Bodies"] = bodyCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void DestroyBodiesInBulk(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the bulk-destruction benchmark world.");
            return;
        }

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            state.PauseTiming();
            const bool created = CreateBodiesForDestruction(
                system,
                worldHandle,
                shapeHandle,
                bodyCount,
                bodyHandles);
            state.ResumeTiming();
            if (!created)
            {
                state.SkipWithError("Failed to create bulk-destruction benchmark bodies.");
                return;
            }

            qualityValid = system.DestroyBodies(worldHandle, bodyHandles)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Bodies"] = bodyCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * bodyCount);
    }

    void ChangeConstraintMembershipIndividually(
        benchmark::State& state)
    {
        const AZ::u32 constraintCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, constraintCount + 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        AZStd::vector<ConstraintHandle> constraintHandles;
        if (!system
            || !worldHandle
            || !shapeHandle
            || !CreateConstraintsForMembership(
                system,
                worldHandle,
                shapeHandle,
                constraintCount,
                constraintHandles))
        {
            state.SkipWithError("Failed to create the individual constraint-membership benchmark world.");
            return;
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (const ConstraintHandle constraintHandle : constraintHandles)
            {
                qualityValid = system.AddConstraintToSimulation(worldHandle, constraintHandle)
                    && qualityValid;
            }
            for (const ConstraintHandle constraintHandle : constraintHandles)
            {
                qualityValid = system.RemoveConstraintFromSimulation(worldHandle, constraintHandle)
                    && qualityValid;
            }
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Constraints"] = constraintCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * constraintCount * 2);
    }

    void ChangeConstraintMembershipInBulk(
        benchmark::State& state)
    {
        const AZ::u32 constraintCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, constraintCount + 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        AZStd::vector<ConstraintHandle> constraintHandles;
        if (!system
            || !worldHandle
            || !shapeHandle
            || !CreateConstraintsForMembership(
                system,
                worldHandle,
                shapeHandle,
                constraintCount,
                constraintHandles))
        {
            state.SkipWithError("Failed to create the bulk constraint-membership benchmark world.");
            return;
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            qualityValid = system.AddConstraintsToSimulation(worldHandle, constraintHandles)
                && qualityValid;
            qualityValid = system.RemoveConstraintsFromSimulation(worldHandle, constraintHandles)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Constraints"] = constraintCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * constraintCount * 2);
    }

    void DestroyConstraintsIndividually(
        benchmark::State& state)
    {
        const AZ::u32 constraintCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, constraintCount + 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        AZStd::vector<BodyHandle> bodyHandles;
        if (!system
            || !worldHandle
            || !shapeHandle
            || !CreateBodiesForConstraintDestruction(
                system,
                worldHandle,
                shapeHandle,
                constraintCount,
                bodyHandles))
        {
            state.SkipWithError("Failed to create the individual constraint-destruction benchmark world.");
            return;
        }

        AZStd::vector<ConstraintHandle> constraintHandles;
        constraintHandles.reserve(constraintCount);
        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            state.PauseTiming();
            const bool created = CreateConstraintsForDestruction(
                system,
                worldHandle,
                bodyHandles,
                constraintHandles);
            state.ResumeTiming();
            if (!created)
            {
                state.SkipWithError("Failed to create individual-destruction benchmark constraints.");
                return;
            }

            for (const ConstraintHandle constraintHandle : constraintHandles)
            {
                qualityValid = system.DestroyConstraint(worldHandle, constraintHandle)
                    && qualityValid;
            }
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Constraints"] = constraintCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * constraintCount);
    }

    void DestroyConstraintsInBulk(
        benchmark::State& state)
    {
        const AZ::u32 constraintCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, constraintCount + 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        AZStd::vector<BodyHandle> bodyHandles;
        if (!system
            || !worldHandle
            || !shapeHandle
            || !CreateBodiesForConstraintDestruction(
                system,
                worldHandle,
                shapeHandle,
                constraintCount,
                bodyHandles))
        {
            state.SkipWithError("Failed to create the bulk constraint-destruction benchmark world.");
            return;
        }

        AZStd::vector<ConstraintHandle> constraintHandles;
        constraintHandles.reserve(constraintCount);
        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            state.PauseTiming();
            const bool created = CreateConstraintsForDestruction(
                system,
                worldHandle,
                bodyHandles,
                constraintHandles);
            state.ResumeTiming();
            if (!created)
            {
                state.SkipWithError("Failed to create bulk-destruction benchmark constraints.");
                return;
            }

            qualityValid = system.DestroyConstraints(worldHandle, constraintHandles)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        state.counters["Constraints"] = constraintCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * constraintCount);
    }

    void RaycastGrid(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, obstacleCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system
            || !worldHandle
            || !CreateQueryGrid(system, worldHandle, obstacleCount))
        {
            state.SkipWithError("Failed to create the Jolt raycast benchmark grid.");
            return;
        }

        RaycastRequest request;
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (!worldQueries)
        {
            state.SkipWithError("Failed to acquire the Jolt query benchmark world view.");
            return;
        }
        request.m_displacement = -AZ::Vector3::CreateAxisZ(20.0f);
        RaycastHit hit;
        AZ::u64 successfulRayCount = 0;
        constexpr AZ::u32 rowWidth = 32;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                request.m_start = {
                    .m_x = 2.0 * static_cast<double>(obstacleIndex % rowWidth),
                    .m_y = 2.0 * static_cast<double>(obstacleIndex / rowWidth),
                    .m_z = 10.0,
                };
                const bool raySucceeded = worldQueries->RaycastClosest(request, hit);
                successfulRayCount += raySucceeded;
                benchmark::DoNotOptimize(raySucceeded);
                benchmark::DoNotOptimize(hit);
            }
        }

        const AZ::u64 expectedRayCount = aznumeric_cast<AZ::u64>(state.iterations()) * rayCount;
        const bool qualityValid = successfulRayCount == expectedRayCount;
        state.counters["Obstacles"] = obstacleCount;
        jobContext.AddCounters(state);
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["SuccessfulQueries"] = aznumeric_cast<double>(successfulRayCount);
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void RaycastEmptyWorld(
        benchmark::State& state)
    {
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system || !worldHandle)
        {
            state.SkipWithError("Failed to create the empty Jolt query world.");
            return;
        }
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (!worldQueries)
        {
            state.SkipWithError("Failed to acquire the empty Jolt query world view.");
            return;
        }

        RaycastRequest request;
        request.m_displacement = AZ::Vector3::CreateAxisX(20.0f);
        RaycastHit hit;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                benchmark::DoNotOptimize(worldQueries->RaycastClosest(request, hit));
            }
        }

        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void QueryFloatScopeOverhead(
        benchmark::State& state)
    {
        for ([[maybe_unused]] auto iteration : state)
        {
            DeterministicFloatScope floatScope;
            benchmark::DoNotOptimize(floatScope);
        }
    }

    void QueryLockOverhead(
        benchmark::State& state)
    {
        DeterministicWorldMutex mutex;
        for ([[maybe_unused]] auto iteration : state)
        {
            DeterministicWorldQueryLock lock(mutex);
            benchmark::DoNotOptimize(lock);
        }
    }

    void RaycastBroadPhaseGrid(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, obstacleCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system
            || !worldHandle
            || !CreateQueryGrid(system, worldHandle, obstacleCount))
        {
            state.SkipWithError("Failed to create the Jolt broad-phase raycast benchmark grid.");
            return;
        }

        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (!worldQueries)
        {
            state.SkipWithError("Failed to acquire the Jolt broad-phase query benchmark world view.");
            return;
        }

        BroadPhaseCastRequest request;
        request.m_geometry = BroadPhaseRay{
            .m_displacement = -AZ::Vector3::CreateAxisZ(20.0f),
        };
        BroadPhaseRay& ray = AZStd::get<BroadPhaseRay>(request.m_geometry);
        BroadPhaseCastHit hit;
        bool qualityValid = false;
        constexpr AZ::u32 rowWidth = 32;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                ray.m_start = {
                    .m_x = 2.0 * static_cast<double>(obstacleIndex % rowWidth),
                    .m_y = 2.0 * static_cast<double>(obstacleIndex / rowWidth),
                    .m_z = 10.0,
                };
                qualityValid = worldQueries->CastBroadPhaseClosest(request, hit);
                benchmark::DoNotOptimize(qualityValid);
                benchmark::DoNotOptimize(hit);
            }
        }

        state.counters["Obstacles"] = obstacleCount;
        jobContext.AddCounters(state);
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
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, obstacleCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system
            || !worldHandle
            || !CreateQueryGrid(system, worldHandle, obstacleCount))
        {
            state.SkipWithError("Failed to create the Jolt batch raycast benchmark grid.");
            return;
        }
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (!worldQueries)
        {
            state.SkipWithError("Failed to acquire the Jolt batch query world view.");
            return;
        }

        AZStd::vector<RaycastRequest> requests(rayCount);
        AZStd::vector<ClosestRaycastResult> results(rayCount);
        constexpr AZ::u32 rowWidth = 32;
        for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
        {
            const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
            requests[rayIndex].m_start = {
                .m_x = 2.0 * static_cast<double>(obstacleIndex % rowWidth),
                .m_y = 2.0 * static_cast<double>(obstacleIndex / rowWidth),
                .m_z = 10.0,
            };
            requests[rayIndex].m_displacement = -AZ::Vector3::CreateAxisZ(20.0f);
        }

        constexpr auto warmupDuration = AZStd::chrono::milliseconds(100);
        const auto warmupDeadline = AZStd::chrono::steady_clock::now() + warmupDuration;
        BufferResult batchResult;
        bool warmupCompleted = true;
        do
        {
            batchResult = worldQueries->RaycastClosestBatch(requests, results);
            warmupCompleted = batchResult.m_count == rayCount
                && batchResult.m_requiredCount == rayCount
                && batchResult.IsComplete()
                && warmupCompleted;
            benchmark::DoNotOptimize(batchResult);
        } while (AZStd::chrono::steady_clock::now() < warmupDeadline);

        AZ::u64 completeBatchCount = 0;
        for ([[maybe_unused]] auto iteration : state)
        {
            batchResult = worldQueries->RaycastClosestBatch(requests, results);
            if (batchResult.m_count == rayCount
                && batchResult.m_requiredCount == rayCount
                && batchResult.IsComplete())
            {
                ++completeBatchCount;
            }
            benchmark::DoNotOptimize(batchResult);
        }

        bool qualityValid = warmupCompleted
            && completeBatchCount == aznumeric_cast<AZ::u64>(state.iterations())
            && batchResult.m_count == rayCount
            && batchResult.m_requiredCount == rayCount
            && batchResult.IsComplete();
        for (const ClosestRaycastResult& result : results)
        {
            qualityValid = result.m_found
                && result.m_hit.m_bodyHandle
                && result.m_hit.m_shapeHandle
                && qualityValid;
        }
        state.counters["Obstacles"] = obstacleCount;
        jobContext.AddCounters(state);
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["CompleteOperations"] = aznumeric_cast<double>(completeBatchCount);
        state.counters["WarmupCompleted"] = 0;
        if (warmupCompleted)
        {
            state.counters["WarmupCompleted"] = 1;
        }
        state.counters["WarmupMs"] = static_cast<double>(warmupDuration.count());
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * rayCount);
    }

    void OverlapSphereGrid(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 queryCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, obstacleCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle queryShape = CreateSphere(system, worldHandle, 5.0f);
        if (!system
            || !worldHandle
            || !queryShape
            || !CreateQueryGrid(system, worldHandle, obstacleCount))
        {
            state.SkipWithError("Failed to create the Jolt overlap benchmark grid.");
            return;
        }
        const IWorldQueries* worldQueries = system.GetWorldQueries(worldHandle);
        if (!worldQueries)
        {
            state.SkipWithError("Failed to acquire the Jolt overlap query world view.");
            return;
        }

        constexpr AZ::u32 expectedHitCount = 25;
        ShapeOverlapRequest request;
        request.m_shapeHandle = queryShape;
        request.m_transform.m_position = {.m_x = 32.0, .m_y = 32.0};
        AZStd::array<OverlapHit, expectedHitCount> hits;
        QueryResult result;
        AZ::u64 completeQueryCount = 0;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (AZ::u32 queryIndex = 0; queryIndex < queryCount; ++queryIndex)
            {
                result = worldQueries->OverlapShape(request, hits);
                if (result.m_hitCount == expectedHitCount
                    && result.m_requiredHitCount == expectedHitCount
                    && result.IsComplete())
                {
                    ++completeQueryCount;
                }
                benchmark::DoNotOptimize(result);
                benchmark::DoNotOptimize(hits);
            }
        }

        const AZ::u64 expectedQueryCount = aznumeric_cast<AZ::u64>(state.iterations()) * queryCount;
        const bool qualityValid = completeQueryCount == expectedQueryCount
            && result.m_hitCount == expectedHitCount
            && result.m_requiredHitCount == expectedHitCount
            && result.IsComplete();
        state.counters["ActualHits"] = result.m_hitCount;
        state.counters["CompleteOperations"] = aznumeric_cast<double>(completeQueryCount);
        jobContext.AddCounters(state);
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * queryCount);
    }

    void OverlapSphereGridCountOnly(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, obstacleCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle queryShape = CreateSphere(system, worldHandle, 5.0f);
        if (!system
            || !worldHandle
            || !queryShape
            || !CreateQueryGrid(system, worldHandle, obstacleCount))
        {
            state.SkipWithError("Failed to create the Jolt count-only overlap benchmark grid.");
            return;
        }

        constexpr AZ::u32 expectedHitCount = 25;
        ShapeOverlapRequest request;
        request.m_shapeHandle = queryShape;
        request.m_transform.m_position = {.m_x = 32.0, .m_y = 32.0};
        QueryResult result;
        for ([[maybe_unused]] auto iteration : state)
        {
            result = system.OverlapShape(worldHandle, request, {});
            benchmark::DoNotOptimize(result);
        }

        state.counters["ActualHits"] = result.m_requiredHitCount;
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = 0;
        if (result.m_hitCount == 0
            && result.m_requiredHitCount == expectedHitCount)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations());
    }

    void OverlapSphereGridBroadPhase(
        benchmark::State& state)
    {
        const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, obstacleCount),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system
            || !worldHandle
            || !CreateQueryGrid(system, worldHandle, obstacleCount))
        {
            state.SkipWithError("Failed to create the Jolt broad-phase overlap benchmark grid.");
            return;
        }

        constexpr AZ::u32 expectedHitCount = 25;
        BroadPhaseOverlapRequest request;
        request.m_geometry = BroadPhaseSphere{
            .m_center = {.m_x = 32.0, .m_y = 32.0},
            .m_radius = 5.0f,
        };
        AZStd::array<BroadPhaseHit, expectedHitCount> hits;
        QueryResult result;
        for ([[maybe_unused]] auto iteration : state)
        {
            result = system.OverlapBroadPhase(worldHandle, request, hits);
            benchmark::DoNotOptimize(result);
            benchmark::DoNotOptimize(hits);
        }

        state.counters["ActualHits"] = result.m_hitCount;
        state.counters["ExpectedHits"] = expectedHitCount;
        state.counters["Obstacles"] = obstacleCount;
        state.counters["QualityValid"] = 0;
        if (result.m_hitCount == expectedHitCount
            && result.m_requiredHitCount == expectedHitCount
            && result.IsComplete())
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations());
    }

    void UpdateHair(
        benchmark::State& state)
    {
        const AZ::u32 strandCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 verticesPerStrand = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 gridSize = aznumeric_cast<AZ::u32>(state.range(2));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(3));
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        HairBenchmarkScenario scenario;
        if (!system
            || !worldHandle
            || !CreateHairBenchmarkScenario(
                system,
                worldHandle,
                strandCount,
                verticesPerStrand,
                gridSize,
                scenario))
        {
            state.SkipWithError("Failed to create the Jolt hair update benchmark scenario.");
            return;
        }

        constexpr float timeStep = 1.0f / 60.0f;
        constexpr AZ::u32 warmupStepCount = 4;
        for (AZ::u32 stepIndex = 0; stepIndex < warmupStepCount; ++stepIndex)
        {
            if (!system.UpdateHair(
                worldHandle,
                scenario.m_hairHandle,
                timeStep,
                AZ::Transform::CreateIdentity(),
                {}))
            {
                state.SkipWithError("Failed to warm up the Jolt hair update benchmark.");
                return;
            }
        }

        bool updateSucceeded = false;
        for ([[maybe_unused]] auto iteration : state)
        {
            updateSucceeded = system.UpdateHair(
                worldHandle,
                scenario.m_hairHandle,
                timeStep,
                AZ::Transform::CreateIdentity(),
                {});
            benchmark::DoNotOptimize(updateSucceeded);
        }

        state.counters["GridCells"] = scenario.m_definitionState.m_gridCellCount;
        state.counters["QualityValid"] = 0;
        if (updateSucceeded)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["SimulationVertices"] = scenario.m_definitionState.m_simulationVertexCount;
        state.counters["Strands"] = strandCount;
        state.counters["VerticesPerStrand"] = verticesPerStrand;
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(
            state.iterations() * scenario.m_definitionState.m_simulationVertexCount);
    }

    void ReadBackHair(
        benchmark::State& state)
    {
        const AZ::u32 strandCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 verticesPerStrand = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 gridSize = aznumeric_cast<AZ::u32>(state.range(2));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        HairBenchmarkScenario scenario;
        if (!system
            || !worldHandle
            || !CreateHairBenchmarkScenario(
                system,
                worldHandle,
                strandCount,
                verticesPerStrand,
                gridSize,
                scenario)
            || !system.UpdateHair(
                worldHandle,
                scenario.m_hairHandle,
                MatchedTimeStep,
                AZ::Transform::CreateIdentity(),
                {}))
        {
            state.SkipWithError("Failed to create the Jolt hair readback benchmark scenario.");
            return;
        }

        AZStd::vector<HairVertexState> vertexStates(scenario.m_definitionState.m_simulationVertexCount);
        AZStd::vector<AZ::Vector3> renderPositions(scenario.m_definitionState.m_renderVertexCount);
        AZStd::vector<HairGridCellState> gridCells(scenario.m_definitionState.m_gridCellCount);
        HairReadbackBuffers buffers{
            .m_vertexStates = vertexStates,
            .m_renderPositions = renderPositions,
            .m_gridCells = gridCells,
        };
        HairReadbackResult result;
        if (!system.GetHairReadback(worldHandle, scenario.m_hairHandle, buffers, result))
        {
            state.SkipWithError("Failed to warm up the Jolt hair readback benchmark.");
            return;
        }

        bool readbackSucceeded = false;
        for ([[maybe_unused]] auto iteration : state)
        {
            readbackSucceeded = system.GetHairReadback(
                worldHandle,
                scenario.m_hairHandle,
                buffers,
                result);
            benchmark::DoNotOptimize(readbackSucceeded);
            benchmark::DoNotOptimize(vertexStates);
            benchmark::DoNotOptimize(renderPositions);
            benchmark::DoNotOptimize(gridCells);
        }

        const bool qualityValid = readbackSucceeded
            && result.m_vertexStates.IsComplete()
            && result.m_renderPositions.IsComplete()
            && result.m_gridCells.IsComplete();
        state.counters["GridCells"] = scenario.m_definitionState.m_gridCellCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["SimulationVertices"] = scenario.m_definitionState.m_simulationVertexCount;
        state.counters["Strands"] = strandCount;
        state.counters["VerticesPerStrand"] = verticesPerStrand;
        state.counters["Workers"] = workerCount;
        const size_t bytesPerReadback = vertexStates.size() * sizeof(HairVertexState)
            + renderPositions.size() * sizeof(AZ::Vector3)
            + gridCells.size() * sizeof(HairGridCellState);
        state.SetBytesProcessed(state.iterations() * bytesPerReadback);
    }

    void StepConstraintCategory(
        benchmark::State& state)
    {
        const AZ::u32 constraintCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 category = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, constraintCount + 16),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.2f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the constraint benchmark world.");
            return;
        }

        AZStd::vector<BodyHandle> bodies;
        bodies.reserve(constraintCount + 1);
        for (AZ::u32 bodyIndex = 0; bodyIndex <= constraintCount; ++bodyIndex)
        {
            const BodyHandle bodyHandle = CreateBody(
                system,
                worldHandle,
                shapeHandle,
                MotionType::Dynamic,
                AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(bodyIndex) * 0.4f));
            if (!bodyHandle)
            {
                state.SkipWithError("Failed to create a constraint benchmark body.");
                return;
            }
            bodies.push_back(bodyHandle);
        }

        for (AZ::u32 constraintIndex = 0; constraintIndex < constraintCount; ++constraintIndex)
        {
            ConstraintConfiguration configuration;
            configuration.m_firstBodyHandle = bodies[constraintIndex];
            configuration.m_secondBodyHandle = bodies[constraintIndex + 1];
            switch (category)
            {
            case 0:
                configuration.m_geometry = ConeConstraintConfiguration{};
                break;
            case 1:
                configuration.m_geometry = DistanceConstraintConfiguration{
                    .m_maximumDistance = 0.4f,
                    .m_minimumDistance = 0.4f,
                };
                break;
            case 2:
                configuration.m_geometry = FixedConstraintConfiguration{};
                break;
            case 3:
                configuration.m_geometry = HingeConstraintConfiguration{};
                break;
            case 4:
                configuration.m_geometry = PointConstraintConfiguration{};
                break;
            case 5:
                configuration.m_geometry = SixDofConstraintConfiguration{};
                break;
            case 6:
                configuration.m_geometry = SliderConstraintConfiguration{};
                break;
            case 7:
                configuration.m_geometry = SwingTwistConstraintConfiguration{};
                break;
            default:
                state.SkipWithError("Unknown constraint benchmark category.");
                return;
            }
            if (!system.CreateConstraint(worldHandle, configuration))
            {
                state.SkipWithError("Failed to create a constraint benchmark constraint.");
                return;
            }
        }

        for (AZ::u32 warmup = 0; warmup < 60; ++warmup)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Constraint benchmark warmup failed.");
                return;
            }
        }
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Constraint benchmark step failed.");
                break;
            }
        }

        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["ConstraintCategory"] = category;
        state.counters["ConstraintCount"] = constraintCount;
        state.counters["Workers"] = workerCount;
    }

    void StepSoftBodies(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        JobContextScope jobContext(workerCount);
        Runtime system(CreateSystemConfiguration(workerCount, bodyCount + 16), &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(CreateSoftBodyBenchmarkDefinition(8));
        if (!system || !definitionHandle)
        {
            state.SkipWithError("Failed to create the soft-body benchmark definition.");
            return;
        }

        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            SoftBodyConfiguration configuration;
            configuration.m_definitionHandle = definitionHandle;
            configuration.m_transform.m_position = {
                .m_x = 1.0 * static_cast<double>(bodyIndex % 8),
                .m_y = 1.0 * static_cast<double>(bodyIndex / 8),
                .m_z = 4.0 + 0.1 * static_cast<double>(bodyIndex),
            };
            if (!system.CreateSoftBody(worldHandle, configuration))
            {
                state.SkipWithError("Failed to create a soft-body benchmark body.");
                return;
            }
        }

        for (AZ::u32 warmup = 0; warmup < 30; ++warmup)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Soft-body benchmark warmup failed.");
                return;
            }
        }
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Soft-body benchmark step failed.");
                break;
            }
        }

        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["SoftBodies"] = bodyCount;
        state.counters["Workers"] = workerCount;
    }

    void UpdateVirtualCharacters(
        benchmark::State& state)
    {
        const AZ::u32 characterCount = aznumeric_cast<AZ::u32>(state.range(0));
        Runtime system(CreateSystemConfiguration(1, characterCount + 16), nullptr);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        AZStd::vector<VirtualCharacterHandle> characters;
        characters.reserve(characterCount);
        for (AZ::u32 characterIndex = 0; characterIndex < characterCount; ++characterIndex)
        {
            VirtualCharacterConfiguration configuration;
            configuration.m_shapeHandle = shapeHandle;
            configuration.m_transform.m_position = {
                .m_x = static_cast<double>(characterIndex % 32),
                .m_y = static_cast<double>(characterIndex / 32),
                .m_z = 1.0,
            };
            const VirtualCharacterHandle handle = system.CreateVirtualCharacter(worldHandle, configuration);
            if (!handle)
            {
                state.SkipWithError("Failed to create a virtual character benchmark resource.");
                return;
            }
            characters.push_back(handle);
        }

        VirtualCharacterUpdateConfiguration updateConfiguration;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (const VirtualCharacterHandle handle : characters)
            {
                if (!system.UpdateVirtualCharacter(
                        worldHandle,
                        handle,
                        MatchedTimeStep,
                        updateConfiguration))
                {
                    state.SkipWithError("Virtual character benchmark update failed.");
                    return;
                }
            }
        }
        state.counters["Characters"] = characterCount;
        state.counters["Workers"] = 1;
    }

    void StepPhysicalCharacters(
        benchmark::State& state)
    {
        const AZ::u32 characterCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, characterCount + 16),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!system || !worldHandle)
        {
            state.SkipWithError("Failed to create the physical-character benchmark world.");
            return;
        }

        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(256.0f, 256.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(
            worldHandle,
            floorShapeConfiguration);
        const BodyHandle floorBodyHandle = CreateBody(
            system,
            worldHandle,
            floorShapeHandle,
            MotionType::Static,
            AZ::Vector3(0.0f, 0.0f, -0.5f));

        ShapeConfiguration characterShapeConfiguration;
        characterShapeConfiguration.m_geometry = CapsuleShapeConfiguration{
            .m_cylinderHeight = 1.0f,
            .m_radius = 0.35f,
        };
        const ShapeHandle characterShapeHandle = system.CreateShape(
            worldHandle,
            characterShapeConfiguration);
        if (!floorShapeHandle || !floorBodyHandle || !characterShapeHandle)
        {
            state.SkipWithError("Failed to prepare physical-character benchmark shapes.");
            return;
        }

        AZStd::vector<CharacterHandle> characterHandles;
        characterHandles.reserve(characterCount);
        for (AZ::u32 characterIndex = 0; characterIndex < characterCount; ++characterIndex)
        {
            CharacterConfiguration configuration;
            configuration.m_shapeHandle = characterShapeHandle;
            configuration.m_transform.m_position = WorldPosition(
                aznumeric_cast<double>(characterIndex % 16) * 3.0,
                aznumeric_cast<double>(characterIndex / 16) * 3.0,
                1.0);
            const CharacterHandle characterHandle = system.CreateCharacter(
                worldHandle,
                configuration);
            if (!characterHandle)
            {
                state.SkipWithError("Failed to create a physical benchmark character.");
                return;
            }
            characterHandles.push_back(characterHandle);
        }

        for (AZ::u32 frameIndex = 0; frameIndex < WarmupFrameCount; ++frameIndex)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Failed to warm the physical-character benchmark.");
                return;
            }
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            for (const CharacterHandle characterHandle : characterHandles)
            {
                qualityValid = system.SetCharacterVelocity(
                    worldHandle,
                    characterHandle,
                    AZ::Vector3::CreateAxisX(2.0f))
                    && qualityValid;
            }
            qualityValid = system.StepWorld(worldHandle, MatchedTimeStep)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        jobContext.AddCounters(state);
        state.counters["Characters"] = characterCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * characterCount);
    }

    void QueryRetainedShapePair(
        benchmark::State& state)
    {
        const bool castShape = state.range(0) != 0;
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = CreateSphere(system, worldHandle, 0.5f);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to prepare the retained-pair benchmark world.");
            return;
        }

        WorldTransform firstTransform;
        WorldTransform secondTransform;
        secondTransform.m_position = WorldPosition(0.75, 0.0, 0.0);
        if (castShape)
        {
            firstTransform.m_position = WorldPosition(-2.0, 0.0, 0.0);
            secondTransform.m_position = WorldPosition();
        }

        TransformedShape firstShape;
        TransformedShape secondShape;
        if (!system.RetainShape(
                worldHandle,
                shapeHandle,
                firstTransform,
                1.0f,
                firstShape)
            || !system.RetainShape(
                worldHandle,
                shapeHandle,
                secondTransform,
                1.0f,
                secondShape))
        {
            state.SkipWithError("Failed to retain benchmark shapes.");
            return;
        }

        bool qualityValid = true;
        AZStd::array<TransformedShapeCollisionHit, 8> collisionHits;
        AZStd::array<TransformedShapeCastHit, 8> castHits;
        if (castShape)
        {
            TransformedShapeCastRequest request;
            request.m_displacement = AZ::Vector3::CreateAxisX(4.0f);
            for ([[maybe_unused]] auto iteration : state)
            {
                const QueryResult result = system.CastTransformedShape(
                    worldHandle,
                    firstShape,
                    secondShape,
                    request,
                    castHits,
                    {});
                qualityValid = result.IsComplete()
                    && result.m_hitCount > 0
                    && qualityValid;
                benchmark::DoNotOptimize(castHits);
            }
        }
        else
        {
            TransformedShapeCollisionRequest request;
            for ([[maybe_unused]] auto iteration : state)
            {
                const QueryResult result = system.CollideTransformedShapes(
                    worldHandle,
                    firstShape,
                    secondShape,
                    request,
                    collisionHits,
                    {});
                qualityValid = result.IsComplete()
                    && result.m_hitCount > 0
                    && qualityValid;
                benchmark::DoNotOptimize(collisionHits);
            }
        }

        jobContext.AddCounters(state);
        state.counters["Cast"] = 0;
        if (castShape)
        {
            state.counters["Cast"] = 1;
        }
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations());
    }

    void MapSkeletonPose(
        benchmark::State& state)
    {
        const AZ::u32 jointCount = aznumeric_cast<AZ::u32>(state.range(0));
        SystemConfiguration configuration;
        configuration.m_createDefaultWorld = false;
        Runtime system(configuration, nullptr);

        SkeletonDefinitionConfiguration sourceConfiguration;
        SkeletonDefinitionConfiguration targetConfiguration;
        SkeletonMapperConfiguration mapperConfiguration;
        sourceConfiguration.m_joints.reserve(jointCount);
        targetConfiguration.m_joints.reserve(jointCount);
        mapperConfiguration.m_sourceNeutralModelTransforms.reserve(jointCount);
        mapperConfiguration.m_targetNeutralModelTransforms.reserve(jointCount);
        mapperConfiguration.m_jointMappings.reserve(jointCount);
        for (AZ::u32 jointIndex = 0; jointIndex < jointCount; ++jointIndex)
        {
            AZ::s32 parentIndex = -1;
            if (jointIndex > 0)
            {
                parentIndex = aznumeric_cast<AZ::s32>(jointIndex - 1);
            }
            const AZ::Name jointName(AZStd::string::format("joint_%u", jointIndex));
            sourceConfiguration.m_joints.push_back({
                .m_name = jointName,
                .m_parentIndex = parentIndex,
            });
            targetConfiguration.m_joints.push_back({
                .m_name = jointName,
                .m_parentIndex = parentIndex,
            });
            const AZ::Transform neutralTransform = AZ::Transform::CreateTranslation(
                AZ::Vector3::CreateAxisZ(aznumeric_cast<float>(jointIndex)));
            mapperConfiguration.m_sourceNeutralModelTransforms.push_back(neutralTransform);
            mapperConfiguration.m_targetNeutralModelTransforms.push_back(neutralTransform);
            mapperConfiguration.m_jointMappings.push_back({
                .m_sourceJoint = jointIndex,
                .m_targetJoint = jointIndex,
            });
        }

        const SkeletonDefinitionHandle sourceHandle = system.CreateSkeletonDefinition(
            sourceConfiguration);
        const SkeletonDefinitionHandle targetHandle = system.CreateSkeletonDefinition(
            targetConfiguration);
        mapperConfiguration.m_sourceSkeletonHandle = sourceHandle;
        mapperConfiguration.m_targetSkeletonHandle = targetHandle;
        const SkeletonMapperHandle mapperHandle = system.CreateSkeletonMapper(
            mapperConfiguration);
        if (!sourceHandle || !targetHandle || !mapperHandle)
        {
            state.SkipWithError("Failed to create the skeleton-mapping benchmark.");
            return;
        }

        AZStd::vector<AZ::Transform> sourceModelTransforms =
            mapperConfiguration.m_sourceNeutralModelTransforms;
        AZStd::vector<AZ::Transform> targetLocalTransforms(jointCount);
        AZStd::vector<AZ::Transform> targetModelTransforms(jointCount);
        for (AZ::Transform& transform : targetLocalTransforms)
        {
            transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ());
        }

        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            qualityValid = system.MapSkeletonPose(
                mapperHandle,
                sourceModelTransforms,
                targetLocalTransforms,
                targetModelTransforms)
                && qualityValid;
            benchmark::DoNotOptimize(targetModelTransforms);
        }

        state.counters["Joints"] = jointCount;
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = 1;
        state.SetItemsProcessed(state.iterations() * jointCount);
    }

    void UpdateHeightfield(
        benchmark::State& state)
    {
        const AZ::u32 sampleCount = aznumeric_cast<AZ::u32>(state.range(0));
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        HeightfieldShapeConfiguration heightfieldConfiguration;
        heightfieldConfiguration.m_sampleCount = sampleCount;
        heightfieldConfiguration.m_blockSize = 4;
        heightfieldConfiguration.m_heights.resize(
            aznumeric_cast<size_t>(sampleCount) * sampleCount,
            0.0f);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = heightfieldConfiguration;
        const ShapeHandle shapeHandle = system.CreateShape(
            worldHandle,
            shapeConfiguration);
        if (!system || !worldHandle || !shapeHandle)
        {
            state.SkipWithError("Failed to create the heightfield-update benchmark.");
            return;
        }

        HeightfieldRegion region{
            .m_columnCount = sampleCount,
            .m_rowCount = sampleCount,
        };
        AZStd::vector<float> heights(
            aznumeric_cast<size_t>(sampleCount) * sampleCount,
            1.0f);
        bool qualityValid = true;
        for ([[maybe_unused]] auto iteration : state)
        {
            qualityValid = system.UpdateHeightfieldHeights(
                worldHandle,
                shapeHandle,
                region,
                heights)
                && qualityValid;
            benchmark::DoNotOptimize(qualityValid);
        }

        jobContext.AddCounters(state);
        state.counters["QualityValid"] = 0;
        if (qualityValid)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Samples"] = aznumeric_cast<double>(heights.size());
        state.counters["Workers"] = workerCount;
        state.SetItemsProcessed(state.iterations() * heights.size());
    }

    void StepWheeledVehicles(
        benchmark::State& state)
    {
        const AZ::u32 vehicleCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 controllerCategory = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContext(workerCount);
        Runtime system(CreateSystemConfiguration(workerCount, vehicleCount + 32), &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle floorShape = CreateBox(system, worldHandle, AZ::Vector3(128.0f, 128.0f, 1.0f));
        const ShapeHandle chassisShape = CreateBox(system, worldHandle, AZ::Vector3(1.8f, 4.0f, 0.6f));
        const BodyHandle floorBody = CreateBody(
            system,
            worldHandle,
            floorShape,
            MotionType::Static,
            AZ::Vector3(0.0f, 0.0f, -0.5f));
        if (!floorBody || !chassisShape)
        {
            state.SkipWithError("Failed to create the vehicle benchmark floor.");
            return;
        }

        for (AZ::u32 vehicleIndex = 0; vehicleIndex < vehicleCount; ++vehicleIndex)
        {
            const BodyHandle chassisBody = CreateBody(
                system,
                worldHandle,
                chassisShape,
                MotionType::Dynamic,
                AZ::Vector3(
                    4.0f * aznumeric_cast<float>(vehicleIndex % 8),
                    6.0f * aznumeric_cast<float>(vehicleIndex / 8),
                    0.8f));
            WheeledVehicleConfiguration wheeledConfiguration;
            wheeledConfiguration.m_bodyHandle = chassisBody;
            wheeledConfiguration.m_wheels = {
                {.m_position = AZ::Vector3(-0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                {.m_position = AZ::Vector3(0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                {.m_position = AZ::Vector3(-0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
                {.m_position = AZ::Vector3(0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
            };
            wheeledConfiguration.m_differentials = {
                {.m_leftWheel = 0, .m_rightWheel = 1},
            };

            VehicleHandle vehicleHandle;
            if (controllerCategory == 0)
            {
                vehicleHandle = system.CreateWheeledVehicle(worldHandle, wheeledConfiguration);
            }
            else if (controllerCategory == 1)
            {
                MotorcycleConfiguration motorcycleConfiguration;
                motorcycleConfiguration.m_wheeled = wheeledConfiguration;
                motorcycleConfiguration.m_wheeled.m_antiRollBars.clear();
                motorcycleConfiguration.m_wheeled.m_wheels = {
                    {.m_position = AZ::Vector3(0.0f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                    {.m_position = AZ::Vector3(0.0f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
                };
                motorcycleConfiguration.m_wheeled.m_differentials = {
                    {.m_leftWheel = 0, .m_rightWheel = 1},
                };
                vehicleHandle = system.CreateMotorcycle(worldHandle, motorcycleConfiguration);
            }
            else if (controllerCategory == 2)
            {
                TrackedVehicleConfiguration trackedConfiguration;
                trackedConfiguration.m_bodyHandle = chassisBody;
                trackedConfiguration.m_wheels = {
                    {.m_common = {.m_position = AZ::Vector3(-0.8f, 1.2f, -0.25f)}},
                    {.m_common = {.m_position = AZ::Vector3(-0.8f, -1.2f, -0.25f)}},
                    {.m_common = {.m_position = AZ::Vector3(0.8f, 1.2f, -0.25f)}},
                    {.m_common = {.m_position = AZ::Vector3(0.8f, -1.2f, -0.25f)}},
                };
                trackedConfiguration.m_tracks[0].m_wheels = {0, 1};
                trackedConfiguration.m_tracks[0].m_drivenWheel = 0;
                trackedConfiguration.m_tracks[1].m_wheels = {2, 3};
                trackedConfiguration.m_tracks[1].m_drivenWheel = 2;
                vehicleHandle = system.CreateTrackedVehicle(worldHandle, trackedConfiguration);
            }
            if (!chassisBody || !vehicleHandle)
            {
                state.SkipWithError("Failed to create a vehicle benchmark resource.");
                return;
            }
        }

        for (AZ::u32 warmup = 0; warmup < 60; ++warmup)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Vehicle benchmark warmup failed.");
                return;
            }
        }
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Vehicle benchmark step failed.");
                break;
            }
        }
        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["ControllerCategory"] = controllerCategory;
        state.counters["Vehicles"] = vehicleCount;
        state.counters["Workers"] = workerCount;
    }

    void StepRagdolls(
        benchmark::State& state)
    {
        const AZ::u32 partCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        JobContextScope jobContext(workerCount);
        Runtime system(CreateSystemConfiguration(workerCount, partCount + 16), &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RagdollBenchmarkScenario scenario;
        if (!system || !CreateRagdollBenchmarkScenario(system, worldHandle, partCount, scenario))
        {
            state.SkipWithError("Failed to create the ragdoll step benchmark.");
            return;
        }

        for (AZ::u32 warmup = 0; warmup < 60; ++warmup)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Ragdoll benchmark warmup failed.");
                return;
            }
        }
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Ragdoll benchmark step failed.");
                break;
            }
        }
        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["Parts"] = partCount;
        state.counters["Workers"] = workerCount;
    }

    void OptimizeBroadPhase(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        Runtime system(CreateSystemConfiguration(1, bodyCount + 16), nullptr);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!CreateQueryGrid(system, worldHandle, bodyCount))
        {
            state.SkipWithError("Failed to create the broadphase benchmark grid.");
            return;
        }
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.OptimizeBroadPhase(worldHandle))
            {
                state.SkipWithError("Broadphase optimization failed.");
                break;
            }
        }
        state.counters["Bodies"] = bodyCount;
        state.counters["Workers"] = 1;
    }

    void InstantiateScenes(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        Runtime system(CreateSystemConfiguration(1, bodyCount + 16), nullptr);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const CookedShapeHandle cookedShapeHandle = system.CookShape(shapeConfiguration);
        SceneConfiguration sceneConfiguration;
        sceneConfiguration.m_bodies.reserve(bodyCount);
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            SceneRigidBodyConfiguration body;
            body.m_cookedShapeHandle = cookedShapeHandle;
            body.m_body.m_transform.m_position.m_z = 0.55 * static_cast<double>(bodyIndex);
            sceneConfiguration.m_bodies.push_back(body);
        }
        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(sceneConfiguration);
        if (!cookedShapeHandle || !definitionHandle)
        {
            state.SkipWithError("Failed to create the scene benchmark definition.");
            return;
        }

        for ([[maybe_unused]] auto iteration : state)
        {
            const SceneInstanceHandle instanceHandle = system.InstantiateScene(worldHandle, definitionHandle);
            if (!instanceHandle || !system.DestroySceneInstance(worldHandle, instanceHandle))
            {
                state.SkipWithError("Scene benchmark lifecycle failed.");
                break;
            }
        }
        state.counters["Bodies"] = bodyCount;
        state.counters["Workers"] = 1;
    }

    void ReadPerformanceStatistics(
        benchmark::State& state)
    {
        Runtime system(CreateSystemConfiguration(1, 1'040), nullptr);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        if (!CreateQueryGrid(system, worldHandle, 1'024)
            || !system.ConfigurePerformanceStatistics(
                worldHandle,
                PerformanceStatisticsFlags::Resources | PerformanceStatisticsFlags::Memory))
        {
            state.SkipWithError("Failed to create the statistics benchmark world.");
            return;
        }

        WorldPerformanceStatistics statistics;
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.GetPerformanceStatistics(worldHandle, statistics, false))
            {
                state.SkipWithError("Performance statistics read failed.");
                break;
            }
            benchmark::DoNotOptimize(statistics.m_wrapperRetainedBytes);
        }
        state.counters["Bodies"] = 1'024;
        state.counters["Workers"] = 1;
    }

    void AcquireRuntimeConfigurationCapability(
        benchmark::State& state)
    {
        constexpr AZ::u32 workerCount = 1;
        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, 1),
            &jobContext.Get());
        RuntimeConfiguration* expectedCapability = static_cast<RuntimeConfiguration*>(&system);
        if (!system || RuntimeConfiguration::Get() != expectedCapability)
        {
            state.SkipWithError("Failed to publish the runtime-configuration capability.");
            return;
        }

        for ([[maybe_unused]] auto iteration : state)
        {
            RuntimeConfiguration* capability = RuntimeConfiguration::Get();
            benchmark::DoNotOptimize(capability);
        }

        jobContext.AddCounters(state);
        state.counters["QualityValid"] = 0;
        if (RuntimeConfiguration::Get() == expectedCapability)
        {
            state.counters["QualityValid"] = 1;
        }
        state.counters["Workers"] = workerCount;
    }

    void StepCollisionPolicy(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        const AZ::u32 policy = aznumeric_cast<AZ::u32>(state.range(2));
        JobContextScope jobContext(workerCount);
        SystemConfiguration configuration = CreateSystemConfiguration(workerCount, bodyCount + 16);
        configuration.m_defaultWorld.m_collectActivationEvents = policy == 0 || policy == 3;
        configuration.m_defaultWorld.m_collectContactEvents = policy == 0 || policy == 1;
        configuration.m_defaultWorld.m_simulation.m_allowSleeping = policy == 3;
        Runtime system(configuration, &jobContext.Get());
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle floorShape = CreateBox(system, worldHandle, AZ::Vector3(64.0f, 64.0f, 1.0f));
        const ShapeHandle sphereShape = CreateSphere(system, worldHandle, 0.25f);
        const BodyHandle floorBody = CreateBody(
            system,
            worldHandle,
            floorShape,
            MotionType::Static,
            AZ::Vector3(0.0f, 0.0f, -0.5f));
        AZStd::vector<BodyHandle> bodies;
        bodies.reserve(bodyCount);
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_shapeHandle = sphereShape;
            bodyConfiguration.m_transform.m_position = {
                .m_x = 0.55 * static_cast<double>(bodyIndex % 16),
                .m_y = 0.55 * static_cast<double>((bodyIndex / 16) % 16),
                .m_z = 0.3 + 0.55 * static_cast<double>(bodyIndex / 256),
            };
            bodyConfiguration.m_isSensor = policy == 1;
            if (policy == 2)
            {
                bodyConfiguration.m_motionQuality = MotionQuality::Continuous;
                bodyConfiguration.m_linearVelocity = AZ::Vector3::CreateAxisZ(-100.0f);
            }
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
            if (!bodyHandle)
            {
                state.SkipWithError("Failed to create a collision-policy benchmark body.");
                return;
            }
            bodies.push_back(bodyHandle);
        }
        if (!floorBody)
        {
            state.SkipWithError("Failed to create the collision-policy benchmark floor.");
            return;
        }

        for ([[maybe_unused]] auto iteration : state)
        {
            if (policy == 3
                && (!system.DeactivateBodies(worldHandle, bodies)
                    || !system.ActivateBodies(worldHandle, bodies)))
            {
                state.SkipWithError("Sleep/wake benchmark activation failed.");
                break;
            }
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Collision-policy benchmark step failed.");
                break;
            }
            const EventBatch events = system.GetEvents(worldHandle);
            benchmark::DoNotOptimize(events.GetContacts().size());
        }
        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["Bodies"] = bodyCount;
        state.counters["Policy"] = policy;
        state.counters["Workers"] = workerCount;
    }

    void StepCustomShapes(
        benchmark::State& state)
    {
        const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
        const AZ::u32 workerCount = aznumeric_cast<AZ::u32>(state.range(1));
        AZStd::shared_ptr<AZ::DynamicModuleHandle> providerModule =
            AZ::DynamicModuleHandle::Create("Jolt.TestProviders");
        if (!providerModule
            || !providerModule->Load(AZ::DynamicModuleHandle::LoadFlags::InitFuncRequired))
        {
            state.SkipWithError("Failed to load the custom shape benchmark provider.");
            return;
        }
        const auto getProvider = providerModule->GetFunction<Tests::GetCustomShapeProviderFunction>(
            Tests::GetCustomShapeProviderFunctionName);
        ICustomShapeProvider* provider = nullptr;
        if (getProvider)
        {
            provider = getProvider();
        }
        if (!provider)
        {
            state.SkipWithError("The custom shape benchmark provider is unavailable.");
            return;
        }

        JobContextScope jobContext(workerCount);
        Runtime system(
            CreateSystemConfiguration(workerCount, bodyCount + 16),
            &jobContext.Get(),
            SystemRegistration::Isolated);
        if (!system)
        {
            state.SkipWithError("Failed to register the custom shape benchmark provider.");
            return;
        }
        const ExtensionRegistrationResult providerRegistration =
            system.RegisterExtension(provider, ExtensionHostLease(providerModule));
        if (!providerRegistration)
        {
            state.SkipWithError("Failed to register the custom shape benchmark provider.");
            return;
        }
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration customConfiguration;
        customConfiguration.m_geometry = CustomShapeConfiguration{
            .m_data = {4},
            .m_providerId = provider->GetId(),
        };
        const CookedShapeHandle cookedCustomShape = system.CookShape(customConfiguration);
        const ShapeHandle customShape = system.CreateShape(worldHandle, cookedCustomShape);
        const ShapeHandle floorShape = CreateBox(system, worldHandle, AZ::Vector3(64.0f, 64.0f, 1.0f));
        const BodyHandle floorBody = CreateBody(
            system,
            worldHandle,
            floorShape,
            MotionType::Static,
            AZ::Vector3(0.0f, 0.0f, -0.5f));
        if (!cookedCustomShape || !customShape || !floorBody)
        {
            state.SkipWithError("Failed to create the custom shape benchmark scene.");
            return;
        }

        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            if (!CreateBody(
                    system,
                    worldHandle,
                    customShape,
                    MotionType::Dynamic,
                    AZ::Vector3(
                        0.55f * aznumeric_cast<float>(bodyIndex % 16),
                        0.55f * aznumeric_cast<float>((bodyIndex / 16) % 16),
                        0.3f + 0.55f * aznumeric_cast<float>(bodyIndex / 256))))
            {
                state.SkipWithError("Failed to create a custom shape benchmark body.");
                return;
            }
        }
        for (AZ::u32 warmup = 0; warmup < 60; ++warmup)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Custom shape benchmark warmup failed.");
                return;
            }
        }
        for ([[maybe_unused]] auto iteration : state)
        {
            if (!system.StepWorld(worldHandle, MatchedTimeStep))
            {
                state.SkipWithError("Custom shape benchmark step failed.");
                break;
            }
        }
        AddWorldCounters(state, system, worldHandle);
        jobContext.AddCounters(state);
        state.counters["Bodies"] = bodyCount;
        state.counters["Workers"] = workerCount;
    }

    BENCHMARK(StepSettledBoxes)
        ->Name("Jolt/Step/SettledBoxes")
        ->Args({128, 1})
        ->Args({128, 4})
        ->Args({128, 8})
        ->Args({1024, 1})
        ->Args({1024, 4})
        ->Args({1024, 8})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(ValidationFrameCount);

    BENCHMARK(StepSettledBoxesTail)
        ->Name("Jolt/Tail/Step/SettledBoxes")
        ->Args({128, 1})
        ->Args({128, 4})
        ->Args({128, 8})
        ->Args({1024, 1})
        ->Args({1024, 4})
        ->Args({1024, 8})
        ->Unit(benchmark::kMicrosecond)
        ->UseManualTime()
        ->Iterations(1);

    BENCHMARK(StepSettledBoxesDefaultQuality)
        ->Name("Jolt/Diagnostic/Step/SettledBoxesDefaultQuality")
        ->Args({1024, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(ValidationFrameCount);

    BENCHMARK(StepAutomaticWorlds)
        ->Name("Jolt/Diagnostic/Step/AutomaticWorlds")
        ->Args({1, 128, 1})
        ->Args({2, 128, 1})
        ->Args({4, 128, 1})
        ->Args({1, 1024, 1})
        ->Args({2, 1024, 1})
        ->Args({4, 1024, 1})
        ->ArgNames({"Worlds", "BodiesPerWorld", "WorkersPerWorld"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(ValidationFrameCount);

    BENCHMARK(CreateDestroyBodies)
        ->Name("Jolt/Lifecycle/CreateDestroyBodies")
        ->Args({128, 1})
        ->Args({1024, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ChangeRagdollMembership)
        ->Name("Jolt/Lifecycle/ChangeRagdollMembership")
        ->Arg(64)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RecreateRagdoll)
        ->Name("Jolt/Lifecycle/RecreateRagdoll")
        ->Arg(64)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(CreateSoftBodyDefinition)
        ->Name("Jolt/Lifecycle/CreateSoftBodyDefinition")
        ->Arg(32)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ImportSoftBodyDefinition)
        ->Name("Jolt/Lifecycle/ImportSoftBodyDefinition")
        ->Arg(32)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(BenchmarkFilteredRollbackState)
        ->Name("Jolt/Rollback/RecaptureFilteredState")
        ->Args({128, 0})
        ->Args({1024, 0})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(BenchmarkFilteredRollbackState)
        ->Name("Jolt/Rollback/RestoreFilteredStateTransactional")
        ->Args({128, 1})
        ->Args({1024, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(BenchmarkFilteredRollbackState)
        ->Name("Jolt/Rollback/RestoreFilteredStateValidated")
        ->Args({128, 2})
        ->Args({1024, 2})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(CreateDestroyCookedCompounds)
        ->Name("Jolt/Lifecycle/CreateDestroyCookedCompounds")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ChangeBodyMembershipIndividually)
        ->Name("Jolt/Lifecycle/ChangeBodyMembershipIndividually")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ReadBodyVelocities)
        ->Name("Jolt/Diagnostic/ReadBodyVelocities")
        ->Args({1024, 0})
        ->Args({1024, 1})
        ->ArgNames({"Reads", "Combined"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ChangeBodyMembershipInBulk)
        ->Name("Jolt/Lifecycle/ChangeBodyMembershipInBulk")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(DestroyBodiesIndividually)
        ->Name("Jolt/Lifecycle/DestroyBodiesIndividually")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(DestroyBodiesInBulk)
        ->Name("Jolt/Lifecycle/DestroyBodiesInBulk")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ChangeConstraintMembershipIndividually)
        ->Name("Jolt/Lifecycle/ChangeConstraintMembershipIndividually")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ChangeConstraintMembershipInBulk)
        ->Name("Jolt/Lifecycle/ChangeConstraintMembershipInBulk")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(DestroyConstraintsIndividually)
        ->Name("Jolt/Lifecycle/DestroyConstraintsIndividually")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(DestroyConstraintsInBulk)
        ->Name("Jolt/Lifecycle/DestroyConstraintsInBulk")
        ->Args({128})
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastGrid)
        ->Name("Jolt/Query/RaycastGrid")
        ->Args({1024, 128, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastEmptyWorld)
        ->Name("Jolt/Diagnostic/RaycastEmptyWorld")
        ->Args({128})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(QueryFloatScopeOverhead)
        ->Name("Jolt/Diagnostic/QueryFloatScopeOverhead")
        ->Unit(benchmark::kNanosecond)
        ->UseRealTime();

    BENCHMARK(QueryLockOverhead)
        ->Name("Jolt/Diagnostic/QueryLockOverhead")
        ->Unit(benchmark::kNanosecond)
        ->UseRealTime();

    BENCHMARK(RaycastBroadPhaseGrid)
        ->Name("Jolt/Diagnostic/RaycastBroadPhaseGrid")
        ->Args({1024, 128})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(RaycastClosestBatchGrid)
        ->Name("Jolt/Query/RaycastClosestBatchGrid")
        ->Args({1024, 1024, 1})
        ->Args({1024, 128, 4})
        ->Args({1024, 1024, 4})
        ->Args({1024, 1024, 8})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(OverlapSphereGrid)
        ->Name("Jolt/Query/OverlapSphereGrid")
        ->Args({1024, 1, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(OverlapSphereGridCountOnly)
        ->Name("Jolt/Diagnostic/OverlapSphereGridCountOnly")
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(OverlapSphereGridBroadPhase)
        ->Name("Jolt/Diagnostic/OverlapSphereGridBroadPhase")
        ->Args({1024})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(UpdateHair)
        ->Name("Jolt/Hair/Update")
        ->Args({256, 16, 32, 1})
        ->Args({256, 16, 32, 4})
        ->Args({1024, 16, 32, 1})
        ->Args({1024, 16, 32, 4})
        ->Args({1024, 16, 32, 8})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ReadBackHair)
        ->Name("Jolt/Hair/Readback")
        ->Args({256, 16, 32})
        ->Args({1024, 16, 32})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(StepConstraintCategory)
        ->Name("Jolt/Constraint/StepCategory")
        ->Args({128, 1, 0})
        ->Args({128, 1, 1})
        ->Args({128, 1, 2})
        ->Args({128, 1, 3})
        ->Args({128, 1, 4})
        ->Args({128, 4, 4})
        ->Args({128, 8, 4})
        ->Args({128, 1, 5})
        ->Args({128, 1, 6})
        ->Args({128, 1, 7})
        ->ArgNames({"Constraints", "Workers", "Category"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(StepSoftBodies)
        ->Name("Jolt/SoftBody/Step")
        ->Args({16, 1})
        ->Args({16, 4})
        ->Args({16, 8})
        ->ArgNames({"Bodies", "Workers"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(StepCustomShapes)
        ->Name("Jolt/CustomShape/Step")
        ->Args({128, 1})
        ->Args({128, 4})
        ->Args({128, 8})
        ->ArgNames({"Bodies", "Workers"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(UpdateVirtualCharacters)
        ->Name("Jolt/Character/VirtualUpdate")
        ->Arg(128)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(StepPhysicalCharacters)
        ->Name("Jolt/Character/PhysicalStep")
        ->Args({128, 1})
        ->Args({128, 4})
        ->Args({128, 8})
        ->ArgNames({"Characters", "Workers"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(QueryRetainedShapePair)
        ->Name("Jolt/Query/RetainedShapePair")
        ->Arg(0)
        ->Arg(1)
        ->ArgNames({"Cast"})
        ->Unit(benchmark::kNanosecond)
        ->UseRealTime();

    BENCHMARK(MapSkeletonPose)
        ->Name("Jolt/Skeleton/MapPose")
        ->Arg(64)
        ->Arg(256)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(UpdateHeightfield)
        ->Name("Jolt/Shape/UpdateHeightfield")
        ->Arg(64)
        ->Arg(128)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(StepWheeledVehicles)
        ->Name("Jolt/Vehicle/StepController")
        ->Args({32, 1, 0})
        ->Args({32, 4, 0})
        ->Args({32, 8, 0})
        ->Args({32, 1, 1})
        ->Args({32, 1, 2})
        ->ArgNames({"Vehicles", "Workers", "Controller"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(VehicleBenchmarkFrameCount);

    BENCHMARK(StepRagdolls)
        ->Name("Jolt/Ragdoll/Step")
        ->Args({64, 1})
        ->Args({64, 4})
        ->Args({64, 8})
        ->ArgNames({"Parts", "Workers"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(StepCollisionPolicy)
        ->Name("Jolt/Simulation/CollisionPolicy")
        ->Args({128, 1, 0})
        ->Args({128, 4, 0})
        ->Args({128, 8, 0})
        ->Args({128, 1, 1})
        ->Args({128, 1, 2})
        ->Args({128, 1, 3})
        ->ArgNames({"Bodies", "Workers", "Policy"})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime()
        ->Iterations(StatefulBenchmarkFrameCount);

    BENCHMARK(OptimizeBroadPhase)
        ->Name("Jolt/BroadPhase/Optimize")
        ->Arg(1'024)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(InstantiateScenes)
        ->Name("Jolt/Scene/InstantiateDestroy")
        ->Arg(128)
        ->Arg(1'024)
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(ReadPerformanceStatistics)
        ->Name("Jolt/Diagnostic/ReadPerformanceStatistics")
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(CreateDestroyBodiesInstrumented)
        ->Name("Jolt/Diagnostic/Lifecycle/CreateDestroyBodiesInstrumented")
        ->Args({128, 1})
        ->Args({1024, 1})
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    BENCHMARK(AcquireRuntimeConfigurationCapability)
        ->Name("Jolt/Diagnostic/AcquireRuntimeConfigurationCapability")
        ->Unit(benchmark::kNanosecond)
        ->UseRealTime();
} // namespace Jolt::Benchmarks

#endif // HAVE_BENCHMARK
