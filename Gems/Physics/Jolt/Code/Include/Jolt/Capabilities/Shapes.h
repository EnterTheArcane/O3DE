/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Query.h>
#include <Jolt/Shape.h>
#include <Jolt/ShapeConfiguration.h>
#include <AzCore/Math/Plane.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Shapes
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Shapes* Get();

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const ShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const CompoundShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            const DecoratedShapeConfiguration& configuration);

        [[nodiscard]]
        ShapeHandle CreateShape(
            WorldHandle worldHandle,
            CookedShapeHandle cookedShapeHandle);

        //! Creates an independent mutable copy of a heightfield or mutable compound shape.
        [[nodiscard]]
        ShapeHandle CloneShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle);

        //! Applies native scale repair and returns a new shape that retains the source shape.
        [[nodiscard]]
        ShapeHandle ScaleShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale);

        bool DestroyShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle) const;

        //! Returns storage owned directly by the root shape.
        [[nodiscard]]
        bool GetShapeStats(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const;

        //! Includes each unique child shape once and may allocate traversal bookkeeping.
        [[nodiscard]]
        bool GetShapeStatsRecursive(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeStats& stats) const;

        [[nodiscard]]
        bool GetShapeProperties(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ShapeProperties& properties) const;

        [[nodiscard]]
        bool GetShapeSubmergedVolume(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const;

        [[nodiscard]]
        bool GetPrimitiveShapeState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            PrimitiveShapeState& state) const;

        [[nodiscard]]
        bool GetConvexHullState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            ConvexHullState& state) const;

        [[nodiscard]]
        BufferResult GetConvexHullPointsRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Vector3> points) const;

        [[nodiscard]]
        BufferResult GetConvexHullPlanesRelativeToCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<AZ::Plane> planes) const;

        [[nodiscard]]
        BufferResult GetConvexHullFaceVertexIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 faceIndex,
            AZStd::span<AZ::u32> vertexIndices) const;

        //! The sub-shape ID must originate from a query against this shape.
        [[nodiscard]]
        bool GetShapeMaterial(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const;

        //! The sub-shape ID must originate from a query against this shape. The position is center-of-mass local space.
        [[nodiscard]]
        bool GetShapeSurfaceNormal(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const;

        [[nodiscard]]
        bool GetShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u64& userData) const;

        //! The sub-shape ID must originate from a query against this shape.
        [[nodiscard]]
        bool GetShapeSubShapeUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u64& userData) const;

        //! Resolves one level of a query sub-shape path in the root shape's center-of-mass space.
        [[nodiscard]]
        bool GetDirectChildShape(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const;

        [[nodiscard]]
        bool GetDecoratedShapeConfiguration(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            DecoratedShapeConfiguration& configuration) const;

        //! Returns the ordered material list for a mesh or decorated mesh root.
        [[nodiscard]]
        BufferResult GetMeshMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        //! The sub-shape ID must identify a mesh triangle in this shape.
        [[nodiscard]]
        bool GetMeshTriangleMaterialIndex(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const;

        //! The sub-shape ID must identify a mesh triangle in this shape.
        [[nodiscard]]
        bool GetMeshTriangleUserData(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            AZ::u32& userData) const;

        [[nodiscard]]
        bool IsShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale) const;

        [[nodiscard]]
        bool MakeShapeScaleValid(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const;

        [[nodiscard]]
        bool GetHeightfieldState(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            HeightfieldState& state) const;

        [[nodiscard]]
        bool GetHeightfieldPosition(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const;

        [[nodiscard]]
        bool ProjectOntoHeightfield(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const AZ::Vector3& localPosition,
            AZ::Vector3& surfacePosition,
            SubShapeId& subShapeId) const;

        [[nodiscard]]
        bool IsHeightfieldNoCollision(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const;

        [[nodiscard]]
        QueryResult GetHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<float> heights) const;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterialIndices(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<AZ::u8> materialIndices) const;

        [[nodiscard]]
        QueryResult GetHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            AZStd::span<MaterialHandle> materialHandles) const;

        [[nodiscard]]
        bool GetHeightfieldSubShapeCoordinates(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const;

        bool UpdateHeightfieldHeights(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const float> heights,
            const HeightfieldUpdateConfiguration& configuration = {});

        bool UpdateHeightfieldMaterials(
            WorldHandle worldHandle,
            ShapeHandle shapeHandle,
            const HeightfieldRegion& region,
            AZStd::span<const AZ::u8> materialIndices,
            AZStd::span<const MaterialHandle> materialHandles,
            bool activateBodies = true);

        bool AddMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            const CompoundChildConfiguration& child,
            AZ::u32 insertionIndex,
            AZ::u32& childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool RemoveMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool UpdateMutableCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            const CompoundChildConfiguration& child,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool UpdateMutableCompoundChildTransforms(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 startIndex,
            AZStd::span<const AZ::Vector3> positions,
            AZStd::span<const AZ::Quaternion> rotations,
            const MutableCompoundUpdateConfiguration& updateConfiguration = {});

        bool AdjustMutableCompoundCenterOfMass(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            bool updateMassProperties,
            bool activateBodies);

        [[nodiscard]]
        bool GetCompoundChildCount(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32& childCount) const;

        [[nodiscard]]
        bool GetCompoundChild(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const;

        //! The sub-shape ID must originate from a query against this compound shape.
        [[nodiscard]]
        bool GetCompoundChildIndex(
            WorldHandle worldHandle,
            ShapeHandle compoundShapeHandle,
            SubShapeId subShapeId,
            AZ::u32& childIndex) const;

    private:
        friend class Runtime;

        Shapes() = default;
        ~Shapes() = default;
    };
} // namespace Jolt
