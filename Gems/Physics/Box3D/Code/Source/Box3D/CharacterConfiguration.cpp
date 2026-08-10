/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/CharacterConfiguration.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Box3D
{
    void CharacterConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<CollisionFilter>()
                ->Field("CategoryBits", &CollisionFilter::m_categoryBits)
                ->Field("MaskBits", &CollisionFilter::m_maskBits)
                ->Field("GroupIndex", &CollisionFilter::m_groupIndex);

            serializeContext
                ->Class<CharacterConfiguration>()
                ->Field("BasePosition", &CharacterConfiguration::m_basePosition)
                ->Field("Rotation", &CharacterConfiguration::m_rotation)
                ->Field("UpDirection", &CharacterConfiguration::m_upDirection)
                ->Field("EntityId", &CharacterConfiguration::m_entityId)
                ->Field("Name", &CharacterConfiguration::m_name)
                ->Field("CollisionFilter", &CharacterConfiguration::m_collisionFilter)
                ->Field("Height", &CharacterConfiguration::m_height)
                ->Field("Radius", &CharacterConfiguration::m_radius)
                ->Field("MaximumSlopeAngle", &CharacterConfiguration::m_maximumSlopeAngle)
                ->Field("StepHeight", &CharacterConfiguration::m_stepHeight)
                ->Field("MinimumMovementDistance", &CharacterConfiguration::m_minimumMovementDistance)
                ->Field("MaximumSpeed", &CharacterConfiguration::m_maximumSpeed)
                ->Field("GroundStickDistance", &CharacterConfiguration::m_groundStickDistance)
                ->Field("InteractionScale", &CharacterConfiguration::m_interactionScale)
                ->Field("MaximumIterations", &CharacterConfiguration::m_maximumIterations)
                ->Field("MaximumContactPlanes", &CharacterConfiguration::m_maximumContactPlanes)
                ->Field("ApplyMoveOnFixedTick", &CharacterConfiguration::m_applyMoveOnFixedTick);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CollisionFilter>("Collision filter", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CollisionFilter::m_categoryBits, "Category bits", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CollisionFilter::m_maskBits, "Mask bits", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CollisionFilter::m_groupIndex, "Group index", "");

                editContext->Class<CharacterConfiguration>("Character", "Capsule mover geometry and movement limits")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_upDirection, "Up direction", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_collisionFilter, "Collision filter", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_height, "Height", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_radius, "Radius", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.001f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_maximumSlopeAngle, "Maximum slope angle", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 90.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_stepHeight, "Step height", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_minimumMovementDistance, "Minimum movement distance", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_maximumSpeed, "Maximum speed", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_groundStickDistance, "Ground stick distance", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_interactionScale, "Interaction scale", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_maximumIterations, "Maximum iterations", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, CharacterConfiguration::MaximumIterationCount)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_maximumContactPlanes, "Contact plane capacity", "")
                    ->Attribute(AZ::Edit::Attributes::Min, 1)
                    ->Attribute(AZ::Edit::Attributes::Max, CharacterConfiguration::MaximumContactPlaneCount)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_applyMoveOnFixedTick, "Move on fixed tick", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Enum<static_cast<AZ::u8>(CharacterSupportState::Unsupported)>("CharacterSupportState_Unsupported")
                ->Enum<static_cast<AZ::u8>(CharacterSupportState::Supported)>("CharacterSupportState_Supported")
                ->Enum<static_cast<AZ::u8>(CharacterSupportState::Sliding)>("CharacterSupportState_Sliding");

            behaviorContext->Class<CollisionFilter>("CollisionFilter")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Constructor<>()
                ->Property("categoryBits", BehaviorValueProperty(&CollisionFilter::m_categoryBits))
                ->Property("maskBits", BehaviorValueProperty(&CollisionFilter::m_maskBits))
                ->Property("groupIndex", BehaviorValueProperty(&CollisionFilter::m_groupIndex));

            behaviorContext->Class<CharacterConfiguration>("CharacterConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("basePosition", BehaviorValueProperty(&CharacterConfiguration::m_basePosition))
                ->Property("rotation", BehaviorValueProperty(&CharacterConfiguration::m_rotation))
                ->Property("upDirection", BehaviorValueProperty(&CharacterConfiguration::m_upDirection))
                ->Property("entityId", BehaviorValueProperty(&CharacterConfiguration::m_entityId))
                ->Property("name", BehaviorValueProperty(&CharacterConfiguration::m_name))
                ->Property("collisionFilter", BehaviorValueProperty(&CharacterConfiguration::m_collisionFilter))
                ->Property("height", BehaviorValueProperty(&CharacterConfiguration::m_height))
                ->Property("radius", BehaviorValueProperty(&CharacterConfiguration::m_radius))
                ->Property("maximumSlopeAngle", BehaviorValueProperty(&CharacterConfiguration::m_maximumSlopeAngle))
                ->Property("stepHeight", BehaviorValueProperty(&CharacterConfiguration::m_stepHeight))
                ->Property("minimumMovementDistance", BehaviorValueProperty(&CharacterConfiguration::m_minimumMovementDistance))
                ->Property("maximumSpeed", BehaviorValueProperty(&CharacterConfiguration::m_maximumSpeed))
                ->Property("groundStickDistance", BehaviorValueProperty(&CharacterConfiguration::m_groundStickDistance))
                ->Property("interactionScale", BehaviorValueProperty(&CharacterConfiguration::m_interactionScale))
                ->Property("maximumIterations", BehaviorValueProperty(&CharacterConfiguration::m_maximumIterations))
                ->Property("maximumContactPlanes", BehaviorValueProperty(&CharacterConfiguration::m_maximumContactPlanes))
                ->Property("applyMoveOnFixedTick", BehaviorValueProperty(&CharacterConfiguration::m_applyMoveOnFixedTick));

            behaviorContext->Class<CharacterSupport>("CharacterSupport")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("state", BehaviorValueProperty(&CharacterSupport::m_state))
                ->Property("bodyHandle", BehaviorValueProperty(&CharacterSupport::m_bodyHandle))
                ->Property("shapeHandle", BehaviorValueProperty(&CharacterSupport::m_shapeHandle))
                ->Property("position", BehaviorValueProperty(&CharacterSupport::m_position))
                ->Property("normal", BehaviorValueProperty(&CharacterSupport::m_normal))
                ->Property("velocity", BehaviorValueProperty(&CharacterSupport::m_velocity));

            behaviorContext->Class<CharacterState>("CharacterState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Property("basePosition", BehaviorValueProperty(&CharacterState::m_basePosition))
                ->Property("centerPosition", BehaviorValueProperty(&CharacterState::m_centerPosition))
                ->Property("velocity", BehaviorValueProperty(&CharacterState::m_velocity))
                ->Property("support", BehaviorValueProperty(&CharacterState::m_support));
        }
    }
} // namespace Box3D
