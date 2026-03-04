/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/base.h>

#include <AzCore/std/iterator/iterator_primitives.h>
#include <AzCore/std/ranges/swap.h>
#include <AzCore/std/typetraits/conjunction.h>
#include <AzCore/std/typetraits/disjunction.h>
#include <AzCore/std/typetraits/extent.h>
#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/std/typetraits/is_class.h>
#include <AzCore/std/typetraits/is_enum.h>
#include <AzCore/std/typetraits/is_void.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/typetraits/void_t.h>
#include <AzCore/std/utility/declval.h>

#include <iterator>

// Note: iter_swap is NOT imported into AZStd::ranges via using because
// std::ranges::iter_swap is a CPO variable, and having a variable named
// iter_swap in AZStd::ranges would conflict with hidden friend function
// declarations of iter_swap in AZStd::ranges view types (e.g. zip_view).
// Use std::ranges::iter_swap directly instead.

namespace AZStd
{
    using std::indirectly_swappable;
}
