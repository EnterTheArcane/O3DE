/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <emscripten/emscripten.h>

namespace AZStd
{
    //! Invokes the JavaScript debugger when available.
    AZ_FORCE_INLINE void breakpoint() noexcept
    {
        emscripten_debugger();
    }
}
