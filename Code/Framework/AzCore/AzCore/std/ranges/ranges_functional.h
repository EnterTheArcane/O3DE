/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/function/identity.h>
#include <AzCore/std/ranges/ranges.h>

namespace AZStd::ranges
{
    using std::ranges::equal_to;
    using std::ranges::not_equal_to;
    using std::ranges::less;
    using std::ranges::greater;
    using std::ranges::greater_equal;
    using std::ranges::less_equal;
} // namespace AZStd::ranges

namespace AZStd
{
    using std::indirectly_comparable;
    using std::permutable;
    using std::mergeable;
    using std::sortable;
} // namespace AZStd
