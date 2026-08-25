/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/WorldBus.h>

#include <Jolt/Capabilities/RuntimeConfiguration.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Jolt
{
    void ReflectWorlds(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        behaviorContext->EBus<WorldRequestBus>("JoltWorldRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Event("CreateWorld", &IWorldRequests::CreateWorld)
            ->Event("DestroyWorld", &IWorldRequests::DestroyWorld)
            ->Event("GetDefaultWorldHandle", &IWorldRequests::GetDefaultWorldHandle)
            ->Event("GetRuntimeInfo", &IWorldRequests::GetRuntimeInfo)
            ->Event("IsWorldValid", &IWorldRequests::IsWorldValid)
            ->Event("GetGravity", &IWorldRequests::GetGravity)
            ->Event("SetGravity", &IWorldRequests::SetGravity)
            ->Event("GetSimulationConfiguration", &IWorldRequests::GetSimulationConfiguration)
            ->Event("UpdateSimulationConfiguration", &IWorldRequests::UpdateSimulationConfiguration)
            ->Event("GetRuntimeConfiguration", &IWorldRequests::GetRuntimeConfiguration)
            ->Event("UpdateRuntimeConfiguration", &IWorldRequests::UpdateRuntimeConfiguration);
    }
} // namespace Jolt
