/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Vector3.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace Jolt::Editor
{
    class ConstraintComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(
            ConstraintComponent,
            EditorConstraintComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        ConstraintComponent() = default;
        explicit ConstraintComponent(ConstraintComponentConfiguration configuration);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;

        void Deactivate() override;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        bool SupportsEditorRayIntersect() override;

        AZ::Aabb GetEditorSelectionBoundsViewport(const AzFramework::ViewportInfo& viewportInfo) override;

        bool EditorSelectionIntersectRayViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            const AZ::Vector3& rayStart,
            const AZ::Vector3& rayDirection,
            float& distance) override;

    private:
        ConstraintComponentConfiguration m_configuration;
        float m_drawScale = 1.0f;
    };
} // namespace Jolt::Editor
