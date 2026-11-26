/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/PlatformIncl.h>
#include <AzCore/Process/ProcessInfo.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <Psapi.h>

namespace UnitTest
{
    class ProcessInfoTestFixture
        : public LeakDetectionFixture
    {
    };

    TEST_F(ProcessInfoTestFixture, QueryMemInfo_Succeeds)
    {
        // The process can fault more pages between samples.
        const HANDLE process = ::GetCurrentProcess();
        PROCESS_MEMORY_COUNTERS before{};
        before.cb = sizeof(before);
        ASSERT_TRUE(::K32GetProcessMemoryInfo(process, &before, before.cb));

        AZ::ProcessMemInfo memInfo{};
        memInfo.m_pageFaultCount = -1;
        ASSERT_TRUE(AZ::QueryMemInfo(memInfo));

        PROCESS_MEMORY_COUNTERS after{};
        after.cb = sizeof(after);
        ASSERT_TRUE(::K32GetProcessMemoryInfo(process, &after, after.cb));

        EXPECT_GT(memInfo.m_workingSet, 0);
        EXPECT_GT(memInfo.m_peakWorkingSet, 0);
        EXPECT_GT(memInfo.m_pagefileUsage, 0);
        EXPECT_GT(memInfo.m_peakPagefileUsage, 0);
        if (before.PageFaultCount <= after.PageFaultCount)
        {
            EXPECT_GE(memInfo.m_pageFaultCount, before.PageFaultCount);
            EXPECT_LE(memInfo.m_pageFaultCount, after.PageFaultCount);
        }
        else
        {
            // The counter wrapped between samples.
            EXPECT_TRUE(
                (memInfo.m_pageFaultCount >= before.PageFaultCount && memInfo.m_pageFaultCount <= MAXDWORD)
                || (memInfo.m_pageFaultCount >= 0 && memInfo.m_pageFaultCount <= after.PageFaultCount))
                << "Page fault count " << memInfo.m_pageFaultCount << " is outside the wrapped range from "
                << before.PageFaultCount << " to " << after.PageFaultCount;
        }
    }
}
