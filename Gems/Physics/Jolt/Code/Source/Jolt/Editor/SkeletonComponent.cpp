/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SkeletonComponent.h>

#include <Jolt/Reflection.h>
#include <Jolt/SkeletonComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Translation/TranslationDef.h>

namespace Jolt::Editor
{
    SkeletonComponent::SkeletonComponent(
        SkeletonComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

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
                ->Class<SkeletonComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &SkeletonComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SkeletonComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Skeleton"),
                        QT_TRANSLATE_NOOP("Jolt", "Loads cooked skeleton and animation resources."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->Attribute(AZ::Edit::Attributes::PrimaryAssetType, SkeletonAssetTypeId)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SkeletonComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Cooked skeleton and animations."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void SkeletonComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::SkeletonComponent::GetProvidedServices(provided);
    }

    void SkeletonComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::SkeletonComponent::GetIncompatibleServices(incompatible);
    }

    void SkeletonComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::SkeletonComponent>(m_configuration);
    }
} // namespace Jolt::Editor
