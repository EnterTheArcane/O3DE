/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/debugging.h>

#if !defined(__cpp_lib_debugging) || __cpp_lib_debugging < 202311L
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>

namespace AZStd
{
    bool is_debugger_present() noexcept
    {
        int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
        kinfo_proc info{};
        size_t size = sizeof(info);
        if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), &info, &size, nullptr, 0) != 0 || size != sizeof(info))
        {
            return false;
        }
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
}
#endif
