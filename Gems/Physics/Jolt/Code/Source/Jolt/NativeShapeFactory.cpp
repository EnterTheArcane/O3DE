/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/NativeShapeFactory.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/utility/move.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/DecoratedShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

namespace Jolt
{
    namespace
    {
        constexpr AZ::u32 MaximumHeightfieldSamplesPerSide = 16'384;
        constexpr AZ::u32 MaximumMeshTrianglesPerLeaf = 8;

        [[nodiscard]]
        JPH::Vec3 ToNativeVector(
            const AZ::Vector3& value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        JPH::Shape::ShapeResult RotateYToZ(
            const JPH::Shape* shape)
        {
            JPH::RotatedTranslatedShapeSettings settings(
                JPH::Vec3::sZero(),
                JPH::Quat::sRotation(JPH::Vec3::sAxisX(), AZ::Constants::HalfPi),
                shape);
            return settings.Create();
        }
    } // namespace

    NativeShapeResult CreateNativeShape(
        const ShapeConfiguration& configuration,
        const JPH::PhysicsMaterialList& materials)
    {
        const JPH::PhysicsMaterial* material = nullptr;
        if (!materials.empty())
        {
            material = materials.front();
        }

        JPH::Shape::ShapeResult nativeResult;
        AZStd::visit(
            [&nativeResult, density = configuration.m_density, material, &materials](const auto& geometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                if constexpr (AZStd::is_same_v<Geometry, BoxShapeConfiguration>)
                {
                    if (!geometry.m_dimensions.IsFinite()
                        || geometry.m_dimensions.GetMinElement() <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_convexRadius)
                        || geometry.m_convexRadius < 0.0f)
                    {
                        nativeResult.SetError("Invalid box dimensions or convex radius.");
                        return;
                    }

                    JPH::BoxShapeSettings settings(
                        ToNativeVector(geometry.m_dimensions * 0.5f),
                        geometry.m_convexRadius,
                        material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, CapsuleShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(geometry.m_cylinderHeight)
                        || geometry.m_cylinderHeight < 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_radius)
                        || geometry.m_radius <= 0.0f)
                    {
                        nativeResult.SetError("Invalid capsule height or radius.");
                        return;
                    }

                    JPH::CapsuleShapeSettings settings(geometry.m_cylinderHeight * 0.5f, geometry.m_radius, material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                    if (!nativeResult.HasError())
                    {
                        nativeResult = RotateYToZ(nativeResult.Get());
                    }
                }
                else if constexpr (AZStd::is_same_v<Geometry, ConvexHullShapeConfiguration>)
                {
                    if (geometry.m_points.size() < 4
                        || !AZ::IsFiniteFloat(geometry.m_hullTolerance)
                        || geometry.m_hullTolerance <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_maximumConvexRadius)
                        || geometry.m_maximumConvexRadius < 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_maximumConvexRadiusError)
                        || geometry.m_maximumConvexRadiusError < 0.0f)
                    {
                        nativeResult.SetError("Invalid convex hull points or tolerances.");
                        return;
                    }

                    JPH::Array<JPH::Vec3> points;
                    points.reserve(geometry.m_points.size());
                    for (const AZ::Vector3& point : geometry.m_points)
                    {
                        if (!point.IsFinite())
                        {
                            nativeResult.SetError("Convex hull points must be finite.");
                            return;
                        }
                        points.push_back(ToNativeVector(point));
                    }

                    JPH::ConvexHullShapeSettings settings(points, geometry.m_maximumConvexRadius, material);
                    settings.mDensity = density;
                    settings.mHullTolerance = geometry.m_hullTolerance;
                    settings.mMaxErrorConvexRadius = geometry.m_maximumConvexRadiusError;
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, CylinderShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(geometry.m_height)
                        || geometry.m_height <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_radius)
                        || geometry.m_radius <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_convexRadius)
                        || geometry.m_convexRadius < 0.0f)
                    {
                        nativeResult.SetError("Invalid cylinder height, radius, or convex radius.");
                        return;
                    }

                    JPH::CylinderShapeSettings settings(
                        geometry.m_height * 0.5f,
                        geometry.m_radius,
                        geometry.m_convexRadius,
                        material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                    if (!nativeResult.HasError())
                    {
                        nativeResult = RotateYToZ(nativeResult.Get());
                    }
                }
                else if constexpr (AZStd::is_same_v<Geometry, CustomConvexShapeConfiguration>)
                {
                    nativeResult.SetError("Custom convex geometry must be resolved by its registered provider.");
                }
                else if constexpr (AZStd::is_same_v<Geometry, EmptyShapeConfiguration>)
                {
                    if (!geometry.m_centerOfMass.IsFinite())
                    {
                        nativeResult.SetError("Empty shape center of mass must be finite.");
                        return;
                    }

                    JPH::EmptyShapeSettings settings(ToNativeVector(geometry.m_centerOfMass));
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, HeightfieldShapeConfiguration>)
                {
                    const size_t sampleCount = geometry.m_sampleCount;
                    if (sampleCount > MaximumHeightfieldSamplesPerSide)
                    {
                        nativeResult.SetError("Heightfield sample count exceeds the native format.");
                        return;
                    }
                    const size_t sampleValueCount = sampleCount * sampleCount;
                    size_t cellCount = 0;
                    if (sampleCount > 0)
                    {
                        cellCount = (sampleCount - 1) * (sampleCount - 1);
                    }
                    size_t materialCount = materials.size();
                    if (materialCount == 0)
                    {
                        materialCount = 1;
                    }
                    if (geometry.m_sampleCount < 4
                        || geometry.m_blockSize < 2
                        || geometry.m_blockSize > 8
                        || geometry.m_sampleCount / geometry.m_blockSize < 2
                        || geometry.m_bitsPerSample == 0
                        || geometry.m_bitsPerSample > JPH::HeightFieldShapeConstants::cMaxBitsPerSample
                        || geometry.m_heights.size() != sampleValueCount
                        || (!geometry.m_materialIndices.empty() && geometry.m_materialIndices.size() != cellCount)
                        || !geometry.m_origin.IsFinite()
                        || !geometry.m_spacing.IsFinite()
                        || geometry.m_spacing.GetX() <= 0.0f
                        || geometry.m_spacing.GetY() <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_activeEdgeCosineThreshold)
                        || geometry.m_activeEdgeCosineThreshold > 1.0f
                        || (geometry.m_overrideHeightRange
                            && (!AZ::IsFiniteFloat(geometry.m_minimumHeight)
                                || !AZ::IsFiniteFloat(geometry.m_maximumHeight)
                                || geometry.m_minimumHeight >= geometry.m_maximumHeight)))
                    {
                        nativeResult.SetError("Invalid heightfield samples, dimensions, materials, or compression settings.");
                        return;
                    }

                    JPH::Array<float> heights;
                    heights.resize(sampleValueCount);
                    for (size_t nativeRow = 0; nativeRow < sampleCount; ++nativeRow)
                    {
                        const size_t sourceRow = sampleCount - nativeRow - 1;
                        for (size_t column = 0; column < sampleCount; ++column)
                        {
                            const float height = geometry.m_heights[sourceRow * sampleCount + column];
                            if (!AZ::IsFiniteFloat(height)
                                && height != NoCollisionHeight)
                            {
                                nativeResult.SetError("Heightfield samples must be finite or the no-collision sentinel.");
                                return;
                            }
                            heights[nativeRow * sampleCount + column] = height;
                        }
                    }

                    JPH::Array<JPH::uint8> materialIndices;
                    if (!geometry.m_materialIndices.empty())
                    {
                        materialIndices.resize(cellCount);
                        const size_t cellRowCount = sampleCount - 1;
                        for (size_t nativeRow = 0; nativeRow < cellRowCount; ++nativeRow)
                        {
                            const size_t sourceRow = cellRowCount - nativeRow - 1;
                            for (size_t column = 0; column < cellRowCount; ++column)
                            {
                                const AZ::u8 materialIndex =
                                    geometry.m_materialIndices[sourceRow * cellRowCount + column];
                                if (materialIndex >= materialCount)
                                {
                                    nativeResult.SetError("Heightfield material index is out of range.");
                                    return;
                                }
                                materialIndices[nativeRow * cellRowCount + column] = materialIndex;
                            }
                        }
                    }

                    const float nativeOffsetY =
                        -(geometry.m_origin.GetY() + static_cast<float>(sampleCount - 1) * geometry.m_spacing.GetY());
                    const JPH::uint8* materialIndexData = nullptr;
                    if (!materialIndices.empty())
                    {
                        materialIndexData = materialIndices.data();
                    }
                    JPH::HeightFieldShapeSettings settings(
                        heights.data(),
                        JPH::Vec3(geometry.m_origin.GetX(), geometry.m_origin.GetZ(), nativeOffsetY),
                        JPH::Vec3(geometry.m_spacing.GetX(), 1.0f, geometry.m_spacing.GetY()),
                        geometry.m_sampleCount,
                        materialIndexData,
                        materials);
                    settings.mActiveEdgeCosThresholdAngle = geometry.m_activeEdgeCosineThreshold;
                    settings.mBitsPerSample = geometry.m_bitsPerSample;
                    settings.mBlockSize = geometry.m_blockSize;
                    settings.mMaterialsCapacity = geometry.m_materialCapacity;
                    if (settings.mMaterialsCapacity < materials.size())
                    {
                        settings.mMaterialsCapacity = static_cast<AZ::u32>(materials.size());
                    }
                    if (geometry.m_overrideHeightRange)
                    {
                        settings.mMinHeightValue = geometry.m_minimumHeight;
                        settings.mMaxHeightValue = geometry.m_maximumHeight;
                    }
                    nativeResult = settings.Create();
                    if (!nativeResult.HasError())
                    {
                        nativeResult = RotateYToZ(nativeResult.Get());
                    }
                }
                else if constexpr (AZStd::is_same_v<Geometry, MeshShapeConfiguration>)
                {
                    if (geometry.m_vertices.size() < 3
                        || geometry.m_triangles.empty()
                        || !AZ::IsFiniteFloat(geometry.m_activeEdgeCosineThreshold)
                        || geometry.m_activeEdgeCosineThreshold > 1.0f
                        || geometry.m_maximumTrianglesPerLeaf == 0
                        || geometry.m_maximumTrianglesPerLeaf > MaximumMeshTrianglesPerLeaf
                        || geometry.m_buildQuality == MeshBuildQuality::None)
                    {
                        nativeResult.SetError("Invalid mesh vertices, triangles, or build settings.");
                        return;
                    }

                    JPH::VertexList vertices;
                    vertices.reserve(geometry.m_vertices.size());
                    for (const AZ::Vector3& vertex : geometry.m_vertices)
                    {
                        if (!vertex.IsFinite())
                        {
                            nativeResult.SetError("Mesh vertices must be finite.");
                            return;
                        }
                        vertices.emplace_back(vertex.GetX(), vertex.GetY(), vertex.GetZ());
                    }

                    JPH::IndexedTriangleList triangles;
                    triangles.reserve(geometry.m_triangles.size());
                    size_t materialCount = materials.size();
                    if (materialCount == 0)
                    {
                        materialCount = 1;
                    }
                    for (const MeshTriangle& triangle : geometry.m_triangles)
                    {
                        if (triangle.m_firstVertex >= vertices.size()
                            || triangle.m_secondVertex >= vertices.size()
                            || triangle.m_thirdVertex >= vertices.size()
                            || triangle.m_materialIndex >= materialCount)
                        {
                            nativeResult.SetError("Mesh triangle indices are invalid or refer to an unavailable material.");
                            return;
                        }
                        triangles.emplace_back(
                            triangle.m_firstVertex,
                            triangle.m_secondVertex,
                            triangle.m_thirdVertex,
                            triangle.m_materialIndex,
                            triangle.m_userData);
                    }

                    JPH::MeshShapeSettings settings(AZStd::move(vertices), AZStd::move(triangles), materials);
                    settings.mActiveEdgeCosThresholdAngle = geometry.m_activeEdgeCosineThreshold;
                    settings.mMaxTrianglesPerLeaf = geometry.m_maximumTrianglesPerLeaf;
                    settings.mPerTriangleUserData = geometry.m_perTriangleUserData;
                    if (geometry.m_buildQuality == MeshBuildQuality::FavorBuildSpeed)
                    {
                        settings.mBuildQuality = JPH::MeshShapeSettings::EBuildQuality::FavorBuildSpeed;
                    }
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, PlaneShapeConfiguration>)
                {
                    const float normalLengthSq = geometry.m_normal.GetLengthSq();
                    if (!geometry.m_normal.IsFinite()
                        || !AZ::IsFiniteFloat(normalLengthSq)
                        || normalLengthSq <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_distance)
                        || !AZ::IsFiniteFloat(geometry.m_halfExtent)
                        || geometry.m_halfExtent <= 0.0f)
                    {
                        nativeResult.SetError("Invalid plane normal, distance, or half extent.");
                        return;
                    }

                    AZ::Vector3 normal = geometry.m_normal;
                    normal.Normalize();
                    JPH::PlaneShapeSettings settings(
                        JPH::Plane(ToNativeVector(normal), -geometry.m_distance),
                        material,
                        geometry.m_halfExtent);
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, SphereShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(geometry.m_radius) || geometry.m_radius <= 0.0f)
                    {
                        nativeResult.SetError("Invalid sphere radius.");
                        return;
                    }

                    JPH::SphereShapeSettings settings(geometry.m_radius, material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, TaperedCapsuleShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(geometry.m_height)
                        || geometry.m_height < 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_bottomRadius)
                        || geometry.m_bottomRadius <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_topRadius)
                        || geometry.m_topRadius <= 0.0f)
                    {
                        nativeResult.SetError("Invalid tapered capsule height or radius.");
                        return;
                    }

                    JPH::TaperedCapsuleShapeSettings settings(
                        geometry.m_height * 0.5f,
                        geometry.m_topRadius,
                        geometry.m_bottomRadius,
                        material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                    if (!nativeResult.HasError())
                    {
                        nativeResult = RotateYToZ(nativeResult.Get());
                    }
                }
                else if constexpr (AZStd::is_same_v<Geometry, TaperedCylinderShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(geometry.m_height)
                        || geometry.m_height <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_bottomRadius)
                        || geometry.m_bottomRadius <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_topRadius)
                        || geometry.m_topRadius <= 0.0f
                        || !AZ::IsFiniteFloat(geometry.m_convexRadius)
                        || geometry.m_convexRadius < 0.0f)
                    {
                        nativeResult.SetError("Invalid tapered cylinder height, radius, or convex radius.");
                        return;
                    }

                    JPH::TaperedCylinderShapeSettings settings(
                        geometry.m_height * 0.5f,
                        geometry.m_topRadius,
                        geometry.m_bottomRadius,
                        geometry.m_convexRadius,
                        material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                    if (!nativeResult.HasError())
                    {
                        nativeResult = RotateYToZ(nativeResult.Get());
                    }
                }
                else if constexpr (AZStd::is_same_v<Geometry, TriangleShapeConfiguration>)
                {
                    if (!geometry.m_firstVertex.IsFinite()
                        || !geometry.m_secondVertex.IsFinite()
                        || !geometry.m_thirdVertex.IsFinite()
                        || !AZ::IsFiniteFloat(geometry.m_convexRadius)
                        || geometry.m_convexRadius < 0.0f)
                    {
                        nativeResult.SetError("Invalid triangle vertices or convex radius.");
                        return;
                    }

                    JPH::TriangleShapeSettings settings(
                        ToNativeVector(geometry.m_firstVertex),
                        ToNativeVector(geometry.m_secondVertex),
                        ToNativeVector(geometry.m_thirdVertex),
                        geometry.m_convexRadius,
                        material);
                    settings.mDensity = density;
                    nativeResult = settings.Create();
                }
            },
            configuration.m_geometry);

        NativeShapeResult result;
        if (nativeResult.HasError())
        {
            result.m_error = nativeResult.GetError().c_str();
            return result;
        }

        nativeResult.Get()->SetUserData(configuration.m_userData);
        result.m_shape = nativeResult.Get();
        return result;
    }

    bool IsPotentiallyValidSubShapeId(
        const JPH::Shape& shape,
        const SubShapeId subShapeId)
    {
        const AZ::u32 bitCount = shape.GetSubShapeIDBitsRecursive();
        const AZ::u32 value = subShapeId.GetValue();
        if (bitCount == 0)
        {
            return value == AZStd::numeric_limits<AZ::u32>::max();
        }
        if (bitCount == AZStd::numeric_limits<AZ::u32>::digits)
        {
            return true;
        }

        const AZ::u32 usedBits = (AZ::u32{1} << bitCount) - 1;
        const AZ::u32 unusedBits = ~usedBits;
        return (value & unusedBits) == unusedBits;
    }

    bool GetNativeDirectChildShape(
        const JPH::Shape& shape,
        const SubShapeId subShapeId,
        SubShapeTransform& transform,
        AZ::u32& childIndex)
    {
        if (!IsPotentiallyValidSubShapeId(shape, subShapeId))
        {
            return false;
        }

        JPH::SubShapeID nativeSubShapeId;
        nativeSubShapeId.SetValue(subShapeId.GetValue());
        JPH::SubShapeID nativeRemainder;
        childIndex = AZStd::numeric_limits<AZ::u32>::max();
        if (shape.GetType() == JPH::EShapeType::Compound)
        {
            const auto& compound = static_cast<const JPH::CompoundShape&>(shape);
            childIndex = compound.GetSubShapeIndexFromID(nativeSubShapeId, nativeRemainder);
            if (childIndex >= compound.GetNumSubShapes())
            {
                return false;
            }
        }

        const JPH::TransformedShape nativeTransform = shape.GetSubShapeTransformedShape(
            nativeSubShapeId,
            JPH::Vec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::Vec3::sReplicate(1.0f),
            nativeRemainder);
        if (!nativeTransform.mShape)
        {
            return false;
        }

        if (nativeTransform.mShape.GetPtr() != &shape
            && childIndex == AZStd::numeric_limits<AZ::u32>::max())
        {
            childIndex = 0;
        }

        const JPH::RVec3 nativePosition = nativeTransform.mShapePositionCOM;
        const JPH::Quat nativeRotation = nativeTransform.mShapeRotation;
        const JPH::Vec3 nativeScale = nativeTransform.GetShapeScale();
        transform = {
            .m_centerOfMassPosition = AZ::Vector3(
                static_cast<float>(nativePosition.GetX()),
                static_cast<float>(nativePosition.GetY()),
                static_cast<float>(nativePosition.GetZ())),
            .m_rotation = AZ::Quaternion(
                nativeRotation.GetX(),
                nativeRotation.GetY(),
                nativeRotation.GetZ(),
                nativeRotation.GetW()),
            .m_scale = AZ::Vector3(nativeScale.GetX(), nativeScale.GetY(), nativeScale.GetZ()),
            .m_remainder = SubShapeId(nativeRemainder.GetValue()),
        };
        return true;
    }

    const JPH::MeshShape* FindNativeMesh(
        const JPH::Shape& shape)
    {
        const JPH::Shape* current = &shape;
        while (current->GetType() == JPH::EShapeType::Decorated)
        {
            current = static_cast<const JPH::DecoratedShape*>(current)->GetInnerShape();
            if (!current)
            {
                return nullptr;
            }
        }

        if (current->GetSubType() != JPH::EShapeSubType::Mesh)
        {
            return nullptr;
        }

        return static_cast<const JPH::MeshShape*>(current);
    }

    bool GetNativeMeshTriangleMaterialIndex(
        const JPH::Shape& shape,
        const SubShapeId subShapeId,
        AZ::u32& materialIndex)
    {
        if (!IsPotentiallyValidSubShapeId(shape, subShapeId))
        {
            return false;
        }

        JPH::SubShapeID nativeSubShapeId;
        nativeSubShapeId.SetValue(subShapeId.GetValue());
        JPH::SubShapeID remainder;
        const JPH::Shape* leafShape = shape.GetLeafShape(nativeSubShapeId, remainder);
        if (!leafShape || leafShape->GetSubType() != JPH::EShapeSubType::Mesh)
        {
            return false;
        }

        const auto* mesh = static_cast<const JPH::MeshShape*>(leafShape);
        materialIndex = mesh->GetMaterialIndex(remainder);
        return true;
    }

    bool GetNativeMeshTriangleUserData(
        const JPH::Shape& shape,
        const SubShapeId subShapeId,
        AZ::u32& userData)
    {
        if (!IsPotentiallyValidSubShapeId(shape, subShapeId))
        {
            return false;
        }

        JPH::SubShapeID nativeSubShapeId;
        nativeSubShapeId.SetValue(subShapeId.GetValue());
        JPH::SubShapeID remainder;
        const JPH::Shape* leafShape = shape.GetLeafShape(nativeSubShapeId, remainder);
        if (!leafShape || leafShape->GetSubType() != JPH::EShapeSubType::Mesh)
        {
            return false;
        }

        const auto* mesh = static_cast<const JPH::MeshShape*>(leafShape);
        userData = mesh->GetTriangleUserData(remainder);
        return true;
    }
} // namespace Jolt
