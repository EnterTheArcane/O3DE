/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Event.h>

#include <Jolt/BehaviorReflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void ReflectEvents(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ContactPoint>()
                ->Field("PositionOnFirstBody", &ContactPoint::m_positionOnFirstBody)
                ->Field("PositionOnSecondBody", &ContactPoint::m_positionOnSecondBody);

            serializeContext
                ->Class<ContactEvent>()
                ->Field("FirstBodyHandle", &ContactEvent::m_firstBodyHandle)
                ->Field("SecondBodyHandle", &ContactEvent::m_secondBodyHandle)
                ->Field("FirstShapeHandle", &ContactEvent::m_firstShapeHandle)
                ->Field("SecondShapeHandle", &ContactEvent::m_secondShapeHandle)
                ->Field("FirstMaterialHandle", &ContactEvent::m_firstMaterialHandle)
                ->Field("SecondMaterialHandle", &ContactEvent::m_secondMaterialHandle)
                ->Field("FirstSubShapeId", &ContactEvent::m_firstSubShapeId)
                ->Field("SecondSubShapeId", &ContactEvent::m_secondSubShapeId)
                ->Field("Normal", &ContactEvent::m_normal)
                ->Field("PenetrationDepth", &ContactEvent::m_penetrationDepth)
                ->Field("FirstPoint", &ContactEvent::m_firstPoint)
                ->Field("PointCount", &ContactEvent::m_pointCount)
                ->Field("Phase", &ContactEvent::m_phase);

            serializeContext
                ->Class<ActivationEvent>()
                ->Field("BodyHandle", &ActivationEvent::m_bodyHandle)
                ->Field("State", &ActivationEvent::m_state);

            serializeContext
                ->Class<BodyMoveEvent>()
                ->Field("Transform", &BodyMoveEvent::m_transform)
                ->Field("BodyHandle", &BodyMoveEvent::m_bodyHandle)
                ->Field("EntityId", &BodyMoveEvent::m_entityId);

            serializeContext
                ->Class<VirtualCharacterMoveEvent>()
                ->Field("Transform", &VirtualCharacterMoveEvent::m_transform)
                ->Field("CharacterHandle", &VirtualCharacterMoveEvent::m_characterHandle)
                ->Field("EntityId", &VirtualCharacterMoveEvent::m_entityId);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ActivationState, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ActivationState, Active);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ActivationState, Inactive);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, Begin);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, End);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, EventPhase, Persist);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, AcceptAllForPair);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, Accept);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, Reject);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, ContactDecision, RejectAllForPair);

            behaviorContext->Class<ContactPoint>("JoltContactPoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "ContactPoint")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ContactPoint")
                ->Property(
                    "positionOnFirstBody",
                    BehaviorValueGetter(&ContactPoint::m_positionOnFirstBody),
                    nullptr)
                ->Property(
                    "positionOnSecondBody",
                    BehaviorValueGetter(&ContactPoint::m_positionOnSecondBody),
                    nullptr);

            behaviorContext->Class<ContactEvent>("JoltContactEvent")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Alias, "ContactEvent")
                ->Attribute(AZ::Script::Attributes::ClassNameOverride, "ContactEvent")
                ->Property("firstBodyHandle", BehaviorValueGetter(&ContactEvent::m_firstBodyHandle), nullptr)
                ->Property("secondBodyHandle", BehaviorValueGetter(&ContactEvent::m_secondBodyHandle), nullptr)
                ->Property("firstShapeHandle", BehaviorValueGetter(&ContactEvent::m_firstShapeHandle), nullptr)
                ->Property("secondShapeHandle", BehaviorValueGetter(&ContactEvent::m_secondShapeHandle), nullptr)
                ->Property(
                    "firstMaterialHandle",
                    BehaviorValueGetter(&ContactEvent::m_firstMaterialHandle),
                    nullptr)
                ->Property(
                    "secondMaterialHandle",
                    BehaviorValueGetter(&ContactEvent::m_secondMaterialHandle),
                    nullptr)
                ->Property("firstSubShapeId", BehaviorValueGetter(&ContactEvent::m_firstSubShapeId), nullptr)
                ->Property("secondSubShapeId", BehaviorValueGetter(&ContactEvent::m_secondSubShapeId), nullptr)
                ->Property("normal", BehaviorValueGetter(&ContactEvent::m_normal), nullptr)
                ->Property(
                    "penetrationDepth",
                    BehaviorValueGetter(&ContactEvent::m_penetrationDepth),
                    nullptr)
                ->Property("pointCount", BehaviorValueGetter(&ContactEvent::m_pointCount), nullptr)
                ->Property("phase", BehaviorValueGetter(&ContactEvent::m_phase), nullptr);

            behaviorContext->Class<ActivationEvent>("ActivationEvent")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("bodyHandle", BehaviorValueGetter(&ActivationEvent::m_bodyHandle), nullptr)
                ->Property("state", BehaviorValueGetter(&ActivationEvent::m_state), nullptr);

            behaviorContext->Class<ContactPointView>("ContactPointView")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Method("GetPointCount", &ContactPointView::GetPointCount)
                ->Method("GetPoint", &ContactPointView::GetPoint);
        }
    }
} // namespace Jolt
