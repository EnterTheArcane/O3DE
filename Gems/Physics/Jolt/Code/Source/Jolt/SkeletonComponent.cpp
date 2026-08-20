/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SkeletonComponent.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/Reflection.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace Jolt
{
    namespace
    {
        class SkeletonComponentNotificationBusBehaviorHandler final
            : public SkeletonComponentNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(
                SkeletonComponentNotificationBusBehaviorHandler,
                "{BB3D5019-F728-459B-914F-26CCD676254E}",
                AZ::SystemAllocator,
                OnSkeletonReady,
                OnSkeletonReloading,
                OnSkeletonReleased);

            void OnSkeletonReady(
                const SkeletonDefinitionHandle skeletonHandle) override
            {
                Call(FN_OnSkeletonReady, skeletonHandle);
            }

            void OnSkeletonReloading(
                const SkeletonDefinitionHandle skeletonHandle) override
            {
                Call(FN_OnSkeletonReloading, skeletonHandle);
            }

            void OnSkeletonReleased() override
            {
                Call(FN_OnSkeletonReleased);
            }
        };
    } // namespace

    struct SkeletonComponent::RuntimeResources final
    {
        struct Animation final
        {
            AZ::Name m_name;
            SkeletalAnimationHandle m_handle;
        };

        AZStd::vector<Animation> m_animations;
        SkeletonDefinitionHandle m_skeletonHandle;
    };

    SkeletonComponent::SkeletonComponent() = default;

    SkeletonComponent::SkeletonComponent(
        SkeletonComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    SkeletonComponent::~SkeletonComponent() = default;

    void SkeletonComponent::Reflect(
        AZ::ReflectContext* context)
    {
        SkeletonComponentConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SkeletonComponent>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SkeletonComponent, AZ::Component>()
                ->Field("Configuration", &SkeletonComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SkeletonComponent>("Jolt Skeleton", "Loads cooked skeleton and animation resources.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SkeletonComponent::m_configuration,
                        "Skeleton",
                        "Cooked skeleton and animations.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<SkeletonComponentRequestBus>("JoltSkeletonComponentRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Event("IsReady", &ISkeletonComponentRequests::IsReady)
                ->Event("GetSkeletonHandle", &ISkeletonComponentRequests::GetSkeletonHandle)
                ->Event("FindAnimation", &ISkeletonComponentRequests::FindAnimation)
                ->Event("CopyAnimationNames", &ISkeletonComponentRequests::CopyAnimationNames);

            behaviorContext->EBus<SkeletonComponentNotificationBus>("JoltSkeletonComponentNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Attribute(AZ::Script::Attributes::Category, "Jolt")
                ->Handler<SkeletonComponentNotificationBusBehaviorHandler>();

            behaviorContext->Class<SkeletonComponent>("Jolt::SkeletonComponent")
                ->RequestBus("JoltSkeletonComponentRequestBus");
        }
    }

    void SkeletonComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("JoltSkeletonService"));
    }

    void SkeletonComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("JoltSkeletonService"));
    }

    bool SkeletonComponent::IsReady() const
    {
        return m_resources && m_resources->m_skeletonHandle;
    }

    SkeletonDefinitionHandle SkeletonComponent::GetSkeletonHandle() const
    {
        if (!m_resources)
        {
            return {};
        }

        return m_resources->m_skeletonHandle;
    }

    SkeletalAnimationHandle SkeletonComponent::FindAnimation(
        const AZ::Name name) const
    {
        if (!m_resources)
        {
            return {};
        }

        const auto animation = AZStd::lower_bound(
            m_resources->m_animations.begin(),
            m_resources->m_animations.end(),
            name,
            [](const RuntimeResources::Animation& candidate, const AZ::Name requestedName)
            {
                return candidate.m_name.GetHash() < requestedName.GetHash();
            });
        if (animation != m_resources->m_animations.end()
            && animation->m_name == name)
        {
            return animation->m_handle;
        }

        return {};
    }

    BufferResult SkeletonComponent::GetAnimationNames(
        const AZStd::span<AZ::Name> names) const
    {
        BufferResult result;
        if (!m_resources)
        {
            return result;
        }

        result.m_requiredCount = aznumeric_cast<AZ::u32>(m_resources->m_animations.size());
        result.m_count = aznumeric_cast<AZ::u32>(AZStd::min(names.size(), m_resources->m_animations.size()));
        for (AZ::u32 animationIndex = 0; animationIndex < result.m_count; ++animationIndex)
        {
            names[animationIndex] = m_resources->m_animations[animationIndex].m_name;
        }
        return result;
    }

    AZStd::vector<AZ::Name> SkeletonComponent::CopyAnimationNames() const
    {
        AZStd::vector<AZ::Name> result;
        if (!m_resources)
        {
            return result;
        }

        result.reserve(m_resources->m_animations.size());
        for (const RuntimeResources::Animation& animation : m_resources->m_animations)
        {
            result.push_back(animation.m_name);
        }
        return result;
    }

    void SkeletonComponent::Activate()
    {
        m_system = GetRuntime();
        SkeletonComponentRequestBus::Handler::BusConnect(GetEntityId());

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

    void SkeletonComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect();
        ReleaseResources(true);
        SkeletonComponentRequestBus::Handler::BusDisconnect();
        m_system = nullptr;
    }

    void SkeletonComponent::OnAssetReady(
        AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() != m_configuration.m_asset.GetId())
        {
            return;
        }

        AZ::Data::Asset<SkeletonAsset> skeletonAsset(asset);
        if (!skeletonAsset || !LoadAsset(*skeletonAsset))
        {
            AZ_Error("Jolt", false, "Failed to import skeleton asset for entity '%s'.", GetEntity()->GetName().c_str());
            return;
        }

        m_configuration.m_asset = AZStd::move(skeletonAsset);
    }

    void SkeletonComponent::OnAssetReloaded(
        AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        OnAssetReady(AZStd::move(asset));
    }

    void SkeletonComponent::OnAssetError(
        AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() == m_configuration.m_asset.GetId())
        {
            ReleaseResources(true);
        }
    }

    bool SkeletonComponent::LoadAsset(
        const SkeletonAsset& asset)
    {
        if (!m_system)
        {
            return false;
        }

        AZStd::unique_ptr<RuntimeResources> resources = AZStd::make_unique<RuntimeResources>();
        resources->m_skeletonHandle = m_system->ImportSkeletonDefinition(asset.m_data.m_skeleton);
        if (!resources->m_skeletonHandle)
        {
            return false;
        }

        resources->m_animations.reserve(asset.m_data.m_animations.size());
        for (const NamedSkeletalAnimationAsset& sourceAnimation : asset.m_data.m_animations)
        {
            if (sourceAnimation.m_name.IsEmpty())
            {
                for (const RuntimeResources::Animation& animation : resources->m_animations)
                {
                    m_system->DestroySkeletalAnimation(animation.m_handle);
                }
                m_system->DestroySkeletonDefinition(resources->m_skeletonHandle);
                return false;
            }

            const SkeletalAnimationHandle animationHandle =
                m_system->ImportSkeletalAnimation(sourceAnimation.m_archive);
            if (!animationHandle)
            {
                for (const RuntimeResources::Animation& animation : resources->m_animations)
                {
                    m_system->DestroySkeletalAnimation(animation.m_handle);
                }
                m_system->DestroySkeletonDefinition(resources->m_skeletonHandle);
                return false;
            }

            resources->m_animations.push_back({
                .m_name = sourceAnimation.m_name,
                .m_handle = animationHandle,
            });
        }

        AZStd::sort(
            resources->m_animations.begin(),
            resources->m_animations.end(),
            [](const RuntimeResources::Animation& first, const RuntimeResources::Animation& second)
            {
                return first.m_name.GetHash() < second.m_name.GetHash();
            });
        const auto duplicateAnimation = AZStd::adjacent_find(
            resources->m_animations.begin(),
            resources->m_animations.end(),
            [](const RuntimeResources::Animation& first, const RuntimeResources::Animation& second)
            {
                return first.m_name == second.m_name;
            });
        if (duplicateAnimation != resources->m_animations.end())
        {
            for (const RuntimeResources::Animation& animation : resources->m_animations)
            {
                m_system->DestroySkeletalAnimation(animation.m_handle);
            }
            m_system->DestroySkeletonDefinition(resources->m_skeletonHandle);
            return false;
        }

        if (m_resources)
        {
            SkeletonComponentNotificationBus::Event(
                GetEntityId(),
                &ISkeletonComponentNotifications::OnSkeletonReloading,
                m_resources->m_skeletonHandle);
        }
        ReleaseResources(false);
        m_resources = AZStd::move(resources);
        m_loadedAsset = &asset;
        SkeletonComponentNotificationBus::Event(
            GetEntityId(),
            &ISkeletonComponentNotifications::OnSkeletonReady,
            m_resources->m_skeletonHandle);
        return true;
    }

    void SkeletonComponent::ReleaseResources(
        const bool notify)
    {
        if (!m_resources || !m_system)
        {
            return;
        }

        for (const RuntimeResources::Animation& animation : m_resources->m_animations)
        {
            m_system->DestroySkeletalAnimation(animation.m_handle);
        }
        m_system->DestroySkeletonDefinition(m_resources->m_skeletonHandle);
        m_resources.reset();
        m_loadedAsset = nullptr;
        if (notify)
        {
            SkeletonComponentNotificationBus::Event(
                GetEntityId(),
                &ISkeletonComponentNotifications::OnSkeletonReleased);
        }
    }
} // namespace Jolt
