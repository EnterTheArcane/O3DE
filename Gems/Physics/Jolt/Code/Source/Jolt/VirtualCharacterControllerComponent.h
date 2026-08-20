/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/CharacterConfiguration.h>
#include <Jolt/TypeIds.h>
#include <Jolt/VirtualCharacterBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>

namespace Jolt
{
    class ColliderComponent;
    class RuntimeImplementation;

    class JOLT_API VirtualCharacterControllerComponent final
        : public AZ::Component
        , public VirtualCharacterRequestBus::Handler
        , private VirtualCharacterNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(VirtualCharacterControllerComponent, VirtualCharacterControllerComponentTypeId);

        VirtualCharacterControllerComponent() = default;
        explicit VirtualCharacterControllerComponent(VirtualCharacterComponentConfiguration configuration);
        ~VirtualCharacterControllerComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        VirtualCharacterHandle GetCharacterHandle() const override;

        [[nodiscard]]
        VirtualCharacterState GetState() const override;

        [[nodiscard]]
        AZ::u64 GetUserData() const override;

        bool SetUserData(AZ::u64 userData) override;

        [[nodiscard]]
        VirtualCharacterRuntimeConfiguration GetRuntimeConfiguration() const override;

        [[nodiscard]]
        AZStd::vector<CharacterCollisionHit> CheckCollision(
            const CharacterCollisionRequest& request) const override;

        bool UpdateRuntimeConfiguration(
            const VirtualCharacterRuntimeConfiguration& configuration) override;

        bool SetTransform(const WorldTransform& transform) override;

        bool SetVelocity(const AZ::Vector3& velocity) override;

        [[nodiscard]]
        AZ::Vector3 CancelVelocityTowardsSteepSlopes(
            const AZ::Vector3& desiredVelocity) const override;

        bool BeginContactTracking() override;

        bool EndContactTracking() override;

        [[nodiscard]]
        bool CanWalkStairs(const AZ::Vector3& desiredVelocity) const override;

        bool WalkStairs(
            const VirtualCharacterStairConfiguration& configuration) override;

        bool StickToFloor(const AZ::Vector3& stepDown) override;

        bool RefreshContacts() override;

        bool UpdateGroundVelocity() override;

        [[nodiscard]]
        AZStd::vector<VirtualCharacterContact> GetContacts() const override;

        [[nodiscard]]
        bool HasCollidedWithBody(BodyHandle bodyHandle) const override;

        [[nodiscard]]
        bool HasCollidedWithCharacter(
            VirtualCharacterHandle characterHandle) const override;

        bool UpdateConfiguration(
            const VirtualCharacterUpdateConfiguration& configuration) override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnCharacterMoved(const VirtualCharacterMoveEvent& event) override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        VirtualCharacterComponentConfiguration m_configuration;

        RuntimeImplementation* m_system = nullptr;
        ColliderComponent* m_collider = nullptr;
        WorldHandle m_worldHandle;
        VirtualCharacterHandle m_characterHandle;
        float m_uniformScale = 1.0f;
        bool m_syncingTransform = false;
    };
} // namespace Jolt
