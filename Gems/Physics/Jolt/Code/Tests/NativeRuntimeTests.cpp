/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/FloatEnvironment.h>
#include <Jolt/JobSystem.h>
#include <Jolt/NativeRuntime.h>

#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h>

#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/limits.h>
#include <AzTest/AzTest.h>

#include <cfenv>

namespace Jolt
{
    TEST(NativeRuntimeTests, ReportsPinnedDeterministicConfiguration)
    {
        NativeRuntime runtime;
        ASSERT_TRUE(runtime);

        const RuntimeInfo runtimeInfo = runtime.GetRuntimeInfo();
        EXPECT_EQ(runtimeInfo.m_version, (Version{.m_major = 5, .m_minor = 6, .m_patch = 0}));
        EXPECT_NE(runtimeInfo.m_buildFingerprint, 0);
        EXPECT_EQ(runtimeInfo.m_patchHash, "297a0dc9ac15dc476c77164bc864dbe41d17cb4d954f1c64fd37b165b8f85757");
        EXPECT_EQ(runtimeInfo.m_patchRevision, "jolt-v5.6.0-o3de-14");
        EXPECT_EQ(runtimeInfo.m_sourceRevision, "e77f175595e64cb44218cc9d9d56fc365ad0e36a");
        EXPECT_EQ(runtimeInfo.m_hairDeterminism, DeterminismCertification::SameBinary);
        EXPECT_EQ(runtimeInfo.m_physicsDeterminism, DeterminismCertification::CrossPlatform);
        EXPECT_FALSE(runtimeInfo.m_configuration.empty());
        EXPECT_NE(
            runtimeInfo.m_configuration.find("Cross Platform Deterministic"),
            AZStd::string_view::npos);
#if defined(JPH_CPU_WASM) && defined(JPH_USE_SSE)
        EXPECT_EQ(runtimeInfo.m_simdLevel, SimdLevel::WasmSimd);
        EXPECT_NE(runtimeInfo.m_configuration.find("WASM"), AZStd::string_view::npos);
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
#if defined(JPH_EXTERNAL_PROFILE)
        EXPECT_TRUE(runtimeInfo.m_detailedProfiling);
#else
        EXPECT_FALSE(runtimeInfo.m_detailedProfiling);
#endif
#if defined(JPH_TRACK_SIMULATION_STATS)
        EXPECT_TRUE(runtimeInfo.m_simulationStatistics);
#else
        EXPECT_FALSE(runtimeInfo.m_simulationStatistics);
#endif
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
            ASSERT_EQ(std::feraiseexcept(FE_OVERFLOW), 0);
        }

        const int restoredExceptions = std::fetestexcept(FE_ALL_EXCEPT);
        ASSERT_EQ(std::fesetenv(&originalEnvironment), 0);
        EXPECT_EQ(restoredExceptions, FE_DIVBYZERO);
    }

    TEST(NativeRuntimeTests, ConfiguresSoftBodyTriangleThicknessForAllLiveOwners)
    {
        constexpr float triangleThickness = 0.25f;
        NativeRuntime first(triangleThickness);
        NativeRuntime second(triangleThickness);

        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_EQ(
            JPH::CollideSoftBodyVerticesVsTriangles::sTriangleThickness,
            triangleThickness);

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

    TEST(NativeRuntimeTests, WaitsForProviderTasksBeforeJobSystemDestruction)
    {
        NativeRuntime runtime;
        ASSERT_TRUE(runtime);

        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        for (AZ::u32 iteration = 0; iteration < 100; ++iteration)
        {
            JobSystem jobSystem(1'024, 8, 4, &jobContext);
            JPH::JobSystem::Barrier* barrier = jobSystem.CreateBarrier();
            ASSERT_TRUE(barrier);
            {
                JPH::JobHandle job = jobSystem.CreateJob(
                    "Lifetime validation",
                    JPH::Color::sWhite,
                    []() {},
                    0);
                ASSERT_TRUE(job.IsValid());
                barrier->AddJob(job);
                jobSystem.WaitForJobs(barrier);
            }
            jobSystem.DestroyBarrier(barrier);
        }
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
