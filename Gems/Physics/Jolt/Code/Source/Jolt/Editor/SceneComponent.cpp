/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SceneComponent.h>

#include <Jolt/Reflection.h>
#include <Jolt/SceneComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Translation/TranslationDef.h>

namespace Jolt::Editor
{
    SceneComponent::SceneComponent(
        SceneComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

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
                ->Class<SceneComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &SceneComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SceneComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Scene"),
                        QT_TRANSLATE_NOOP("Jolt", "Instantiates a cooked physics scene."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->Attribute(AZ::Edit::Attributes::PrimaryAssetType, SceneAssetTypeId)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SceneComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Cooked scene asset."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void SceneComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::SceneComponent::GetProvidedServices(provided);
    }

    void SceneComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::SceneComponent::GetIncompatibleServices(incompatible);
    }

    void SceneComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::SceneComponent>(m_configuration);
    }
} // namespace Jolt::Editor
