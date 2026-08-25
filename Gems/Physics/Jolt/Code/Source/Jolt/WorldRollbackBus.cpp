/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/WorldRollbackBus.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Jolt
{
    void ReflectWorldRollback(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        behaviorContext->EBus<WorldRollbackRequestBus>("JoltWorldRollbackRequestBus")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
            ->Attribute(AZ::Script::Attributes::Module, "jolt")
            ->Event("CaptureWorldState", &IWorldRollbackRequests::CaptureWorldState)
            ->Event("CaptureWorldStateConfigured", &IWorldRollbackRequests::CaptureWorldStateConfigured)
            ->Event("CaptureWorldStateParts", &IWorldRollbackRequests::CaptureWorldStateParts)
            ->Event("ExportWorldStateArchive", &IWorldRollbackRequests::ExportWorldStateArchive)
            ->Event("ImportWorldStateArchive", &IWorldRollbackRequests::ImportWorldStateArchive)
            ->Event("RecaptureWorldState", &IWorldRollbackRequests::RecaptureWorldState)
            ->Event("RecaptureWorldStateConfigured", &IWorldRollbackRequests::RecaptureWorldStateConfigured)
            ->Event("DestroyStateSnapshot", &IWorldRollbackRequests::DestroyStateSnapshot)
            ->Event("IsStateSnapshotValid", &IWorldRollbackRequests::IsStateSnapshotValid)
            ->Event("RestoreWorldState", &IWorldRollbackRequests::RestoreWorldState)
            ->Event("RestoreWorldStateParts", &IWorldRollbackRequests::RestoreWorldStateParts)
            ->Event("ValidateWorldState", &IWorldRollbackRequests::ValidateWorldState)
            ->Event("GetWorldStateDigest", &IWorldRollbackRequests::GetWorldStateDigest);
    }
} // namespace Jolt
