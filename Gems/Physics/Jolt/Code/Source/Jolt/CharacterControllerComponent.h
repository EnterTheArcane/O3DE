/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/BodyBus.h>
#include <Jolt/CharacterBus.h>
#include <Jolt/CharacterConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>

namespace Jolt
{
    class ColliderComponent;
    class RuntimeImplementation;

    class JOLT_API CharacterControllerComponent final
        : public AZ::Component
        , public CharacterRequestBus::Handler
        , public BodyRequestBus::Handler
        , private BodyNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(CharacterControllerComponent, CharacterControllerComponentTypeId);

        CharacterControllerComponent() = default;
        explicit CharacterControllerComponent(CharacterComponentConfiguration configuration);
        ~CharacterControllerComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        CharacterHandle GetCharacterHandle() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        BodyHandle GetBodyHandle() const override;

        [[nodiscard]]
        AZ::u64 GetUserData() const override;

        bool SetUserData(AZ::u64 userData) override;

        [[nodiscard]]
        WorldTransform GetCenterOfMassTransform() const override;

        [[nodiscard]]
        CharacterState GetState() const override;

        [[nodiscard]]
        CharacterRuntimeConfiguration GetRuntimeConfiguration() const override;

        [[nodiscard]]
        AZStd::vector<CharacterCollisionHit> CheckCollision(
            const CharacterCollisionRequest& request) const override;

        bool UpdateRuntimeConfiguration(
            const CharacterRuntimeConfiguration& configuration) override;

        bool SetTransform(
            const WorldTransform& transform,
            bool activate) override;

        bool SetVelocity(const AZ::Vector3& velocity) override;

        bool AddImpulse(const AZ::Vector3& impulse) override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnBodyMoved(const BodyMoveEvent& event) override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        CharacterComponentConfiguration m_configuration;

        RuntimeImplementation* m_system = nullptr;
        ColliderComponent* m_collider = nullptr;
        WorldHandle m_worldHandle;
        CharacterHandle m_characterHandle;
        BodyHandle m_bodyHandle;
        float m_uniformScale = 1.0f;
        bool m_syncingTransform = false;
    };
} // namespace Jolt
