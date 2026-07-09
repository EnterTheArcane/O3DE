/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <cassert>
#include <cctype>
#include <cfloat>
#include <cstdint>

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <stack>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>

inline constexpr auto none = std::nullopt;

using namespace std::literals::string_view_literals;

namespace AZ
{
    // C++17 `std::variant` for C++11/14/17
    using std::get;
    using std::holds_alternative;
    using std::monostate;
    using std::variant;
    using std::optional;

    // Configure basic symbols so we can use them unqualified -> easy to change to AzStd without big refactorings.

    using std::enable_if_t;
    using std::is_same_v;

    using std::count;
    using std::exception;
    using std::pair;
    using std::tuple;

    using std::string;
    using std::string_view;

    using std::array;
    using std::map;
    using std::set;
    using std::stack;
    using std::unordered_map;
    using std::unordered_set;
    using std::vector;
}
