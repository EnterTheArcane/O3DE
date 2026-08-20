/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Material.h>
#include <Jolt/Shape.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Plane.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

#include <cstddef>

namespace Jolt
{
    inline constexpr AZ::u32 MaximumScriptConvexHullElements = 65'536;
    inline constexpr AZ::u32 MaximumScriptHeightfieldElements = 1'048'576;

    struct ColliderShapeConfiguration final
    {
        AZ_TYPE_INFO(ColliderShapeConfiguration, ColliderShapeConfigurationTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        ShapeConfiguration m_shape;
        AZStd::vector<MaterialConfiguration> m_materials;
        AZ::Transform m_localTransform = AZ::Transform::CreateIdentity();
        AZ::u32 m_compoundUserData = 0;
    };

    class JOLT_API HeightfieldSampleCollection final
    {
    public:
        AZ_TYPE_INFO(HeightfieldSampleCollection, HeightfieldSampleCollectionTypeId);

        bool AddSample(float sample);

        void Clear();

        [[nodiscard]]
        AZ::u32 GetSampleCount() const;

        [[nodiscard]]
        float GetSample(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredSampleCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

        [[nodiscard]]
        AZStd::span<const float> GetSamples() const;

    private:
        friend class ColliderComponent;

        AZStd::vector<float> m_samples;
        AZ::u32 m_requiredSampleCount = 0;
    };

    class JOLT_API HeightfieldMaterialIndexCollection final
    {
    public:
        AZ_TYPE_INFO(HeightfieldMaterialIndexCollection, HeightfieldMaterialIndexCollectionTypeId);

        bool AddIndex(AZ::u8 index);

        void Clear();

        [[nodiscard]]
        AZ::u32 GetIndexCount() const;

        [[nodiscard]]
        AZ::u8 GetIndex(AZ::u32 position) const;

        [[nodiscard]]
        AZ::u32 GetRequiredIndexCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

        [[nodiscard]]
        AZStd::span<const AZ::u8> GetIndices() const;

    private:
        friend class ColliderComponent;

        AZStd::vector<AZ::u8> m_indices;
        AZ::u32 m_requiredIndexCount = 0;
    };

    class JOLT_API MaterialCollection final
    {
    public:
        AZ_TYPE_INFO(MaterialCollection, MaterialCollectionTypeId);

        bool AddMaterial(MaterialHandle materialHandle);

        void Clear();

        [[nodiscard]]
        AZ::u32 GetMaterialCount() const;

        [[nodiscard]]
        MaterialHandle GetMaterial(AZ::u32 index) const;

        [[nodiscard]]
        AZ::u32 GetRequiredMaterialCount() const;

        [[nodiscard]]
        bool HasOverflow() const;

        [[nodiscard]]
        AZStd::span<const MaterialHandle> GetMaterials() const;

    private:
        friend class ColliderComponent;

        AZStd::vector<MaterialHandle> m_materials;
        AZ::u32 m_requiredMaterialCount = 0;
    };

    class JOLT_API ConvexHullTopology final
    {
    public:
        AZ_TYPE_INFO(ConvexHullTopology, ConvexHullTopologyTypeId);

        [[nodiscard]]
        ConvexHullState GetState() const;

        [[nodiscard]]
        AZ::u32 GetPointCount() const;

        [[nodiscard]]
        AZ::Vector3 GetPointRelativeToCenterOfMass(AZ::u32 pointIndex) const;

        [[nodiscard]]
        AZ::u32 GetPlaneCount() const;

        [[nodiscard]]
        AZ::Plane GetPlaneRelativeToCenterOfMass(AZ::u32 planeIndex) const;

        [[nodiscard]]
        AZ::u32 GetFaceCount() const;

        [[nodiscard]]
        AZ::u32 GetFaceVertexCount(AZ::u32 faceIndex) const;

        [[nodiscard]]
        AZ::u32 GetFaceVertexIndex(
            AZ::u32 faceIndex,
            AZ::u32 faceVertexIndex) const;

        [[nodiscard]]
        bool HasOverflow() const;

        [[nodiscard]]
        bool IsComplete() const;

    private:
        friend class ColliderComponent;

        ConvexHullState m_state;

        AZStd::vector<AZ::Vector3> m_pointsRelativeToCenterOfMass;
        AZStd::vector<AZ::Plane> m_planesRelativeToCenterOfMass;
        AZStd::vector<AZ::u32> m_faceVertexOffsetsAndIndices;

        bool m_complete = false;
        bool m_overflow = false;
    };

    class IColliderRequests
        : public AZ::ComponentBus
    {
    public:
        [[nodiscard]]
        virtual AZStd::span<const ColliderShapeConfiguration> GetShapeConfigurations() const = 0;

        [[nodiscard]]
        virtual AZStd::span<const ShapeHandle> GetShapeHandles() const = 0;

        [[nodiscard]]
        virtual ShapeHandle GetRootShapeHandle() const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeStats(ShapeStats& stats) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeStatsRecursive(ShapeStats& stats) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeProperties(ShapeProperties& properties) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeSubmergedVolume(
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const = 0;

        [[nodiscard]]
        virtual bool GetRootBoxConfiguration(BoxShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootCapsuleConfiguration(CapsuleShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootConvexHullState(ConvexHullState& state) const = 0;

        [[nodiscard]]
        virtual ConvexHullTopology GetRootConvexHullTopology() const = 0;

        [[nodiscard]]
        virtual bool GetRootCylinderConfiguration(CylinderShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootEmptyConfiguration(EmptyShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootPlaneConfiguration(PlaneShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootSphereConfiguration(SphereShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootTaperedCapsuleConfiguration(
            TaperedCapsuleShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootTaperedCylinderConfiguration(
            TaperedCylinderShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootTriangleConfiguration(TriangleShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeMaterial(
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeSurfaceNormal(
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeUserData(AZ::u64& userData) const = 0;

        [[nodiscard]]
        virtual bool GetRootShapeSubShapeUserData(
            SubShapeId subShapeId,
            AZ::u64& userData) const = 0;

        [[nodiscard]]
        virtual bool GetRootDirectChildShape(
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const = 0;

        [[nodiscard]]
        virtual bool GetRootOffsetCenterOfMassConfiguration(
            OffsetCenterOfMassShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootRotatedTranslatedConfiguration(
            RotatedTranslatedShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual bool GetRootScaledConfiguration(
            ScaledShapeConfiguration& configuration) const = 0;

        [[nodiscard]]
        virtual MaterialCollection GetRootMeshMaterials() const = 0;

        [[nodiscard]]
        virtual bool GetRootMeshTriangleMaterialIndex(
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const = 0;

        [[nodiscard]]
        virtual bool GetRootMeshTriangleUserData(
            SubShapeId subShapeId,
            AZ::u32& userData) const = 0;

        [[nodiscard]]
        virtual bool GetRootHeightfieldState(HeightfieldState& state) const = 0;

        [[nodiscard]]
        virtual bool GetRootHeightfieldPosition(
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const = 0;

        [[nodiscard]]
        virtual bool IsRootHeightfieldNoCollision(
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const = 0;

        [[nodiscard]]
        virtual HeightfieldSampleCollection GetRootHeightfieldHeights(
            const HeightfieldRegion& region) const = 0;

        [[nodiscard]]
        virtual HeightfieldMaterialIndexCollection GetRootHeightfieldMaterialIndices(
            const HeightfieldRegion& region) const = 0;

        [[nodiscard]]
        virtual MaterialCollection GetRootHeightfieldMaterials() const = 0;

        [[nodiscard]]
        virtual bool GetRootHeightfieldSubShapeCoordinates(
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const = 0;

        virtual bool UpdateRootHeightfieldHeights(
            const HeightfieldRegion& region,
            const HeightfieldSampleCollection& samples,
            const HeightfieldUpdateConfiguration& configuration) = 0;

        virtual bool UpdateRootHeightfieldMaterials(
            const HeightfieldRegion& region,
            const HeightfieldMaterialIndexCollection& materialIndices,
            const MaterialCollection& materials,
            bool activateBodies) = 0;

        [[nodiscard]]
        virtual bool GetRootCompoundChildCount(AZ::u32& childCount) const = 0;

        [[nodiscard]]
        virtual bool GetRootCompoundChild(
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const = 0;

        [[nodiscard]]
        virtual bool GetRootCompoundChildIndex(
            SubShapeId subShapeId,
            AZ::u32& childIndex) const = 0;

        [[nodiscard]]
        virtual bool IsRootShapeScaleValid(const AZ::Vector3& scale) const = 0;

        [[nodiscard]]
        virtual bool MakeRootShapeScaleValid(
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const = 0;

        [[nodiscard]]
        virtual size_t GetShapeCount() const
        {
            return GetShapeHandles().size();
        }

        [[nodiscard]]
        virtual ShapeHandle GetShapeHandleAt(
            const size_t index) const
        {
            const AZStd::span<const ShapeHandle> handles = GetShapeHandles();
            if (index < handles.size())
            {
                return handles[index];
            }

            return {};
        }
    };

    using ColliderRequestBus = AZ::EBus<IColliderRequests>;
} // namespace Jolt
