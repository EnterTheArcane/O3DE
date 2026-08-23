/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Architecture.h>
#include <Jolt/CustomShape.h>
#include <Jolt/FloatEnvironment.h>
#include <Jolt/JobSystem.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/SystemInternal.h>

#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>
#include <AzTest/AzTest.h>

#include <cfenv>
#include <chrono>

#if defined(JOLT_TESTS_DEFINE_NATIVE_ASSERT_HANDLER) && defined(JPH_ENABLE_ASSERTS)
namespace JPH
{
    namespace
    {
        bool ReportTestNativeAssertion(
            const char* expression,
            const char* message,
            const char* fileName,
            const JPH::uint lineNumber)
        {
            const char* reportedExpression = "<unknown>";
            if (expression)
            {
                reportedExpression = expression;
            }

            const char* reportedMessage = "<none>";
            if (message)
            {
                reportedMessage = message;
            }

            const char* reportedFileName = "<unknown>";
            if (fileName)
            {
                reportedFileName = fileName;
            }

            AZ_Error(
                "Jolt",
                false,
                "Native assertion failed in the test module: %s; %s (%s:%u)",
                reportedExpression,
                reportedMessage,
                reportedFileName,
                lineNumber);
            return false;
        }
    } // namespace

    AssertFailedFunction AssertFailed = ReportTestNativeAssertion;
} // namespace JPH
#endif

namespace Jolt
{
    namespace
    {
        inline constexpr AZ::TypeId BlockingCustomConvexShapeProviderTypeId{"{E8755B7C-FE3E-4788-9B05-438E9EA2938C}"};

        template<class Predicate>
        [[nodiscard]]
        bool WaitForRuntimeCondition(
            Predicate&& predicate,
            const std::chrono::milliseconds timeout = std::chrono::seconds(5))
        {
            const std::chrono::steady_clock::time_point deadline =
                std::chrono::steady_clock::now() + timeout;
            while (!predicate())
            {
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    return false;
                }

                AZStd::this_thread::yield();
            }

            return true;
        }

        class BlockingCustomConvexShapeProvider final
            : public ICustomConvexShapeProvider
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return BlockingCustomConvexShapeProviderTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 1;
            }

            [[nodiscard]]
            bool Cook(
                [[maybe_unused]] const AZStd::span<const AZ::u8> input,
                CustomConvexShapeData& output) const override
            {
                m_entered.store(true, AZStd::memory_order_release);
                while (!m_release.load(AZStd::memory_order_acquire))
                {
                    AZStd::this_thread::yield();
                }

                output.m_points = {
                    {-1.0f, -1.0f, -1.0f},
                    {1.0f, -1.0f, -1.0f},
                    {-1.0f, 1.0f, -1.0f},
                    {1.0f, 1.0f, -1.0f},
                    {-1.0f, -1.0f, 1.0f},
                    {1.0f, -1.0f, 1.0f},
                    {-1.0f, 1.0f, 1.0f},
                    {1.0f, 1.0f, 1.0f},
                };
                return true;
            }

            mutable AZStd::atomic_bool m_entered = false;
            mutable AZStd::atomic_bool m_release = false;
        };

        bool CapabilitiesMatch(Runtime* expectedRuntime)
        {
            return RuntimeConfiguration::Get() == static_cast<RuntimeConfiguration*>(expectedRuntime)
                && Extensions::Get() == static_cast<Extensions*>(expectedRuntime)
                && Materials::Get() == static_cast<Materials*>(expectedRuntime)
                && CollisionFilters::Get() == static_cast<CollisionFilters*>(expectedRuntime)
                && Cooking::Get() == static_cast<Cooking*>(expectedRuntime)
                && Paths::Get() == static_cast<Paths*>(expectedRuntime)
                && Skeletons::Get() == static_cast<Skeletons*>(expectedRuntime)
                && Scenes::Get() == static_cast<Scenes*>(expectedRuntime)
                && Worlds::Get() == static_cast<Worlds*>(expectedRuntime)
                && WorldSimulation::Get() == static_cast<WorldSimulation*>(expectedRuntime)
                && WorldQueries::Get() == static_cast<WorldQueries*>(expectedRuntime)
                && Shapes::Get() == static_cast<Shapes*>(expectedRuntime)
                && Bodies::Get() == static_cast<Bodies*>(expectedRuntime)
                && Constraints::Get() == static_cast<Constraints*>(expectedRuntime)
                && Characters::Get() == static_cast<Characters*>(expectedRuntime)
                && Vehicles::Get() == static_cast<Vehicles*>(expectedRuntime)
                && Ragdolls::Get() == static_cast<Ragdolls*>(expectedRuntime)
                && SoftBodies::Get() == static_cast<SoftBodies*>(expectedRuntime)
                && Hair::Get() == static_cast<Hair*>(expectedRuntime)
                && Rollback::Get() == static_cast<Rollback*>(expectedRuntime)
                && Diagnostics::Get() == static_cast<Diagnostics*>(expectedRuntime);
        }

        void ExpectPublishedCapabilities(Runtime* expectedRuntime)
        {
            EXPECT_EQ(RuntimeConfiguration::Get(), static_cast<RuntimeConfiguration*>(expectedRuntime));
            EXPECT_EQ(Extensions::Get(), static_cast<Extensions*>(expectedRuntime));
            EXPECT_EQ(Materials::Get(), static_cast<Materials*>(expectedRuntime));
            EXPECT_EQ(CollisionFilters::Get(), static_cast<CollisionFilters*>(expectedRuntime));
            EXPECT_EQ(Cooking::Get(), static_cast<Cooking*>(expectedRuntime));
            EXPECT_EQ(Paths::Get(), static_cast<Paths*>(expectedRuntime));
            EXPECT_EQ(Skeletons::Get(), static_cast<Skeletons*>(expectedRuntime));
            EXPECT_EQ(Scenes::Get(), static_cast<Scenes*>(expectedRuntime));
            EXPECT_EQ(Worlds::Get(), static_cast<Worlds*>(expectedRuntime));
            EXPECT_EQ(WorldSimulation::Get(), static_cast<WorldSimulation*>(expectedRuntime));
            EXPECT_EQ(WorldQueries::Get(), static_cast<WorldQueries*>(expectedRuntime));
            EXPECT_EQ(Shapes::Get(), static_cast<Shapes*>(expectedRuntime));
            EXPECT_EQ(Bodies::Get(), static_cast<Bodies*>(expectedRuntime));
            EXPECT_EQ(Constraints::Get(), static_cast<Constraints*>(expectedRuntime));
            EXPECT_EQ(Characters::Get(), static_cast<Characters*>(expectedRuntime));
            EXPECT_EQ(Vehicles::Get(), static_cast<Vehicles*>(expectedRuntime));
            EXPECT_EQ(Ragdolls::Get(), static_cast<Ragdolls*>(expectedRuntime));
            EXPECT_EQ(SoftBodies::Get(), static_cast<SoftBodies*>(expectedRuntime));
            EXPECT_EQ(Hair::Get(), static_cast<Hair*>(expectedRuntime));
            EXPECT_EQ(Rollback::Get(), static_cast<Rollback*>(expectedRuntime));
            EXPECT_EQ(Diagnostics::Get(), static_cast<Diagnostics*>(expectedRuntime));
        }
    } // namespace

    TEST(NativeRuntimeTests, PublishesCapabilitiesOnlyForTheActiveGlobalRuntime)
    {
        ExpectPublishedCapabilities(nullptr);

        {
            SystemConfiguration configuration;
            configuration.m_createDefaultWorld = false;
            System system(AZStd::move(configuration));
            ASSERT_TRUE(system);

            Runtime* publishedRuntime = static_cast<Runtime*>(RuntimeConfiguration::Get());
            ASSERT_TRUE(publishedRuntime);
            ExpectPublishedCapabilities(publishedRuntime);

            Runtime isolated(
                SystemConfiguration{},
                nullptr,
                SystemRegistration::Isolated);
            ASSERT_TRUE(isolated);
            ExpectPublishedCapabilities(publishedRuntime);

            AZ_TEST_START_TRACE_SUPPRESSION;
            System secondSystem(SystemConfiguration{});
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
            EXPECT_FALSE(secondSystem);
            ExpectPublishedCapabilities(publishedRuntime);
        }

        ExpectPublishedCapabilities(nullptr);
    }

    TEST(NativeRuntimeTests, CapabilityReadersObserveOneStableRuntimeRoot)
    {
        SystemConfiguration configuration;
        configuration.m_createDefaultWorld = false;
        System system(AZStd::move(configuration));
        ASSERT_TRUE(system);

        Runtime* publishedRuntime = static_cast<Runtime*>(RuntimeConfiguration::Get());
        ASSERT_TRUE(publishedRuntime);

        constexpr size_t ReaderCount = 8;
        constexpr size_t IterationCount = 10'000;
        AZStd::atomic_bool mismatch = false;
        AZStd::array<AZStd::thread, ReaderCount> readers;
        for (AZStd::thread& reader : readers)
        {
            reader = AZStd::thread(
                [publishedRuntime, &mismatch]()
                {
                    for (size_t iteration = 0; iteration < IterationCount; ++iteration)
                    {
                        RuntimeConfiguration* runtimeConfiguration = RuntimeConfiguration::Get();
                        if (!CapabilitiesMatch(publishedRuntime)
                            || !runtimeConfiguration
                            || runtimeConfiguration->GetConfiguration().m_createDefaultWorld)
                        {
                            mismatch.store(true, AZStd::memory_order_relaxed);
                            return;
                        }
                    }
                });
        }

        for (AZStd::thread& reader : readers)
        {
            reader.join();
        }

        EXPECT_FALSE(mismatch.load(AZStd::memory_order_relaxed));
        ExpectPublishedCapabilities(publishedRuntime);
    }

    TEST(NativeRuntimeTests, SequentialSystemsReplaceThePublishedRuntimeRoot)
    {
        ExpectPublishedCapabilities(nullptr);

        constexpr size_t ReplacementCount = 4;
        for (size_t replacement = 0; replacement < ReplacementCount; ++replacement)
        {
            {
                SystemConfiguration configuration;
                configuration.m_createDefaultWorld = false;
                System system(AZStd::move(configuration));
                ASSERT_TRUE(system);

                Runtime* publishedRuntime = static_cast<Runtime*>(RuntimeConfiguration::Get());
                ASSERT_TRUE(publishedRuntime);
                ExpectPublishedCapabilities(publishedRuntime);
            }

            ExpectPublishedCapabilities(nullptr);
        }
    }

    TEST(NativeRuntimeTests, ReportsPinnedDeterministicConfiguration)
    {
        NativeRuntime runtime;
        ASSERT_TRUE(runtime);

        const RuntimeInfo runtimeInfo = runtime.GetRuntimeInfo();
        EXPECT_EQ(runtimeInfo.m_version, (Version{.m_major = 5, .m_minor = 6, .m_patch = 0}));
        EXPECT_NE(runtimeInfo.m_buildFingerprint, 0);
        EXPECT_EQ(runtimeInfo.m_hairDeterminism, DeterminismCertification::SameBinary);
        EXPECT_EQ(runtimeInfo.m_physicsDeterminism, DeterminismCertification::CrossPlatform);
        EXPECT_FALSE(runtimeInfo.m_configuration.empty());
        EXPECT_NE(
            runtimeInfo.m_configuration.find("Cross Platform Deterministic"),
            AZStd::string_view::npos);
#if defined(JPH_CPU_WASM) && defined(JPH_USE_SSE)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::WasmSimd);
        EXPECT_NE(runtimeInfo.m_configuration.find("WASM"), AZStd::string_view::npos);
#elif JOLT_ARCH_FAMILY_X86
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Sse41);
        EXPECT_NE(runtimeInfo.m_configuration.find("SSE4.1"), AZStd::string_view::npos);
#elif defined(JPH_USE_AVX512)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Avx512);
        EXPECT_NE(runtimeInfo.m_configuration.find("AVX512"), AZStd::string_view::npos);
#elif defined(JPH_USE_AVX2)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Avx2);
        EXPECT_NE(runtimeInfo.m_configuration.find("AVX2"), AZStd::string_view::npos);
#elif defined(JPH_USE_AVX)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Avx);
        EXPECT_NE(runtimeInfo.m_configuration.find("AVX"), AZStd::string_view::npos);
#elif defined(JPH_USE_SSE4_2)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Sse42);
        EXPECT_NE(runtimeInfo.m_configuration.find("SSE4.2"), AZStd::string_view::npos);
#elif defined(JPH_USE_SSE4_1)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Sse41);
        EXPECT_NE(runtimeInfo.m_configuration.find("SSE4.1"), AZStd::string_view::npos);
#elif defined(JPH_USE_SSE)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Sse2);
        EXPECT_NE(runtimeInfo.m_configuration.find("SSE2"), AZStd::string_view::npos);
#elif defined(JPH_USE_NEON)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Neon);
        EXPECT_NE(runtimeInfo.m_configuration.find("NEON"), AZStd::string_view::npos);
#elif defined(JPH_USE_RVV)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Rvv);
        EXPECT_NE(runtimeInfo.m_configuration.find("RVV"), AZStd::string_view::npos);
#else
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::Scalar);
#endif
#ifdef JPH_EXTERNAL_PROFILE
        EXPECT_TRUE(runtimeInfo.m_detailedProfiling);
#else
        EXPECT_FALSE(runtimeInfo.m_detailedProfiling);
#endif
#ifdef JPH_DEBUG_RENDERER
        EXPECT_TRUE(runtimeInfo.m_debugRendering);
#else
        EXPECT_FALSE(runtimeInfo.m_debugRendering);
#endif
#ifdef JPH_TRACK_BROADPHASE_STATS
        EXPECT_TRUE(runtimeInfo.m_broadPhaseStatistics);
#else
        EXPECT_FALSE(runtimeInfo.m_broadPhaseStatistics);
#endif
#ifdef JPH_TRACK_NARROWPHASE_STATS
        EXPECT_TRUE(runtimeInfo.m_narrowPhaseStatistics);
#else
        EXPECT_FALSE(runtimeInfo.m_narrowPhaseStatistics);
#endif
#ifdef JPH_TRACK_SIMULATION_STATS
        EXPECT_TRUE(runtimeInfo.m_simulationStatistics);
#else
        EXPECT_FALSE(runtimeInfo.m_simulationStatistics);
#endif
    }

    TEST(NativeRuntimeTests, FailedDefaultWorldCreationDoesNotPublishCapabilities)
    {
        SystemConfiguration configuration;
        configuration.m_defaultWorld.m_simulation.m_positionStepCount = 0;

        AZ_TEST_START_TRACE_SUPPRESSION;
        System system(AZStd::move(configuration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(2);

        EXPECT_FALSE(system);
        ExpectPublishedCapabilities(nullptr);
    }

    TEST(NativeRuntimeTests, SupportsOverlappingOwners)
    {
        NativeRuntime first;
        NativeRuntime second;

        EXPECT_TRUE(first);
        EXPECT_TRUE(second);
        EXPECT_EQ(first.GetRuntimeInfo().m_configuration, second.GetRuntimeInfo().m_configuration);
    }

    TEST(NativeRuntimeTests, DeterministicFloatScopeRestoresExceptionFlags)
    {
        std::fenv_t originalEnvironment;
        ASSERT_EQ(std::fegetenv(&originalEnvironment), 0);
        ASSERT_EQ(std::fesetenv(FE_DFL_ENV), 0);
        ASSERT_EQ(std::feraiseexcept(FE_DIVBYZERO), 0);
        ASSERT_EQ(std::fetestexcept(FE_ALL_EXCEPT), FE_DIVBYZERO);

        {
            DeterministicFloatScope scope;
            EXPECT_EQ(std::fetestexcept(FE_ALL_EXCEPT), 0);
            ASSERT_EQ(std::feraiseexcept(FE_OVERFLOW), 0);
        }

        const int restoredExceptions = std::fetestexcept(FE_ALL_EXCEPT);
        ASSERT_EQ(std::fesetenv(&originalEnvironment), 0);
        EXPECT_EQ(restoredExceptions, FE_DIVBYZERO);
    }

    TEST(NativeRuntimeTests, DeterministicFloatScopeRestoresNoncanonicalEnvironment)
    {
        std::fenv_t originalEnvironment;
        ASSERT_EQ(std::fegetenv(&originalEnvironment), 0);
        ASSERT_EQ(std::fesetenv(FE_DFL_ENV), 0);
        ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);
        ASSERT_EQ(std::feraiseexcept(FE_DIVBYZERO), 0);

        {
            DeterministicFloatScope scope;
            EXPECT_EQ(std::fegetround(), FE_TONEAREST);
            EXPECT_EQ(std::fetestexcept(FE_ALL_EXCEPT), 0);
            ASSERT_EQ(std::feraiseexcept(FE_OVERFLOW), 0);
        }

        const int restoredRoundingMode = std::fegetround();
        const int restoredExceptions = std::fetestexcept(FE_ALL_EXCEPT);
        ASSERT_EQ(std::fesetenv(&originalEnvironment), 0);
        EXPECT_EQ(restoredRoundingMode, FE_DOWNWARD);
        EXPECT_EQ(restoredExceptions, FE_DIVBYZERO);
    }

    TEST(NativeRuntimeTests, ConfiguresSoftBodyTriangleThicknessForAllLiveOwners)
    {
        constexpr float triangleThickness = 0.25f;
        NativeRuntime first(triangleThickness);
        NativeRuntime second(triangleThickness);

        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_EQ(GetSoftBodyTriangleThickness(), triangleThickness);

        AZ_TEST_START_TRACE_SUPPRESSION;
        NativeRuntime conflicting(0.5f);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_FALSE(conflicting);
    }

    TEST(NativeRuntimeTests, RejectsInvalidSoftBodyTriangleThickness)
    {
        AZ_TEST_START_TRACE_SUPPRESSION;
        NativeRuntime negative(-1.0f);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        AZ_TEST_START_TRACE_SUPPRESSION;
        NativeRuntime nonFinite(AZStd::numeric_limits<float>::quiet_NaN());
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        EXPECT_FALSE(negative);
        EXPECT_FALSE(nonFinite);
    }

    TEST(NativeRuntimeTests, CompletesProviderTasksBeforeSystemDestruction)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        for (AZ::u32 iteration = 0; iteration < 20; ++iteration)
        {
            Runtime system(SystemConfiguration{}, &jobContext, SystemRegistration::Isolated);
            ASSERT_TRUE(system);

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();

            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{};
            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            ASSERT_TRUE(shapeHandle);

            constexpr AZ::u32 bodyCount = 64;
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
            {
                BodyConfiguration bodyConfiguration;
                bodyConfiguration.m_shapeHandle = shapeHandle;
                bodyConfiguration.m_transform.m_position = {
                    .m_x = aznumeric_cast<double>(bodyIndex % 8),
                    .m_y = aznumeric_cast<double>(bodyIndex / 8),
                    .m_z = 1.0,
                };
                ASSERT_TRUE(system.CreateBody(worldHandle, bodyConfiguration));
            }

            EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }
    }

    TEST(NativeRuntimeTests, RevokesCapabilityRootBeforeDrainingProviderOperations)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        BlockingCustomConvexShapeProvider provider;
        Operation<CookedShapeHandle> operation;
        AZStd::atomic_bool revokedBeforeDrain = false;
        AZStd::thread observer;
        {
            SystemConfiguration configuration;
            configuration.m_createDefaultWorld = false;
            System system(AZStd::move(configuration), &jobContext);
            ASSERT_TRUE(system);

            Extensions* extensions = Extensions::Get();
            Cooking* cooking = Cooking::Get();
            ASSERT_TRUE(extensions);
            ASSERT_TRUE(cooking);
            ASSERT_TRUE(extensions->RegisterExtension(&provider, {}));

            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = CustomConvexShapeConfiguration{
                .m_data = {1},
                .m_providerId = provider.GetId(),
            };
            operation = cooking->CookShapeAsync(shapeConfiguration);
            ASSERT_TRUE(operation);

            const bool providerEntered = WaitForRuntimeCondition(
                [&provider]
                {
                    return provider.m_entered.load(AZStd::memory_order_acquire);
                });
            if (!providerEntered)
            {
                provider.m_release.store(true, AZStd::memory_order_release);
            }
            ASSERT_TRUE(providerEntered);

            observer = AZStd::thread(
                [&provider, &revokedBeforeDrain]
                {
                    // Observe only publication state; never dereference a borrowed capability during teardown.
                    const bool observedRevocation = WaitForRuntimeCondition(
                        []
                        {
                            return !RuntimeConfiguration::Get();
                        });
                    revokedBeforeDrain.store(observedRevocation, AZStd::memory_order_release);
                    provider.m_release.store(true, AZStd::memory_order_release);
                });
        }

        observer.join();
        EXPECT_TRUE(revokedBeforeDrain.load(AZStd::memory_order_acquire));
        EXPECT_EQ(operation.GetStatus(), OperationStatus::Succeeded);
        ExpectPublishedCapabilities(nullptr);
        operation.Reset();
    }

    TEST(NativeRuntimeTests, JobSystemCountsTheCallingThreadAsAvailableConcurrency)
    {
        NativeRuntime runtime;
        ASSERT_TRUE(runtime);

        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(2);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        JobSystem parallelJobSystem(1'024, 8, 64, &jobContext);
        EXPECT_EQ(parallelJobSystem.GetMaxConcurrency(), 3);

        JobSystem cappedJobSystem(1'024, 8, 2, &jobContext);
        EXPECT_EQ(cappedJobSystem.GetMaxConcurrency(), 2);

        JobSystem serialJobSystem(1'024, 8, 64, nullptr);
        EXPECT_EQ(serialJobSystem.GetMaxConcurrency(), 1);
    }

    TEST(NativeRuntimeTests, AxisAlignedRayAabbPathMatchesGeneralSlabTest)
    {
        const JPH::Vec3 origin = JPH::Vec3::sZero();
        const JPH::Vec4 boundsMinX(-1.0f, 2.0f, -1.0f, 1.0f);
        const JPH::Vec4 boundsMinY(-1.0f, -1.0f, 2.0f, -1.0f);
        const JPH::Vec4 boundsMinZ(2.0f, -4.0f, -1.0f, -1.0f);
        const JPH::Vec4 boundsMaxX(1.0f, 4.0f, 1.0f, -1.0f);
        const JPH::Vec4 boundsMaxY(1.0f, 1.0f, 4.0f, 1.0f);
        const JPH::Vec4 boundsMaxZ(4.0f, -2.0f, 1.0f, 1.0f);
        constexpr AZStd::array<float, 2> rayLengths = {-10.0f, 10.0f};

        for (int axis = 0; axis < 3; ++axis)
        {
            for (const float rayLength : rayLengths)
            {
                JPH::Vec3 direction = JPH::Vec3::sZero();
                direction.SetComponent(axis, rayLength);
                const JPH::RayInvDirection inverseDirection(direction);
                const JPH::Vec4 expected = JPH::RayAABox4(
                    origin,
                    inverseDirection,
                    boundsMinX,
                    boundsMinY,
                    boundsMinZ,
                    boundsMaxX,
                    boundsMaxY,
                    boundsMaxZ);

                JPH::Vec4 actual;
                switch (axis)
                {
                case 0:
                    actual = JPH::RayAABox4AxisAligned(
                        origin.GetY(),
                        origin.GetZ(),
                        origin.GetX(),
                        inverseDirection.mInvDirection.GetX(),
                        boundsMinY,
                        boundsMinZ,
                        boundsMinX,
                        boundsMaxY,
                        boundsMaxZ,
                        boundsMaxX);
                    break;
                case 1:
                    actual = JPH::RayAABox4AxisAligned(
                        origin.GetX(),
                        origin.GetZ(),
                        origin.GetY(),
                        inverseDirection.mInvDirection.GetY(),
                        boundsMinX,
                        boundsMinZ,
                        boundsMinY,
                        boundsMaxX,
                        boundsMaxZ,
                        boundsMaxY);
                    break;
                case 2:
                    actual = JPH::RayAABox4AxisAligned(
                        origin.GetX(),
                        origin.GetY(),
                        origin.GetZ(),
                        inverseDirection.mInvDirection.GetZ(),
                        boundsMinX,
                        boundsMinY,
                        boundsMinZ,
                        boundsMaxX,
                        boundsMaxY,
                        boundsMaxZ);
                    break;
                default:
                    AZ_Assert(false, "The test axis is invalid.");
                    break;
                }

                EXPECT_EQ(actual.GetX(), expected.GetX());
                EXPECT_EQ(actual.GetY(), expected.GetY());
                EXPECT_EQ(actual.GetZ(), expected.GetZ());
                EXPECT_EQ(actual.GetW(), expected.GetW());
            }
        }
    }
} // namespace Jolt
