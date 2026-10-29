/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/debugging.h>

#if !defined(__cpp_lib_debugging) || __cpp_lib_debugging < 202311L
#include <AzCore/PlatformIncl.h>

namespace AZStd
{
    bool is_debugger_present() noexcept
    {
        return ::IsDebuggerPresent() != 0;
    }
}
#endif
