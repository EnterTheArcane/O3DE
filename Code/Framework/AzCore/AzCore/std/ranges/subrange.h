/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/ranges/subrange_fwd.h>

#include <AzCore/std/ranges/ranges_adaptor.h>
#include <AzCore/std/typetraits/is_reference.h>
#include <AzCore/std/tuple.h>

namespace AZStd::ranges
{
    using std::ranges::subrange_kind;
    using std::ranges::subrange;
    using std::ranges::get;
}

namespace AZStd
{
    using ranges::get;
}
