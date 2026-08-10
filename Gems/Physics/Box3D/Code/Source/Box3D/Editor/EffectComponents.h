/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/Manipulators/RadiusManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/ShapeManipulatorRequestBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <Box3D/Effects.h>
#include <Box3D/TypeIds.h>

namespace Box3D::Editor
{
    class ExplosionComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
        , private AzToolsFramework::RadiusManipulatorRequestBus::Handler
        , private AzToolsFramework::ShapeManipulatorRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(ExplosionComponent, ExplosionComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

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
        float GetRadius() const override;

        void SetRadius(float radius) override;

        AZ::Vector3 GetTranslationOffset() const override;

        void SetTranslationOffset(const AZ::Vector3& translationOffset) override;

        AZ::Transform GetManipulatorSpace() const override;

        AZ::Quaternion GetRotationOffset() const override;

        AZ::u32 ConfigurationChanged();

        ExplosionConfiguration m_configuration;
        AZ::Name m_worldName;
        bool m_explodeOnActivate = false;

        AzToolsFramework::ComponentModeFramework::ComponentModeDelegate m_componentModeDelegate;
    };

    class WindComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
        , private AzToolsFramework::ShapeManipulatorRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(WindComponent, WindComponentTypeId, AzToolsFramework::Components::EditorComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

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
        AZ::Vector3 GetTranslationOffset() const override;

        void SetTranslationOffset(const AZ::Vector3& translationOffset) override;

        AZ::Transform GetManipulatorSpace() const override;

        AZ::Quaternion GetRotationOffset() const override;

        AZ::u32 ConfigurationChanged();

        WindConfiguration m_configuration;
        bool m_enabled = true;

        AzToolsFramework::ComponentModeFramework::ComponentModeDelegate m_componentModeDelegate;
    };
} // namespace Box3D::Editor
