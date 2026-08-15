/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/ColliderBus.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/Manipulators/BoxManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CapsuleManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CylinderManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/RadiusManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/ShapeManipulatorRequestBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <cstddef>

namespace Jolt::Editor
{
    class ColliderComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
        , private AzToolsFramework::BoxManipulatorRequestBus::Handler
        , private AzToolsFramework::CapsuleManipulatorRequestBus::Handler
        , private AzToolsFramework::CylinderManipulatorRequestBus::Handler
        , private AzToolsFramework::RadiusManipulatorRequestBus::Handler
        , private AzToolsFramework::ShapeManipulatorRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(
            ColliderComponent,
            EditorColliderComponentTypeId,
            AzToolsFramework::Components::EditorComponentBase);

        ColliderComponent() = default;
        explicit ColliderComponent(AZStd::vector<ColliderShapeConfiguration> configurations);

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
        [[nodiscard]]
        float GetRadius() const override;

        void SetRadius(float radius) override;

        [[nodiscard]]
        float GetHeight() const override;

        void SetHeight(float height) override;

        [[nodiscard]]
        AZ::Vector3 GetDimensions() const override;

        void SetDimensions(const AZ::Vector3& dimensions) override;

        [[nodiscard]]
        AZ::Transform GetCurrentLocalTransform() const override;

        [[nodiscard]]
        AZ::Vector3 GetTranslationOffset() const override;

        void SetTranslationOffset(const AZ::Vector3& translationOffset) override;

        [[nodiscard]]
        AZ::Transform GetManipulatorSpace() const override;

        [[nodiscard]]
        AZ::Quaternion GetRotationOffset() const override;

        AZ::u32 ConfigurationChanged();

        AZStd::vector<ColliderShapeConfiguration> m_configurations{ColliderShapeConfiguration{}};

        AZ::u32 m_activeShapeIndex = 0;
        size_t m_componentModeGeometryIndex = AZStd::variant_npos;
        AzToolsFramework::ComponentModeFramework::ComponentModeDelegate m_componentModeDelegate;
    };
} // namespace Jolt::Editor
