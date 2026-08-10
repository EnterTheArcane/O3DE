/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/EffectComponents.h>

#include <Box3D/System.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Box3D
{
    ExplosionComponent::ExplosionComponent(
        ExplosionConfiguration configuration,
        AZ::Name worldName,
        const bool explodeOnActivate)
        : m_configuration(AZStd::move(configuration))
        , m_worldName(AZStd::move(worldName))
        , m_explodeOnActivate(explodeOnActivate)
    {
    }

    void ExplosionComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ExplosionComponent, AZ::Component>()
                ->Field("Configuration", &ExplosionComponent::m_configuration)
                ->Field("WorldName", &ExplosionComponent::m_worldName)
                ->Field("ExplodeOnActivate", &ExplosionComponent::m_explodeOnActivate);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<ExplosionComponent>("Box3D Explosion", "Applies a radial impulse")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionComponent::m_worldName, "World", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ExplosionComponent::m_configuration, "Configuration", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &ExplosionComponent::m_explodeOnActivate,
                        "Explode on activate",
                        "Trigger the explosion when the component activates.");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<ExplosionRequestBus>("Box3DExplosionRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("Explode", &ExplosionRequests::Explode)
                ->Event("GetConfiguration", &ExplosionRequests::GetConfiguration)
                ->Event("UpdateConfiguration", &ExplosionRequests::UpdateConfiguration);

            behaviorContext->Class<ExplosionComponent>("Box3D::ExplosionComponent")->RequestBus("Box3DExplosionRequestBus");
        }
    }

    void ExplosionComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DExplosionService"));
    }

    void ExplosionComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DExplosionService"));
    }

    void ExplosionComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    bool ExplosionComponent::Explode()
    {
        if (!m_effects || !m_system)
        {
            return false;
        }
        WorldHandle worldHandle = m_system->GetDefaultWorldHandle();
        if (!m_worldName.IsEmpty())
        {
            worldHandle = m_system->FindWorld(m_worldName);
        }
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(transform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        ExplosionConfiguration configuration = m_configuration;
        configuration.m_position = transform.TransformPoint(m_configuration.m_position);
        return m_effects->Explode(worldHandle, configuration);
    }

    ExplosionConfiguration ExplosionComponent::GetConfiguration() const
    {
        return m_configuration;
    }

    void ExplosionComponent::UpdateConfiguration(
        const ExplosionConfiguration& configuration)
    {
        m_configuration = configuration;
    }

    void ExplosionComponent::Activate()
    {
        m_effects = AZ::Interface<IEffects>::Get();
        m_system = AZ::Interface<ISystem>::Get();
        ExplosionRequestBus::Handler::BusConnect(GetEntityId());
        if (m_explodeOnActivate)
        {
            [[maybe_unused]] const bool exploded = Explode();
        }
    }

    void ExplosionComponent::Deactivate()
    {
        ExplosionRequestBus::Handler::BusDisconnect();
        m_system = nullptr;
        m_effects = nullptr;
    }

    WindComponent::WindComponent(
        WindConfiguration configuration,
        const bool enabled)
        : m_configuration(AZStd::move(configuration))
        , m_enabled(enabled)
    {
    }

    void WindComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<WindComponent, AZ::Component>()
                ->Field("Configuration", &WindComponent::m_configuration)
                ->Field("Enabled", &WindComponent::m_enabled);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<WindComponent>("Box3D Wind", "Applies drag and lift to this body")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindComponent::m_configuration, "Configuration", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindComponent::m_enabled, "Enabled", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<WindRequestBus>("Box3DWindRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("ApplyWind", &WindRequests::ApplyWind)
                ->Event("GetConfiguration", &WindRequests::GetConfiguration)
                ->Event("UpdateConfiguration", &WindRequests::UpdateConfiguration)
                ->Event("SetEnabled", &WindRequests::SetEnabled)
                ->Event("IsEnabled", &WindRequests::IsEnabled);

            behaviorContext->Class<WindComponent>("Box3D::WindComponent")->RequestBus("Box3DWindRequestBus");
        }
    }

    void WindComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DWindService"));
    }

    void WindComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DWindService"));
    }

    void WindComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("Box3DBodyService"));
    }

    void WindComponent::ApplyWind()
    {
        if (!m_effects)
        {
            return;
        }
        WorldHandle worldHandle;
        BodyHandle bodyHandle;
        RigidBodyRequestBus::EventResult(worldHandle, GetEntityId(), &RigidBodyRequests::GetWorldHandle);
        RigidBodyRequestBus::EventResult(bodyHandle, GetEntityId(), &RigidBodyRequests::GetBodyHandle);
        [[maybe_unused]] const bool applied = m_effects->ApplyWind(worldHandle, bodyHandle, m_configuration);
    }

    WindConfiguration WindComponent::GetConfiguration() const
    {
        return m_configuration;
    }

    void WindComponent::UpdateConfiguration(
        const WindConfiguration& configuration)
    {
        m_configuration = configuration;
    }

    void WindComponent::SetEnabled(
        const bool enabled)
    {
        m_enabled = enabled;
    }

    bool WindComponent::IsEnabled() const
    {
        return m_enabled;
    }

    void WindComponent::Activate()
    {
        m_effects = AZ::Interface<IEffects>::Get();
        WindRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
    }

    void WindComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        WindRequestBus::Handler::BusDisconnect();
        m_effects = nullptr;
    }

    void WindComponent::OnTick(
        [[maybe_unused]] float deltaTime,
        [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_enabled)
        {
            ApplyWind();
        }
    }

    int WindComponent::GetTickOrder()
    {
        return AZ::ComponentTickBus::TICK_PHYSICS_SYSTEM - 1;
    }
} // namespace Box3D
