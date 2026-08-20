/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/NativeRuntime.h>

#include <Jolt/Allocator.h>
#include <Jolt/CustomConvexShape.h>
#include <Jolt/CustomShapeInternal.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Memory/AllocatorInstance.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/parallel/atomic.h>
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

#if defined(_M_IX86) || defined(_M_X64)
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

#if (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)) \
    && !defined(JPH_USE_SSE4_1)
#error "The Jolt provider requires SSE4.1 on x86."
#endif

#if defined(JPH_USE_AVX) \
    || defined(JPH_USE_AVX2) \
    || defined(JPH_USE_AVX512) \
    || defined(JPH_USE_F16C) \
    || defined(JPH_USE_FMADD) \
    || defined(JPH_USE_LZCNT) \
    || defined(JPH_USE_SSE4_2) \
    || defined(JPH_USE_TZCNT)
#error "The Jolt provider's x86 instruction set must remain exactly SSE4.1."
#endif

namespace Jolt
{
    namespace
    {
        constexpr size_t TraceBufferSize = 4'096;

        AZStd::mutex RuntimeMutex;
        AZ::u32 RuntimeReferenceCount = 0;
        float RuntimeSoftBodyTriangleThickness = 0.1f;

        AZStd::atomic_uint32_t NativeMemoryStatisticsReferenceCount{0};
        AZStd::atomic_uint64_t NativeAllocationCount{0};
        AZStd::atomic_uint64_t NativeFreeCount{0};
        AZStd::atomic<AZ::u64> NativePeakAllocatedBytes{0};
        AZStd::atomic_uint64_t NativeReallocationCount{0};
        AZStd::mutex NativeMemoryStatisticsMutex;

        [[nodiscard]]
        bool SupportsRequiredInstructionSet()
        {
#if defined(_M_IX86) || defined(_M_X64)
            int registers[4]{};
            __cpuid(registers, 0);
            if (registers[0] < 1)
            {
                return false;
            }

            __cpuidex(registers, 1, 0);
            return (registers[2] & (1 << 19)) != 0;
#elif defined(__i386__) || defined(__x86_64__)
            unsigned int eax = 0;
            unsigned int ebx = 0;
            unsigned int ecx = 0;
            unsigned int edx = 0;
            return __get_cpuid(1, &eax, &ebx, &ecx, &edx) && (ecx & bit_SSE4_1) != 0;
#else
            return true;
#endif
        }

        [[nodiscard]]
        bool IsNativeMemoryStatisticsEnabled()
        {
            return NativeMemoryStatisticsReferenceCount.load(AZStd::memory_order_relaxed) > 0;
        }

        void UpdateNativePeakAllocatedBytes()
        {
            const AZ::u64 allocatedBytes = AZ::AllocatorInstance<NativeAllocator>::Get().NumAllocatedBytes();
            AZ::u64 peakBytes = NativePeakAllocatedBytes.load(AZStd::memory_order_relaxed);
            while (peakBytes < allocatedBytes
                && !NativePeakAllocatedBytes.compare_exchange_weak(
                    peakBytes,
                    allocatedBytes,
                    AZStd::memory_order_relaxed))
            {
            }
        }

        void* AllocateNativeMemory(
            const size_t size)
        {
            void* memory = AZ::AllocatorInstance<NativeAllocator>::Get().Allocate(
                size,
                JPH_DEFAULT_ALLOCATE_ALIGNMENT,
                0,
                "Jolt Native");
            if (IsNativeMemoryStatisticsEnabled())
            {
                NativeAllocationCount.fetch_add(1, AZStd::memory_order_relaxed);
                UpdateNativePeakAllocatedBytes();
            }
            return memory;
        }

        void* ReallocateNativeMemory(
            void* memory,
            [[maybe_unused]] const size_t oldSize,
            const size_t newSize)
        {
            void* reallocatedMemory = AZ::AllocatorInstance<NativeAllocator>::Get().ReAllocate(
                memory,
                newSize,
                JPH_DEFAULT_ALLOCATE_ALIGNMENT);
            if (IsNativeMemoryStatisticsEnabled())
            {
                NativeReallocationCount.fetch_add(1, AZStd::memory_order_relaxed);
                UpdateNativePeakAllocatedBytes();
            }
            return reallocatedMemory;
        }

        void FreeNativeMemory(
            void* memory)
        {
            AZ::AllocatorInstance<NativeAllocator>::Get().DeAllocate(memory);
            if (memory && IsNativeMemoryStatisticsEnabled())
            {
                NativeFreeCount.fetch_add(1, AZStd::memory_order_relaxed);
            }
        }

        void* AllocateAlignedNativeMemory(
            const size_t size,
            const size_t alignment)
        {
            void* memory = AZ::AllocatorInstance<NativeAllocator>::Get().Allocate(
                size,
                alignment,
                0,
                "Jolt Native Aligned");
            if (IsNativeMemoryStatisticsEnabled())
            {
                NativeAllocationCount.fetch_add(1, AZStd::memory_order_relaxed);
                UpdateNativePeakAllocatedBytes();
            }
            return memory;
        }

        void FreeAlignedNativeMemory(
            void* memory)
        {
            AZ::AllocatorInstance<NativeAllocator>::Get().DeAllocate(memory);
            if (memory && IsNativeMemoryStatisticsEnabled())
            {
                NativeFreeCount.fetch_add(1, AZStd::memory_order_relaxed);
            }
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

#ifdef JPH_ENABLE_ASSERTS
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
            if (!SupportsRequiredInstructionSet())
            {
                AZ_Error("Jolt", false, "Jolt requires a processor with SSE4.1 support.");
                return false;
            }

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
#ifdef JPH_ENABLE_ASSERTS
            JPH::AssertFailed = ReportNativeAssertion;
#endif

            if (!JPH::VerifyJoltVersionID())
            {
                AZ_Error("Jolt", false, "The loaded native library does not match the provider's build configuration.");
                return false;
            }

            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
            RegisterCustomShapeType();
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
            constexpr AZ::u32 version = JPH_VERSION_MAJOR << 16 | JPH_VERSION_MINOR << 8 | JPH_VERSION_PATCH;
            const char* configuration = JPH::GetConfigurationString();
            const AZ::HashValue64 configurationHash = AZ::TypeHash64(reinterpret_cast<const AZ::u8*>(configuration), std::strlen(configuration));
            AZ::HashValue64 buildHash = AZ::TypeHash64(version, configurationHash);
            buildHash = AZ::TypeHash64(JOLT_NATIVE_PATCH_FINGERPRINT, buildHash);
            return static_cast<AZ::u64>(buildHash);
        }();
        return fingerprint;
    }

    void AcquireNativeMemoryStatistics()
    {
        AZStd::lock_guard lock(NativeMemoryStatisticsMutex);
        const AZ::u32 previousReferenceCount = NativeMemoryStatisticsReferenceCount.load(
            AZStd::memory_order_relaxed);
        if (previousReferenceCount == 0)
        {
            NativeAllocationCount.store(0, AZStd::memory_order_relaxed);
            NativeFreeCount.store(0, AZStd::memory_order_relaxed);
            NativeReallocationCount.store(0, AZStd::memory_order_relaxed);
            NativePeakAllocatedBytes.store(
                AZ::AllocatorInstance<NativeAllocator>::Get().NumAllocatedBytes(),
                AZStd::memory_order_relaxed);
        }
        NativeMemoryStatisticsReferenceCount.store(
            previousReferenceCount + 1,
            AZStd::memory_order_release);
    }

    void ReleaseNativeMemoryStatistics()
    {
        AZStd::lock_guard lock(NativeMemoryStatisticsMutex);
        const AZ::u32 previousReferenceCount = NativeMemoryStatisticsReferenceCount.load(
            AZStd::memory_order_relaxed);
        AZ_Assert(previousReferenceCount > 0, "Jolt native memory statistics reference count underflowed.");
        if (previousReferenceCount == 0)
        {
            return;
        }
        NativeMemoryStatisticsReferenceCount.store(
            previousReferenceCount - 1,
            AZStd::memory_order_release);
    }

    NativeMemoryStatistics GetNativeMemoryStatistics(
        const bool reset)
    {
        NativeMemoryStatistics statistics{
            .m_allocatedBytes = AZ::AllocatorInstance<NativeAllocator>::Get().NumAllocatedBytes(),
        };
        if (reset)
        {
            statistics.m_peakAllocatedBytes = NativePeakAllocatedBytes.exchange(
                statistics.m_allocatedBytes,
                AZStd::memory_order_relaxed);
            statistics.m_allocationCount = NativeAllocationCount.exchange(0, AZStd::memory_order_relaxed);
            statistics.m_freeCount = NativeFreeCount.exchange(0, AZStd::memory_order_relaxed);
            statistics.m_reallocationCount = NativeReallocationCount.exchange(0, AZStd::memory_order_relaxed);
            return statistics;
        }

        statistics.m_peakAllocatedBytes = NativePeakAllocatedBytes.load(AZStd::memory_order_relaxed);
        statistics.m_allocationCount = NativeAllocationCount.load(AZStd::memory_order_relaxed);
        statistics.m_freeCount = NativeFreeCount.load(AZStd::memory_order_relaxed);
        statistics.m_reallocationCount = NativeReallocationCount.load(AZStd::memory_order_relaxed);
        return statistics;
    }

    float GetSoftBodyTriangleThickness()
    {
        return JPH::CollideSoftBodyVerticesVsTriangles::sTriangleThickness;
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
        RuntimeInfo runtimeInfo;
        runtimeInfo.m_version = {
            .m_major = JPH_VERSION_MAJOR,
            .m_minor = JPH_VERSION_MINOR,
            .m_patch = JPH_VERSION_PATCH,
        };
        runtimeInfo.m_buildFingerprint = GetNativeBuildFingerprint();
        runtimeInfo.m_configuration = JPH::GetConfigurationString();
        runtimeInfo.m_hairDeterminism = DeterminismCertification::SameBinary;
        runtimeInfo.m_precision = Precision::Single;
        runtimeInfo.m_simdLevel = SimdLevel::Scalar;

#ifdef JPH_DOUBLE_PRECISION
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
#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
        runtimeInfo.m_physicsDeterminism = DeterminismCertification::CrossPlatform;
#else
        runtimeInfo.m_physicsDeterminism = DeterminismCertification::SameBinary;
#endif
#ifdef JPH_EXTERNAL_PROFILE
        runtimeInfo.m_detailedProfiling = true;
#endif
#ifdef JPH_TRACK_BROADPHASE_STATS
        runtimeInfo.m_broadPhaseStatistics = true;
#endif
#ifdef JPH_TRACK_NARROWPHASE_STATS
        runtimeInfo.m_narrowPhaseStatistics = true;
#endif
#ifdef JPH_TRACK_SIMULATION_STATS
        runtimeInfo.m_simulationStatistics = true;
#endif

        return runtimeInfo;
    }
} // namespace Jolt
