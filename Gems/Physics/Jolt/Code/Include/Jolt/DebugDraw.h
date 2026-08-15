/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/PackedVector2.h>
#include <AzCore/Math/PackedVector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/string/string_view.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    void ReflectDebugDraw(AZ::ReflectContext* context);

    enum class DebugCaptureFlags : AZ::u16
    {
        None = 0,
        ContactManifolds = 1 << 0,
        ContactPointReduction = 1 << 1,
        ContactPoints = 1 << 2,
        ContactSupportingFaces = 1 << 3,
        MotionQualityLinearCasts = 1 << 4,
        SubmergedVolumes = 1 << 5,
        VirtualCharacterConstraints = 1 << 6,
        VirtualCharacterStickToFloor = 1 << 7,
        VirtualCharacterSupportingVolumes = 1 << 8,
        VirtualCharacterWalkStairs = 1 << 9,
        All = (1 << 10) - 1,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(DebugCaptureFlags)

    enum class DebugCullMode : AZ::u8
    {
        None = 0,
        BackFace,
        FrontFace,
    };

    enum class DebugDrawMode : AZ::u8
    {
        None = 0,
        Solid,
        Wireframe,
    };

    enum class DebugHairDrawFlags : AZ::u16
    {
        None = 0,
        AngularVelocities = 1 << 0,
        GridDensities = 1 << 1,
        GridVelocities = 1 << 2,
        InitialGravity = 1 << 3,
        NeutralDensities = 1 << 4,
        Orientations = 1 << 5,
        RenderStrands = 1 << 6,
        Rods = 1 << 7,
        SkinPoints = 1 << 8,
        UnloadedRods = 1 << 9,
        VertexVelocities = 1 << 10,
        All = (1 << 11) - 1,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(DebugHairDrawFlags)

    enum class DebugHairStrandColor : AZ::u8
    {
        None = 0,
        GlobalPose,
        GravityFactor,
        GridVelocityFactor,
        PerRenderStrand,
        PerSimulatedStrand,
        SkinGlobalPose,
        WorldTransformInfluence,
    };

    enum class DebugShapeColor : AZ::u8
    {
        None = 0,
        Instance,
        Island,
        Material,
        MotionType,
        ShapeType,
        SleepState,
    };

    enum class DebugSoftBodyConstraintColor : AZ::u8
    {
        None = 0,
        ConstraintType,
        ConstraintGroup,
        ConstraintOrder,
    };

    enum class DebugDrawFlags : AZ::u32
    {
        None = 0,
        BoundingBoxes = 1 << 0,
        CenterOfMassTransforms = 1 << 1,
        Constraints = 1 << 2,
        ConstraintLimits = 1 << 3,
        ConstraintReferenceFrames = 1 << 4,
        ConvexHullFaceOutlines = 1 << 5,
        HeightfieldTriangleOutlines = 1 << 6,
        MassAndInertia = 1 << 7,
        MeshTriangleGroups = 1 << 8,
        MeshTriangleOutlines = 1 << 9,
        Shapes = 1 << 10,
        ShapeSupportingFaces = 1 << 11,
        ShapeSupportDirections = 1 << 12,
        ShapeSupportFunctions = 1 << 13,
        ShapeWireframes = 1 << 14,
        SleepStatistics = 1 << 15,
        SoftBodyBendConstraints = 1 << 16,
        SoftBodyEdgeConstraints = 1 << 17,
        SoftBodyLongRangeConstraints = 1 << 18,
        SoftBodyPredictedBounds = 1 << 19,
        SoftBodyRodBendTwistConstraints = 1 << 20,
        SoftBodyRods = 1 << 21,
        SoftBodyRodStates = 1 << 22,
        SoftBodySkinConstraints = 1 << 23,
        SoftBodyVertexVelocities = 1 << 24,
        SoftBodyVertices = 1 << 25,
        SoftBodyVolumeConstraints = 1 << 26,
        Velocities = 1 << 27,
        WorldTransforms = 1 << 28,
        CapturedSimulation = 1 << 29,
        All = (1 << 30) - 1,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(DebugDrawFlags)

    struct DebugCaptureConfiguration final
    {
        AZ_TYPE_INFO(DebugCaptureConfiguration, DebugCaptureConfigurationTypeId);

        DebugCaptureFlags m_flags = DebugCaptureFlags::None;
        AZ::u32 m_geometryCapacity = 256;
        AZ::u32 m_lineCapacity = 4'096;
        AZ::u32 m_textCapacity = 256;
        AZ::u32 m_textByteCapacity = 16 * 1'024;
        AZ::u32 m_triangleCapacity = 1'024;
    };

    struct DebugCaptureStatistics final
    {
        AZ_TYPE_INFO(DebugCaptureStatistics, DebugCaptureStatisticsTypeId);

        AZ::u32 m_geometryCount = 0;
        AZ::u32 m_droppedGeometryCount = 0;
        AZ::u32 m_lineCount = 0;
        AZ::u32 m_droppedLineCount = 0;
        AZ::u32 m_textCount = 0;
        AZ::u32 m_droppedTextCount = 0;
        AZ::u32 m_textByteCount = 0;
        AZ::u32 m_triangleCount = 0;
        AZ::u32 m_droppedTriangleCount = 0;
    };

    struct DebugHairDrawSettings final
    {
        AZ_TYPE_INFO(DebugHairDrawSettings, DebugHairDrawSettingsTypeId);

        AZ::u32 m_strandBegin = 0;
        AZ::u32 m_strandEnd = AZStd::numeric_limits<AZ::u32>::max();
        DebugHairDrawFlags m_flags = DebugHairDrawFlags::None;
        DebugHairStrandColor m_strandColor = DebugHairStrandColor::PerSimulatedStrand;
    };

    struct DebugDrawSettings final
    {
        AZ_TYPE_INFO(DebugDrawSettings, DebugDrawSettingsTypeId);

        WorldPosition m_cameraPosition;
        DebugHairDrawSettings m_hair;
        DebugDrawFlags m_flags = DebugDrawFlags::Shapes;
        DebugShapeColor m_shapeColor = DebugShapeColor::MotionType;
        DebugSoftBodyConstraintColor m_softBodyConstraintColor = DebugSoftBodyConstraintColor::ConstraintType;
    };

    struct DebugVertex final
    {
        [[nodiscard]]
        AZ::Color GetColor() const
        {
            AZ::Color color;
            color.FromU32(m_colorRgba8);
            return color;
        }

        AZ::PackedVector3f m_position;
        AZ::PackedVector3f m_normal;
        AZ::PackedVector2f m_uv;
        AZ::u32 m_colorRgba8 = AZ::Color::CreateU32(255, 255, 255, 255);
    };

    struct DebugGeometryConfiguration final
    {
        AZ::Matrix3x3 m_basis = AZ::Matrix3x3::CreateIdentity();
        AZ::Color m_modelColor = AZ::Colors::White;
        WorldPosition m_translation;
        DebugCullMode m_cullMode = DebugCullMode::BackFace;
        DebugDrawMode m_drawMode = DebugDrawMode::Solid;
        bool m_castsShadow = true;
    };

    class IDebugFilter
    {
    public:
        virtual ~IDebugFilter() = default;

        //! Called while the world is locked. Implementations must not call ISystem.
        [[nodiscard]]
        virtual bool ShouldDraw([[maybe_unused]] BodyHandle bodyHandle) const
        {
            return true;
        }

        //! Called while the world is locked. Implementations must not call ISystem.
        [[nodiscard]]
        virtual bool ShouldDraw([[maybe_unused]] HairHandle hairHandle) const
        {
            return true;
        }
    };

    class IDebugRenderer
    {
    public:
        virtual ~IDebugRenderer() = default;

        //! Drawing callbacks run while the world is locked and must not call ISystem.
        //! An empty index view identifies a non-indexed triangle list of consecutive vertex triples.
        virtual void DrawGeometry(
            AZStd::span<const DebugVertex> vertices,
            AZStd::span<const AZ::u32> indices,
            const DebugGeometryConfiguration& configuration) = 0;

        virtual void DrawLine(
            const WorldPosition& start,
            const WorldPosition& end,
            const AZ::Color& color) = 0;

        virtual void DrawText(
            const WorldPosition& position,
            AZStd::string_view text,
            const AZ::Color& color,
            float height) = 0;

        virtual void DrawTriangle(
            const WorldPosition& first,
            const WorldPosition& second,
            const WorldPosition& third,
            const AZ::Color& color,
            bool castsShadow) = 0;
    };
} // namespace Jolt
