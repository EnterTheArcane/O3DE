/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/Benchmark/BenchmarkEnvironment.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/PlatformDef.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#if defined(AZ_PLATFORM_WINDOWS)
#include <AzCore/PlatformIncl.h>
#elif defined(AZ_PLATFORM_LINUX)
#include <sched.h>
#endif

namespace AZ::Test
{
    namespace
    {
#if defined(AZ_PLATFORM_LINUX)
        [[nodiscard]]
        bool IsPhysicalCoreSelected(
            const AZ::u32 processorIndex,
            const cpu_set_t& selectedProcessors)
        {
            char path[128]{};
            std::snprintf(
                path,
                AZ_ARRAY_SIZE(path),
                "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list",
                processorIndex);

            std::FILE* siblingsFile = std::fopen(path, "r");
            if (!siblingsFile)
            {
                return false;
            }

            char siblings[1024]{};
            const bool readSucceeded = std::fgets(
                siblings,
                AZ_ARRAY_SIZE(siblings),
                siblingsFile);
            std::fclose(siblingsFile);
            if (!readSucceeded)
            {
                return false;
            }

            const char* cursor = siblings;
            while (*cursor)
            {
                while (*cursor
                    && (*cursor < '0' || *cursor > '9'))
                {
                    ++cursor;
                }
                if (!*cursor)
                {
                    break;
                }

                char* rangeEnd = nullptr;
                const unsigned long firstProcessor = std::strtoul(
                    cursor,
                    &rangeEnd,
                    10);
                cursor = rangeEnd;
                unsigned long lastProcessor = firstProcessor;
                if (*cursor == '-')
                {
                    ++cursor;
                    lastProcessor = std::strtoul(
                        cursor,
                        &rangeEnd,
                        10);
                    cursor = rangeEnd;
                }

                const unsigned long cappedLastProcessor = AZStd::min(
                    lastProcessor,
                    static_cast<unsigned long>(CPU_SETSIZE - 1));
                for (unsigned long sibling = firstProcessor;
                    sibling <= cappedLastProcessor;
                    ++sibling)
                {
                    if (CPU_ISSET(static_cast<int>(sibling), &selectedProcessors))
                    {
                        return true;
                    }
                }
            }

            return false;
        }
#endif
    } // namespace

    struct ScopedBenchmarkCpuAffinity::Implementation final
    {
        explicit Implementation(
            const AZ::u32 requestedProcessorCount)
        {
#if defined(AZ_PLATFORM_WINDOWS)
            if (requestedProcessorCount == 0)
            {
                return;
            }

            HANDLE process = GetCurrentProcess();
            m_previousPowerThrottlingState.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            if (GetProcessInformation(
                    process,
                    ProcessPowerThrottling,
                    &m_previousPowerThrottlingState,
                    sizeof(m_previousPowerThrottlingState)))
            {
                PROCESS_POWER_THROTTLING_STATE highQualityOfService{};
                highQualityOfService.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
                highQualityOfService.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
                if (SetProcessInformation(
                        process,
                        ProcessPowerThrottling,
                        &highQualityOfService,
                        sizeof(highQualityOfService)))
                {
                    m_isHighQualityOfService = true;
                    m_restorePowerThrottling = true;
                }
            }

            DWORD_PTR systemAffinityMask = 0;
            if (!GetProcessAffinityMask(process, &m_previousProcessAffinityMask, &systemAffinityMask))
            {
                return;
            }
            m_previousProcessAffinityMask &= systemAffinityMask;

            DWORD byteCount = 0;
            GetLogicalProcessorInformationEx(RelationAll, nullptr, &byteCount);
            if (byteCount == 0)
            {
                return;
            }

            AZStd::vector<AZ::u8> informationBytes(byteCount);
            auto* information = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(informationBytes.data());
            if (!GetLogicalProcessorInformationEx(RelationAll, information, &byteCount))
            {
                return;
            }

            struct Processor final
            {
                DWORD_PTR m_affinityMask = 0;
                AZ::u8 m_efficiencyClass = 0;
            };

            AZStd::vector<Processor> processors;
            AZStd::vector<DWORD_PTR> cacheDomains;
            AZ::u8 lastLevelCache = 0;
            constexpr size_t informationHeaderSize = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
            size_t byteOffset = 0;
            while (byteOffset + informationHeaderSize <= byteCount)
            {
                const auto* processorInformation = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                    informationBytes.data() + byteOffset);
                if (processorInformation->Size < informationHeaderSize
                    || byteOffset + processorInformation->Size > byteCount)
                {
                    return;
                }

                if (processorInformation->Relationship == RelationProcessorCore)
                {
                    const PROCESSOR_RELATIONSHIP& processor = processorInformation->Processor;
                    for (WORD groupIndex = 0; groupIndex < processor.GroupCount; ++groupIndex)
                    {
                        if (processor.GroupMask[groupIndex].Group == 0)
                        {
                            const DWORD_PTR affinityMask = processor.GroupMask[groupIndex].Mask
                                & m_previousProcessAffinityMask;
                            if (affinityMask != 0)
                            {
                                processors.push_back({
                                    .m_affinityMask = affinityMask,
                                    .m_efficiencyClass = processor.EfficiencyClass,
                                });
                            }
                        }
                    }
                }
                else if (processorInformation->Relationship == RelationCache)
                {
                    const CACHE_RELATIONSHIP& cache = processorInformation->Cache;
                    if (cache.Type == CacheUnified || cache.Type == CacheData)
                    {
                        if (cache.Level > lastLevelCache)
                        {
                            cacheDomains.clear();
                            lastLevelCache = cache.Level;
                        }
                        if (cache.Level == lastLevelCache)
                        {
                            for (WORD groupIndex = 0; groupIndex < cache.GroupCount; ++groupIndex)
                            {
                                if (cache.GroupMasks[groupIndex].Group == 0)
                                {
                                    const DWORD_PTR affinityMask = cache.GroupMasks[groupIndex].Mask
                                        & m_previousProcessAffinityMask;
                                    if (affinityMask != 0)
                                    {
                                        cacheDomains.push_back(affinityMask);
                                    }
                                }
                            }
                        }
                    }
                }

                byteOffset += processorInformation->Size;
            }

            AZStd::sort(
                processors.begin(),
                processors.end(),
                [](const Processor& left, const Processor& right)
                {
                    if (left.m_efficiencyClass != right.m_efficiencyClass)
                    {
                        return left.m_efficiencyClass > right.m_efficiencyClass;
                    }
                    return left.m_affinityMask < right.m_affinityMask;
                });

            if (cacheDomains.empty())
            {
                cacheDomains.push_back(m_previousProcessAffinityMask);
            }

            DWORD_PTR selectedAffinityMask = 0;
            AZ::u32 selectedProcessorCount = 0;
            AZ::u32 selectedEfficiencyScore = 0;
            AZStd::vector<DWORD_PTR> selectedProcessorAffinityMasks;
            for (const DWORD_PTR cacheDomain : cacheDomains)
            {
                DWORD_PTR candidateAffinityMask = 0;
                AZ::u32 candidateProcessorCount = 0;
                AZ::u32 candidateEfficiencyScore = 0;
                AZStd::vector<DWORD_PTR> candidateProcessorAffinityMasks;
                for (const Processor& processor : processors)
                {
                    const DWORD_PTR availableAffinityMask = processor.m_affinityMask & cacheDomain;
                    if (availableAffinityMask == 0)
                    {
                        continue;
                    }

                    const DWORD_PTR processorAffinityMask = availableAffinityMask & (~availableAffinityMask + 1);
                    candidateAffinityMask |= processorAffinityMask;
                    candidateProcessorAffinityMasks.push_back(processorAffinityMask);
                    candidateEfficiencyScore += processor.m_efficiencyClass;
                    ++candidateProcessorCount;
                    if (candidateProcessorCount == requestedProcessorCount)
                    {
                        break;
                    }
                }

                if (candidateProcessorCount == requestedProcessorCount
                    && (selectedProcessorCount != requestedProcessorCount
                        || candidateEfficiencyScore > selectedEfficiencyScore))
                {
                    selectedAffinityMask = candidateAffinityMask;
                    selectedProcessorCount = candidateProcessorCount;
                    selectedEfficiencyScore = candidateEfficiencyScore;
                    selectedProcessorAffinityMasks = AZStd::move(candidateProcessorAffinityMasks);
                }
            }

            if (selectedProcessorCount != requestedProcessorCount)
            {
                selectedAffinityMask = 0;
                selectedProcessorCount = 0;
                selectedProcessorAffinityMasks.clear();
                for (const Processor& processor : processors)
                {
                    const DWORD_PTR processorAffinityMask = processor.m_affinityMask & (~processor.m_affinityMask + 1);
                    selectedAffinityMask |= processorAffinityMask;
                    selectedProcessorAffinityMasks.push_back(processorAffinityMask);
                    ++selectedProcessorCount;
                    if (selectedProcessorCount == requestedProcessorCount)
                    {
                        break;
                    }
                }
            }

            if (selectedProcessorCount == 0)
            {
                return;
            }

            if (selectedAffinityMask != m_previousProcessAffinityMask)
            {
                if (!SetProcessAffinityMask(process, selectedAffinityMask))
                {
                    return;
                }
                m_restoreProcessAffinity = true;
            }

            m_processorCount = selectedProcessorCount;
            m_workerProcessorAffinities.reserve(selectedProcessorAffinityMasks.size());
            for (const DWORD_PTR processorAffinityMask : selectedProcessorAffinityMasks)
            {
                m_workerProcessorAffinities.push_back(static_cast<AZ::u64>(processorAffinityMask));
            }
            m_isConstrained = true;

            if (DuplicateHandle(
                    process,
                    GetCurrentThread(),
                    process,
                    &m_benchmarkThreadHandle,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS))
            {
                m_previousThreadAffinityMask = SetThreadAffinityMask(
                    m_benchmarkThreadHandle,
                    selectedProcessorAffinityMasks.front());
                if (m_previousThreadAffinityMask == 0)
                {
                    CloseHandle(m_benchmarkThreadHandle);
                    m_benchmarkThreadHandle = nullptr;
                }
            }
#elif defined(AZ_PLATFORM_LINUX)
            if (requestedProcessorCount == 0
                || sched_getaffinity(
                    0,
                    sizeof(m_previousThreadAffinity),
                    &m_previousThreadAffinity) != 0)
            {
                return;
            }

            cpu_set_t selectedProcessors;
            CPU_ZERO(&selectedProcessors);
            AZStd::vector<int> selectedProcessorIndices;
            selectedProcessorIndices.reserve(requestedProcessorCount);
            for (int processorIndex = 0;
                processorIndex < CPU_SETSIZE
                    && m_processorCount < requestedProcessorCount;
                ++processorIndex)
            {
                if (CPU_ISSET(processorIndex, &m_previousThreadAffinity)
                    && !IsPhysicalCoreSelected(
                        aznumeric_cast<AZ::u32>(processorIndex),
                        selectedProcessors))
                {
                    CPU_SET(processorIndex, &selectedProcessors);
                    selectedProcessorIndices.push_back(processorIndex);
                    ++m_processorCount;
                }
            }

            for (int processorIndex = 0;
                processorIndex < CPU_SETSIZE
                    && m_processorCount < requestedProcessorCount;
                ++processorIndex)
            {
                if (CPU_ISSET(processorIndex, &m_previousThreadAffinity)
                    && !CPU_ISSET(processorIndex, &selectedProcessors))
                {
                    CPU_SET(processorIndex, &selectedProcessors);
                    selectedProcessorIndices.push_back(processorIndex);
                    ++m_processorCount;
                }
            }

            if (m_processorCount == 0)
            {
                return;
            }

            for (const int processorIndex : selectedProcessorIndices)
            {
                m_workerProcessorAffinities.push_back(
                    aznumeric_cast<AZ::u64>(processorIndex));
            }

            cpu_set_t benchmarkThreadProcessors = selectedProcessors;
            if (m_workerProcessorAffinities.size() == selectedProcessorIndices.size())
            {
                CPU_ZERO(&benchmarkThreadProcessors);
                CPU_SET(selectedProcessorIndices.front(), &benchmarkThreadProcessors);
            }

            if (!CPU_EQUAL(&benchmarkThreadProcessors, &m_previousThreadAffinity))
            {
                if (sched_setaffinity(
                        0,
                        sizeof(benchmarkThreadProcessors),
                        &benchmarkThreadProcessors) != 0)
                {
                    m_workerProcessorAffinities.clear();
                    m_processorCount = 0;
                    return;
                }
                m_restoreThreadAffinity = true;
            }
            m_isConstrained = true;
#else
            AZ_UNUSED(requestedProcessorCount);
#endif
        }

        ~Implementation()
        {
#if defined(AZ_PLATFORM_WINDOWS)
            if (m_restorePowerThrottling)
            {
                SetProcessInformation(
                    GetCurrentProcess(),
                    ProcessPowerThrottling,
                    &m_previousPowerThrottlingState,
                    sizeof(m_previousPowerThrottlingState));
            }
            if (m_restoreProcessAffinity)
            {
                SetProcessAffinityMask(GetCurrentProcess(), m_previousProcessAffinityMask);
            }
            if (m_benchmarkThreadHandle)
            {
                SetThreadAffinityMask(m_benchmarkThreadHandle, m_previousThreadAffinityMask);
                CloseHandle(m_benchmarkThreadHandle);
            }
#elif defined(AZ_PLATFORM_LINUX)
            if (m_restoreThreadAffinity)
            {
                sched_setaffinity(
                    0,
                    sizeof(m_previousThreadAffinity),
                    &m_previousThreadAffinity);
            }
#endif
        }

        AZ::u32 m_processorCount = 0;
        AZStd::vector<AZ::u64> m_workerProcessorAffinities;
        bool m_isConstrained = false;
        bool m_isHighQualityOfService = false;

#if defined(AZ_PLATFORM_WINDOWS)
        PROCESS_POWER_THROTTLING_STATE m_previousPowerThrottlingState{};
        DWORD_PTR m_previousProcessAffinityMask = 0;
        DWORD_PTR m_previousThreadAffinityMask = 0;
        HANDLE m_benchmarkThreadHandle = nullptr;
        bool m_restorePowerThrottling = false;
        bool m_restoreProcessAffinity = false;
#elif defined(AZ_PLATFORM_LINUX)
        cpu_set_t m_previousThreadAffinity{};
        bool m_restoreThreadAffinity = false;
#endif
    };

    ScopedBenchmarkCpuAffinity::ScopedBenchmarkCpuAffinity(
        const AZ::u32 processorCount)
        : m_implementation(AZStd::make_unique<Implementation>(processorCount))
    {
    }

    ScopedBenchmarkCpuAffinity::~ScopedBenchmarkCpuAffinity() = default;

    AZ::u32 ScopedBenchmarkCpuAffinity::GetProcessorCount() const
    {
        return m_implementation->m_processorCount;
    }

    bool ScopedBenchmarkCpuAffinity::IsConstrained() const
    {
        return m_implementation->m_isConstrained;
    }

    bool ScopedBenchmarkCpuAffinity::IsHighQualityOfService() const
    {
        return m_implementation->m_isHighQualityOfService;
    }

    void ScopedBenchmarkCpuAffinity::ConfigureJobManagerThreads(
        AZ::JobManagerDesc& descriptor,
        const AZ::u32 workerCount) const
    {
        descriptor.m_workerThreads.reserve(workerCount);
        for (AZ::u32 workerIndex = 0; workerIndex < workerCount; ++workerIndex)
        {
            int processorAffinityMask = -1;
            if (!m_implementation->m_workerProcessorAffinities.empty())
            {
                const AZ::u64 nativeProcessorAffinity = m_implementation->m_workerProcessorAffinities[
                    (workerIndex + 1) % m_implementation->m_workerProcessorAffinities.size()];
                if (nativeProcessorAffinity <= aznumeric_cast<AZ::u64>(AZStd::numeric_limits<int>::max()))
                {
                    processorAffinityMask = aznumeric_cast<int>(nativeProcessorAffinity);
                }
            }
            descriptor.m_workerThreads.emplace_back(processorAffinityMask);
        }
    }

    const ScopedBenchmarkCpuAffinity& GetBenchmarkCpuAffinity()
    {
        constexpr AZ::u32 maximumProcessorCount = 8;
        static const ScopedBenchmarkCpuAffinity cpuAffinity(maximumProcessorCount);
        return cpuAffinity;
    }
} // namespace AZ::Test
