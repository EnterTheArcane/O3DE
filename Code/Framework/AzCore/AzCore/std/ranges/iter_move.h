/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/base.h>

#include <AzCore/std/typetraits/conditional.h>

#include <AzCore/std/typetraits/is_class.h>
#include <AzCore/std/typetraits/is_enum.h>
#include <AzCore/std/typetraits/is_lvalue_reference.h>
#include <AzCore/std/typetraits/is_rvalue_reference.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/typetraits/void_t.h>
#include <AzCore/std/utility/move.h>
#include <AzCore/std/utility/declval.h>

#include <ranges>

namespace AZStd
{
    using std::forward;
}

// Note: iter_move is NOT imported into AZStd::ranges via using because
// std::ranges::iter_move is a CPO variable, and having a variable named
// iter_move in AZStd::ranges would conflict with hidden friend function
// declarations of iter_move in AZStd::ranges view types (e.g. zip_view).
// Use std::ranges::iter_move directly instead.
