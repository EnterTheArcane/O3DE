/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/SkeletonComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Jolt::Editor
{
    class SkeletonComponent final
        : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            SkeletonComponent,
            EditorSkeletonComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        SkeletonComponent() = default;
        explicit SkeletonComponent(SkeletonComponentConfiguration configuration);
        ~SkeletonComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        SkeletonComponentConfiguration m_configuration;
    };
} // namespace Jolt::Editor
