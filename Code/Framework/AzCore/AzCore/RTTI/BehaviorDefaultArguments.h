/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/std/containers/fixed_vector.h>

namespace AZ::Internal
{
    using BehaviorArgumentList = AZStd::fixed_vector<BehaviorArgument, MaxBehaviorParameters>;

    inline bool ExpandDefaultArguments(
        const BehaviorMethod& method,
        AZStd::span<BehaviorArgument> arguments,
        size_t totalArguments,
        BehaviorArgumentList& expandedArguments)
    {
        if (totalArguments > expandedArguments.capacity())
        {
            return false;
        }
        for (const BehaviorArgument& argument : arguments)
        {
            expandedArguments.emplace_back(argument);
        }
        for (size_t index = arguments.size(); index < totalArguments; ++index)
        {
            BehaviorDefaultValuePtr defaultValue = method.GetDefaultValue(index);
            if (!defaultValue)
            {
                return false;
            }
            expandedArguments.emplace_back(defaultValue->GetValue());
        }
        return true;
    }

    // Keep the fixed-capacity buffer off the stack of calls that supply all arguments.
    template<typename Method>
    AZ_NOINLINE bool CallWithDefaultArguments(
        const Method& method,
        AZStd::span<BehaviorArgument> arguments,
        size_t totalArguments,
        BehaviorArgument* result)
    {
        BehaviorArgumentList expandedArguments;
        if (!ExpandDefaultArguments(method, arguments, totalArguments, expandedArguments))
        {
            AZ_Warning("Behavior", false, "Not enough arguments to make a call! %zu needed %zu", arguments.size(), totalArguments);
            return false;
        }
        return method.Method::Call(expandedArguments, result);
    }

    template<typename Method>
    AZ_NOINLINE BehaviorMethod::ResultOutcome IsCallableWithDefaultArguments(
        const Method& method,
        AZStd::span<BehaviorArgument> arguments,
        size_t totalArguments,
        BehaviorArgument* result)
    {
        BehaviorArgumentList expandedArguments;
        if (!ExpandDefaultArguments(method, arguments, totalArguments, expandedArguments))
        {
            return BehaviorMethod::ResultOutcome{ AZStd::unexpect, BehaviorMethod::ResultOutcome::ErrorType::format(
                "Not enough arguments to make call to method %s. %zu supplied, needed %zu",
                method.m_name.c_str(), arguments.size(), totalArguments) };
        }
        return method.Method::IsCallable(expandedArguments, result);
    }
} // namespace AZ::Internal
