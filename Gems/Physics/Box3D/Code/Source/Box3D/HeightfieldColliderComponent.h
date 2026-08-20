/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Configuration.h>
#include <Box3D/ColliderBus.h>
#include <Box3D/HeightfieldBus.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace Box3D
{
    class System;

    class BOX3D_API HeightfieldColliderComponent final
        : public AZ::Component
        , public ColliderRequestBus::Handler
        , public HeightfieldRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(HeightfieldColliderComponent, HeightfieldColliderComponentTypeId);

        HeightfieldColliderComponent() = default;

        explicit HeightfieldColliderComponent(
            ShapeConfiguration configuration,
            AZ::Name worldName = {});

        ~HeightfieldColliderComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        [[nodiscard]]
        AZStd::span<const ShapeConfiguration> GetShapeConfigurations() const override;

        [[nodiscard]]
        AZStd::span<const ShapeHandle> GetShapeHandles() const override;

        [[nodiscard]]
        AZ::Aabb GetAabb() const override;

        [[nodiscard]]
        bool IsSensor() const override;

        bool UpdateShape(
            size_t index,
            const ShapeConfiguration& configuration) override;

        bool SetCollisionFilter(
            size_t index,
            const CollisionFilter& collisionFilter) override;

        bool SetMaterials(
            size_t index,
            AZStd::span<const MaterialHandle> materials) override;

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        BodyHandle GetBodyHandle() const override;

        [[nodiscard]]
        ShapeHandle GetShapeHandle() const override;

        [[nodiscard]]
        AZ::u32 GetColumnCount() const override;

        [[nodiscard]]
        AZ::u32 GetRowCount() const override;

        [[nodiscard]]
        AZStd::span<const float> GetHeights() const override;

        [[nodiscard]]
        AZStd::span<const AZ::u8> GetMaterialIndices() const override;

        bool ReplaceHeightfield(const HeightfieldShapeConfiguration& configuration) override;

        bool UpdateHeights(
            AZ::u32 startColumn,
            AZ::u32 startRow,
            AZ::u32 columnCount,
            AZ::u32 rowCount,
            AZStd::span<const float> heights) override;

        bool UpdateMaterials(
            AZ::u32 startColumn,
            AZ::u32 startRow,
            AZ::u32 columnCount,
            AZ::u32 rowCount,
            AZStd::span<const AZ::u8> materialIndices) override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        [[nodiscard]]
        bool UpdateScale(float scale);

        ShapeConfiguration m_configuration{HeightfieldShapeConfiguration{{0.0f, 0.0f, 0.0f, 0.0f}, {}, 2, 2}};
        AZ::Name m_worldName;
        AZStd::vector<MaterialHandle> m_ownedMaterials;

        System* m_system = nullptr;

        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        ShapeHandle m_shapeHandle;

        float m_uniformScale = 1.0f;
    };
} // namespace Box3D
