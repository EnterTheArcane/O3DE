/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/WorldDiagnosticsBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Jolt
{
    void ReflectWorldDiagnostics(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        behaviorContext->EBus<WorldDiagnosticsRequestBus>("JoltWorldDiagnosticsRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Event("GetWorldStatistics", &IWorldDiagnosticsRequests::GetWorldStatistics)
            ->Event("ConfigurePerformanceStatistics", &IWorldDiagnosticsRequests::ConfigurePerformanceStatistics)
            ->Event("GetPerformanceStatistics", &IWorldDiagnosticsRequests::GetPerformanceStatistics)
            ->Event("ConfigureDebugCapture", &IWorldDiagnosticsRequests::ConfigureDebugCapture)
            ->Event("GetDebugCaptureStatistics", &IWorldDiagnosticsRequests::GetDebugCaptureStatistics);
    }
} // namespace Jolt
