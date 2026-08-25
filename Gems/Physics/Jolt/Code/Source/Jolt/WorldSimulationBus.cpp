/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/WorldSimulationBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Jolt
{
    void ReflectWorldSimulation(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        behaviorContext->EBus<WorldSimulationRequestBus>("JoltWorldSimulationRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Event("StepWorld", &IWorldSimulationRequests::StepWorld);
    }
} // namespace Jolt
