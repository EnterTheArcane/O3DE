/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <functional>

namespace AZStd
{
    namespace placeholders
    {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
        using std::placeholders::_4;
        using std::placeholders::_5;
        using std::placeholders::_6;
        using std::placeholders::_7;
        using std::placeholders::_8;
        using std::placeholders::_9;
        using std::placeholders::_10;
    }

    using std::bind;

    using std::is_bind_expression;
    using std::is_bind_expression_v;

    using std::is_placeholder;
    using std::is_placeholder_v;
}
