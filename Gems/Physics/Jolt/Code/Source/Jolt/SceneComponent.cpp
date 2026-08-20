/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SceneComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        class SceneNotificationBusBehaviorHandler final
            : public SceneNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(
                SceneNotificationBusBehaviorHandler,
                "{E2F9AAC9-A748-46FC-88E3-91EBAF737499}",
                AZ::SystemAllocator,
                OnSceneReady,
                OnSceneReloading,
                OnSceneReleased);

            void OnSceneReady(
                const SceneInstanceHandle instanceHandle) override
            {
                Call(FN_OnSceneReady, instanceHandle);
            }

            void OnSceneReloading(
                const SceneInstanceHandle instanceHandle) override
            {
                Call(FN_OnSceneReloading, instanceHandle);
            }

            void OnSceneReleased() override
            {
                Call(FN_OnSceneReleased);
            }
        };
    } // namespace

    struct SceneComponent::RuntimeResources final
    {
        AZStd::vector<BodyHandle> m_bodyHandles;
        AZStd::vector<ConstraintHandle> m_constraintHandles;
        SceneDefinitionHandle m_definitionHandle;
        SceneInstanceHandle m_instanceHandle;
        WorldHandle m_worldHandle;
    };

    SceneComponent::SceneComponent() = default;

    SceneComponent::SceneComponent(
        SceneComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    SceneComponent::~SceneComponent() = default;

    void SceneComponent::Reflect(
        AZ::ReflectContext* context)
    {
        SceneComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SceneComponent>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SceneComponent, AZ::Component>()
                ->Field("Configuration", &SceneComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SceneComponent>("Jolt Scene", "Instantiates a cooked physics scene.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SceneComponent::m_configuration,
                        "Scene",
                        "Cooked scene asset.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<SceneRequestBus>("JoltSceneRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("CopyBodies", &ISceneRequests::CopyBodies)
                ->Event("CopyConstraints", &ISceneRequests::CopyConstraints)
                ->Event("GetDefinitionHandle", &ISceneRequests::GetDefinitionHandle)
                ->Event("GetInstanceHandle", &ISceneRequests::GetInstanceHandle)
                ->Event("IsReady", &ISceneRequests::IsReady);

            behaviorContext->EBus<SceneNotificationBus>("JoltSceneNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<SceneNotificationBusBehaviorHandler>();

            behaviorContext->Class<SceneComponent>("Jolt::SceneComponent")
                ->RequestBus("JoltSceneRequestBus");
        }
    }

    void SceneComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltSceneService"));
    }

    void SceneComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltSceneService"));
    }

    AZStd::vector<BodyHandle> SceneComponent::CopyBodies() const
    {
        if (!m_resources)
        {
            return {};
        }
        return m_resources->m_bodyHandles;
    }

    AZStd::vector<ConstraintHandle> SceneComponent::CopyConstraints() const
    {
        if (!m_resources)
        {
            return {};
        }
        return m_resources->m_constraintHandles;
    }

    SceneDefinitionHandle SceneComponent::GetDefinitionHandle() const
    {
        if (!m_resources)
        {
            return {};
        }
        return m_resources->m_definitionHandle;
    }

    SceneInstanceHandle SceneComponent::GetInstanceHandle() const
    {
        if (!m_resources)
        {
            return {};
        }
        return m_resources->m_instanceHandle;
    }

    bool SceneComponent::IsReady() const
    {
        return m_resources
            && m_resources->m_definitionHandle
            && m_resources->m_instanceHandle;
    }

    void SceneComponent::Activate()
    {
        m_system = GetRuntime();
        SceneRequestBus::Handler::BusConnect(GetEntityId());

        const AZ::Data::AssetId assetId = m_configuration.m_asset.GetId();
        if (!m_system || !assetId.IsValid())
        {
            return;
        }

        AZ::Data::AssetBus::Handler::BusConnect(assetId);
        if (m_configuration.m_asset.IsReady())
        {
            if (m_loadedAsset != m_configuration.m_asset.Get())
            {
                LoadAsset(*m_configuration.m_asset);
            }
            return;
        }

        m_configuration.m_asset.QueueLoad();
    }

    void SceneComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();
        if (!ReleaseResources(true))
        {
            AZ_Error("Jolt", false, "Failed to release scene resources for entity '%s'.", GetEntity()->GetName().c_str());
        }
        SceneRequestBus::Handler::BusDisconnect();
        m_system = nullptr;
    }

    void SceneComponent::OnAssetReady(
        AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() != m_configuration.m_asset.GetId())
        {
            return;
        }

        AZ::Data::Asset<SceneAsset> sceneAsset(asset);
        if (!sceneAsset || !LoadAsset(*sceneAsset))
        {
            AZ_Error("Jolt", false, "Failed to import scene asset for entity '%s'.", GetEntity()->GetName().c_str());
            return;
        }

        m_configuration.m_asset = AZStd::move(sceneAsset);
    }

    void SceneComponent::OnAssetReloaded(
        AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        OnAssetReady(AZStd::move(asset));
    }

    void SceneComponent::OnAssetError(
        [[maybe_unused]] AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
    }

    bool SceneComponent::LoadAsset(
        const SceneAsset& asset)
    {
        if (!m_system)
        {
            return false;
        }

        AZStd::unique_ptr<RuntimeResources> resources = AZStd::make_unique<RuntimeResources>();
        resources->m_worldHandle = m_system->GetDefaultWorldHandle();
        resources->m_definitionHandle = m_system->CreateSceneDefinition(asset.m_data);
        if (!resources->m_worldHandle || !resources->m_definitionHandle)
        {
            if (resources->m_definitionHandle)
            {
                m_system->DestroySceneDefinition(resources->m_definitionHandle);
            }
            return false;
        }

        resources->m_instanceHandle = m_system->InstantiateScene(
            resources->m_worldHandle,
            resources->m_definitionHandle);
        SceneInstanceState state;
        if (!resources->m_instanceHandle
            || !m_system->GetSceneInstanceState(
                resources->m_worldHandle,
                resources->m_instanceHandle,
                state))
        {
            if (resources->m_instanceHandle)
            {
                m_system->DestroySceneInstance(resources->m_worldHandle, resources->m_instanceHandle);
            }
            m_system->DestroySceneDefinition(resources->m_definitionHandle);
            return false;
        }

        resources->m_bodyHandles.resize(state.m_bodyCount);
        resources->m_constraintHandles.resize(state.m_constraintCount);
        const QueryResult bodyResult = m_system->GetSceneBodies(
            resources->m_worldHandle,
            resources->m_instanceHandle,
            resources->m_bodyHandles);
        const QueryResult constraintResult = m_system->GetSceneConstraints(
            resources->m_worldHandle,
            resources->m_instanceHandle,
            resources->m_constraintHandles);
        if (!bodyResult.IsComplete()
            || !constraintResult.IsComplete())
        {
            m_system->DestroySceneInstance(resources->m_worldHandle, resources->m_instanceHandle);
            m_system->DestroySceneDefinition(resources->m_definitionHandle);
            return false;
        }

        SceneInstanceHandle previousInstanceHandle;
        if (m_resources)
        {
            previousInstanceHandle = m_resources->m_instanceHandle;
            if (!ReleaseResources(false))
            {
                [[maybe_unused]] const bool replacementDestroyed = m_system->DestroySceneResources(
                    resources->m_worldHandle,
                    resources->m_instanceHandle,
                    resources->m_definitionHandle);
                AZ_Assert(replacementDestroyed, "Unpublished scene resources must remain destroyable.");
                return false;
            }
            SceneNotificationBus::Event(
                GetEntityId(),
                &ISceneNotifications::OnSceneReloading,
                previousInstanceHandle);
        }
        m_resources = AZStd::move(resources);
        m_loadedAsset = &asset;
        SceneNotificationBus::Event(
            GetEntityId(),
            &ISceneNotifications::OnSceneReady,
            m_resources->m_instanceHandle);
        return true;
    }

    bool SceneComponent::ReleaseResources(
        const bool notify)
    {
        if (!m_resources || !m_system)
        {
            return true;
        }

        if (!m_system->DestroySceneResources(
            m_resources->m_worldHandle,
            m_resources->m_instanceHandle,
            m_resources->m_definitionHandle))
        {
            return false;
        }
        m_resources.reset();
        m_loadedAsset = nullptr;
        if (notify)
        {
            SceneNotificationBus::Event(
                GetEntityId(),
                &ISceneNotifications::OnSceneReleased);
        }
        return true;
    }
} // namespace Jolt
