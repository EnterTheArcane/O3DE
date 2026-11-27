/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/time.h>
#include <AzCore/PlatformIncl.h>
#include <AzCore/std/string/fixed_string.h>

#if defined(_M_X64) && !defined(_M_ARM64EC)
#include <intrin.h>
#endif

namespace AZStd
{
    AZStd::sys_time_t GetTimeTicksPerSecond()
    {
        static AZStd::sys_time_t freq = 0;
        if (freq == 0)
        {
            QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        }
        return freq;
    }

    AZStd::sys_time_t GetTimeNowTicks()
    {
        AZStd::sys_time_t timeNow;
        QueryPerformanceCounter((LARGE_INTEGER*)&timeNow);
        return timeNow;
    }

    AZStd::sys_time_t GetTimeNowMicroSecond()
    {
        AZStd::sys_time_t timeNowMicroSecond;
        // NOTE: The default line below was not working on systems with smaller TicksPerSecond() values (like in Windows7, for example)
        // So we are now spreading the division between the numerator and denominator to maintain better precision
        timeNowMicroSecond = (GetTimeNowTicks() * 1000) / (GetTimeTicksPerSecond() / 1000);
        return timeNowMicroSecond;
    }

    AZStd::sys_time_t GetTimeNowSecond()
    {
        AZStd::sys_time_t timeNowSecond;
        // Using get tick count, since it's more stable for longer time measurements.
        timeNowSecond = GetTickCount() / 1000;
        return timeNowSecond;
    }

    // Keep the diagnostic buffer off the successful query's stack frame.
    static AZ_NOINLINE AZStd::chrono::microseconds ReportCpuThreadTimeQueryFailure(const char* failedFunction, DWORD error)
    {
        auto FormatFunc = [error](wchar_t* buffer, size_t size)
        {
            return ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, buffer, static_cast<DWORD>(size), nullptr);
        };
        constexpr size_t messageBufferSize = 1024;
        AZStd::fixed_wstring<messageBufferSize> errorMessage;
        errorMessage.resize_and_overwrite(errorMessage.capacity(), AZStd::move(FormatFunc));
        AZ_Assert(false, "Failed to query CPU thread time through %s: "
            AZ_TRAIT_FORMAT_STRING_PRINTF_WSTRING "\n", failedFunction, errorMessage.c_str());
        return AZStd::chrono::microseconds::zero();
    }

    static AZ_NOINLINE AZStd::chrono::microseconds GetCpuThreadTimeFromThreadTimes(HANDLE thread)
    {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (!::GetThreadTimes(thread, &creationTime, &exitTime, &kernelTime, &userTime))
        {
            return ReportCpuThreadTimeQueryFailure("GetThreadTimes", ::GetLastError());
        }
        const uint64_t kernelTicks = (static_cast<uint64_t>(kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        const uint64_t userTicks = (static_cast<uint64_t>(userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
        // FILETIME counts intervals of 100 nanoseconds.
        return AZStd::chrono::microseconds((kernelTicks + userTicks) / 10);
    }

#if defined(_M_X64) && !defined(_M_ARM64EC)
    static AZ_NOINLINE uint64_t CalibrateTimestampCounterPerMicrosecond(HANDLE thread)
    {
        // Skip calibration when the runtime does not implement the cycle counter.
        unsigned long long unusedCycleCount;
        if (!::QueryThreadCycleTime(thread, &unusedCycleCount) && ::GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
        {
            return 0;
        }

        struct Sample
        {
            AZStd::chrono::steady_clock::time_point m_time;
            uint64_t m_ticks;
            AZStd::chrono::steady_clock::duration m_window;
        };

        const auto sampleCounter = []
        {
            Sample bestSample{ {}, 0, AZStd::chrono::steady_clock::duration::max() };
            constexpr int sampleCount = 3;
            // Use the closest clock readings to limit the effect of preemption during a sample.
            for (int sample = 0; sample < sampleCount; ++sample)
            {
                const auto before = AZStd::chrono::steady_clock::now();
                _mm_lfence();
                const uint64_t ticks = __rdtsc();
                _mm_lfence();
                const auto window = AZStd::chrono::steady_clock::now() - before;
                if (window < bestSample.m_window)
                {
                    bestSample = { before + window / 2, ticks, window };
                }
            }
            return bestSample;
        };

        const Sample start = sampleCounter();
        const auto deadline = start.m_time + AZStd::chrono::milliseconds(2);
        while (AZStd::chrono::steady_clock::now() < deadline)
        {
            YieldProcessor();
        }
        const Sample end = sampleCounter();
        const auto elapsed = AZStd::chrono::duration_cast<AZStd::chrono::microseconds>(end.m_time - start.m_time);
        if (end.m_ticks > start.m_ticks)
        {
            return (end.m_ticks - start.m_ticks) / elapsed.count();
        }
        return 0;
    }
#endif

    AZStd::chrono::microseconds GetCpuThreadTimeNowMicrosecond()
    {
        const HANDLE thread = ::GetCurrentThread();

#if defined(_M_X64) && !defined(_M_ARM64EC)
        // Preserve the fine-grained estimate from thread cycles. Windows does not guarantee a fixed cycle rate.
        static const uint64_t timestampCounterPerMicrosecond = CalibrateTimestampCounterPerMicrosecond(thread);
        if (timestampCounterPerMicrosecond != 0)
        {
            unsigned long long threadCycleCount;
            if (::QueryThreadCycleTime(thread, &threadCycleCount))
            {
                return AZStd::chrono::microseconds(threadCycleCount / timestampCounterPerMicrosecond);
            }
            const DWORD error = ::GetLastError();
            if (error != ERROR_CALL_NOT_IMPLEMENTED)
            {
                return ReportCpuThreadTimeQueryFailure("QueryThreadCycleTime", error);
            }
        }
#endif
        return GetCpuThreadTimeFromThreadTimes(thread);
    }
}
