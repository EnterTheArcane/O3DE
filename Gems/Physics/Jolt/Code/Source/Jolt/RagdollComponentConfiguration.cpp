/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/RagdollComponentConfiguration.h>

#include <Jolt/ConstraintComponentConfiguration.h>
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
        configuration.m_parts[1].m_toParent = hinge;
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
                ->Class<AdditionalRagdollConstraint>()
                ->Field("Geometry", &AdditionalRagdollConstraint::m_geometry)
                ->Field("FirstPartIndex", &AdditionalRagdollConstraint::m_firstPartIndex)
                ->Field("SecondPartIndex", &AdditionalRagdollConstraint::m_secondPartIndex);

            serializeContext
                ->Class<RagdollPartComponentConfiguration>()
                ->Field("Shapes", &RagdollPartComponentConfiguration::m_shapes)
                ->Field("Body", &RagdollPartComponentConfiguration::m_body)
                ->Field("ModelTransform", &RagdollPartComponentConfiguration::m_modelTransform)
                ->Field("ToParent", &RagdollPartComponentConfiguration::m_toParent)
                ->Field("MotionType", &RagdollPartComponentConfiguration::m_motionType);

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
                    ->Class<AdditionalRagdollConstraint>(
                        "Additional constraint",
                        "A constraint between two non-parental parts.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AdditionalRagdollConstraint::m_geometry,
                        "Geometry",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AdditionalRagdollConstraint::m_firstPartIndex,
                        "First part index",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &AdditionalRagdollConstraint::m_secondPartIndex,
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
                        &RagdollPartComponentConfiguration::m_toParent,
                        "To parent",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &RagdollPartComponentConfiguration::m_motionType,
                        "Motion type",
                        "");

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
