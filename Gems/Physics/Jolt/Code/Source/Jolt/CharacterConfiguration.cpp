/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CharacterConfiguration.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void CharacterComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionGroupConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            const bool reflectCollisionHit = ShouldReflect<CharacterCollisionHit>(*serializeContext);
            const bool reflectCollisionRequest = ShouldReflect<CharacterCollisionRequest>(*serializeContext);
            const bool reflectRuntime = ShouldReflect<CharacterRuntimeConfiguration>(*serializeContext);
            const bool reflectComponent = ShouldReflect<CharacterComponentConfiguration>(*serializeContext);
            if (reflectCollisionHit)
            {
                serializeContext
                    ->Class<CharacterCollisionHit>()
                    ->Field("QueryContactPosition", &CharacterCollisionHit::m_queryContactPosition)
                    ->Field("TargetContactPosition", &CharacterCollisionHit::m_targetContactPosition)
                    ->Field("PenetrationAxis", &CharacterCollisionHit::m_penetrationAxis)
                    ->Field("BodyHandle", &CharacterCollisionHit::m_bodyHandle)
                    ->Field("MaterialHandle", &CharacterCollisionHit::m_materialHandle)
                    ->Field("ShapeHandle", &CharacterCollisionHit::m_shapeHandle)
                    ->Field("CharacterHandle", &CharacterCollisionHit::m_characterHandle)
                    ->Field("VirtualCharacterHandle", &CharacterCollisionHit::m_virtualCharacterHandle)
                    ->Field("QuerySubShapeId", &CharacterCollisionHit::m_querySubShapeId)
                    ->Field("TargetSubShapeId", &CharacterCollisionHit::m_targetSubShapeId)
                    ->Field("PenetrationDepth", &CharacterCollisionHit::m_penetrationDepth);
            }

            if (reflectCollisionRequest)
            {
                serializeContext
                    ->Class<CharacterCollisionRequest>()
                    ->Field("Transform", &CharacterCollisionRequest::m_transform)
                    ->Field("MovementDirection", &CharacterCollisionRequest::m_movementDirection)
                    ->Field("ShapeHandle", &CharacterCollisionRequest::m_shapeHandle)
                    ->Field(
                        "MaximumSeparationDistance",
                        &CharacterCollisionRequest::m_maximumSeparationDistance);
            }

            if (reflectRuntime)
            {
                serializeContext
                    ->Class<CharacterRuntimeConfiguration>()
                    ->Field("ObjectLayer", &CharacterRuntimeConfiguration::m_objectLayer)
                    ->Field("SupportingPlaneNormal", &CharacterRuntimeConfiguration::m_supportingPlaneNormal)
                    ->Field("Up", &CharacterRuntimeConfiguration::m_up)
                    ->Field(
                        "MaximumSeparationDistance",
                        &CharacterRuntimeConfiguration::m_maximumSeparationDistance)
                    ->Field("MaximumSlopeAngle", &CharacterRuntimeConfiguration::m_maximumSlopeAngle)
                    ->Field("SupportingPlaneDistance", &CharacterRuntimeConfiguration::m_supportingPlaneDistance);
            }

            if (reflectComponent)
            {
                serializeContext
                    ->Class<CharacterComponentConfiguration>()
                    ->Field("CollisionGroup", &CharacterComponentConfiguration::m_collisionGroup)
                    ->Field("UserData", &CharacterComponentConfiguration::m_userData)
                    ->Field("ObjectLayer", &CharacterComponentConfiguration::m_objectLayer)
                    ->Field("AllowedDofs", &CharacterComponentConfiguration::m_allowedDofs)
                    ->Field("SupportingPlaneNormal", &CharacterComponentConfiguration::m_supportingPlaneNormal)
                    ->Field("Up", &CharacterComponentConfiguration::m_up)
                    ->Field("Friction", &CharacterComponentConfiguration::m_friction)
                    ->Field("GravityFactor", &CharacterComponentConfiguration::m_gravityFactor)
                    ->Field("Mass", &CharacterComponentConfiguration::m_mass)
                    ->Field("MaximumPenetrationDepth", &CharacterComponentConfiguration::m_maximumPenetrationDepth)
                    ->Field("MaximumSeparationDistance", &CharacterComponentConfiguration::m_maximumSeparationDistance)
                    ->Field("MaximumSlopeAngle", &CharacterComponentConfiguration::m_maximumSlopeAngle)
                    ->Field("SupportingPlaneDistance", &CharacterComponentConfiguration::m_supportingPlaneDistance)
                    ->Field("Activate", &CharacterComponentConfiguration::m_activate)
                    ->Field("EnhancedInternalEdgeRemoval", &CharacterComponentConfiguration::m_enhancedInternalEdgeRemoval);
            }
        }
    }

    void VirtualCharacterComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            const bool reflectContact = ShouldReflect<VirtualCharacterContact>(*serializeContext);
            const bool reflectRuntime = ShouldReflect<VirtualCharacterRuntimeConfiguration>(*serializeContext);
            const bool reflectStair = ShouldReflect<VirtualCharacterStairConfiguration>(*serializeContext);
            const bool reflectUpdate = ShouldReflect<VirtualCharacterUpdateConfiguration>(*serializeContext);
            const bool reflectComponent = ShouldReflect<VirtualCharacterComponentConfiguration>(*serializeContext);
            if (reflectContact)
            {
                serializeContext
                    ->Class<VirtualCharacterContact>()
                    ->Field("Position", &VirtualCharacterContact::m_position)
                    ->Field("ContactNormal", &VirtualCharacterContact::m_contactNormal)
                    ->Field("LinearVelocity", &VirtualCharacterContact::m_linearVelocity)
                    ->Field("SurfaceNormal", &VirtualCharacterContact::m_surfaceNormal)
                    ->Field("BodyHandle", &VirtualCharacterContact::m_bodyHandle)
                    ->Field("MaterialHandle", &VirtualCharacterContact::m_materialHandle)
                    ->Field("ShapeHandle", &VirtualCharacterContact::m_shapeHandle)
                    ->Field("CharacterHandle", &VirtualCharacterContact::m_characterHandle)
                    ->Field("SubShapeId", &VirtualCharacterContact::m_subShapeId)
                    ->Field("Distance", &VirtualCharacterContact::m_distance)
                    ->Field("Fraction", &VirtualCharacterContact::m_fraction)
                    ->Field("MotionType", &VirtualCharacterContact::m_motionType)
                    ->Field("CanPushCharacter", &VirtualCharacterContact::m_canPushCharacter)
                    ->Field("HadCollision", &VirtualCharacterContact::m_hadCollision)
                    ->Field("IsBackFacing", &VirtualCharacterContact::m_isBackFacing)
                    ->Field("IsSensor", &VirtualCharacterContact::m_isSensor)
                    ->Field("WasDiscarded", &VirtualCharacterContact::m_wasDiscarded);
            }

            if (reflectRuntime)
            {
                serializeContext
                    ->Class<VirtualCharacterRuntimeConfiguration>()
                    ->Field("ShapeOffset", &VirtualCharacterRuntimeConfiguration::m_shapeOffset)
                    ->Field(
                        "SupportingPlaneNormal",
                        &VirtualCharacterRuntimeConfiguration::m_supportingPlaneNormal)
                    ->Field("Up", &VirtualCharacterRuntimeConfiguration::m_up)
                    ->Field(
                        "HitReductionCosMaximumAngle",
                        &VirtualCharacterRuntimeConfiguration::m_hitReductionCosMaximumAngle)
                    ->Field("Mass", &VirtualCharacterRuntimeConfiguration::m_mass)
                    ->Field("MaximumSlopeAngle", &VirtualCharacterRuntimeConfiguration::m_maximumSlopeAngle)
                    ->Field("MaximumStrength", &VirtualCharacterRuntimeConfiguration::m_maximumStrength)
                    ->Field(
                        "PenetrationRecoverySpeed",
                        &VirtualCharacterRuntimeConfiguration::m_penetrationRecoverySpeed)
                    ->Field(
                        "SupportingPlaneDistance",
                        &VirtualCharacterRuntimeConfiguration::m_supportingPlaneDistance)
                    ->Field("MaximumHitCount", &VirtualCharacterRuntimeConfiguration::m_maximumHitCount)
                    ->Field(
                        "EnhancedInternalEdgeRemoval",
                        &VirtualCharacterRuntimeConfiguration::m_enhancedInternalEdgeRemoval);
            }

            if (reflectStair)
            {
                serializeContext
                    ->Class<VirtualCharacterStairConfiguration>()
                    ->Field("StepDownExtra", &VirtualCharacterStairConfiguration::m_stepDownExtra)
                    ->Field("StepForward", &VirtualCharacterStairConfiguration::m_stepForward)
                    ->Field("StepForwardTest", &VirtualCharacterStairConfiguration::m_stepForwardTest)
                    ->Field("StepUp", &VirtualCharacterStairConfiguration::m_stepUp)
                    ->Field("DeltaTime", &VirtualCharacterStairConfiguration::m_deltaTime);
            }

            if (reflectUpdate)
            {
                serializeContext
                    ->Class<VirtualCharacterUpdateConfiguration>()
                    ->Field("Gravity", &VirtualCharacterUpdateConfiguration::m_gravity)
                    ->Field("StickToFloorStepDown", &VirtualCharacterUpdateConfiguration::m_stickToFloorStepDown)
                    ->Field("WalkStairsStepDownExtra", &VirtualCharacterUpdateConfiguration::m_walkStairsStepDownExtra)
                    ->Field("WalkStairsStepUp", &VirtualCharacterUpdateConfiguration::m_walkStairsStepUp)
                    ->Field(
                        "WalkStairsCosAngleForwardContact",
                        &VirtualCharacterUpdateConfiguration::m_walkStairsCosAngleForwardContact)
                    ->Field("WalkStairsMinimumStepForward", &VirtualCharacterUpdateConfiguration::m_walkStairsMinimumStepForward)
                    ->Field("WalkStairsStepForwardTest", &VirtualCharacterUpdateConfiguration::m_walkStairsStepForwardTest)
                    ->Field("Extended", &VirtualCharacterUpdateConfiguration::m_extended);
            }
            if (reflectComponent)
            {
                serializeContext
                    ->Class<VirtualCharacterComponentConfiguration>()
                    ->Field("Update", &VirtualCharacterComponentConfiguration::m_update)
                    ->Field("UserData", &VirtualCharacterComponentConfiguration::m_userData)
                    ->Field("InnerBodyObjectLayer", &VirtualCharacterComponentConfiguration::m_innerBodyObjectLayer)
                    ->Field("ObjectLayer", &VirtualCharacterComponentConfiguration::m_objectLayer)
                    ->Field("ShapeOffset", &VirtualCharacterComponentConfiguration::m_shapeOffset)
                    ->Field("SupportingPlaneNormal", &VirtualCharacterComponentConfiguration::m_supportingPlaneNormal)
                    ->Field("Up", &VirtualCharacterComponentConfiguration::m_up)
                    ->Field("CharacterPadding", &VirtualCharacterComponentConfiguration::m_characterPadding)
                    ->Field("CollisionTolerance", &VirtualCharacterComponentConfiguration::m_collisionTolerance)
                    ->Field("HitReductionCosMaximumAngle", &VirtualCharacterComponentConfiguration::m_hitReductionCosMaximumAngle)
                    ->Field("Mass", &VirtualCharacterComponentConfiguration::m_mass)
                    ->Field("MaximumPenetrationDepth", &VirtualCharacterComponentConfiguration::m_maximumPenetrationDepth)
                    ->Field("MaximumSlopeAngle", &VirtualCharacterComponentConfiguration::m_maximumSlopeAngle)
                    ->Field("MaximumStrength", &VirtualCharacterComponentConfiguration::m_maximumStrength)
                    ->Field("MinimumTimeRemaining", &VirtualCharacterComponentConfiguration::m_minimumTimeRemaining)
                    ->Field("PenetrationRecoverySpeed", &VirtualCharacterComponentConfiguration::m_penetrationRecoverySpeed)
                    ->Field("PredictiveContactDistance", &VirtualCharacterComponentConfiguration::m_predictiveContactDistance)
                    ->Field("SupportingPlaneDistance", &VirtualCharacterComponentConfiguration::m_supportingPlaneDistance)
                    ->Field("MaximumCollisionIterations", &VirtualCharacterComponentConfiguration::m_maximumCollisionIterations)
                    ->Field("MaximumConstraintIterations", &VirtualCharacterComponentConfiguration::m_maximumConstraintIterations)
                    ->Field("MaximumHitCount", &VirtualCharacterComponentConfiguration::m_maximumHitCount)
                    ->Field("CollideWithBackFaces", &VirtualCharacterComponentConfiguration::m_collideWithBackFaces)
                    ->Field("CreateInnerBody", &VirtualCharacterComponentConfiguration::m_createInnerBody)
                    ->Field("Enabled", &VirtualCharacterComponentConfiguration::m_enabled)
                    ->Field("EnhancedInternalEdgeRemoval", &VirtualCharacterComponentConfiguration::m_enhancedInternalEdgeRemoval);
            }

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                if (reflectUpdate)
                {
                    editContext
                        ->Class<VirtualCharacterUpdateConfiguration>("Update", "Gravity, stairs, and floor adhesion.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VirtualCharacterUpdateConfiguration::m_gravity, "Gravity", "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterUpdateConfiguration::m_stickToFloorStepDown,
                        "Stick-to-floor step down",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterUpdateConfiguration::m_walkStairsStepDownExtra,
                        "Stair step-down extra",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterUpdateConfiguration::m_walkStairsStepUp,
                        "Stair step up",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterUpdateConfiguration::m_walkStairsCosAngleForwardContact,
                        "Forward-contact cosine",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterUpdateConfiguration::m_walkStairsMinimumStepForward,
                        "Minimum step forward",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterUpdateConfiguration::m_walkStairsStepForwardTest,
                        "Step-forward test",
                        "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VirtualCharacterUpdateConfiguration::m_extended,
                            "Extended update",
                            "");
                }

                if (reflectComponent)
                {
                    editContext
                        ->Class<VirtualCharacterComponentConfiguration>("Configuration", "Virtual character collision and movement.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VirtualCharacterComponentConfiguration::m_update, "Update", "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VirtualCharacterComponentConfiguration::m_userData, "User data", "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VirtualCharacterComponentConfiguration::m_innerBodyObjectLayer,
                            "Inner-body object layer",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VirtualCharacterComponentConfiguration::m_objectLayer,
                            "Object layer",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VirtualCharacterComponentConfiguration::m_shapeOffset,
                            "Shape offset",
                            "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_supportingPlaneNormal,
                        "Supporting-plane normal",
                        "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VirtualCharacterComponentConfiguration::m_up, "Up", "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_characterPadding,
                        "Character padding",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_collisionTolerance,
                        "Collision tolerance",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_hitReductionCosMaximumAngle,
                        "Hit-reduction cosine",
                        "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VirtualCharacterComponentConfiguration::m_mass, "Mass", "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_maximumPenetrationDepth,
                        "Maximum penetration depth",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_maximumSlopeAngle,
                        "Maximum slope angle",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_maximumStrength,
                        "Maximum strength",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_minimumTimeRemaining,
                        "Minimum time remaining",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_penetrationRecoverySpeed,
                        "Penetration recovery speed",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_predictiveContactDistance,
                        "Predictive contact distance",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_supportingPlaneDistance,
                        "Supporting-plane distance",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_maximumCollisionIterations,
                        "Maximum collision iterations",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_maximumConstraintIterations,
                        "Maximum constraint iterations",
                        "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_maximumHitCount,
                        "Maximum hit count",
                        "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VirtualCharacterComponentConfiguration::m_collideWithBackFaces,
                            "Collide with back faces",
                            "")
                        ->DataElement(
                            AZ::Edit::UIHandlers::Default,
                            &VirtualCharacterComponentConfiguration::m_createInnerBody,
                            "Create inner body",
                            "")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &VirtualCharacterComponentConfiguration::m_enabled, "Enabled", "")
                        ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &VirtualCharacterComponentConfiguration::m_enhancedInternalEdgeRemoval,
                        "Enhanced internal-edge removal",
                        "");
                }
            }
        }
    }
} // namespace Jolt
