/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Name/Name.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <Box3D/RigidBodyConfiguration.h>
#include <Box3D/TypeIds.h>

namespace Box3D::Editor
{
    class RigidBodyComponent final
        : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(RigidBodyComponent, RigidBodyComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        RigidBodyConfiguration m_configuration;
        AZ::Name m_worldName;
    };
} // namespace Box3D::Editor
