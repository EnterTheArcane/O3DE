/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/DebugDraw.h>

#include <Jolt/BehaviorReflection.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void ReflectDebugDraw(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<DebugCaptureConfiguration>()
                ->Field("Flags", &DebugCaptureConfiguration::m_flags)
                ->Field("GeometryCapacity", &DebugCaptureConfiguration::m_geometryCapacity)
                ->Field("LineCapacity", &DebugCaptureConfiguration::m_lineCapacity)
                ->Field("TextCapacity", &DebugCaptureConfiguration::m_textCapacity)
                ->Field("TextByteCapacity", &DebugCaptureConfiguration::m_textByteCapacity)
                ->Field("TriangleCapacity", &DebugCaptureConfiguration::m_triangleCapacity);

            serializeContext
                ->Class<DebugCaptureStatistics>()
                ->Field("GeometryCount", &DebugCaptureStatistics::m_geometryCount)
                ->Field("DroppedGeometryCount", &DebugCaptureStatistics::m_droppedGeometryCount)
                ->Field("LineCount", &DebugCaptureStatistics::m_lineCount)
                ->Field("DroppedLineCount", &DebugCaptureStatistics::m_droppedLineCount)
                ->Field("TextCount", &DebugCaptureStatistics::m_textCount)
                ->Field("DroppedTextCount", &DebugCaptureStatistics::m_droppedTextCount)
                ->Field("TextByteCount", &DebugCaptureStatistics::m_textByteCount)
                ->Field("TriangleCount", &DebugCaptureStatistics::m_triangleCount)
                ->Field("DroppedTriangleCount", &DebugCaptureStatistics::m_droppedTriangleCount);

            serializeContext
                ->Class<DebugHairDrawSettings>()
                ->Field("StrandBegin", &DebugHairDrawSettings::m_strandBegin)
                ->Field("StrandEnd", &DebugHairDrawSettings::m_strandEnd)
                ->Field("Flags", &DebugHairDrawSettings::m_flags)
                ->Field("StrandColor", &DebugHairDrawSettings::m_strandColor);

            serializeContext
                ->Class<DebugDrawSettings>()
                ->Field("CameraPosition", &DebugDrawSettings::m_cameraPosition)
                ->Field("Hair", &DebugDrawSettings::m_hair)
                ->Field("Flags", &DebugDrawSettings::m_flags)
                ->Field("ShapeColor", &DebugDrawSettings::m_shapeColor)
                ->Field("SoftBodyConstraintColor", &DebugDrawSettings::m_softBodyConstraintColor);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, All);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, ContactManifolds);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, ContactPointReduction);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, ContactPoints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, ContactSupportingFaces);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, MotionQualityLinearCasts);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, SubmergedVolumes);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, VirtualCharacterConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, VirtualCharacterStickToFloor);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, VirtualCharacterSupportingVolumes);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, VirtualCharacterWalkStairs);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, BroadPhaseBounds);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, CharacterGround);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, Constraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, ConstraintLimits);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, ConstraintReferenceFrames);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, Hair);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, Queries);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, RagdollHierarchy);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, SoftBodies);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCaptureFlags, VehicleContacts);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCullMode, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCullMode, BackFace);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugCullMode, FrontFace);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawMode, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawMode, Solid);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawMode, Wireframe);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, All);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, AngularVelocities);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, GridDensities);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, GridVelocities);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, InitialGravity);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, NeutralDensities);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, Orientations);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, RenderStrands);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, Rods);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, SkinPoints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, UnloadedRods);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairDrawFlags, VertexVelocities);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, GlobalPose);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, GravityFactor);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, GridVelocityFactor);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, PerRenderStrand);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, PerSimulatedStrand);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, SkinGlobalPose);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugHairStrandColor, WorldTransformInfluence);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, Instance);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, Island);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, Material);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, MotionType);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, ShapeType);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugShapeColor, SleepState);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugSoftBodyConstraintColor, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugSoftBodyConstraintColor, ConstraintGroup);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugSoftBodyConstraintColor, ConstraintOrder);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugSoftBodyConstraintColor, ConstraintType);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, All);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, BoundingBoxes);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, CapturedSimulation);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, CenterOfMassTransforms);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, Constraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ConstraintLimits);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ConstraintReferenceFrames);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ConvexHullFaceOutlines);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, HeightfieldTriangleOutlines);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, MassAndInertia);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, MeshTriangleGroups);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, MeshTriangleOutlines);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, Shapes);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ShapeSupportingFaces);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ShapeSupportDirections);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ShapeSupportFunctions);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, ShapeWireframes);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SleepStatistics);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyBendConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyEdgeConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyLongRangeConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyPredictedBounds);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyRodBendTwistConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyRods);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyRodStates);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodySkinConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyVertexVelocities);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyVertices);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, SoftBodyVolumeConstraints);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, Velocities);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, DebugDrawFlags, WorldTransforms);

            behaviorContext->Class<DebugCaptureConfiguration>("DebugCaptureConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("flags", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugCaptureConfiguration::m_flags))
                ->Property(
                    "geometryCapacity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugCaptureConfiguration::m_geometryCapacity))
                ->Property("lineCapacity", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugCaptureConfiguration::m_lineCapacity))
                ->Property("textCapacity", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugCaptureConfiguration::m_textCapacity))
                ->Property(
                    "textByteCapacity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugCaptureConfiguration::m_textByteCapacity))
                ->Property(
                    "triangleCapacity",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugCaptureConfiguration::m_triangleCapacity));

            behaviorContext->Class<DebugCaptureStatistics>("DebugCaptureStatistics")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Property("geometryCount", BehaviorValueGetter(&DebugCaptureStatistics::m_geometryCount), nullptr)
                ->Property(
                    "droppedGeometryCount",
                    BehaviorValueGetter(&DebugCaptureStatistics::m_droppedGeometryCount),
                    nullptr)
                ->Property("lineCount", BehaviorValueGetter(&DebugCaptureStatistics::m_lineCount), nullptr)
                ->Property(
                    "droppedLineCount",
                    BehaviorValueGetter(&DebugCaptureStatistics::m_droppedLineCount),
                    nullptr)
                ->Property("textCount", BehaviorValueGetter(&DebugCaptureStatistics::m_textCount), nullptr)
                ->Property(
                    "droppedTextCount",
                    BehaviorValueGetter(&DebugCaptureStatistics::m_droppedTextCount),
                    nullptr)
                ->Property("textByteCount", BehaviorValueGetter(&DebugCaptureStatistics::m_textByteCount), nullptr)
                ->Property("triangleCount", BehaviorValueGetter(&DebugCaptureStatistics::m_triangleCount), nullptr)
                ->Property(
                    "droppedTriangleCount",
                    BehaviorValueGetter(&DebugCaptureStatistics::m_droppedTriangleCount),
                    nullptr);

            behaviorContext->Class<DebugHairDrawSettings>("DebugHairDrawSettings")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("strandBegin", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugHairDrawSettings::m_strandBegin))
                ->Property("strandEnd", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugHairDrawSettings::m_strandEnd))
                ->Property("flags", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugHairDrawSettings::m_flags))
                ->Property("strandColor", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugHairDrawSettings::m_strandColor));

            behaviorContext->Class<DebugDrawSettings>("DebugDrawSettings")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("cameraPosition", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugDrawSettings::m_cameraPosition))
                ->Property("hair", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugDrawSettings::m_hair))
                ->Property("flags", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugDrawSettings::m_flags))
                ->Property("shapeColor", JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugDrawSettings::m_shapeColor))
                ->Property(
                    "softBodyConstraintColor",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&DebugDrawSettings::m_softBodyConstraintColor));
        }
    }
} // namespace Jolt
