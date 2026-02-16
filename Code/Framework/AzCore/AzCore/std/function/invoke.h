/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/typetraits/invoke_traits.h>

namespace AZStd
{
    using std::is_invocable;
    using std::is_invocable_r;

    using std::is_nothrow_invocable;
    using std::is_nothrow_invocable_r;

    using std::is_invocable_v;
    using std::is_invocable_r_v;
    using std::is_nothrow_invocable_v;
    using std::is_nothrow_invocable_r_v;

    using std::invoke_result;
    using std::invoke_result_t;

    using std::invoke;

    using std::invocable;
    using std::regular_invocable;
}
