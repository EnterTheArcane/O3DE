/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/ColliderBus.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/vector.h>

namespace Box3D
{
    class ISystem;
    class System;
    class RigidBodyComponent;
    class StaticRigidBodyComponent;

    class ColliderComponent final
        : public AZ::Component
        , public ColliderRequestBus::Handler
    {
    public:
        AZ_COMPONENT(ColliderComponent, ColliderComponentTypeId);

        ColliderComponent() = default;
        explicit ColliderComponent(ShapeConfiguration shapeConfiguration);
        explicit ColliderComponent(AZStd::vector<ShapeConfiguration> shapeConfigurations);
        ~ColliderComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        [[nodiscard]] AZStd::span<const ShapeConfiguration> GetShapeConfigurations() const override;
        [[nodiscard]] AZStd::span<const ShapeHandle> GetShapeHandles() const override;
        [[nodiscard]] AZ::Aabb GetAabb() const override;
        [[nodiscard]] bool IsSensor() const override;
        bool UpdateShape(size_t index, const ShapeConfiguration& configuration) override;
        bool SetCollisionFilter(size_t index, const CollisionFilter& collisionFilter) override;
        bool SetMaterials(size_t index, AZStd::span<const MaterialHandle> materials) override;

    private:
        void Activate() override;
        void Deactivate() override;
        [[nodiscard]] bool Attach(ISystem& system, WorldHandle worldHandle, BodyHandle bodyHandle, float uniformScale);
        [[nodiscard]] bool UpdateUniformScale(float uniformScale);
        void Detach();

        AZStd::vector<ShapeConfiguration> m_shapeConfigurations{ ShapeConfiguration{} };
        AZStd::vector<ShapeHandle> m_shapeHandles;
        AZStd::vector<AZStd::vector<MaterialHandle>> m_ownedMaterials;
        System* m_system = nullptr;
        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        float m_uniformScale = 1.0f;

        friend class RigidBodyComponent;
        friend class StaticRigidBodyComponent;
    };
} // namespace Box3D
