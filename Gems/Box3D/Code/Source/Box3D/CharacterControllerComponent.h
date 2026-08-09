/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/CharacterBus.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Name/Name.h>

namespace Box3D
{
    class ISystem;

    class CharacterControllerComponent final
        : public AZ::Component
        , public CharacterRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(CharacterControllerComponent, CharacterControllerComponentTypeId);

        CharacterControllerComponent() = default;
        explicit CharacterControllerComponent(CharacterConfiguration configuration, AZ::Name worldName = {});
        ~CharacterControllerComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool EnableSimulation() override;
        bool DisableSimulation() override;
        [[nodiscard]] bool IsSimulationEnabled() const override;
        [[nodiscard]] WorldHandle GetWorldHandle() const override;
        [[nodiscard]] CharacterHandle GetCharacterHandle() const override;
        [[nodiscard]] CharacterConfiguration GetConfiguration() const override;
        bool UpdateConfiguration(const CharacterConfiguration& configuration) override;
        [[nodiscard]] CharacterState GetState() const override;
        bool Move(const AZ::Vector3& velocity, float fixedTimeStep) override;

    private:
        void Activate() override;
        void Deactivate() override;
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        int GetTickOrder() override;

        CharacterConfiguration m_configuration;
        AZ::Name m_worldName;
        ISystem* m_system = nullptr;
        WorldHandle m_worldHandle;
        CharacterHandle m_characterHandle;
        bool m_syncingTransform = false;
    };
} // namespace Box3D
