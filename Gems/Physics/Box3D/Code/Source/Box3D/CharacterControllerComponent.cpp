/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/CharacterControllerComponent.h>

#include <Box3D/System.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Box3D
{
    CharacterControllerComponent::CharacterControllerComponent(
        CharacterConfiguration configuration,
        AZ::Name worldName)
        : m_configuration(AZStd::move(configuration))
        , m_worldName(AZStd::move(worldName))
    {
    }

    void CharacterControllerComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<CharacterControllerComponent, AZ::Component>()
                ->Field("Configuration", &CharacterControllerComponent::m_configuration)
                ->Field("WorldName", &CharacterControllerComponent::m_worldName);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CharacterControllerComponent>("Box3D Character Controller", "Moves a swept capsule")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterControllerComponent::m_worldName, "World", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterControllerComponent::m_configuration, "Configuration", "");
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {

            behaviorContext->EBus<CharacterRequestBus>("Box3DCharacterRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "box3d")
                ->Attribute(AZ::Script::Attributes::Category, "Box3D")
                ->Event("EnableSimulation", &CharacterRequests::EnableSimulation)
                ->Event("DisableSimulation", &CharacterRequests::DisableSimulation)
                ->Event("IsSimulationEnabled", &CharacterRequests::IsSimulationEnabled)
                ->Event("GetWorldHandle", &CharacterRequests::GetWorldHandle)
                ->Event("GetCharacterHandle", &CharacterRequests::GetCharacterHandle)
                ->Event("GetConfiguration", &CharacterRequests::GetConfiguration)
                ->Event("UpdateConfiguration", &CharacterRequests::UpdateConfiguration)
                ->Event("GetState", &CharacterRequests::GetState)
                ->Event("Move", &CharacterRequests::Move);

            behaviorContext->Class<CharacterControllerComponent>("Box3D::CharacterControllerComponent")
                ->RequestBus("Box3DCharacterRequestBus");
        }
    }

    void CharacterControllerComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("Box3DCharacterService"));
    }

    void CharacterControllerComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("Box3DCharacterService"));
        incompatible.push_back(AZ_CRC_CE("Box3DBodyService"));
    }

    void CharacterControllerComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    bool CharacterControllerComponent::EnableSimulation()
    {
        if (m_characterHandle.IsValid())
        {
            return true;
        }
        if (!m_system)
        {
            return false;
        }

        m_worldHandle = m_system->GetDefaultWorldHandle();
        if (!m_worldName.IsEmpty())
        {
            m_worldHandle = m_system->FindWorld(m_worldName);
        }
        if (!m_worldHandle.IsValid())
        {
            return false;
        }

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(transform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        m_configuration.m_basePosition = transform.GetTranslation();
        m_configuration.m_rotation = transform.GetRotation();
        m_configuration.m_entityId = GetEntityId();
        m_configuration.m_name = AZ::Name(GetEntity()->GetName());
        m_characterHandle = m_system->CreateCharacter(m_worldHandle, m_configuration);
        return m_characterHandle.IsValid();
    }

    bool CharacterControllerComponent::DisableSimulation()
    {
        if (!m_characterHandle.IsValid())
        {
            return true;
        }
        if (!m_system->DestroyCharacter(m_worldHandle, m_characterHandle))
        {
            return false;
        }
        m_characterHandle = {};
        return true;
    }

    bool CharacterControllerComponent::IsSimulationEnabled() const
    {
        return m_system && m_characterHandle.IsValid();
    }

    WorldHandle CharacterControllerComponent::GetWorldHandle() const
    {
        return m_worldHandle;
    }

    CharacterHandle CharacterControllerComponent::GetCharacterHandle() const
    {
        return m_characterHandle;
    }

    CharacterConfiguration CharacterControllerComponent::GetConfiguration() const
    {
        return m_configuration;
    }

    bool CharacterControllerComponent::UpdateConfiguration(
        const CharacterConfiguration& configuration)
    {
        if (m_system && m_characterHandle.IsValid() && !m_system->UpdateCharacter(m_worldHandle, m_characterHandle, configuration))
        {
            return false;
        }
        m_configuration = configuration;
        return true;
    }

    CharacterState CharacterControllerComponent::GetState() const
    {
        CharacterState state;
        if (m_system)
        {
            [[maybe_unused]] const bool stateFound = m_system->GetCharacterState(m_worldHandle, m_characterHandle, state);
        }
        return state;
    }

    bool CharacterControllerComponent::Move(
        const AZ::Vector3& velocity,
        const float fixedTimeStep)
    {
        if (!m_system || !m_system->MoveCharacter(m_worldHandle, m_characterHandle, velocity, fixedTimeStep))
        {
            return false;
        }
        if (!m_configuration.m_applyMoveOnFixedTick)
        {
            OnCharacterMoved(GetState());
        }
        return true;
    }

    void CharacterControllerComponent::Activate()
    {
        m_system = AZ::Interface<ISystem>::Get();
        AZ_Error("Box3D", m_system, "Box3D SystemComponent must be active before character components.");
        CharacterRequestBus::Handler::BusConnect(GetEntityId());
        CharacterNotificationBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        EnableSimulation();
    }

    void CharacterControllerComponent::Deactivate()
    {
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        CharacterNotificationBus::Handler::BusDisconnect();
        CharacterRequestBus::Handler::BusDisconnect();
        DisableSimulation();
        m_system = nullptr;
        m_worldHandle = {};
    }

    void CharacterControllerComponent::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local,
        const AZ::Transform& world)
    {
        if (m_syncingTransform || !m_system || !m_characterHandle.IsValid())
        {
            return;
        }
        CharacterConfiguration configuration = m_configuration;
        configuration.m_basePosition = world.GetTranslation();
        configuration.m_rotation = world.GetRotation();
        UpdateConfiguration(configuration);
    }

    void CharacterControllerComponent::OnCharacterMoved(
        const CharacterState& state)
    {
        AZ::Transform currentTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(currentTransform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        if (currentTransform.GetTranslation().IsClose(state.m_basePosition))
        {
            return;
        }

        m_syncingTransform = true;
        const AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(m_configuration.m_rotation, state.m_basePosition);
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTM, transform);
        m_syncingTransform = false;
    }
} // namespace Box3D
