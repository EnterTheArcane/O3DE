/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/NativeRuntime.h>

#include <Jolt/Allocator.h>
#include <Jolt/CustomConvexShape.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Memory/AllocatorInstance.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/parallel/mutex.h>

#include <Jolt/Jolt.h>
#include <Jolt/ConfigurationString.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h>
#include <Jolt/Physics/Hair/RegisterHair.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstring>

namespace Jolt
{
    namespace
    {
        constexpr size_t TraceBufferSize = 4'096;

        AZStd::mutex RuntimeMutex;
        AZ::u32 RuntimeReferenceCount = 0;
        float RuntimeSoftBodyTriangleThickness = 0.1f;

        void* AllocateNativeMemory(
            const size_t size)
        {
            return AZ::AllocatorInstance<NativeAllocator>::Get().Allocate(
                size,
                JPH_DEFAULT_ALLOCATE_ALIGNMENT,
                0,
                "Jolt Native");
        }

        void* ReallocateNativeMemory(
            void* memory,
            [[maybe_unused]] const size_t oldSize,
            const size_t newSize)
        {
            return AZ::AllocatorInstance<NativeAllocator>::Get().ReAllocate(
                memory,
                newSize,
                JPH_DEFAULT_ALLOCATE_ALIGNMENT);
        }

        void FreeNativeMemory(
            void* memory)
        {
            AZ::AllocatorInstance<NativeAllocator>::Get().DeAllocate(memory);
        }

        void* AllocateAlignedNativeMemory(
            const size_t size,
            const size_t alignment)
        {
            return AZ::AllocatorInstance<NativeAllocator>::Get().Allocate(
                size,
                alignment,
                0,
                "Jolt Native Aligned");
        }

        void FreeAlignedNativeMemory(
            void* memory)
        {
            AZ::AllocatorInstance<NativeAllocator>::Get().DeAllocate(memory);
        }

        void TraceNativeMessage(
            const char* format,
            ...)
        {
            char message[TraceBufferSize]{};
            va_list arguments;
            va_start(arguments, format);
            azvsnprintf(message, AZ_ARRAY_SIZE(message), format, arguments);
            va_end(arguments);
            AZ_TracePrintf("Jolt", "%s", message);
        }

#if defined(JPH_ENABLE_ASSERTS)
        bool ReportNativeAssertion(
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
                "Native assertion failed: %s; %s (%s:%u)",
                reportedExpression,
                reportedMessage,
                reportedFileName,
                lineNumber);
            return true;
        }
#endif

        bool AcquireNativeRuntime(
            const float softBodyTriangleThickness)
        {
            AZStd::lock_guard lock(RuntimeMutex);
            if (!AZ::IsFiniteFloat(softBodyTriangleThickness)
                || softBodyTriangleThickness < 0.0f)
            {
                AZ_Error("Jolt", false, "Soft-body triangle thickness must be finite and non-negative.");
                return false;
            }

            if (RuntimeReferenceCount > 0)
            {
                if (softBodyTriangleThickness != RuntimeSoftBodyTriangleThickness)
                {
                    AZ_Error(
                        "Jolt",
                        false,
                        "All live Jolt systems must use the same soft-body triangle thickness.");
                    return false;
                }

                ++RuntimeReferenceCount;
                return true;
            }

            JPH::Allocate = AllocateNativeMemory;
            JPH::Reallocate = ReallocateNativeMemory;
            JPH::Free = FreeNativeMemory;
            JPH::AlignedAllocate = AllocateAlignedNativeMemory;
            JPH::AlignedFree = FreeAlignedNativeMemory;
            JPH::Trace = TraceNativeMessage;
#if defined(JPH_ENABLE_ASSERTS)
            JPH::AssertFailed = ReportNativeAssertion;
#endif

            if (!JPH::VerifyJoltVersionID())
            {
                AZ_Error("Jolt", false, "The loaded native library does not match the provider's build configuration.");
                return false;
            }

            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
            RegisterCustomConvexShapeType();
            JPH::RegisterHair();
            JPH::CollideSoftBodyVerticesVsTriangles::sTriangleThickness = softBodyTriangleThickness;
            RuntimeSoftBodyTriangleThickness = softBodyTriangleThickness;
            RuntimeReferenceCount = 1;
            return true;
        }

        void ReleaseNativeRuntime()
        {
            AZStd::lock_guard lock(RuntimeMutex);
            AZ_Assert(RuntimeReferenceCount > 0, "Jolt native runtime reference count underflow.");
            --RuntimeReferenceCount;
            if (RuntimeReferenceCount > 0)
            {
                return;
            }

            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    } // namespace

    AZ::u64 GetNativeBuildFingerprint()
    {
        static const AZ::u64 fingerprint = []
        {
            constexpr AZ::u32 version = JPH_VERSION_MAJOR << 16
                | JPH_VERSION_MINOR << 8
                | JPH_VERSION_PATCH;
            const char* configuration = JPH::GetConfigurationString();
            const AZ::HashValue64 configurationHash = AZ::TypeHash64(
                reinterpret_cast<const AZ::u8*>(configuration),
                std::strlen(configuration));
            constexpr AZStd::string_view sourceRevision = JOLT_NATIVE_SOURCE_REVISION;
            const AZ::HashValue64 sourceRevisionHash = AZ::TypeHash64(
                reinterpret_cast<const AZ::u8*>(sourceRevision.data()),
                sourceRevision.size());
            constexpr AZStd::string_view patchRevision = JOLT_NATIVE_PATCH_REVISION;
            const AZ::HashValue64 patchRevisionHash = AZ::TypeHash64(
                reinterpret_cast<const AZ::u8*>(patchRevision.data()),
                patchRevision.size());
            constexpr AZStd::string_view patchHash = JOLT_NATIVE_PATCH_HASH;
            const AZ::HashValue64 patchContentHash = AZ::TypeHash64(
                reinterpret_cast<const AZ::u8*>(patchHash.data()),
                patchHash.size());

            AZ::HashValue64 buildHash = AZ::TypeHash64(version, configurationHash);
            buildHash = AZ::TypeHash64(sourceRevisionHash, buildHash);
            buildHash = AZ::TypeHash64(patchRevisionHash, buildHash);
            buildHash = AZ::TypeHash64(patchContentHash, buildHash);
            return static_cast<AZ::u64>(buildHash);
        }();
        return fingerprint;
    }

    NativeRuntime::NativeRuntime(
        const float softBodyTriangleThickness)
        : m_initialized(AcquireNativeRuntime(softBodyTriangleThickness))
    {
    }

    NativeRuntime::~NativeRuntime()
    {
        if (m_initialized)
        {
            ReleaseNativeRuntime();
        }
    }

    RuntimeInfo NativeRuntime::GetRuntimeInfo() const
    {
        RuntimeInfo runtimeInfo{
            .m_version = {
                .m_major = JPH_VERSION_MAJOR,
                .m_minor = JPH_VERSION_MINOR,
                .m_patch = JPH_VERSION_PATCH,
            },
            .m_buildFingerprint = GetNativeBuildFingerprint(),
            .m_configuration = JPH::GetConfigurationString(),
            .m_patchHash = JOLT_NATIVE_PATCH_HASH,
            .m_patchRevision = JOLT_NATIVE_PATCH_REVISION,
            .m_sourceRevision = JOLT_NATIVE_SOURCE_REVISION,
            .m_hairDeterminism = DeterminismCertification::SameBinary,
            .m_physicsDeterminism = DeterminismCertification::None,
            .m_precision = Precision::Single,
            .m_simdLevel = SimdLevel::Scalar,
        };

#if defined(JPH_DOUBLE_PRECISION)
        runtimeInfo.m_precision = Precision::Double;
#endif
#if defined(JPH_CPU_WASM) && defined(JPH_USE_SSE)
        runtimeInfo.m_simdLevel = SimdLevel::WasmSimd;
#elif defined(JPH_USE_AVX512)
        runtimeInfo.m_simdLevel = SimdLevel::Avx512;
#elif defined(JPH_USE_AVX2)
        runtimeInfo.m_simdLevel = SimdLevel::Avx2;
#elif defined(JPH_USE_AVX)
        runtimeInfo.m_simdLevel = SimdLevel::Avx;
#elif defined(JPH_USE_SSE4_2)
        runtimeInfo.m_simdLevel = SimdLevel::Sse42;
#elif defined(JPH_USE_SSE4_1)
        runtimeInfo.m_simdLevel = SimdLevel::Sse41;
#elif defined(JPH_USE_SSE)
        runtimeInfo.m_simdLevel = SimdLevel::Sse2;
#elif defined(JPH_USE_NEON)
        runtimeInfo.m_simdLevel = SimdLevel::Neon;
#elif defined(JPH_USE_RVV)
        runtimeInfo.m_simdLevel = SimdLevel::Rvv;
#endif
#if defined(JPH_CROSS_PLATFORM_DETERMINISTIC)
        runtimeInfo.m_physicsDeterminism = DeterminismCertification::CrossPlatform;
#else
        runtimeInfo.m_physicsDeterminism = DeterminismCertification::SameBinary;
#endif
#if defined(JPH_EXTERNAL_PROFILE)
        runtimeInfo.m_detailedProfiling = true;
#endif
#if defined(JPH_TRACK_SIMULATION_STATS)
        runtimeInfo.m_simulationStatistics = true;
#endif

        return runtimeInfo;
    }
} // namespace Jolt
