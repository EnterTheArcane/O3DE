/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#ifdef HAVE_BENCHMARK

#include <Benchmarks/PhysXBenchmarksCommon.h>
#include <PhysX/Material/PhysXMaterialConfiguration.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/string.h>
#include <AzTest/Benchmark/BenchmarkEnvironment.h>
#include <AzFramework/Physics/Common/PhysicsSceneQueries.h>
#include <AzFramework/Physics/Common/PhysicsSimulatedBody.h>
#include <AzFramework/Physics/Common/PhysicsTypes.h>
#include <AzFramework/Physics/Configuration/RigidBodyConfiguration.h>
#include <AzFramework/Physics/Configuration/SceneConfiguration.h>
#include <AzFramework/Physics/Configuration/StaticRigidBodyConfiguration.h>
#include <AzFramework/Physics/PhysicsScene.h>
#include <AzFramework/Physics/Shape.h>
#include <AzFramework/Physics/ShapeConfiguration.h>

namespace PhysX::Benchmarks
{
    namespace
    {
        constexpr float MatchedTimeStep = 1.0f / 60.0f;
        constexpr float MatchedGridSpacing = 1.1f;
        constexpr size_t TailSampleCount = 8192;
        constexpr AZ::u32 WarmupFrameCount = 600;
        constexpr AZ::u32 ValidationFrameCount = 600;

        [[nodiscard]] bool IsValid(AzPhysics::SimulatedBodyHandle handle)
        {
            return handle != AzPhysics::InvalidSimulatedBodyHandle;
        }

        [[nodiscard]] AzPhysics::SceneConfiguration CreateDeterministicSceneConfiguration()
        {
            AzPhysics::SceneConfiguration configuration = AzPhysics::SceneConfiguration::CreateDefault();
            configuration.m_enableCcd = false;
            configuration.m_enableActiveActors = false;
            configuration.m_enableEnhancedDeterminism = true;
            return configuration;
        }

        class JobContextScope final
        {
        public:
            explicit JobContextScope(AZ::u32 workerCount)
                : m_cpuAffinity(AZ::Test::GetBenchmarkCpuAffinity())
                , m_previousJobContext(AZ::JobContext::GetGlobalContext())
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
                AZ::JobContext::SetGlobalContext(nullptr);
                AZ::JobContext::SetGlobalContext(&m_jobContext);
            }

            ~JobContextScope()
            {
                AZ::JobContext::SetGlobalContext(nullptr);
                AZ::JobContext::SetGlobalContext(m_previousJobContext);
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
            AZ::JobContext* m_previousJobContext = nullptr;
            AZ::JobManager m_jobManager;
            AZ::JobContext m_jobContext;
        };

        class SettledBoxesFixture
            : public PhysXBaseBenchmarkFixture
        {
        public:
            void SetUp(const benchmark::State& state) override
            {
                SetUpWorld(state);
            }

            void SetUp(benchmark::State& state) override
            {
                SetUpWorld(state);
            }

            void TearDown(const benchmark::State&) override
            {
                TearDownInternal();
                m_jobContext.reset();
            }

            void TearDown(benchmark::State&) override
            {
                TearDownInternal();
                m_jobContext.reset();
            }

        protected:
            template<bool CaptureTailSamples>
            void RunStepBenchmark(
                benchmark::State& state);

            AzPhysics::SceneConfiguration GetDefaultSceneConfiguration() override
            {
                return CreateDeterministicSceneConfiguration();
            }

            void SetUpWorld(const benchmark::State& state)
            {
                m_maximumDisplacement = 0.0f;
                m_minimumHeight = AZStd::numeric_limits<float>::max();
                m_qualityValid = false;
                m_warmupCompleted = false;
                m_warmupTickCount = 0;
                m_workerCount = aznumeric_cast<AZ::u32>(state.range(1));
                m_jobContext = AZStd::make_unique<JobContextScope>(m_workerCount);
                SetUpInternal();
                const auto collider = AZStd::make_shared<Physics::ColliderConfiguration>();
                PhysX::MaterialConfiguration materialConfiguration;
                materialConfiguration.m_dynamicFriction = 0.0f;
                materialConfiguration.m_staticFriction = 0.0f;
                materialConfiguration.m_restitution = 0.0f;
                collider->m_materialSlots.SetSlots(Physics::MaterialDefaultSlot::Default);
                collider->m_materialSlots.SetMaterialAsset(
                    0,
                    materialConfiguration.CreateMaterialAsset());

                AzPhysics::StaticRigidBodyConfiguration groundConfiguration;
                groundConfiguration.m_position = AZ::Vector3(0.0f, 0.0f, -0.5f);
                groundConfiguration.m_colliderAndShapeData = AzPhysics::ShapeColliderPair(
                    collider, AZStd::make_shared<Physics::BoxShapeConfiguration>(AZ::Vector3(128.0f, 128.0f, 1.0f)));
                m_ready = IsValid(m_defaultScene->AddSimulatedBody(&groundConfiguration));

                const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
                if (bodyCount > m_bodies.size())
                {
                    m_ready = false;
                    return;
                }
                m_bodyCount = bodyCount;
                const auto box = AZStd::make_shared<Physics::BoxShapeConfiguration>(AZ::Vector3::CreateOne());
                AzPhysics::RigidBodyConfiguration bodyConfiguration;
                bodyConfiguration.m_linearDamping = 1.0f;
                bodyConfiguration.m_angularDamping = 1.0f;
                bodyConfiguration.m_sleepMinEnergy = 0.0f;
                bodyConfiguration.m_colliderAndShapeData = AzPhysics::ShapeColliderPair(collider, box);

                AZ::u32 rowWidth = 16;
                if (bodyCount > rowWidth * rowWidth)
                {
                    rowWidth = 32;
                }
                m_rowWidth = rowWidth;
                for (AZ::u32 bodyIndex = 0; m_ready && bodyIndex < bodyCount; ++bodyIndex)
                {
                    const AZ::u32 layerIndex = bodyIndex / (rowWidth * rowWidth);
                    const AZ::u32 rowIndex = (bodyIndex / rowWidth) % rowWidth;
                    const AZ::u32 columnIndex = bodyIndex % rowWidth;
                    bodyConfiguration.m_position = AZ::Vector3(
                        MatchedGridSpacing
                            * (static_cast<float>(columnIndex) - 0.5f * static_cast<float>(rowWidth)),
                        MatchedGridSpacing
                            * (static_cast<float>(rowIndex) - 0.5f * static_cast<float>(rowWidth)),
                        0.6f + static_cast<float>(layerIndex) * MatchedGridSpacing);
                    const AzPhysics::SimulatedBodyHandle bodyHandle = m_defaultScene->AddSimulatedBody(&bodyConfiguration);
                    m_ready = IsValid(bodyHandle);
                    m_bodies[bodyIndex] = bodyHandle;
                }

                for (AZ::u32 warmupTick = 0; m_ready && warmupTick < WarmupFrameCount; ++warmupTick)
                {
                    StepScene1Tick(MatchedTimeStep);
                }
                m_warmupTickCount = WarmupFrameCount;
                m_warmupCompleted = m_ready;
            }

            static constexpr size_t MaximumMatchedBodyCount = 1024;

            AZStd::array<AzPhysics::SimulatedBodyHandle, MaximumMatchedBodyCount> m_bodies;
            bool m_ready = false;
            bool m_qualityValid = false;
            bool m_warmupCompleted = false;
            float m_maximumDisplacement = 0.0f;
            float m_minimumHeight = AZStd::numeric_limits<float>::max();
            AZ::u32 m_workerCount = 1;
            AZ::u32 m_warmupTickCount = 0;
            AZ::u32 m_rowWidth = 16;
            size_t m_bodyCount = 0;
            AZStd::unique_ptr<JobContextScope> m_jobContext;
        };

        template<bool CaptureTailSamples>
        void SettledBoxesFixture::RunStepBenchmark(
            benchmark::State& state)
        {
            if (!m_ready)
            {
                state.SkipWithError("Failed to create the matched PhysX sphere workload.");
                return;
            }

            AZStd::vector<AZ::Vector3> positions;
            positions.reserve(m_bodyCount);
            bool statesValid = true;
            for (size_t bodyIndex = 0; bodyIndex < m_bodyCount; ++bodyIndex)
            {
                const AzPhysics::SimulatedBodyHandle bodyHandle = m_bodies[bodyIndex];
                const AzPhysics::SimulatedBody* body = m_defaultScene->GetSimulatedBodyFromHandle(bodyHandle);
                if (!body)
                {
                    positions.push_back(AZ::Vector3::CreateZero());
                    statesValid = false;
                    continue;
                }
                positions.push_back(body->GetTransform().GetTranslation());
            }

            AZ::u64 validationFrameCount = ValidationFrameCount;
            if constexpr (CaptureTailSamples)
            {
                AZStd::array<double, TailSampleCount> tailSamples;
                for ([[maybe_unused]] auto iteration : state)
                {
                    for (double& sample : tailSamples)
                    {
                        const auto start = AZStd::chrono::steady_clock::now();
                        m_defaultScene->StartSimulation(MatchedTimeStep);
                        m_defaultScene->FinishSimulation();
                        const auto end = AZStd::chrono::steady_clock::now();
                        sample = AZStd::chrono::duration<double, AZStd::nano>(end - start).count();
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
                    m_defaultScene->StartSimulation(MatchedTimeStep);
                    m_defaultScene->FinishSimulation();
                }
            }

            m_maximumDisplacement = 0.0f;
            m_minimumHeight = AZStd::numeric_limits<float>::max();
            for (size_t bodyIndex = 0; bodyIndex < m_bodyCount; ++bodyIndex)
            {
                const AzPhysics::SimulatedBody* body =
                    m_defaultScene->GetSimulatedBodyFromHandle(m_bodies[bodyIndex]);
                if (!body)
                {
                    statesValid = false;
                    continue;
                }

                const AZ::Vector3 position = body->GetTransform().GetTranslation();
                statesValid = position.IsFinite() && positions[bodyIndex].IsFinite() && statesValid;
                m_minimumHeight = AZStd::min(m_minimumHeight, position.GetZ());
                m_maximumDisplacement = AZStd::max(
                    m_maximumDisplacement,
                    position.GetDistance(positions[bodyIndex]));
            }
            m_qualityValid = m_warmupCompleted
                && statesValid
                && m_minimumHeight >= 0.45f
                && m_maximumDisplacement <= 0.02f;

            const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
            state.SetItemsProcessed(validationFrameCount * bodyCount);
            state.counters["AngularDamping"] = 1;
            state.counters["Ccd"] = 0;
            state.counters["DynamicBodies"] = bodyCount;
            state.counters["Friction"] = 0;
            state.counters["Layers"] = (bodyCount - 1) / (m_rowWidth * m_rowWidth) + 1;
            state.counters["LinearDamping"] = 1;
            state.counters["MaximumDisplacement"] = m_maximumDisplacement;
            state.counters["MinimumHeight"] = m_minimumHeight;
            state.counters["Notifications"] = 0;
            state.counters["QualityValid"] = 0;
            if (m_qualityValid)
            {
                state.counters["QualityValid"] = 1;
            }
            state.counters["Sleep"] = 0;
            state.counters["GridSpacingMillimeters"] = 1100;
            state.counters["RowWidth"] = m_rowWidth;
            state.counters["Restitution"] = 0;
            state.counters["ValidationFrames"] = aznumeric_cast<double>(validationFrameCount);
            state.counters["WarmupTicks"] = m_warmupTickCount;
            state.counters["WarmupCompleted"] = 0;
            if (m_warmupCompleted)
            {
                state.counters["WarmupCompleted"] = 1;
            }
            m_jobContext->AddCounters(state);
            state.counters["Workers"] = m_workerCount;
        }

        BENCHMARK_DEFINE_F(SettledBoxesFixture, Step)(benchmark::State& state)
        {
            RunStepBenchmark<false>(state);
        }

        BENCHMARK_DEFINE_F(SettledBoxesFixture, StepTail)(benchmark::State& state)
        {
            RunStepBenchmark<true>(state);
        }

        BENCHMARK_REGISTER_F(SettledBoxesFixture, Step)
            ->Name("PhysX/Step/SettledBoxes")
            ->Args({ 128, 1 })
            ->Args({ 128, 4 })
            ->Args({ 128, 8 })
            ->Args({ 1024, 1 })
            ->Args({ 1024, 4 })
            ->Args({ 1024, 8 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime()
            ->Iterations(ValidationFrameCount);

        BENCHMARK_REGISTER_F(SettledBoxesFixture, StepTail)
            ->Name("PhysX/Tail/Step/SettledBoxes")
            ->Args({ 128, 1 })
            ->Args({ 128, 4 })
            ->Args({ 128, 8 })
            ->Args({ 1024, 1 })
            ->Args({ 1024, 4 })
            ->Args({ 1024, 8 })
            ->Unit(benchmark::kMicrosecond)
            ->UseManualTime()
            ->Iterations(1);

        class LifecycleFixture
            : public PhysXBaseBenchmarkFixture
        {
        public:
            void SetUp(const benchmark::State& state) override
            {
                SetUpWorld(state);
            }

            void SetUp(benchmark::State& state) override
            {
                SetUpWorld(state);
            }

            void TearDown(const benchmark::State&) override
            {
                TearDownInternal();
                m_jobContext.reset();
            }

            void TearDown(benchmark::State&) override
            {
                TearDownInternal();
                m_jobContext.reset();
            }

        protected:
            AzPhysics::SceneConfiguration GetDefaultSceneConfiguration() override
            {
                return CreateDeterministicSceneConfiguration();
            }

            void SetUpWorld(const benchmark::State& state)
            {
                m_workerCount = aznumeric_cast<AZ::u32>(state.range(1));
                m_jobContext = AZStd::make_unique<JobContextScope>(m_workerCount);
                SetUpInternal();
            }

            AZ::u32 m_workerCount = 1;
            AZStd::unique_ptr<JobContextScope> m_jobContext;
        };

        BENCHMARK_DEFINE_F(LifecycleFixture, CreateDestroyBodies)(benchmark::State& state)
        {
            const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
            const auto collider = AZStd::make_shared<Physics::ColliderConfiguration>();
            const auto sphere = AZStd::make_shared<Physics::SphereShapeConfiguration>(0.5f);
            AzPhysics::RigidBodyConfiguration configuration;
            configuration.m_gravityEnabled = false;
            configuration.m_colliderAndShapeData = AzPhysics::ShapeColliderPair(collider, sphere);
            AzPhysics::SimulatedBodyHandleList bodies;
            bodies.reserve(bodyCount);

            for ([[maybe_unused]] auto iteration : state)
            {
                bool iterationValid = true;
                for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
                {
                    configuration.m_position = AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(bodyIndex));
                    const AzPhysics::SimulatedBodyHandle bodyHandle = m_defaultScene->AddSimulatedBody(&configuration);
                    iterationValid = IsValid(bodyHandle) && iterationValid;
                    bodies.push_back(bodyHandle);
                }
                for (AzPhysics::SimulatedBodyHandle& bodyHandle : bodies)
                {
                    m_defaultScene->RemoveSimulatedBody(bodyHandle);
                    iterationValid = !IsValid(bodyHandle) && iterationValid;
                }
                bodies.clear();
                if (!iterationValid)
                {
                    state.SkipWithError("Failed to create or destroy a PhysX lifecycle benchmark body.");
                    return;
                }
            }

            state.counters["DynamicBodies"] = bodyCount;
            m_jobContext->AddCounters(state);
            state.counters["InstrumentationEnabled"] = 0;
            state.counters["Notifications"] = 0;
            state.counters["Workers"] = m_workerCount;
            state.SetItemsProcessed(state.iterations() * bodyCount);
        }

        BENCHMARK_REGISTER_F(LifecycleFixture, CreateDestroyBodies)
            ->Name("PhysX/Lifecycle/CreateDestroyBodies")
            ->Args({ 128, 1 })
            ->Args({ 1024, 1 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime();

        class RaycastGridFixture
            : public PhysXBaseBenchmarkFixture
        {
        public:
            void SetUp(const benchmark::State& state) override
            {
                SetUpWorld(state);
            }

            void SetUp(benchmark::State& state) override
            {
                SetUpWorld(state);
            }

            void TearDown(const benchmark::State&) override
            {
                TearDownInternal();
                m_jobContext.reset();
            }

            void TearDown(benchmark::State&) override
            {
                TearDownInternal();
                m_jobContext.reset();
            }

        protected:
            AzPhysics::SceneConfiguration GetDefaultSceneConfiguration() override
            {
                return CreateDeterministicSceneConfiguration();
            }

            void SetUpWorld(const benchmark::State& state)
            {
                m_workerCount = aznumeric_cast<AZ::u32>(state.range(2));
                m_jobContext = AZStd::make_unique<JobContextScope>(m_workerCount);
                SetUpInternal();
                const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
                const auto collider = AZStd::make_shared<Physics::ColliderConfiguration>();
                const auto box = AZStd::make_shared<Physics::BoxShapeConfiguration>(AZ::Vector3::CreateOne());
                AzPhysics::StaticRigidBodyConfiguration configuration;
                configuration.m_colliderAndShapeData = AzPhysics::ShapeColliderPair(collider, box);

                constexpr AZ::u32 rowWidth = 32;
                m_ready = true;
                for (AZ::u32 obstacleIndex = 0; m_ready && obstacleIndex < obstacleCount; ++obstacleIndex)
                {
                    configuration.m_position = AZ::Vector3(
                        2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), -0.5f);
                    m_ready = IsValid(m_defaultScene->AddSimulatedBody(&configuration));
                }
            }

            bool m_ready = false;
            AZ::u32 m_workerCount = 1;
            AZStd::unique_ptr<JobContextScope> m_jobContext;
        };

        BENCHMARK_DEFINE_F(RaycastGridFixture, Raycast)(benchmark::State& state)
        {
            if (!m_ready)
            {
                state.SkipWithError("Failed to create the matched PhysX raycast workload.");
                return;
            }

            const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
            const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
            AzPhysics::RayCastRequest request;
            request.m_direction = -AZ::Vector3::CreateAxisZ();
            request.m_distance = 20.0f;
            request.m_maxResults = 1;
            AzPhysics::SceneQueryHits hits;
            hits.m_hits.reserve(1);

            constexpr AZ::u32 rowWidth = 32;
            AZ::u64 successfulRayCount = 0;
            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
                {
                    const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                    request.m_start = AZ::Vector3(
                        2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 10.0f);
                    hits.m_hits.clear();
                    const bool raySucceeded = m_defaultScene->QueryScene(&request, hits)
                        && hits.m_hits.size() == 1;
                    successfulRayCount += raySucceeded;
                    benchmark::DoNotOptimize(raySucceeded);
                }
            }

            const AZ::u64 expectedRayCount = aznumeric_cast<AZ::u64>(state.iterations()) * rayCount;
            const bool qualityValid = successfulRayCount == expectedRayCount;
            state.SetItemsProcessed(state.iterations() * rayCount);
            m_jobContext->AddCounters(state);
            state.counters["Obstacles"] = obstacleCount;
            state.counters["QualityValid"] = 0;
            if (qualityValid)
            {
                state.counters["QualityValid"] = 1;
            }
            state.counters["SuccessfulQueries"] = aznumeric_cast<double>(successfulRayCount);
            state.counters["Workers"] = m_workerCount;
        }

        BENCHMARK_DEFINE_F(RaycastGridFixture, RaycastEmptyWorld)(benchmark::State& state)
        {
            if (!m_ready)
            {
                state.SkipWithError("Failed to create the matched PhysX empty-world raycast workload.");
                return;
            }

            const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
            AzPhysics::RayCastRequest request;
            request.m_direction = AZ::Vector3::CreateAxisX();
            request.m_distance = 20.0f;
            request.m_maxResults = 1;
            AzPhysics::SceneQueryHits hits;
            hits.m_hits.reserve(1);

            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
                {
                    hits.m_hits.clear();
                    benchmark::DoNotOptimize(m_defaultScene->QueryScene(&request, hits));
                }
            }

            state.SetItemsProcessed(state.iterations() * rayCount);
            m_jobContext->AddCounters(state);
            state.counters["Workers"] = m_workerCount;
        }

        BENCHMARK_DEFINE_F(RaycastGridFixture, RaycastBatch)(benchmark::State& state)
        {
            if (!m_ready)
            {
                state.SkipWithError("Failed to create the matched PhysX batch raycast workload.");
                return;
            }

            const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
            const AZ::u32 rayCount = aznumeric_cast<AZ::u32>(state.range(1));
            constexpr AZ::u32 rowWidth = 32;
            AzPhysics::SceneQueryRequests requests;
            requests.reserve(rayCount);
            for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            {
                const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                auto request = AZStd::make_shared<AzPhysics::RayCastRequest>();
                request->m_start = AZ::Vector3(
                    2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 10.0f);
                request->m_direction = -AZ::Vector3::CreateAxisZ();
                request->m_distance = 20.0f;
                request->m_maxResults = 1;
                requests.push_back(AZStd::move(request));
            }

            constexpr auto warmupDuration = AZStd::chrono::milliseconds(100);
            const auto warmupDeadline = AZStd::chrono::steady_clock::now() + warmupDuration;
            bool warmupCompleted = true;
            do
            {
                const AzPhysics::SceneQueryHitsList warmupResults = m_defaultScene->QuerySceneBatch(requests);
                warmupCompleted = warmupResults.size() == rayCount && warmupCompleted;
                benchmark::DoNotOptimize(warmupResults);
            } while (AZStd::chrono::steady_clock::now() < warmupDeadline);

            AZ::u64 completeBatchCount = 0;
            for ([[maybe_unused]] auto iteration : state)
            {
                const AzPhysics::SceneQueryHitsList results = m_defaultScene->QuerySceneBatch(requests);
                if (results.size() == rayCount)
                {
                    ++completeBatchCount;
                }
                benchmark::DoNotOptimize(results);
            }

            const AzPhysics::SceneQueryHitsList validationResults =
                m_defaultScene->QuerySceneBatch(requests);
            const bool qualityValid = warmupCompleted
                && completeBatchCount == aznumeric_cast<AZ::u64>(state.iterations())
                && validationResults.size() == rayCount
                && AZStd::all_of(
                    validationResults.begin(),
                    validationResults.end(),
                    [](const AzPhysics::SceneQueryHits& result)
                    {
                        return result.m_hits.size() == 1;
                    });
            state.SetItemsProcessed(state.iterations() * rayCount);
            m_jobContext->AddCounters(state);
            state.counters["Obstacles"] = obstacleCount;
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
            state.counters["Workers"] = m_workerCount;
        }

        BENCHMARK_DEFINE_F(RaycastGridFixture, OverlapSphere)(benchmark::State& state)
        {
            if (!m_ready)
            {
                state.SkipWithError("Failed to create the matched PhysX overlap workload.");
                return;
            }

            constexpr size_t expectedHitCount = 25;
            const AZ::u32 queryCount = aznumeric_cast<AZ::u32>(state.range(1));
            AzPhysics::OverlapRequest request = AzPhysics::OverlapRequestHelpers::CreateSphereOverlapRequest(
                5.0f, AZ::Transform::CreateTranslation(AZ::Vector3(32.0f, 32.0f, 0.0f)));
            request.m_maxResults = expectedHitCount;
            AzPhysics::SceneQueryHits hits;
            AZ::u64 completeQueryCount = 0;
            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 queryIndex = 0; queryIndex < queryCount; ++queryIndex)
                {
                    hits.m_hits.clear();
                    const bool querySucceeded = m_defaultScene->QueryScene(&request, hits);
                    if (querySucceeded && hits.m_hits.size() == expectedHitCount)
                    {
                        ++completeQueryCount;
                    }
                    benchmark::DoNotOptimize(querySucceeded);
                }
            }

            const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
            const AZ::u64 expectedQueryCount = aznumeric_cast<AZ::u64>(state.iterations()) * queryCount;
            state.counters["ActualHits"] = aznumeric_cast<double>(hits.m_hits.size());
            state.counters["CompleteOperations"] = aznumeric_cast<double>(completeQueryCount);
            m_jobContext->AddCounters(state);
            state.counters["ExpectedHits"] = expectedHitCount;
            state.counters["Obstacles"] = obstacleCount;
            state.counters["QualityValid"] = 0;
            if (completeQueryCount == expectedQueryCount
                && hits.m_hits.size() == expectedHitCount)
            {
                state.counters["QualityValid"] = 1;
            }
            state.counters["Workers"] = m_workerCount;
            state.SetItemsProcessed(state.iterations() * queryCount);
        }

        BENCHMARK_REGISTER_F(RaycastGridFixture, Raycast)
            ->Name("PhysX/Query/RaycastGrid")
            ->Args({ 1024, 128, 1 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime();

        BENCHMARK_REGISTER_F(RaycastGridFixture, RaycastEmptyWorld)
            ->Name("PhysX/Diagnostic/RaycastEmptyWorld")
            ->Args({ 0, 128, 1 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime();

        BENCHMARK_REGISTER_F(RaycastGridFixture, RaycastBatch)
            ->Name("PhysX/Query/RaycastClosestBatchGrid")
            ->Args({ 1024, 128, 4 })
            ->Args({ 1024, 1024, 4 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime();

        BENCHMARK_REGISTER_F(RaycastGridFixture, OverlapSphere)
            ->Name("PhysX/Query/OverlapSphereGrid")
            ->Args({ 1024, 1, 1 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime();
    } // namespace
} // namespace PhysX::Benchmarks

#endif // HAVE_BENCHMARK
