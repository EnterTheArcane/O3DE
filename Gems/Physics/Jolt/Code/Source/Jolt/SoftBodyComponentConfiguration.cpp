/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SoftBodyComponentConfiguration.h>

#include <Jolt/Reflection.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void SoftBodyDefinitionArchive::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<SoftBodyDefinitionArchive>()
                ->Field("BinaryState", &SoftBodyDefinitionArchive::m_binaryState)
                ->Field("BuildFingerprint", &SoftBodyDefinitionArchive::m_buildFingerprint)
                ->Field("ContentHash", &SoftBodyDefinitionArchive::m_contentHash)
                ->Field("FormatVersion", &SoftBodyDefinitionArchive::m_formatVersion)
                ->Field("MaterialCount", &SoftBodyDefinitionArchive::m_materialCount);
        }
    }

    namespace
    {
        void ReflectSoftBodyTypes(AZ::SerializeContext& serializeContext)
        {
            if (!ShouldReflect<SoftBodyVertex>(serializeContext))
            {
                return;
            }

            serializeContext
                .Class<SoftBodyVertex>()
                ->Field("Position", &SoftBodyVertex::m_position)
                ->Field("Velocity", &SoftBodyVertex::m_velocity)
                ->Field("InverseMass", &SoftBodyVertex::m_inverseMass);

            serializeContext
                .Class<SoftBodyFace>()
                ->Field("FirstVertex", &SoftBodyFace::m_firstVertex)
                ->Field("SecondVertex", &SoftBodyFace::m_secondVertex)
                ->Field("ThirdVertex", &SoftBodyFace::m_thirdVertex)
                ->Field("MaterialIndex", &SoftBodyFace::m_materialIndex);

            serializeContext
                .Class<SoftBodyVertexAttributes>()
                ->Field("BendCompliance", &SoftBodyVertexAttributes::m_bendCompliance)
                ->Field("Compliance", &SoftBodyVertexAttributes::m_compliance)
                ->Field(
                    "LongRangeMaximumDistanceMultiplier",
                    &SoftBodyVertexAttributes::m_longRangeMaximumDistanceMultiplier)
                ->Field("ShearCompliance", &SoftBodyVertexAttributes::m_shearCompliance)
                ->Field("LongRangeAttachmentType", &SoftBodyVertexAttributes::m_longRangeAttachmentType);

            serializeContext
                .Class<SoftBodyEdgeConstraint>()
                ->Field("FirstVertex", &SoftBodyEdgeConstraint::m_firstVertex)
                ->Field("SecondVertex", &SoftBodyEdgeConstraint::m_secondVertex)
                ->Field("Compliance", &SoftBodyEdgeConstraint::m_compliance)
                ->Field("RestLength", &SoftBodyEdgeConstraint::m_restLength);

            serializeContext
                .Class<SoftBodyDihedralBendConstraint>()
                ->Field("FirstVertex", &SoftBodyDihedralBendConstraint::m_firstVertex)
                ->Field("SecondVertex", &SoftBodyDihedralBendConstraint::m_secondVertex)
                ->Field("ThirdVertex", &SoftBodyDihedralBendConstraint::m_thirdVertex)
                ->Field("FourthVertex", &SoftBodyDihedralBendConstraint::m_fourthVertex)
                ->Field("Compliance", &SoftBodyDihedralBendConstraint::m_compliance)
                ->Field("InitialAngle", &SoftBodyDihedralBendConstraint::m_initialAngle)
                ->Field("CalculateInitialAngle", &SoftBodyDihedralBendConstraint::m_calculateInitialAngle);

            serializeContext
                .Class<SoftBodyLongRangeConstraint>()
                ->Field("FixedVertex", &SoftBodyLongRangeConstraint::m_fixedVertex)
                ->Field("DynamicVertex", &SoftBodyLongRangeConstraint::m_dynamicVertex)
                ->Field("MaximumDistance", &SoftBodyLongRangeConstraint::m_maximumDistance);

            serializeContext
                .Class<SoftBodyRodStretchShearConstraint>()
                ->Field("FirstVertex", &SoftBodyRodStretchShearConstraint::m_firstVertex)
                ->Field("SecondVertex", &SoftBodyRodStretchShearConstraint::m_secondVertex)
                ->Field("BishopRotation", &SoftBodyRodStretchShearConstraint::m_bishopRotation)
                ->Field("Compliance", &SoftBodyRodStretchShearConstraint::m_compliance)
                ->Field("InverseMass", &SoftBodyRodStretchShearConstraint::m_inverseMass)
                ->Field("RestLength", &SoftBodyRodStretchShearConstraint::m_restLength)
                ->Field("CalculateBishopRotation", &SoftBodyRodStretchShearConstraint::m_calculateBishopRotation);

            serializeContext
                .Class<SoftBodyRodBendTwistConstraint>()
                ->Field("FirstRod", &SoftBodyRodBendTwistConstraint::m_firstRod)
                ->Field("SecondRod", &SoftBodyRodBendTwistConstraint::m_secondRod)
                ->Field("InitialRotation", &SoftBodyRodBendTwistConstraint::m_initialRotation)
                ->Field("Compliance", &SoftBodyRodBendTwistConstraint::m_compliance)
                ->Field("CalculateInitialRotation", &SoftBodyRodBendTwistConstraint::m_calculateInitialRotation);

            serializeContext
                .Class<SoftBodyVolumeConstraint>()
                ->Field("FirstVertex", &SoftBodyVolumeConstraint::m_firstVertex)
                ->Field("SecondVertex", &SoftBodyVolumeConstraint::m_secondVertex)
                ->Field("ThirdVertex", &SoftBodyVolumeConstraint::m_thirdVertex)
                ->Field("FourthVertex", &SoftBodyVolumeConstraint::m_fourthVertex)
                ->Field("Compliance", &SoftBodyVolumeConstraint::m_compliance)
                ->Field("RestVolume", &SoftBodyVolumeConstraint::m_restVolume)
                ->Field("CalculateRestVolume", &SoftBodyVolumeConstraint::m_calculateRestVolume);

            serializeContext
                .Class<SoftBodyInverseBind>()
                ->Field("Transform", &SoftBodyInverseBind::m_transform)
                ->Field("JointIndex", &SoftBodyInverseBind::m_jointIndex);

            serializeContext
                .Class<SoftBodySkinWeight>()
                ->Field("InverseBindIndex", &SoftBodySkinWeight::m_inverseBindIndex)
                ->Field("Weight", &SoftBodySkinWeight::m_weight);

            serializeContext
                .Class<SoftBodySkinConstraint>()
                ->Field("Weights", &SoftBodySkinConstraint::m_weights)
                ->Field("Vertex", &SoftBodySkinConstraint::m_vertex)
                ->Field("BackstopDistance", &SoftBodySkinConstraint::m_backstopDistance)
                ->Field("BackstopRadius", &SoftBodySkinConstraint::m_backstopRadius)
                ->Field("MaximumDistance", &SoftBodySkinConstraint::m_maximumDistance);

            serializeContext
                .Class<SoftBodyDefinitionConfiguration>()
                ->Field("Vertices", &SoftBodyDefinitionConfiguration::m_vertices)
                ->Field("Faces", &SoftBodyDefinitionConfiguration::m_faces)
                ->Field("VertexAttributes", &SoftBodyDefinitionConfiguration::m_vertexAttributes)
                ->Field("EdgeConstraints", &SoftBodyDefinitionConfiguration::m_edgeConstraints)
                ->Field("DihedralBendConstraints", &SoftBodyDefinitionConfiguration::m_dihedralBendConstraints)
                ->Field("LongRangeConstraints", &SoftBodyDefinitionConfiguration::m_longRangeConstraints)
                ->Field("RodStretchShearConstraints", &SoftBodyDefinitionConfiguration::m_rodStretchShearConstraints)
                ->Field("RodBendTwistConstraints", &SoftBodyDefinitionConfiguration::m_rodBendTwistConstraints)
                ->Field("VolumeConstraints", &SoftBodyDefinitionConfiguration::m_volumeConstraints)
                ->Field("InverseBinds", &SoftBodyDefinitionConfiguration::m_inverseBinds)
                ->Field("SkinConstraints", &SoftBodyDefinitionConfiguration::m_skinConstraints)
                ->Field("ShearAngleTolerance", &SoftBodyDefinitionConfiguration::m_shearAngleTolerance)
                ->Field("BendType", &SoftBodyDefinitionConfiguration::m_bendType)
                ->Field("CreateFaceConstraints", &SoftBodyDefinitionConfiguration::m_createFaceConstraints)
                ->Field("Optimize", &SoftBodyDefinitionConfiguration::m_optimize);

            serializeContext
                .Class<SoftBodyConfiguration>()
                ->Field("CollisionGroup", &SoftBodyConfiguration::m_collisionGroup)
                ->Field("UserData", &SoftBodyConfiguration::m_userData)
                ->Field("ObjectLayer", &SoftBodyConfiguration::m_objectLayer)
                ->Field("Friction", &SoftBodyConfiguration::m_friction)
                ->Field("GravityFactor", &SoftBodyConfiguration::m_gravityFactor)
                ->Field("LinearDamping", &SoftBodyConfiguration::m_linearDamping)
                ->Field("MaximumLinearVelocity", &SoftBodyConfiguration::m_maximumLinearVelocity)
                ->Field("Pressure", &SoftBodyConfiguration::m_pressure)
                ->Field("Restitution", &SoftBodyConfiguration::m_restitution)
                ->Field(
                    "SkinnedMaximumDistanceMultiplier",
                    &SoftBodyConfiguration::m_skinnedMaximumDistanceMultiplier)
                ->Field("VertexRadius", &SoftBodyConfiguration::m_vertexRadius)
                ->Field("IterationCount", &SoftBodyConfiguration::m_iterationCount)
                ->Field("Activate", &SoftBodyConfiguration::m_activate)
                ->Field("AllowSleeping", &SoftBodyConfiguration::m_allowSleeping)
                ->Field("EnableSkinConstraints", &SoftBodyConfiguration::m_enableSkinConstraints)
                ->Field("FacesDoubleSided", &SoftBodyConfiguration::m_facesDoubleSided)
                ->Field("MakeRotationIdentity", &SoftBodyConfiguration::m_makeRotationIdentity)
                ->Field("ManualUpdate", &SoftBodyConfiguration::m_manualUpdate)
                ->Field("UpdatePosition", &SoftBodyConfiguration::m_updatePosition);

            serializeContext
                .Class<SoftBodyRuntimeConfiguration>()
                ->Field("Friction", &SoftBodyRuntimeConfiguration::m_friction)
                ->Field("GravityFactor", &SoftBodyRuntimeConfiguration::m_gravityFactor)
                ->Field("LinearDamping", &SoftBodyRuntimeConfiguration::m_linearDamping)
                ->Field("MaximumLinearVelocity", &SoftBodyRuntimeConfiguration::m_maximumLinearVelocity)
                ->Field("Pressure", &SoftBodyRuntimeConfiguration::m_pressure)
                ->Field("Restitution", &SoftBodyRuntimeConfiguration::m_restitution)
                ->Field(
                    "SkinnedMaximumDistanceMultiplier",
                    &SoftBodyRuntimeConfiguration::m_skinnedMaximumDistanceMultiplier)
                ->Field("VertexRadius", &SoftBodyRuntimeConfiguration::m_vertexRadius)
                ->Field("IterationCount", &SoftBodyRuntimeConfiguration::m_iterationCount)
                ->Field("AllowSleeping", &SoftBodyRuntimeConfiguration::m_allowSleeping)
                ->Field("EnableSkinConstraints", &SoftBodyRuntimeConfiguration::m_enableSkinConstraints)
                ->Field("FacesDoubleSided", &SoftBodyRuntimeConfiguration::m_facesDoubleSided)
                ->Field("UpdatePosition", &SoftBodyRuntimeConfiguration::m_updatePosition);
        }

        void ReflectSoftBodyEditTypes(AZ::EditContext& editContext)
        {
            editContext
                .Class<SoftBodyVertex>("Vertex", "A simulated soft-body vertex.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyVertex::m_position, "Position", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyVertex::m_velocity, "Velocity", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyVertex::m_inverseMass, "Inverse mass", "");

            editContext
                .Class<SoftBodyFace>("Face", "A material-indexed triangular face.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyFace::m_firstVertex, "First vertex", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyFace::m_secondVertex, "Second vertex", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyFace::m_thirdVertex, "Third vertex", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyFace::m_materialIndex, "Material index", "");

            editContext
                .Class<SoftBodyVertexAttributes>("Vertex attributes", "Generated face-constraint properties.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVertexAttributes::m_bendCompliance,
                    "Bend compliance",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyVertexAttributes::m_compliance, "Compliance", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVertexAttributes::m_longRangeMaximumDistanceMultiplier,
                    "Long-range maximum distance multiplier",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVertexAttributes::m_shearCompliance,
                    "Shear compliance",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVertexAttributes::m_longRangeAttachmentType,
                    "Long-range attachment type",
                    "");

            editContext
                .Class<SoftBodyEdgeConstraint>("Edge constraint", "Maintains the distance between two vertices.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyEdgeConstraint::m_firstVertex,
                    "First vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyEdgeConstraint::m_secondVertex,
                    "Second vertex",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyEdgeConstraint::m_compliance, "Compliance", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyEdgeConstraint::m_restLength, "Rest length", "");

            editContext
                .Class<SoftBodyDihedralBendConstraint>(
                    "Dihedral bend constraint",
                    "Maintains the angle between two adjacent triangles.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_firstVertex,
                    "First vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_secondVertex,
                    "Second vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_thirdVertex,
                    "Third vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_fourthVertex,
                    "Fourth vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_compliance,
                    "Compliance",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_initialAngle,
                    "Initial angle",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDihedralBendConstraint::m_calculateInitialAngle,
                    "Calculate initial angle",
                    "");

            editContext
                .Class<SoftBodyLongRangeConstraint>(
                    "Long-range constraint",
                    "Limits a dynamic vertex relative to a fixed vertex.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyLongRangeConstraint::m_fixedVertex,
                    "Fixed vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyLongRangeConstraint::m_dynamicVertex,
                    "Dynamic vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyLongRangeConstraint::m_maximumDistance,
                    "Maximum distance",
                    "");

            editContext
                .Class<SoftBodyRodStretchShearConstraint>(
                    "Rod stretch-shear constraint",
                    "Maintains rod length and orientation.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_firstVertex,
                    "First vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_secondVertex,
                    "Second vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_bishopRotation,
                    "Bishop rotation",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_compliance,
                    "Compliance",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_inverseMass,
                    "Inverse mass",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_restLength,
                    "Rest length",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodStretchShearConstraint::m_calculateBishopRotation,
                    "Calculate bishop rotation",
                    "");

            editContext
                .Class<SoftBodyRodBendTwistConstraint>(
                    "Rod bend-twist constraint",
                    "Maintains the relative rotation of adjacent rods.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodBendTwistConstraint::m_firstRod,
                    "First rod",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodBendTwistConstraint::m_secondRod,
                    "Second rod",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodBendTwistConstraint::m_initialRotation,
                    "Initial rotation",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodBendTwistConstraint::m_compliance,
                    "Compliance",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyRodBendTwistConstraint::m_calculateInitialRotation,
                    "Calculate initial rotation",
                    "");

            editContext
                .Class<SoftBodyVolumeConstraint>("Volume constraint", "Maintains tetrahedron volume.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_firstVertex,
                    "First vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_secondVertex,
                    "Second vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_thirdVertex,
                    "Third vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_fourthVertex,
                    "Fourth vertex",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_compliance,
                    "Compliance",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_restVolume,
                    "Rest volume",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyVolumeConstraint::m_calculateRestVolume,
                    "Calculate rest volume",
                    "");

            editContext
                .Class<SoftBodyInverseBind>("Inverse bind", "Joint index and inverse bind transform.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyInverseBind::m_transform, "Transform", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyInverseBind::m_jointIndex, "Joint index", "");

            editContext
                .Class<SoftBodySkinWeight>("Skin weight", "A weighted inverse-bind reference.")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodySkinWeight::m_inverseBindIndex,
                    "Inverse bind index",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodySkinWeight::m_weight, "Weight", "");

            editContext
                .Class<SoftBodySkinConstraint>("Skin constraint", "Constrains one vertex to a skinned pose.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodySkinConstraint::m_weights, "Weights", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodySkinConstraint::m_vertex, "Vertex", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodySkinConstraint::m_backstopDistance,
                    "Backstop distance",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodySkinConstraint::m_backstopRadius,
                    "Backstop radius",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodySkinConstraint::m_maximumDistance,
                    "Maximum distance",
                    "");

            editContext
                .Class<SoftBodyDefinitionConfiguration>("Definition", "Rest geometry and solver constraints.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyDefinitionConfiguration::m_vertices, "Vertices", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyDefinitionConfiguration::m_faces, "Faces", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_vertexAttributes,
                    "Vertex attributes",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_edgeConstraints,
                    "Edge constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_dihedralBendConstraints,
                    "Dihedral bend constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_longRangeConstraints,
                    "Long-range constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_rodStretchShearConstraints,
                    "Rod stretch-shear constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_rodBendTwistConstraints,
                    "Rod bend-twist constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_volumeConstraints,
                    "Volume constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_inverseBinds,
                    "Inverse binds",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_skinConstraints,
                    "Skin constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_shearAngleTolerance,
                    "Shear angle tolerance",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyDefinitionConfiguration::m_bendType, "Bend type", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyDefinitionConfiguration::m_createFaceConstraints,
                    "Create face constraints",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyDefinitionConfiguration::m_optimize, "Optimize", "");

            editContext
                .Class<SoftBodyConfiguration>("Body", "Instance and runtime solver properties.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_userData, "User data", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_objectLayer, "Object layer", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_friction, "Friction", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_gravityFactor, "Gravity factor", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_linearDamping, "Linear damping", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyConfiguration::m_maximumLinearVelocity,
                    "Maximum linear velocity",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_pressure, "Pressure", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_restitution, "Restitution", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyConfiguration::m_skinnedMaximumDistanceMultiplier,
                    "Skinned maximum distance multiplier",
                    "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_vertexRadius, "Vertex radius", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_iterationCount, "Iterations", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_activate, "Activate", "")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_allowSleeping, "Allow sleeping", "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyConfiguration::m_enableSkinConstraints,
                    "Enable skin constraints",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyConfiguration::m_facesDoubleSided,
                    "Double-sided faces",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyConfiguration::m_makeRotationIdentity,
                    "Make rotation identity",
                    "")
                ->DataElement(
                    AZ::Edit::UIHandlers::Default,
                    &SoftBodyConfiguration::m_manualUpdate,
                    "Manual update",
                    "Exclude the body from automatic world stepping so UpdateManually controls simulation.")
                ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyConfiguration::m_updatePosition, "Update position", "");
        }
    } // namespace

    SoftBodyComponentConfiguration SoftBodyComponentConfiguration::CreateDefault()
    {
        SoftBodyComponentConfiguration configuration;
        configuration.m_definition.m_vertices = {
            {
                .m_position = AZ::Vector3(-0.5f, 0.0f, 0.5f),
                .m_inverseMass = 0.0f,
            },
            {
                .m_position = AZ::Vector3(0.5f, 0.0f, 0.5f),
                .m_inverseMass = 0.0f,
            },
            {.m_position = AZ::Vector3(-0.5f, 0.0f, -0.5f)},
            {.m_position = AZ::Vector3(0.5f, 0.0f, -0.5f)},
        };
        configuration.m_definition.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 2, .m_thirdVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2, .m_thirdVertex = 3},
        };
        configuration.m_definition.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 0, .m_secondVertex = 2},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 1, .m_secondVertex = 3},
            {.m_firstVertex = 2, .m_secondVertex = 3},
        };
        configuration.m_definition.m_createFaceConstraints = false;
        return configuration;
    }

    void SoftBodyComponentConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        CollisionGroupConfiguration::Reflect(context);
        MaterialConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SoftBodyComponentConfiguration>(*serializeContext))
            {
                return;
            }

            ReflectSoftBodyTypes(*serializeContext);

            serializeContext
                ->Class<SoftBodyComponentConfiguration>()
                ->Field("Definition", &SoftBodyComponentConfiguration::m_definition)
                ->Field("Body", &SoftBodyComponentConfiguration::m_body)
                ->Field("Materials", &SoftBodyComponentConfiguration::m_materials)
                ->Field("Enabled", &SoftBodyComponentConfiguration::m_enabled);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                ReflectSoftBodyEditTypes(*editContext);

                editContext
                    ->Class<SoftBodyComponentConfiguration>("Soft body", "Authored soft-body definition and instance.")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SoftBodyComponentConfiguration::m_definition,
                        "Definition",
                        "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyComponentConfiguration::m_body, "Body", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SoftBodyComponentConfiguration::m_materials,
                        "Materials",
                        "Materials referenced by face material indices.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SoftBodyComponentConfiguration::m_enabled, "Enabled", "");
            }
        }
    }
} // namespace Jolt
