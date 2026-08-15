/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/ColliderBus.h>
#include <Jolt/ShapeOwner.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class ISystem;
    class RigidBodyComponent;
    class StaticRigidBodyComponent;
    class CharacterControllerComponent;

    class ColliderComponent final
        : public AZ::Component
        , public ColliderRequestBus::Handler
    {
    public:
        AZ_COMPONENT(ColliderComponent, ColliderComponentTypeId);

        ColliderComponent() = default;
        explicit ColliderComponent(AZStd::vector<ColliderShapeConfiguration> configurations);
        ~ColliderComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        [[nodiscard]]
        AZStd::span<const ColliderShapeConfiguration> GetShapeConfigurations() const override;

        [[nodiscard]]
        AZStd::span<const ShapeHandle> GetShapeHandles() const override;

        [[nodiscard]]
        ShapeHandle GetRootShapeHandle() const override;

        [[nodiscard]]
        bool GetRootShapeStats(ShapeStats& stats) const override;

        [[nodiscard]]
        bool GetRootShapeStatsRecursive(ShapeStats& stats) const override;

        [[nodiscard]]
        bool GetRootShapeProperties(ShapeProperties& properties) const override;

        [[nodiscard]]
        bool GetRootShapeSubmergedVolume(
            const SubmergedVolumeRequest& request,
            SubmergedVolumeResult& result) const override;

        [[nodiscard]]
        bool GetRootBoxConfiguration(BoxShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootCapsuleConfiguration(CapsuleShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootConvexHullState(ConvexHullState& state) const override;

        [[nodiscard]]
        ConvexHullTopology GetRootConvexHullTopology() const override;

        [[nodiscard]]
        bool GetRootCylinderConfiguration(CylinderShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootEmptyConfiguration(EmptyShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootPlaneConfiguration(PlaneShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootSphereConfiguration(SphereShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootTaperedCapsuleConfiguration(
            TaperedCapsuleShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootTaperedCylinderConfiguration(
            TaperedCylinderShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootTriangleConfiguration(TriangleShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootShapeMaterial(
            SubShapeId subShapeId,
            MaterialHandle& materialHandle) const override;

        [[nodiscard]]
        bool GetRootShapeSurfaceNormal(
            SubShapeId subShapeId,
            const AZ::Vector3& localSurfacePosition,
            AZ::Vector3& normal) const override;

        [[nodiscard]]
        bool GetRootShapeUserData(AZ::u64& userData) const override;

        [[nodiscard]]
        bool GetRootShapeSubShapeUserData(
            SubShapeId subShapeId,
            AZ::u64& userData) const override;

        [[nodiscard]]
        bool GetRootDirectChildShape(
            SubShapeId subShapeId,
            ShapeHandle& childShapeHandle,
            SubShapeTransform& transform) const override;

        [[nodiscard]]
        bool GetRootOffsetCenterOfMassConfiguration(
            OffsetCenterOfMassShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootRotatedTranslatedConfiguration(
            RotatedTranslatedShapeConfiguration& configuration) const override;

        [[nodiscard]]
        bool GetRootScaledConfiguration(
            ScaledShapeConfiguration& configuration) const override;

        [[nodiscard]]
        MaterialCollection GetRootMeshMaterials() const override;

        [[nodiscard]]
        bool GetRootMeshTriangleMaterialIndex(
            SubShapeId subShapeId,
            AZ::u32& materialIndex) const override;

        [[nodiscard]]
        bool GetRootMeshTriangleUserData(
            SubShapeId subShapeId,
            AZ::u32& userData) const override;

        [[nodiscard]]
        bool GetRootHeightfieldState(HeightfieldState& state) const override;

        [[nodiscard]]
        bool GetRootHeightfieldPosition(
            AZ::u32 column,
            AZ::u32 row,
            AZ::Vector3& position) const override;

        [[nodiscard]]
        bool IsRootHeightfieldNoCollision(
            AZ::u32 column,
            AZ::u32 row,
            bool& noCollision) const override;

        [[nodiscard]]
        HeightfieldSampleCollection GetRootHeightfieldHeights(
            const HeightfieldRegion& region) const override;

        [[nodiscard]]
        HeightfieldMaterialIndexCollection GetRootHeightfieldMaterialIndices(
            const HeightfieldRegion& region) const override;

        [[nodiscard]]
        MaterialCollection GetRootHeightfieldMaterials() const override;

        [[nodiscard]]
        bool GetRootHeightfieldSubShapeCoordinates(
            SubShapeId subShapeId,
            HeightfieldSubShapeCoordinates& coordinates) const override;

        bool UpdateRootHeightfieldHeights(
            const HeightfieldRegion& region,
            const HeightfieldSampleCollection& samples,
            const HeightfieldUpdateConfiguration& configuration) override;

        bool UpdateRootHeightfieldMaterials(
            const HeightfieldRegion& region,
            const HeightfieldMaterialIndexCollection& materialIndices,
            const MaterialCollection& materials,
            bool activateBodies) override;

        [[nodiscard]]
        bool GetRootCompoundChildCount(AZ::u32& childCount) const override;

        [[nodiscard]]
        bool GetRootCompoundChild(
            AZ::u32 childIndex,
            CompoundChildConfiguration& child) const override;

        [[nodiscard]]
        bool GetRootCompoundChildIndex(
            SubShapeId subShapeId,
            AZ::u32& childIndex) const override;

        [[nodiscard]]
        bool IsRootShapeScaleValid(const AZ::Vector3& scale) const override;

        [[nodiscard]]
        bool MakeRootShapeScaleValid(
            const AZ::Vector3& scale,
            AZ::Vector3& validScale) const override;

    private:
        void Activate() override;

        void Deactivate() override;

        [[nodiscard]]
        bool CreateShapes(
            ISystem& system,
            WorldHandle worldHandle,
            float uniformScale);

        [[nodiscard]]
        bool UpdateUniformScale(
            BodyHandle bodyHandle,
            float uniformScale);

        [[nodiscard]]
        bool UpdateCharacterUniformScale(
            CharacterHandle characterHandle,
            float maximumPenetrationDepth,
            float uniformScale);

        [[nodiscard]]
        bool UpdateVirtualCharacterUniformScale(
            VirtualCharacterHandle characterHandle,
            float maximumPenetrationDepth,
            bool updateInnerBody,
            float uniformScale);

        void DestroyShapes();

        AZStd::vector<ColliderShapeConfiguration> m_configurations{ColliderShapeConfiguration{}};
        Internal::ShapeSet m_shapeSet;

        ISystem* m_system = nullptr;
        WorldHandle m_worldHandle;
        float m_uniformScale = 1.0f;

        friend class RigidBodyComponent;
        friend class StaticRigidBodyComponent;
        friend class CharacterControllerComponent;
        friend class VirtualCharacterControllerComponent;
    };
} // namespace Jolt
