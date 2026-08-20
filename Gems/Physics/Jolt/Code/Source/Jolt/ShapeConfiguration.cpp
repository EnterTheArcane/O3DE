/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ShapeConfiguration.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Jolt
{
    void ShapeConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<ShapeConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<BoxShapeConfiguration>()
                ->Field("Dimensions", &BoxShapeConfiguration::m_dimensions)
                ->Field("ConvexRadius", &BoxShapeConfiguration::m_convexRadius);

            serializeContext
                ->Class<CapsuleShapeConfiguration>()
                ->Field("CylinderHeight", &CapsuleShapeConfiguration::m_cylinderHeight)
                ->Field("Radius", &CapsuleShapeConfiguration::m_radius);

            serializeContext
                ->Class<ConvexHullShapeConfiguration>()
                ->Field("Points", &ConvexHullShapeConfiguration::m_points)
                ->Field("HullTolerance", &ConvexHullShapeConfiguration::m_hullTolerance)
                ->Field("MaximumConvexRadius", &ConvexHullShapeConfiguration::m_maximumConvexRadius)
                ->Field("MaximumConvexRadiusError", &ConvexHullShapeConfiguration::m_maximumConvexRadiusError);

            serializeContext
                ->Class<ConvexHullState>()
                ->Field("ConvexRadius", &ConvexHullState::m_convexRadius)
                ->Field("FaceCount", &ConvexHullState::m_faceCount)
                ->Field("PointCount", &ConvexHullState::m_pointCount);

            serializeContext
                ->Class<CustomConvexShapeConfiguration>()
                ->Field("Data", &CustomConvexShapeConfiguration::m_data)
                ->Field("EditorBounds", &CustomConvexShapeConfiguration::m_editorBounds)
                ->Field("ProviderId", &CustomConvexShapeConfiguration::m_providerId);

            serializeContext
                ->Class<CustomShapeTriangle>()
                ->Field("FirstVertex", &CustomShapeTriangle::m_firstVertex)
                ->Field("SecondVertex", &CustomShapeTriangle::m_secondVertex)
                ->Field("ThirdVertex", &CustomShapeTriangle::m_thirdVertex)
                ->Field("MaterialIndex", &CustomShapeTriangle::m_materialIndex)
                ->Field("UserData", &CustomShapeTriangle::m_userData);

            serializeContext
                ->Class<CustomShapeDependency>()
                ->Field("Path", &CustomShapeDependency::m_path)
                ->Field("ContentHash", &CustomShapeDependency::m_contentHash);

            serializeContext
                ->Class<CustomShapeConfiguration>()
                ->Field("Data", &CustomShapeConfiguration::m_data)
                ->Field("EditorBounds", &CustomShapeConfiguration::m_editorBounds)
                ->Field("ProviderId", &CustomShapeConfiguration::m_providerId);

            serializeContext
                ->Class<CylinderShapeConfiguration>()
                ->Field("Height", &CylinderShapeConfiguration::m_height)
                ->Field("Radius", &CylinderShapeConfiguration::m_radius)
                ->Field("ConvexRadius", &CylinderShapeConfiguration::m_convexRadius);

            serializeContext
                ->Class<EmptyShapeConfiguration>()
                ->Field("CenterOfMass", &EmptyShapeConfiguration::m_centerOfMass);

            serializeContext
                ->Class<HeightfieldShapeConfiguration>()
                ->Field("Heights", &HeightfieldShapeConfiguration::m_heights)
                ->Field("MaterialIndices", &HeightfieldShapeConfiguration::m_materialIndices)
                ->Field("Origin", &HeightfieldShapeConfiguration::m_origin)
                ->Field("Spacing", &HeightfieldShapeConfiguration::m_spacing)
                ->Field("ActiveEdgeCosineThreshold", &HeightfieldShapeConfiguration::m_activeEdgeCosineThreshold)
                ->Field("MaximumHeight", &HeightfieldShapeConfiguration::m_maximumHeight)
                ->Field("MinimumHeight", &HeightfieldShapeConfiguration::m_minimumHeight)
                ->Field("BitsPerSample", &HeightfieldShapeConfiguration::m_bitsPerSample)
                ->Field("BlockSize", &HeightfieldShapeConfiguration::m_blockSize)
                ->Field("MaterialCapacity", &HeightfieldShapeConfiguration::m_materialCapacity)
                ->Field("SampleCount", &HeightfieldShapeConfiguration::m_sampleCount)
                ->Field("OverrideHeightRange", &HeightfieldShapeConfiguration::m_overrideHeightRange);

            serializeContext
                ->Class<HeightfieldRegion>()
                ->Field("StartColumn", &HeightfieldRegion::m_startColumn)
                ->Field("StartRow", &HeightfieldRegion::m_startRow)
                ->Field("ColumnCount", &HeightfieldRegion::m_columnCount)
                ->Field("RowCount", &HeightfieldRegion::m_rowCount);

            serializeContext
                ->Class<HeightfieldState>()
                ->Field("MaximumHeight", &HeightfieldState::m_maximumHeight)
                ->Field("MinimumHeight", &HeightfieldState::m_minimumHeight)
                ->Field("BlockSize", &HeightfieldState::m_blockSize)
                ->Field("MaterialCount", &HeightfieldState::m_materialCount)
                ->Field("SampleCount", &HeightfieldState::m_sampleCount)
                ->Field("CanUpdateHeights", &HeightfieldState::m_canUpdateHeights);

            serializeContext
                ->Class<HeightfieldSubShapeCoordinates>()
                ->Field("Column", &HeightfieldSubShapeCoordinates::m_column)
                ->Field("Row", &HeightfieldSubShapeCoordinates::m_row)
                ->Field("Triangle", &HeightfieldSubShapeCoordinates::m_triangle);

            serializeContext
                ->Class<HeightfieldUpdateConfiguration>()
                ->Field("ActiveEdgeCosineThreshold", &HeightfieldUpdateConfiguration::m_activeEdgeCosineThreshold)
                ->Field("ActivateBodies", &HeightfieldUpdateConfiguration::m_activateBodies);

            serializeContext
                ->Class<MeshTriangle>()
                ->Field("FirstVertex", &MeshTriangle::m_firstVertex)
                ->Field("SecondVertex", &MeshTriangle::m_secondVertex)
                ->Field("ThirdVertex", &MeshTriangle::m_thirdVertex)
                ->Field("MaterialIndex", &MeshTriangle::m_materialIndex)
                ->Field("UserData", &MeshTriangle::m_userData);

            serializeContext
                ->Class<MeshShapeConfiguration>()
                ->Field("Vertices", &MeshShapeConfiguration::m_vertices)
                ->Field("Triangles", &MeshShapeConfiguration::m_triangles)
                ->Field("ActiveEdgeCosineThreshold", &MeshShapeConfiguration::m_activeEdgeCosineThreshold)
                ->Field("MaximumTrianglesPerLeaf", &MeshShapeConfiguration::m_maximumTrianglesPerLeaf)
                ->Field("BuildQuality", &MeshShapeConfiguration::m_buildQuality)
                ->Field("PerTriangleUserData", &MeshShapeConfiguration::m_perTriangleUserData);

            serializeContext
                ->Class<PlaneShapeConfiguration>()
                ->Field("Normal", &PlaneShapeConfiguration::m_normal)
                ->Field("Distance", &PlaneShapeConfiguration::m_distance)
                ->Field("HalfExtent", &PlaneShapeConfiguration::m_halfExtent);

            serializeContext
                ->Class<SphereShapeConfiguration>()
                ->Field("Radius", &SphereShapeConfiguration::m_radius);

            serializeContext
                ->Class<TaperedCapsuleShapeConfiguration>()
                ->Field("Height", &TaperedCapsuleShapeConfiguration::m_height)
                ->Field("BottomRadius", &TaperedCapsuleShapeConfiguration::m_bottomRadius)
                ->Field("TopRadius", &TaperedCapsuleShapeConfiguration::m_topRadius);

            serializeContext
                ->Class<TaperedCylinderShapeConfiguration>()
                ->Field("Height", &TaperedCylinderShapeConfiguration::m_height)
                ->Field("BottomRadius", &TaperedCylinderShapeConfiguration::m_bottomRadius)
                ->Field("TopRadius", &TaperedCylinderShapeConfiguration::m_topRadius)
                ->Field("ConvexRadius", &TaperedCylinderShapeConfiguration::m_convexRadius);

            serializeContext
                ->Class<TriangleShapeConfiguration>()
                ->Field("FirstVertex", &TriangleShapeConfiguration::m_firstVertex)
                ->Field("SecondVertex", &TriangleShapeConfiguration::m_secondVertex)
                ->Field("ThirdVertex", &TriangleShapeConfiguration::m_thirdVertex)
                ->Field("ConvexRadius", &TriangleShapeConfiguration::m_convexRadius);

            serializeContext
                ->Class<ShapeConfiguration>()
                ->Field("Geometry", &ShapeConfiguration::m_geometry)
                ->Field("Materials", &ShapeConfiguration::m_materials)
                ->Field("UserData", &ShapeConfiguration::m_userData)
                ->Field("Density", &ShapeConfiguration::m_density);

            serializeContext
                ->Class<PrimitiveShapeState>()
                ->Field("Geometry", &PrimitiveShapeState::m_geometry);

            serializeContext
                ->Class<CompoundChildConfiguration>()
                ->Field("ShapeHandle", &CompoundChildConfiguration::m_shapeHandle)
                ->Field("Position", &CompoundChildConfiguration::m_position)
                ->Field("Rotation", &CompoundChildConfiguration::m_rotation)
                ->Field("UserData", &CompoundChildConfiguration::m_userData);

            serializeContext
                ->Class<CompoundShapeConfiguration>()
                ->Field("Children", &CompoundShapeConfiguration::m_children)
                ->Field("UserData", &CompoundShapeConfiguration::m_userData)
                ->Field("Kind", &CompoundShapeConfiguration::m_kind);

            serializeContext
                ->Class<OffsetCenterOfMassShapeConfiguration>()
                ->Field("ShapeHandle", &OffsetCenterOfMassShapeConfiguration::m_shapeHandle)
                ->Field("Offset", &OffsetCenterOfMassShapeConfiguration::m_offset);

            serializeContext
                ->Class<RotatedTranslatedShapeConfiguration>()
                ->Field("ShapeHandle", &RotatedTranslatedShapeConfiguration::m_shapeHandle)
                ->Field("Position", &RotatedTranslatedShapeConfiguration::m_position)
                ->Field("Rotation", &RotatedTranslatedShapeConfiguration::m_rotation);

            serializeContext
                ->Class<ScaledShapeConfiguration>()
                ->Field("ShapeHandle", &ScaledShapeConfiguration::m_shapeHandle)
                ->Field("Scale", &ScaledShapeConfiguration::m_scale);

            serializeContext
                ->Class<DecoratedShapeConfiguration>()
                ->Field("Geometry", &DecoratedShapeConfiguration::m_geometry)
                ->Field("UserData", &DecoratedShapeConfiguration::m_userData);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            JOLT_BEHAVIOR_ENUM(*behaviorContext, CompoundShapeKind, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, CompoundShapeKind, Mutable);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, CompoundShapeKind, Static);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, MeshBuildQuality, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MeshBuildQuality, FavorBuildSpeed);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, MeshBuildQuality, FavorRuntimePerformance);

            JOLT_BEHAVIOR_ENUM(*behaviorContext, CustomShapeGeometryKind, None);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, CustomShapeGeometryKind, Convex);
            JOLT_BEHAVIOR_ENUM(*behaviorContext, CustomShapeGeometryKind, Mesh);

            if (ShouldReflect(
                *behaviorContext,
                behaviorContext->m_classes.contains("JoltBoxShapeConfiguration")))
            {
                behaviorContext->Class<BoxShapeConfiguration>("JoltBoxShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Attribute(AZ::Script::Attributes::Alias, "BoxShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::ClassNameOverride, "BoxShapeConfiguration")
                    ->Property("dimensions", JOLT_BEHAVIOR_VALUE_PROPERTY(&BoxShapeConfiguration::m_dimensions))
                    ->Property("convexRadius", JOLT_BEHAVIOR_VALUE_PROPERTY(&BoxShapeConfiguration::m_convexRadius));

                behaviorContext->Class<CapsuleShapeConfiguration>("JoltCapsuleShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Attribute(AZ::Script::Attributes::Alias, "CapsuleShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::ClassNameOverride, "CapsuleShapeConfiguration")
                    ->Property(
                        "cylinderHeight",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&CapsuleShapeConfiguration::m_cylinderHeight))
                    ->Property("radius", JOLT_BEHAVIOR_VALUE_PROPERTY(&CapsuleShapeConfiguration::m_radius));

                behaviorContext->Class<ConvexHullState>("ConvexHullState")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("convexRadius", BehaviorValueGetter(&ConvexHullState::m_convexRadius), nullptr)
                    ->Property("faceCount", BehaviorValueGetter(&ConvexHullState::m_faceCount), nullptr)
                    ->Property("pointCount", BehaviorValueGetter(&ConvexHullState::m_pointCount), nullptr);

                behaviorContext->Class<CustomConvexShapeConfiguration>("CustomConvexShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("data", JOLT_BEHAVIOR_VALUE_PROPERTY(&CustomConvexShapeConfiguration::m_data))
                    ->Property(
                        "editorBounds",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&CustomConvexShapeConfiguration::m_editorBounds))
                    ->Property(
                        "providerId",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&CustomConvexShapeConfiguration::m_providerId));

                behaviorContext->Class<CustomShapeConfiguration>("CustomShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("data", JOLT_BEHAVIOR_VALUE_PROPERTY(&CustomShapeConfiguration::m_data))
                    ->Property(
                        "editorBounds",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&CustomShapeConfiguration::m_editorBounds))
                    ->Property(
                        "providerId",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&CustomShapeConfiguration::m_providerId));

                behaviorContext->Class<CylinderShapeConfiguration>("JoltCylinderShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Attribute(AZ::Script::Attributes::Alias, "CylinderShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::ClassNameOverride, "CylinderShapeConfiguration")
                    ->Property("height", JOLT_BEHAVIOR_VALUE_PROPERTY(&CylinderShapeConfiguration::m_height))
                    ->Property("radius", JOLT_BEHAVIOR_VALUE_PROPERTY(&CylinderShapeConfiguration::m_radius))
                    ->Property(
                        "convexRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&CylinderShapeConfiguration::m_convexRadius));

                behaviorContext->Class<EmptyShapeConfiguration>("EmptyShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property(
                        "centerOfMass",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&EmptyShapeConfiguration::m_centerOfMass));

                behaviorContext->Class<PlaneShapeConfiguration>("PlaneShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("normal", JOLT_BEHAVIOR_VALUE_PROPERTY(&PlaneShapeConfiguration::m_normal))
                    ->Property("distance", JOLT_BEHAVIOR_VALUE_PROPERTY(&PlaneShapeConfiguration::m_distance))
                    ->Property("halfExtent", JOLT_BEHAVIOR_VALUE_PROPERTY(&PlaneShapeConfiguration::m_halfExtent));

                behaviorContext->Class<SphereShapeConfiguration>("JoltSphereShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Attribute(AZ::Script::Attributes::Alias, "SphereShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::ClassNameOverride, "SphereShapeConfiguration")
                    ->Property("radius", JOLT_BEHAVIOR_VALUE_PROPERTY(&SphereShapeConfiguration::m_radius));

                behaviorContext->Class<TaperedCapsuleShapeConfiguration>("TaperedCapsuleShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("height", JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCapsuleShapeConfiguration::m_height))
                    ->Property(
                        "bottomRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCapsuleShapeConfiguration::m_bottomRadius))
                    ->Property(
                        "topRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCapsuleShapeConfiguration::m_topRadius));

                behaviorContext->Class<TaperedCylinderShapeConfiguration>("TaperedCylinderShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("height", JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCylinderShapeConfiguration::m_height))
                    ->Property(
                        "bottomRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCylinderShapeConfiguration::m_bottomRadius))
                    ->Property(
                        "topRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCylinderShapeConfiguration::m_topRadius))
                    ->Property(
                        "convexRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TaperedCylinderShapeConfiguration::m_convexRadius));

                behaviorContext->Class<TriangleShapeConfiguration>("TriangleShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property(
                        "firstVertex",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TriangleShapeConfiguration::m_firstVertex))
                    ->Property(
                        "secondVertex",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TriangleShapeConfiguration::m_secondVertex))
                    ->Property(
                        "thirdVertex",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TriangleShapeConfiguration::m_thirdVertex))
                    ->Property(
                        "convexRadius",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&TriangleShapeConfiguration::m_convexRadius));
            }

            if (ShouldReflect(
                *behaviorContext,
                behaviorContext->m_classes.contains("HeightfieldRegion")))
            {
                behaviorContext->Class<HeightfieldRegion>("HeightfieldRegion")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("startColumn", JOLT_BEHAVIOR_VALUE_PROPERTY(&HeightfieldRegion::m_startColumn))
                    ->Property("startRow", JOLT_BEHAVIOR_VALUE_PROPERTY(&HeightfieldRegion::m_startRow))
                    ->Property("columnCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&HeightfieldRegion::m_columnCount))
                    ->Property("rowCount", JOLT_BEHAVIOR_VALUE_PROPERTY(&HeightfieldRegion::m_rowCount));

                behaviorContext->Class<HeightfieldState>("HeightfieldState")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("maximumHeight", BehaviorValueGetter(&HeightfieldState::m_maximumHeight), nullptr)
                    ->Property("minimumHeight", BehaviorValueGetter(&HeightfieldState::m_minimumHeight), nullptr)
                    ->Property("blockSize", BehaviorValueGetter(&HeightfieldState::m_blockSize), nullptr)
                    ->Property("materialCount", BehaviorValueGetter(&HeightfieldState::m_materialCount), nullptr)
                    ->Property("sampleCount", BehaviorValueGetter(&HeightfieldState::m_sampleCount), nullptr)
                    ->Property("canUpdateHeights", BehaviorValueGetter(&HeightfieldState::m_canUpdateHeights), nullptr);

                behaviorContext->Class<HeightfieldSubShapeCoordinates>("HeightfieldSubShapeCoordinates")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("column", BehaviorValueGetter(&HeightfieldSubShapeCoordinates::m_column), nullptr)
                    ->Property("row", BehaviorValueGetter(&HeightfieldSubShapeCoordinates::m_row), nullptr)
                    ->Property("triangle", BehaviorValueGetter(&HeightfieldSubShapeCoordinates::m_triangle), nullptr);

                behaviorContext->Class<HeightfieldUpdateConfiguration>("HeightfieldUpdateConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property(
                        "activeEdgeCosineThreshold",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&HeightfieldUpdateConfiguration::m_activeEdgeCosineThreshold))
                    ->Property(
                        "activateBodies",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&HeightfieldUpdateConfiguration::m_activateBodies));
            }

            if (ShouldReflect(
                *behaviorContext,
                behaviorContext->m_classes.contains("OffsetCenterOfMassShapeConfiguration")))
            {
                behaviorContext->Class<OffsetCenterOfMassShapeConfiguration>(
                    "OffsetCenterOfMassShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property(
                        "shapeHandle",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&OffsetCenterOfMassShapeConfiguration::m_shapeHandle))
                    ->Property(
                        "offset",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&OffsetCenterOfMassShapeConfiguration::m_offset));

                behaviorContext->Class<RotatedTranslatedShapeConfiguration>(
                    "RotatedTranslatedShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property(
                        "shapeHandle",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&RotatedTranslatedShapeConfiguration::m_shapeHandle))
                    ->Property(
                        "position",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&RotatedTranslatedShapeConfiguration::m_position))
                    ->Property(
                        "rotation",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&RotatedTranslatedShapeConfiguration::m_rotation));

                behaviorContext->Class<ScaledShapeConfiguration>("ScaledShapeConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property(
                        "shapeHandle",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&ScaledShapeConfiguration::m_shapeHandle))
                    ->Property(
                        "scale",
                        JOLT_BEHAVIOR_VALUE_PROPERTY(&ScaledShapeConfiguration::m_scale));
            }

            if (ShouldReflect(
                *behaviorContext,
                behaviorContext->m_classes.contains("CompoundChildConfiguration")))
            {
                behaviorContext->Class<CompoundChildConfiguration>("CompoundChildConfiguration")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Module, "jolt")
                    ->Property("shapeHandle", JOLT_BEHAVIOR_VALUE_PROPERTY(&CompoundChildConfiguration::m_shapeHandle))
                    ->Property("position", JOLT_BEHAVIOR_VALUE_PROPERTY(&CompoundChildConfiguration::m_position))
                    ->Property("rotation", JOLT_BEHAVIOR_VALUE_PROPERTY(&CompoundChildConfiguration::m_rotation))
                    ->Property("userData", JOLT_BEHAVIOR_VALUE_PROPERTY(&CompoundChildConfiguration::m_userData));
            }
        }
    }
} // namespace Jolt
