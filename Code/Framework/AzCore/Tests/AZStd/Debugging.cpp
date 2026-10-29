/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/debugging.h>
#include <AzCore/std/typetraits/is_same.h>
#include <UnixLike/AzCore/std/internal/debugging_procfs.h>
#include <AzTest/AzTest.h>

namespace UnitTest
{
    static_assert(AZStd::is_same_v<decltype(&AZStd::breakpoint), void (*)() noexcept>);
    static_assert(AZStd::is_same_v<decltype(&AZStd::breakpoint_if_debugging), void (*)() noexcept>);
    static_assert(AZStd::is_same_v<decltype(&AZStd::is_debugger_present), bool (*)() noexcept>);

#if defined(__cpp_lib_debugging) && __cpp_lib_debugging >= 202311L
    static_assert(&AZStd::breakpoint == &std::breakpoint);
    static_assert(&AZStd::breakpoint_if_debugging == &std::breakpoint_if_debugging);
    static_assert(&AZStd::is_debugger_present == &std::is_debugger_present);
#endif

    TEST(DebuggingTests, ConditionalBreakpointWithoutDebuggerReturns)
    {
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            if (AZStd::is_debugger_present())
            {
                GTEST_SKIP() << "This test requires running without a debugger.";
            }
            AZStd::breakpoint_if_debugging();
        }
    }

    TEST(DebuggingTests, ProcStatusAcceptsOnlyCompleteNonzeroTracerPid)
    {
        constexpr const char* tracedLines[] = {
            "TracerPid:\t1\n",
            "TracerPid: 2147483647\n",
            "TracerPid:00042\n",
            "TracerPid: \t 123 \t\r\n",
        };
        for (const char* line : tracedLines)
        {
            EXPECT_TRUE(AZStd::Internal::IsDebuggerPresentInProcStatusLine(line, strlen(line))) << line;
        }

        constexpr const char* untracedOrInvalidLines[] = {
            "",
            "Name:\tDebugger\n",
            "TracerPid:\t0\n",
            "TracerPid:00000\n",
            "TracerPid:",
            "TracerPid:\n",
            "TracerPid: \t\n",
            "TracerPid:1",
            "TracerPid:1 ",
            "TracerPid: -1\n",
            "TracerPid: +1\n",
            "TracerPid: 1x\n",
            "TracerPid: 1 2\n",
            "TracerPid: 2147483648\n",
            "TracerPid: 999999999999999999999999999999999999\n",
            "NotTracerPid:1\n",
            " TracerPid:1\n",
            "tracerpid:1\n",
            "TracerPid:1\nTrailing data",
        };
        for (const char* line : untracedOrInvalidLines)
        {
            EXPECT_FALSE(AZStd::Internal::IsDebuggerPresentInProcStatusLine(line, strlen(line))) << line;
        }
    }
}
