/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/CustomShape.h>
#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    inline constexpr AZ::u32 AppendCompoundChildIndex = AZStd::numeric_limits<AZ::u32>::max();
    inline constexpr float NoCollisionHeight = AZStd::numeric_limits<float>::max();

    struct BoxShapeConfiguration final
    {
        AZ_TYPE_INFO(BoxShapeConfiguration, BoxShapeConfigurationTypeId);

        AZ::Vector3 m_dimensions = AZ::Vector3::CreateOne();
        float m_convexRadius = 0.05f;
    };

    struct SphereShapeConfiguration final
    {
        AZ_TYPE_INFO(SphereShapeConfiguration, SphereShapeConfigurationTypeId);

        float m_radius = 0.5f;
    };

    struct CapsuleShapeConfiguration final
    {
        AZ_TYPE_INFO(CapsuleShapeConfiguration, CapsuleShapeConfigurationTypeId);

        float m_cylinderHeight = 1.0f;
        float m_radius = 0.5f;
    };

    struct ConvexHullShapeConfiguration final
    {
        AZ_TYPE_INFO(ConvexHullShapeConfiguration, ConvexHullShapeConfigurationTypeId);

        AZStd::vector<AZ::Vector3> m_points;
        float m_hullTolerance = 1.0e-3f;
        float m_maximumConvexRadius = 0.05f;
        float m_maximumConvexRadiusError = 0.05f;
    };

    struct ConvexHullState final
    {
        AZ_TYPE_INFO(ConvexHullState, ConvexHullStateTypeId);

        float m_convexRadius = 0.0f;
        AZ::u32 m_faceCount = 0;
        AZ::u32 m_pointCount = 0;
    };

    struct CylinderShapeConfiguration final
    {
        AZ_TYPE_INFO(CylinderShapeConfiguration, CylinderShapeConfigurationTypeId);

        float m_height = 1.0f;
        float m_radius = 0.5f;
        float m_convexRadius = 0.05f;
    };

    struct EmptyShapeConfiguration final
    {
        AZ_TYPE_INFO(EmptyShapeConfiguration, EmptyShapeConfigurationTypeId);

        AZ::Vector3 m_centerOfMass = AZ::Vector3::CreateZero();
    };

    struct HeightfieldShapeConfiguration final
    {
        AZ_TYPE_INFO(HeightfieldShapeConfiguration, HeightfieldShapeConfigurationTypeId);

        AZStd::vector<float> m_heights;
        AZStd::vector<AZ::u8> m_materialIndices;
        AZ::Vector3 m_origin = AZ::Vector3::CreateZero();
        AZ::Vector2 m_spacing = AZ::Vector2::CreateOne();

        float m_activeEdgeCosineThreshold = 0.996195f;
        float m_maximumHeight = 0.0f;
        float m_minimumHeight = 0.0f;
        AZ::u32 m_bitsPerSample = 8;
        AZ::u32 m_blockSize = 2;
        AZ::u32 m_materialCapacity = 0;
        AZ::u32 m_sampleCount = 0;
        bool m_overrideHeightRange = false;
    };

    struct HeightfieldRegion final
    {
        AZ_TYPE_INFO(HeightfieldRegion, HeightfieldRegionTypeId);

        AZ::u32 m_startColumn = 0;
        AZ::u32 m_startRow = 0;
        AZ::u32 m_columnCount = 0;
        AZ::u32 m_rowCount = 0;
    };

    struct HeightfieldState final
    {
        AZ_TYPE_INFO(HeightfieldState, HeightfieldStateTypeId);

        float m_maximumHeight = 0.0f;
        float m_minimumHeight = 0.0f;

        AZ::u32 m_blockSize = 0;
        AZ::u32 m_materialCount = 0;
        AZ::u32 m_sampleCount = 0;
        bool m_canUpdateHeights = false;
    };

    struct HeightfieldSubShapeCoordinates final
    {
        AZ_TYPE_INFO(HeightfieldSubShapeCoordinates, HeightfieldSubShapeCoordinatesTypeId);

        AZ::u32 m_column = 0;
        AZ::u32 m_row = 0;
        AZ::u32 m_triangle = 0;
    };

    struct HeightfieldUpdateConfiguration final
    {
        AZ_TYPE_INFO(HeightfieldUpdateConfiguration, HeightfieldUpdateConfigurationTypeId);

        float m_activeEdgeCosineThreshold = 0.996195f;
        bool m_activateBodies = true;
    };

    enum class MeshBuildQuality : AZ::u8
    {
        None = 0,
        FavorBuildSpeed,
        FavorRuntimePerformance,
    };

    enum class CompoundShapeKind : AZ::u8
    {
        None = 0,
        Mutable,
        Static,
    };

    struct MeshTriangle final
    {
        AZ_TYPE_INFO(MeshTriangle, MeshTriangleTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::u32 m_thirdVertex = 0;
        AZ::u32 m_materialIndex = 0;
        AZ::u32 m_userData = 0;
    };

    struct MeshShapeConfiguration final
    {
        AZ_TYPE_INFO(MeshShapeConfiguration, MeshShapeConfigurationTypeId);

        AZStd::vector<AZ::Vector3> m_vertices;
        AZStd::vector<MeshTriangle> m_triangles;

        float m_activeEdgeCosineThreshold = 0.996195f;
        AZ::u32 m_maximumTrianglesPerLeaf = 8;
        MeshBuildQuality m_buildQuality = MeshBuildQuality::FavorRuntimePerformance;
        bool m_perTriangleUserData = false;
    };

    struct PlaneShapeConfiguration final
    {
        AZ_TYPE_INFO(PlaneShapeConfiguration, PlaneShapeConfigurationTypeId);

        AZ::Vector3 m_normal = AZ::Vector3::CreateAxisZ();
        float m_distance = 0.0f;
        float m_halfExtent = 1'000.0f;
    };

    struct TaperedCapsuleShapeConfiguration final
    {
        AZ_TYPE_INFO(TaperedCapsuleShapeConfiguration, TaperedCapsuleShapeConfigurationTypeId);

        float m_height = 1.0f;
        float m_bottomRadius = 0.5f;
        float m_topRadius = 0.25f;
    };

    struct TaperedCylinderShapeConfiguration final
    {
        AZ_TYPE_INFO(TaperedCylinderShapeConfiguration, TaperedCylinderShapeConfigurationTypeId);

        float m_height = 1.0f;
        float m_bottomRadius = 0.5f;
        float m_topRadius = 0.25f;
        float m_convexRadius = 0.05f;
    };

    struct TriangleShapeConfiguration final
    {
        AZ_TYPE_INFO(TriangleShapeConfiguration, TriangleShapeConfigurationTypeId);

        AZ::Vector3 m_firstVertex = AZ::Vector3(-0.5f, -0.5f, 0.0f);
        AZ::Vector3 m_secondVertex = AZ::Vector3(0.5f, -0.5f, 0.0f);
        AZ::Vector3 m_thirdVertex = AZ::Vector3(0.0f, 0.5f, 0.0f);
        float m_convexRadius = 0.0f;
    };

    using ShapeGeometry = AZStd::variant<
        BoxShapeConfiguration,
        CapsuleShapeConfiguration,
        ConvexHullShapeConfiguration,
        CustomShapeConfiguration,
        CustomConvexShapeConfiguration,
        CylinderShapeConfiguration,
        EmptyShapeConfiguration,
        HeightfieldShapeConfiguration,
        MeshShapeConfiguration,
        PlaneShapeConfiguration,
        SphereShapeConfiguration,
        TaperedCapsuleShapeConfiguration,
        TaperedCylinderShapeConfiguration,
        TriangleShapeConfiguration>;

    using PrimitiveShapeGeometry = AZStd::variant<
        BoxShapeConfiguration,
        CapsuleShapeConfiguration,
        CylinderShapeConfiguration,
        EmptyShapeConfiguration,
        PlaneShapeConfiguration,
        SphereShapeConfiguration,
        TaperedCapsuleShapeConfiguration,
        TaperedCylinderShapeConfiguration,
        TriangleShapeConfiguration>;

    struct PrimitiveShapeState final
    {
        AZ_TYPE_INFO(PrimitiveShapeState, PrimitiveShapeStateTypeId);

        PrimitiveShapeGeometry m_geometry;
    };

    struct ShapeConfiguration final
    {
        AZ_TYPE_INFO(ShapeConfiguration, ShapeConfigurationTypeId);

        static void Reflect(AZ::ReflectContext* context);

        ShapeGeometry m_geometry;
        AZStd::vector<MaterialHandle> m_materials;
        AZ::u64 m_userData = 0;
        float m_density = 1'000.0f;
    };

    struct CompoundChildConfiguration final
    {
        AZ_TYPE_INFO(CompoundChildConfiguration, CompoundChildConfigurationTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        ShapeHandle m_shapeHandle;
        AZ::u32 m_userData = 0;
    };

    struct CompoundShapeConfiguration final
    {
        AZ_TYPE_INFO(CompoundShapeConfiguration, CompoundShapeConfigurationTypeId);

        AZStd::vector<CompoundChildConfiguration> m_children;
        AZ::u64 m_userData = 0;
        CompoundShapeKind m_kind = CompoundShapeKind::Static;
    };

    struct MutableCompoundUpdateConfiguration final
    {
        bool m_activateBodies = true;
        bool m_adjustCenterOfMass = true;
        bool m_updateMassProperties = true;
    };

    struct OffsetCenterOfMassShapeConfiguration final
    {
        AZ_TYPE_INFO(OffsetCenterOfMassShapeConfiguration, OffsetCenterOfMassShapeConfigurationTypeId);

        ShapeHandle m_shapeHandle;
        AZ::Vector3 m_offset = AZ::Vector3::CreateZero();
    };

    struct RotatedTranslatedShapeConfiguration final
    {
        AZ_TYPE_INFO(RotatedTranslatedShapeConfiguration, RotatedTranslatedShapeConfigurationTypeId);

        ShapeHandle m_shapeHandle;
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
    };

    struct ScaledShapeConfiguration final
    {
        AZ_TYPE_INFO(ScaledShapeConfiguration, ScaledShapeConfigurationTypeId);

        ShapeHandle m_shapeHandle;
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
    };

    using DecoratedShapeGeometry = AZStd::variant<
        OffsetCenterOfMassShapeConfiguration,
        RotatedTranslatedShapeConfiguration,
        ScaledShapeConfiguration>;

    struct DecoratedShapeConfiguration final
    {
        AZ_TYPE_INFO(DecoratedShapeConfiguration, DecoratedShapeConfigurationTypeId);

        DecoratedShapeGeometry m_geometry;
        AZ::u64 m_userData = 0;
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::CompoundShapeKind, "{370A6936-5E8B-4CE5-8EF0-9FC07E967A8E}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::MeshBuildQuality, "{6FABEC0C-7416-4796-95B3-BF1A76579FF6}");
