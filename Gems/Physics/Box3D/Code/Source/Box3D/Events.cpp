/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Events.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace Box3D
{
    void ReflectEvents(
        AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (!behaviorContext)
        {
            return;
        }

        behaviorContext->Enum<static_cast<AZ::u8>(EventPhase::Begin)>("EventPhase_Begin")
            ->Enum<static_cast<AZ::u8>(EventPhase::Persist)>("EventPhase_Persist")
            ->Enum<static_cast<AZ::u8>(EventPhase::End)>("EventPhase_End");

        behaviorContext->Class<ContactPoint>("ContactPoint")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("position", BehaviorValueProperty(&ContactPoint::m_position))
            ->Property("normal", BehaviorValueProperty(&ContactPoint::m_normal))
            ->Property("impulse", BehaviorValueProperty(&ContactPoint::m_impulse))
            ->Property("separation", BehaviorValueProperty(&ContactPoint::m_separation))
            ->Property("faceIndex", BehaviorValueProperty(&ContactPoint::m_faceIndex));

        behaviorContext->Class<ContactEvent>("ContactEvent")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("bodyA", BehaviorValueProperty(&ContactEvent::m_bodyA))
            ->Property("bodyB", BehaviorValueProperty(&ContactEvent::m_bodyB))
            ->Property("shapeA", BehaviorValueProperty(&ContactEvent::m_shapeA))
            ->Property("shapeB", BehaviorValueProperty(&ContactEvent::m_shapeB))
            ->Property("phase", BehaviorValueProperty(&ContactEvent::m_phase))
            ->Property("firstPoint", BehaviorValueProperty(&ContactEvent::m_firstPoint))
            ->Property("pointCount", BehaviorValueProperty(&ContactEvent::m_pointCount))
            ->Property("childIndexA", BehaviorValueProperty(&ContactEvent::m_childIndexA))
            ->Property("childIndexB", BehaviorValueProperty(&ContactEvent::m_childIndexB));

        behaviorContext->Class<ContactHitEvent>("ContactHitEvent")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("bodyA", BehaviorValueProperty(&ContactHitEvent::m_bodyA))
            ->Property("bodyB", BehaviorValueProperty(&ContactHitEvent::m_bodyB))
            ->Property("shapeA", BehaviorValueProperty(&ContactHitEvent::m_shapeA))
            ->Property("shapeB", BehaviorValueProperty(&ContactHitEvent::m_shapeB))
            ->Property("materialA", BehaviorValueProperty(&ContactHitEvent::m_materialA))
            ->Property("materialB", BehaviorValueProperty(&ContactHitEvent::m_materialB))
            ->Property("position", BehaviorValueProperty(&ContactHitEvent::m_position))
            ->Property("normal", BehaviorValueProperty(&ContactHitEvent::m_normal))
            ->Property("approachSpeed", BehaviorValueProperty(&ContactHitEvent::m_approachSpeed))
            ->Property("faceIndex", BehaviorValueProperty(&ContactHitEvent::m_faceIndex))
            ->Property("childIndexA", BehaviorValueProperty(&ContactHitEvent::m_childIndexA))
            ->Property("childIndexB", BehaviorValueProperty(&ContactHitEvent::m_childIndexB));

        behaviorContext->Class<SensorEvent>("SensorEvent")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("sensorBody", BehaviorValueProperty(&SensorEvent::m_sensorBody))
            ->Property("visitorBody", BehaviorValueProperty(&SensorEvent::m_visitorBody))
            ->Property("sensorShape", BehaviorValueProperty(&SensorEvent::m_sensorShape))
            ->Property("visitorShape", BehaviorValueProperty(&SensorEvent::m_visitorShape))
            ->Property("phase", BehaviorValueProperty(&SensorEvent::m_phase));

        behaviorContext->Class<BodyMoveEvent>("BodyMoveEvent")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("bodyHandle", BehaviorValueProperty(&BodyMoveEvent::m_bodyHandle))
            ->Property("transform", BehaviorValueProperty(&BodyMoveEvent::m_transform))
            ->Property("fellAsleep", BehaviorValueProperty(&BodyMoveEvent::m_fellAsleep));

        behaviorContext->Class<JointThresholdEvent>("JointThresholdEvent")
            ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
            ->Attribute(AZ::Script::Attributes::Module, "box3d")
            ->Property("jointHandle", BehaviorValueProperty(&JointThresholdEvent::m_jointHandle));
    }
} // namespace Box3D
