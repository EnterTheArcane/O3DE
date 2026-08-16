/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/RigidBodyConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Jolt::Editor
{
    class StaticRigidBodyComponent final
        : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(
            StaticRigidBodyComponent,
            EditorStaticRigidBodyComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        StaticRigidBodyComponent() = default;
        explicit StaticRigidBodyComponent(StaticRigidBodyConfiguration configuration);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        StaticRigidBodyConfiguration m_configuration;
    };
} // namespace Jolt::Editor
