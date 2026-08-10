/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Effects.h>
#include <Box3D/EffectsBus.h>
#include <Box3D/RigidBodyBus.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Name/Name.h>

namespace Box3D
{
    class ExplosionComponent final
        : public AZ::Component
        , public ExplosionRequestBus::Handler
    {
    public:
        AZ_COMPONENT(ExplosionComponent, ExplosionComponentTypeId);

        ExplosionComponent() = default;
        explicit ExplosionComponent(
            ExplosionConfiguration configuration,
            AZ::Name worldName = {},
            bool explodeOnActivate = false);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        [[nodiscard]]
        bool Explode() override;

        [[nodiscard]]
        ExplosionConfiguration GetConfiguration() const override;

        void UpdateConfiguration(const ExplosionConfiguration& configuration) override;

    private:
        void Activate() override;

        void Deactivate() override;

        ExplosionConfiguration m_configuration;
        AZ::Name m_worldName;

        IEffects* m_effects = nullptr;
        ISystem* m_system = nullptr;

        bool m_explodeOnActivate = false;
    };

    class WindComponent final
        : public AZ::Component
        , public WindRequestBus::Handler
        , private AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(WindComponent, WindComponentTypeId);

        WindComponent() = default;
        explicit WindComponent(
            WindConfiguration configuration,
            bool enabled = true);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void ApplyWind() override;

        [[nodiscard]]
        WindConfiguration GetConfiguration() const override;

        void UpdateConfiguration(const WindConfiguration& configuration) override;

        void SetEnabled(bool enabled) override;

        [[nodiscard]]
        bool IsEnabled() const override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnTick(
            float deltaTime,
            AZ::ScriptTimePoint time) override;

        int GetTickOrder() override;

        WindConfiguration m_configuration;

        IEffects* m_effects = nullptr;

        bool m_enabled = true;
    };
} // namespace Box3D
