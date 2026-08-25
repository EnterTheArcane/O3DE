/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/RagdollComponentConfiguration.h>

#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/Ragdoll.h>
#include <Jolt/Reflection.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    RagdollComponentConfiguration RagdollComponentConfiguration::CreateDefault()
    {
        RagdollComponentConfiguration configuration;
        configuration.m_skeleton.m_joints = {
            {.m_name = AZ_NAME_LITERAL("root"), .m_parentIndex = -1},
            {.m_name = AZ_NAME_LITERAL("child"), .m_parentIndex = 0},
        };
        configuration.m_parts.resize(2);
        for (RagdollPartComponentConfiguration& part : configuration.m_parts)
        {
            part.m_shapes.front().m_shape.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        }

        configuration.m_parts[1].m_modelTransform =
            AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(0.5f));
        HingeConstraintConfiguration hinge;
        hinge.m_firstPoint.m_z = 0.25;
        hinge.m_secondPoint.m_z = -0.25;
        configuration.m_parts[1].m_parentConstraint.m_geometry = hinge;
        configuration.m_parts[1].m_hasParentConstraint = true;
        return configuration;
    }

    void RagdollComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        ColliderShapeConfiguration::Reflect(context);
        ConstraintComponentConfiguration::Reflect(context);
        RigidBodyConfiguration::Reflect(context);
        SkeletonDefinitionConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<RagdollComponentConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<RagdollConstraintConfiguration>()
                ->Field("Geometry", &RagdollConstraintConfiguration::m_geometry)
                ->Field("Id", &RagdollConstraintConfiguration::m_id)
                ->Field("FirstLinkedConstraintId", &RagdollConstraintConfiguration::m_firstLinkedConstraintId)
                ->Field("SecondLinkedConstraintId", &RagdollConstraintConfiguration::m_secondLinkedConstraintId);

            serializeContext
                ->Class<AdditionalRagdollConstraint>()
                ->Field("Constraint", &AdditionalRagdollConstraint::m_constraint)
                ->Field("FirstPartIndex", &AdditionalRagdollConstraint::m_firstPartIndex)
                ->Field("SecondPartIndex", &AdditionalRagdollConstraint::m_secondPartIndex);

            serializeContext
                ->Class<RagdollGearConstraintConfiguration>()
                ->Field("FirstHingeAxis", &RagdollGearConstraintConfiguration::m_firstHingeAxis)
                ->Field("SecondHingeAxis", &RagdollGearConstraintConfiguration::m_secondHingeAxis)
                ->Field("Ratio", &RagdollGearConstraintConfiguration::m_ratio)
                ->Field("Space", &RagdollGearConstraintConfiguration::m_space);

            serializeContext
                ->Class<RagdollRackAndPinionConstraintConfiguration>()
                ->Field("HingeAxis", &RagdollRackAndPinionConstraintConfiguration::m_hingeAxis)
                ->Field("SliderAxis", &RagdollRackAndPinionConstraintConfiguration::m_sliderAxis)
                ->Field("Ratio", &RagdollRackAndPinionConstraintConfiguration::m_ratio)
                ->Field("Space", &RagdollRackAndPinionConstraintConfiguration::m_space);

            serializeContext->RegisterGenericType<RagdollConstraintComponentGeometry>();

            serializeContext
                ->Class<RagdollConstraintComponentConfiguration>()
                ->Field("Geometry", &RagdollConstraintComponentConfiguration::m_geometry)
                ->Field("Id", &RagdollConstraintComponentConfiguration::m_id)
                ->Field(
                    "FirstLinkedConstraintId",
                    &RagdollConstraintComponentConfiguration::m_firstLinkedConstraintId)
                ->Field(
                    "SecondLinkedConstraintId",
                    &RagdollConstraintComponentConfiguration::m_secondLinkedConstraintId);

            serializeContext
                ->Class<AdditionalRagdollConstraintComponentConfiguration>()
                ->Field("Constraint", &AdditionalRagdollConstraintComponentConfiguration::m_constraint)
                ->Field("FirstPartIndex", &AdditionalRagdollConstraintComponentConfiguration::m_firstPartIndex)
                ->Field("SecondPartIndex", &AdditionalRagdollConstraintComponentConfiguration::m_secondPartIndex);

            serializeContext
                ->Class<RagdollPartComponentConfiguration>()
                ->Field("Shapes", &RagdollPartComponentConfiguration::m_shapes)
                ->Field("Body", &RagdollPartComponentConfiguration::m_body)
                ->Field("ModelTransform", &RagdollPartComponentConfiguration::m_modelTransform)
                ->Field("ParentConstraint", &RagdollPartComponentConfiguration::m_parentConstraint)
                ->Field("MotionType", &RagdollPartComponentConfiguration::m_motionType)
                ->Field("HasParentConstraint", &RagdollPartComponentConfiguration::m_hasParentConstraint);

            serializeContext
                ->Class<RagdollComponentConfiguration>()
                ->Field("Skeleton", &RagdollComponentConfiguration::m_skeleton)
                ->Field("Parts", &RagdollComponentConfiguration::m_parts)
                ->Field("AdditionalConstraints", &RagdollComponentConfiguration::m_additionalConstraints)
                ->Field("BaseConstraintPriority", &RagdollComponentConfiguration::m_baseConstraintPriority)
                ->Field("CollisionGroupId", &RagdollComponentConfiguration::m_collisionGroupId)
                ->Field("MinimumCollisionSeparation", &RagdollComponentConfiguration::m_minimumCollisionSeparation)
                ->Field("Activate", &RagdollComponentConfiguration::m_activate)
                ->Field(
                    "CalculateConstraintPriorities",
                    &RagdollComponentConfiguration::m_calculateConstraintPriorities)
                ->Field("DisableSelfCollisions", &RagdollComponentConfiguration::m_disableSelfCollisions)
                ->Field("Enabled", &RagdollComponentConfiguration::m_enabled)
                ->Field("Stabilize", &RagdollComponentConfiguration::m_stabilize);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<RagdollConstraintComponentConfiguration>(
                        "Constraint",
                        "Ragdoll constraint geometry, stable identity, and optional local links.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollConstraintComponentConfiguration::m_geometry,
                        "Geometry",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollConstraintComponentConfiguration::m_id,
                        "ID",
                        "Stable identity within this ragdoll definition.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollConstraintComponentConfiguration::m_firstLinkedConstraintId,
                        "First linked constraint ID",
                        "First ragdoll-local dependency for gear or rack-and-pinion constraints.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollConstraintComponentConfiguration::m_secondLinkedConstraintId,
                        "Second linked constraint ID",
                        "Second ragdoll-local dependency for gear or rack-and-pinion constraints.");

                editContext
                    ->Class<AdditionalRagdollConstraintComponentConfiguration>(
                        "Additional constraint",
                        "A constraint between two non-parental parts.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AdditionalRagdollConstraintComponentConfiguration::m_constraint,
                        "Constraint",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AdditionalRagdollConstraintComponentConfiguration::m_firstPartIndex,
                        "First part index",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AdditionalRagdollConstraintComponentConfiguration::m_secondPartIndex,
                        "Second part index",
                        "");

                editContext
                    ->Class<RagdollPartComponentConfiguration>("Part", "One skeleton joint's body and parent constraint.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollPartComponentConfiguration::m_shapes,
                        "Shapes",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollPartComponentConfiguration::m_body, "Body", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollPartComponentConfiguration::m_modelTransform,
                        "Model transform",
                        "Neutral model-space transform relative to the ragdoll root.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollPartComponentConfiguration::m_parentConstraint,
                        "Parent constraint",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollPartComponentConfiguration::m_motionType,
                        "Motion type",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollPartComponentConfiguration::m_hasParentConstraint,
                        "Has parent constraint",
                        "Root parts must disable this; every other part must enable it.");

                editContext
                    ->Class<RagdollComponentConfiguration>("Ragdoll", "Skeleton, parts, constraints, and creation policy.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_skeleton,
                        "Skeleton",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponentConfiguration::m_parts, "Parts", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_additionalConstraints,
                        "Additional constraints",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_baseConstraintPriority,
                        "Base constraint priority",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_collisionGroupId,
                        "Collision group ID",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_minimumCollisionSeparation,
                        "Minimum collision separation",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponentConfiguration::m_activate, "Activate", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_calculateConstraintPriorities,
                        "Calculate constraint priorities",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollComponentConfiguration::m_disableSelfCollisions,
                        "Disable self collisions",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponentConfiguration::m_enabled, "Enabled", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RagdollComponentConfiguration::m_stabilize, "Stabilize", "");
            }
        }
    }
} // namespace Jolt
