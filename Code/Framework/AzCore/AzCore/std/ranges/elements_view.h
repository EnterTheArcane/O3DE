/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/ranges/all_view.h>
#include <AzCore/std/ranges/ranges_adaptor.h>

namespace AZStd::ranges
{
    using std::ranges::elements_view;

    // Alias for elements_view which is useful for extracting keys from associative containers
    template<class View>
    using keys_view = elements_view<View, 0>;
    // Alias for elements_view which is useful for extracting values from associative containers
    template<class View>
    using values_view = elements_view<View, 1>;
}

namespace AZStd::ranges::views
{
    using std::ranges::views::elements;
    using std::ranges::views::keys;
    using std::ranges::views::values;
}
