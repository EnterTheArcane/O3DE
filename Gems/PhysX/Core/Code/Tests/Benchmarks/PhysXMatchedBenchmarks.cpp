/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#ifdef HAVE_BENCHMARK

#include <Benchmarks/PhysXBenchmarksCommon.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
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
                : m_previousJobContext(AZ::JobContext::GetGlobalContext())
                , m_jobManager(
                      [workerCount]
                      {
                          AZ::JobManagerDesc descriptor;
                          descriptor.m_workerThreads.resize(workerCount);
                          return descriptor;
                      }())
                , m_jobContext(m_jobManager)
            {
                AZ::JobContext::SetGlobalContext(nullptr);
                AZ::JobContext::SetGlobalContext(&m_jobContext);
            }

            ~JobContextScope()
            {
                AZ::JobContext::SetGlobalContext(nullptr);
                AZ::JobContext::SetGlobalContext(m_previousJobContext);
            }

        private:
            AZ::JobContext* m_previousJobContext = nullptr;
            AZ::JobManager m_jobManager;
            AZ::JobContext m_jobContext;
        };

        class FallingSpheresFixture
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
                m_maximumDisplacement = 0.0f;
                m_minimumHeight = AZStd::numeric_limits<float>::max();
                m_qualityValid = false;
                m_warmupStable = false;
                m_warmupTickCount = 0;
                m_workerCount = aznumeric_cast<AZ::u32>(state.range(1));
                m_jobContext = AZStd::make_unique<JobContextScope>(m_workerCount);
                SetUpInternal();
                const auto collider = AZStd::make_shared<Physics::ColliderConfiguration>();

                AzPhysics::StaticRigidBodyConfiguration groundConfiguration;
                groundConfiguration.m_position = AZ::Vector3(0.0f, 0.0f, -0.5f);
                groundConfiguration.m_colliderAndShapeData = AzPhysics::ShapeColliderPair(
                    collider, AZStd::make_shared<Physics::BoxShapeConfiguration>(AZ::Vector3(128.0f, 128.0f, 1.0f)));
                m_ready = IsValid(m_defaultScene->AddSimulatedBody(&groundConfiguration));

                const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
                AzPhysics::SimulatedBodyHandleList bodies;
                bodies.reserve(bodyCount);
                const auto sphere = AZStd::make_shared<Physics::SphereShapeConfiguration>(0.5f);
                AzPhysics::RigidBodyConfiguration bodyConfiguration;
                bodyConfiguration.m_linearDamping = 0.0f;
                bodyConfiguration.m_angularDamping = 0.0f;
                bodyConfiguration.m_sleepMinEnergy = 0.0f;
                bodyConfiguration.m_colliderAndShapeData = AzPhysics::ShapeColliderPair(collider, sphere);

                constexpr AZ::u32 rowWidth = 16;
                for (AZ::u32 bodyIndex = 0; m_ready && bodyIndex < bodyCount; ++bodyIndex)
                {
                    const AZ::u32 layerIndex = bodyIndex / (rowWidth * rowWidth);
                    const AZ::u32 rowIndex = (bodyIndex / rowWidth) % rowWidth;
                    const AZ::u32 columnIndex = bodyIndex % rowWidth;
                    bodyConfiguration.m_position = AZ::Vector3(
                        static_cast<float>(columnIndex) - 0.5f * static_cast<float>(rowWidth),
                        static_cast<float>(rowIndex) - 0.5f * static_cast<float>(rowWidth),
                        0.6f + static_cast<float>(layerIndex) * 1.1f);
                    const AzPhysics::SimulatedBodyHandle bodyHandle = m_defaultScene->AddSimulatedBody(&bodyConfiguration);
                    m_ready = IsValid(bodyHandle);
                    bodies.push_back(bodyHandle);
                }

                constexpr AZ::u32 minimumWarmupTickCount = 30;
                constexpr AZ::u32 maximumWarmupTickCount = 600;
                constexpr AZ::u32 requiredStableTickCount = 5;
                constexpr float maximumStableDisplacement = 0.02f;
                AZStd::vector<AZ::Vector3> previousPositions(bodyCount);
                AZ::u32 stableTickCount = 0;
                for (; m_ready && m_warmupTickCount < maximumWarmupTickCount && stableTickCount < requiredStableTickCount;
                     ++m_warmupTickCount)
                {
                    for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
                    {
                        AzPhysics::SimulatedBody* body = m_defaultScene->GetSimulatedBodyFromHandle(bodies[bodyIndex]);
                        previousPositions[bodyIndex] = body != nullptr ? body->GetTransform().GetTranslation() : AZ::Vector3::CreateZero();
                    }
                    StepScene1Tick(MatchedTimeStep);

                    m_maximumDisplacement = 0.0f;
                    m_minimumHeight = AZStd::numeric_limits<float>::max();
                    for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
                    {
                        AzPhysics::SimulatedBody* body = m_defaultScene->GetSimulatedBodyFromHandle(bodies[bodyIndex]);
                        const AZ::Vector3 position = body != nullptr ? body->GetTransform().GetTranslation() : AZ::Vector3::CreateZero();
                        m_minimumHeight = AZStd::min(m_minimumHeight, position.GetZ());
                        m_maximumDisplacement = AZStd::max(m_maximumDisplacement, position.GetDistance(previousPositions[bodyIndex]));
                    }
                    stableTickCount = m_warmupTickCount + 1 >= minimumWarmupTickCount && m_maximumDisplacement <= maximumStableDisplacement
                        ? stableTickCount + 1
                        : 0;
                }
                m_warmupStable = stableTickCount == requiredStableTickCount;
                m_qualityValid = m_warmupStable && m_minimumHeight >= 0.45f;
            }

            bool m_ready = false;
            bool m_qualityValid = false;
            bool m_warmupStable = false;
            float m_maximumDisplacement = 0.0f;
            float m_minimumHeight = AZStd::numeric_limits<float>::max();
            AZ::u32 m_workerCount = 1;
            AZ::u32 m_warmupTickCount = 0;
            AZStd::unique_ptr<JobContextScope> m_jobContext;
        };

        BENCHMARK_DEFINE_F(FallingSpheresFixture, Step)(benchmark::State& state)
        {
            if (!m_ready)
            {
                state.SkipWithError("Failed to create the matched PhysX sphere workload.");
                return;
            }

            for ([[maybe_unused]] auto iteration : state)
            {
                m_defaultScene->StartSimulation(MatchedTimeStep);
                m_defaultScene->FinishSimulation();
            }

            const AZ::u32 bodyCount = aznumeric_cast<AZ::u32>(state.range(0));
            state.SetItemsProcessed(state.iterations() * bodyCount);
            state.counters["Ccd"] = 0;
            state.counters["DynamicBodies"] = bodyCount;
            state.counters["MaximumDisplacement"] = m_maximumDisplacement;
            state.counters["MinimumHeight"] = m_minimumHeight;
            state.counters["Notifications"] = 0;
            state.counters["QualityValid"] = m_qualityValid ? 1 : 0;
            state.counters["Sleep"] = 0;
            state.counters["WarmupTicks"] = m_warmupTickCount;
            state.counters["WarmupStable"] = m_warmupStable ? 1 : 0;
            state.counters["Workers"] = m_workerCount;
        }

        BENCHMARK_REGISTER_F(FallingSpheresFixture, Step)
            ->Name("PhysX/Step/FallingSpheres")
            ->Args({ 128, 1 })
            ->Args({ 128, 4 })
            ->Args({ 128, 8 })
            ->Args({ 1024, 1 })
            ->Args({ 1024, 4 })
            ->Args({ 1024, 8 })
            ->Unit(benchmark::kMicrosecond)
            ->UseRealTime();

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
                for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
                {
                    configuration.m_position = AZ::Vector3(0.0f, 0.0f, aznumeric_cast<float>(bodyIndex));
                    bodies.push_back(m_defaultScene->AddSimulatedBody(&configuration));
                }
                for (AzPhysics::SimulatedBodyHandle& bodyHandle : bodies)
                {
                    m_defaultScene->RemoveSimulatedBody(bodyHandle);
                }
                bodies.clear();
            }

            state.counters["DynamicBodies"] = bodyCount;
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
            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 rayIndex = 0; rayIndex < rayCount; ++rayIndex)
                {
                    const AZ::u32 obstacleIndex = rayIndex % obstacleCount;
                    request.m_start = AZ::Vector3(
                        2.0f * static_cast<float>(obstacleIndex % rowWidth), 2.0f * static_cast<float>(obstacleIndex / rowWidth), 10.0f);
                    hits.m_hits.clear();
                    benchmark::DoNotOptimize(m_defaultScene->QueryScene(&request, hits));
                }
            }

            state.SetItemsProcessed(state.iterations() * rayCount);
            state.counters["Obstacles"] = obstacleCount;
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
            do
            {
                benchmark::DoNotOptimize(m_defaultScene->QuerySceneBatch(requests));
            } while (AZStd::chrono::steady_clock::now() < warmupDeadline);

            for ([[maybe_unused]] auto iteration : state)
            {
                benchmark::DoNotOptimize(m_defaultScene->QuerySceneBatch(requests));
            }

            state.SetItemsProcessed(state.iterations() * rayCount);
            state.counters["Obstacles"] = obstacleCount;
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
            for ([[maybe_unused]] auto iteration : state)
            {
                for (AZ::u32 queryIndex = 0; queryIndex < queryCount; ++queryIndex)
                {
                    hits.m_hits.clear();
                    benchmark::DoNotOptimize(m_defaultScene->QueryScene(&request, hits));
                }
            }

            const AZ::u32 obstacleCount = aznumeric_cast<AZ::u32>(state.range(0));
            state.counters["ActualHits"] = aznumeric_cast<double>(hits.m_hits.size());
            state.counters["ExpectedHits"] = expectedHitCount;
            state.counters["Obstacles"] = obstacleCount;
            state.counters["QualityValid"] = hits.m_hits.size() == expectedHitCount ? 1 : 0;
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
