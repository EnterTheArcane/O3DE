/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <type_traits>

namespace AZStd
{
    using std::is_destructible;
    using std::is_trivially_destructible;
    using std::is_nothrow_destructible;

    using std::is_destructible_v;
    using std::is_trivially_destructible_v;
    using std::is_nothrow_destructible_v;

    using std::destructible;

    // BACKWARDS COMPATIBILITY
    template<class T>
    concept is_default_destructible_v = std::is_destructible_v<T>;
}
