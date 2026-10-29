/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <signal.h>

namespace AZStd
{
    //! Unconditionally breaks into the debugger. Without a debugger, the process may terminate.
    AZ_FORCE_INLINE void breakpoint() noexcept
    {
#if defined(__has_builtin)
#   if __has_builtin(__builtin_debugtrap)
        __builtin_debugtrap();
#   else
        raise(SIGTRAP);
#   endif
#else
        raise(SIGTRAP);
#endif
    }
}
