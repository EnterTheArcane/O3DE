/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <utility>

// Forward declare the pair class to prevent a circular include with needing to know about the tuple type in order
// to define the pair-like concept
namespace AZStd
{
    // forward declare the pair type
    template<class T1, class T2>
    struct pair;

    using std::piecewise_construct_t;
    using std::piecewise_construct;
} // namespace AZStd
