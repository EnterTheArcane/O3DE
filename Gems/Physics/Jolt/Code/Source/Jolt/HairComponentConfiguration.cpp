/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/HairComponentConfiguration.h>

#include <Jolt/Reflection.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    HairComponentConfiguration HairComponentConfiguration::CreateDefault()
    {
        HairComponentConfiguration configuration;
        configuration.m_definition.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 1.0f), .m_inverseMass = 0.0f},
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.5f)},
            {.m_position = AZ::Vector3::CreateZero()},
        };
        configuration.m_definition.m_strands = {
            {.m_beginVertex = 0, .m_endVertex = 3, .m_materialIndex = 0},
        };
        configuration.m_definition.m_materials.emplace_back();
        return configuration;
    }

    void HairComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<HairComponentConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<HairGradient>()
                ->Field("Minimum", &HairGradient::m_minimum)
                ->Field("Maximum", &HairGradient::m_maximum)
                ->Field("MinimumFraction", &HairGradient::m_minimumFraction)
                ->Field("MaximumFraction", &HairGradient::m_maximumFraction);

            serializeContext
                ->Class<HairMaterialConfiguration>()
                ->Field("GlobalPose", &HairMaterialConfiguration::m_globalPose)
                ->Field("GravityFactor", &HairMaterialConfiguration::m_gravityFactor)
                ->Field("GridVelocityFactor", &HairMaterialConfiguration::m_gridVelocityFactor)
                ->Field("Radius", &HairMaterialConfiguration::m_radius)
                ->Field("SkinGlobalPose", &HairMaterialConfiguration::m_skinGlobalPose)
                ->Field("WorldTransformInfluence", &HairMaterialConfiguration::m_worldTransformInfluence)
                ->Field("BendComplianceMultipliers", &HairMaterialConfiguration::m_bendComplianceMultipliers)
                ->Field("AngularDamping", &HairMaterialConfiguration::m_angularDamping)
                ->Field("BendCompliance", &HairMaterialConfiguration::m_bendCompliance)
                ->Field("Friction", &HairMaterialConfiguration::m_friction)
                ->Field("GravityPreloadFactor", &HairMaterialConfiguration::m_gravityPreloadFactor)
                ->Field("GridDensityForceFactor", &HairMaterialConfiguration::m_gridDensityForceFactor)
                ->Field("InertiaMultiplier", &HairMaterialConfiguration::m_inertiaMultiplier)
                ->Field("LinearDamping", &HairMaterialConfiguration::m_linearDamping)
                ->Field("MaximumAngularVelocity", &HairMaterialConfiguration::m_maximumAngularVelocity)
                ->Field("MaximumLinearVelocity", &HairMaterialConfiguration::m_maximumLinearVelocity)
                ->Field("SimulationStrandFraction", &HairMaterialConfiguration::m_simulationStrandFraction)
                ->Field("StretchCompliance", &HairMaterialConfiguration::m_stretchCompliance)
                ->Field("EnableCollision", &HairMaterialConfiguration::m_enableCollision)
                ->Field("EnableLongRangeAttachments", &HairMaterialConfiguration::m_enableLongRangeAttachments);

            serializeContext
                ->Class<HairVertex>()
                ->Field("Position", &HairVertex::m_position)
                ->Field("InverseMass", &HairVertex::m_inverseMass);

            serializeContext
                ->Class<HairStrand>()
                ->Field("BeginVertex", &HairStrand::m_beginVertex)
                ->Field("EndVertex", &HairStrand::m_endVertex)
                ->Field("MaterialIndex", &HairStrand::m_materialIndex);

            serializeContext
                ->Class<HairTriangle>()
                ->Field("FirstVertex", &HairTriangle::m_firstVertex)
                ->Field("SecondVertex", &HairTriangle::m_secondVertex)
                ->Field("ThirdVertex", &HairTriangle::m_thirdVertex);

            serializeContext
                ->Class<HairSkinWeight>()
                ->Field("JointIndex", &HairSkinWeight::m_jointIndex)
                ->Field("Weight", &HairSkinWeight::m_weight);

            serializeContext
                ->Class<HairDefinitionConfiguration>()
                ->Field("Vertices", &HairDefinitionConfiguration::m_vertices)
                ->Field("Strands", &HairDefinitionConfiguration::m_strands)
                ->Field("Materials", &HairDefinitionConfiguration::m_materials)
                ->Field("ScalpVertices", &HairDefinitionConfiguration::m_scalpVertices)
                ->Field("ScalpTriangles", &HairDefinitionConfiguration::m_scalpTriangles)
                ->Field("ScalpInverseBindPoses", &HairDefinitionConfiguration::m_scalpInverseBindPoses)
                ->Field("ScalpSkinWeights", &HairDefinitionConfiguration::m_scalpSkinWeights)
                ->Field("InitialGravity", &HairDefinitionConfiguration::m_initialGravity)
                ->Field("SimulationBoundsPadding", &HairDefinitionConfiguration::m_simulationBoundsPadding)
                ->Field("GridSizeX", &HairDefinitionConfiguration::m_gridSizeX)
                ->Field("GridSizeY", &HairDefinitionConfiguration::m_gridSizeY)
                ->Field("GridSizeZ", &HairDefinitionConfiguration::m_gridSizeZ)
                ->Field("IterationsPerSecond", &HairDefinitionConfiguration::m_iterationsPerSecond)
                ->Field("ScalpSkinWeightsPerVertex", &HairDefinitionConfiguration::m_scalpSkinWeightsPerVertex)
                ->Field("VerticesPerStrand", &HairDefinitionConfiguration::m_verticesPerStrand)
                ->Field("MaximumDeltaTime", &HairDefinitionConfiguration::m_maximumDeltaTime);

            serializeContext
                ->Class<HairComponentConfiguration>()
                ->Field("Definition", &HairComponentConfiguration::m_definition)
                ->Field("JointModelTransforms", &HairComponentConfiguration::m_jointModelTransforms)
                ->Field("JointToHair", &HairComponentConfiguration::m_jointToHair)
                ->Field("ScalpToHeadTransform", &HairComponentConfiguration::m_scalpToHeadTransform)
                ->Field("ObjectLayer", &HairComponentConfiguration::m_objectLayer)
                ->Field("AutoUpdate", &HairComponentConfiguration::m_autoUpdate)
                ->Field("Enabled", &HairComponentConfiguration::m_enabled);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<HairGradient>("Gradient", "A value varying from hair root to tip.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairGradient::m_minimum, "Minimum", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairGradient::m_maximum, "Maximum", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairGradient::m_minimumFraction,
                        "Minimum fraction",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairGradient::m_maximumFraction,
                        "Maximum fraction",
                        "");

                editContext
                    ->Class<HairMaterialConfiguration>("Material", "Physical behavior for a group of strands.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairMaterialConfiguration::m_globalPose, "Global pose", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairMaterialConfiguration::m_gravityFactor, "Gravity", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_gridVelocityFactor,
                        "Grid velocity factor",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairMaterialConfiguration::m_radius, "Radius", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_skinGlobalPose,
                        "Skin global pose",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_worldTransformInfluence,
                        "World transform influence",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_bendComplianceMultipliers,
                        "Bend compliance multipliers",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_angularDamping,
                        "Angular damping",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_bendCompliance,
                        "Bend compliance",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairMaterialConfiguration::m_friction, "Friction", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_gravityPreloadFactor,
                        "Gravity preload factor",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_gridDensityForceFactor,
                        "Grid density force factor",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_inertiaMultiplier,
                        "Inertia multiplier",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_linearDamping,
                        "Linear damping",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_maximumAngularVelocity,
                        "Maximum angular velocity",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_maximumLinearVelocity,
                        "Maximum linear velocity",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_simulationStrandFraction,
                        "Simulation strand fraction",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_stretchCompliance,
                        "Stretch compliance",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_enableCollision,
                        "Enable collision",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairMaterialConfiguration::m_enableLongRangeAttachments,
                        "Enable long-range attachments",
                        "");

                editContext
                    ->Class<HairVertex>("Vertex", "An authored strand vertex.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairVertex::m_position, "Position", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairVertex::m_inverseMass, "Inverse mass", "");

                editContext
                    ->Class<HairStrand>("Strand", "A half-open range of vertices using one material.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairStrand::m_beginVertex, "Begin vertex", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairStrand::m_endVertex, "End vertex", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairStrand::m_materialIndex, "Material index", "");

                editContext
                    ->Class<HairTriangle>("Triangle", "A scalp mesh triangle.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairTriangle::m_firstVertex, "First vertex", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairTriangle::m_secondVertex, "Second vertex", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairTriangle::m_thirdVertex, "Third vertex", "");

                editContext
                    ->Class<HairSkinWeight>("Skin weight", "One scalp vertex influence.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairSkinWeight::m_jointIndex, "Joint index", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairSkinWeight::m_weight, "Weight", "");

                editContext
                    ->Class<HairDefinitionConfiguration>("Definition", "Strands, materials, scalp, and solver settings.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairDefinitionConfiguration::m_vertices, "Vertices", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairDefinitionConfiguration::m_strands, "Strands", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairDefinitionConfiguration::m_materials, "Materials", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_scalpVertices,
                        "Scalp vertices",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_scalpTriangles,
                        "Scalp triangles",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_scalpInverseBindPoses,
                        "Scalp inverse bind poses",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_scalpSkinWeights,
                        "Scalp skin weights",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_initialGravity,
                        "Initial gravity",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_simulationBoundsPadding,
                        "Simulation bounds padding",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_iterationsPerSecond,
                        "Iterations per second",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_gridSizeX,
                        "Grid size X",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_gridSizeY,
                        "Grid size Y",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_gridSizeZ,
                        "Grid size Z",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_scalpSkinWeightsPerVertex,
                        "Scalp skin weights per vertex",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_verticesPerStrand,
                        "Vertices per strand",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairDefinitionConfiguration::m_maximumDeltaTime,
                        "Maximum delta time",
                        "");

                editContext
                    ->Class<HairComponentConfiguration>("Hair", "Authoring and fixed-step update settings.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponentConfiguration::m_definition,
                        "Definition",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponentConfiguration::m_jointModelTransforms,
                        "Joint model transforms",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponentConfiguration::m_jointToHair,
                        "Joint to hair",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponentConfiguration::m_scalpToHeadTransform,
                        "Scalp to head",
                        "Transforms the authored scalp mesh into head space.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponentConfiguration::m_objectLayer,
                        "Object layer",
                        "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &HairComponentConfiguration::m_autoUpdate,
                        "Auto update",
                        "Update on deterministic world fixed steps.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HairComponentConfiguration::m_enabled, "Enabled", "");
            }
        }
    }
} // namespace Jolt
