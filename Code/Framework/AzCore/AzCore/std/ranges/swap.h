/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/base.h>

#include <AzCore/std/concepts/concepts_assignable.h>
#include <AzCore/std/concepts/concepts_constructible.h>
#include <AzCore/std/typetraits/conjunction.h>
#include <AzCore/std/typetraits/disjunction.h>
#include <AzCore/std/typetraits/extent.h>
#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/std/typetraits/is_assignable.h>
#include <AzCore/std/typetraits/is_array.h>
#include <AzCore/std/typetraits/is_class.h>
#include <AzCore/std/typetraits/is_enum.h>
#include <AzCore/std/typetraits/is_void.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/typetraits/void_t.h>
#include <AzCore/std/utility/move.h>
#include <AzCore/std/utility/declval.h>

namespace AZStd
{
    using std::forward;
}

namespace AZStd::ranges
{
    using std::ranges::swap;
}

namespace AZStd
{
    using std::swappable;
    using std::swappable_with;
}
