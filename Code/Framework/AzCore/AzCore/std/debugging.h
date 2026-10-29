/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <version>

#if defined(__cpp_lib_debugging) && __cpp_lib_debugging >= 202311L

#include <debugging>

namespace AZStd
{
    using std::breakpoint;
    using std::breakpoint_if_debugging;
    using std::is_debugger_present;
}

#else

#include <AzCore/base.h>
#include <AzCore/std/debugging_Platform.h>

namespace AZStd
{
    //! Queries the current debugger state without caching. Detection failures return false.
    //! This PAL fallback does not support application replacement.
    AZCORE_API bool is_debugger_present() noexcept;

    //! Breaks only when a debugger is detected. Available in every build configuration.
    AZ_FORCE_INLINE void breakpoint_if_debugging() noexcept
    {
        if (AZStd::is_debugger_present())
        {
            AZStd::breakpoint();
        }
    }
}

#endif
