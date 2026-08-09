/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <Box3D/Joints.h>
#include <Box3D/TypeIds.h>

namespace Box3D::Editor
{
    enum class JointFrame : AZ::u8
    {
        Parent,
        Child
    };

    class IJointManipulatorRequests
        : public AZ::EntityComponentBus
    {
    public:
        [[nodiscard]] virtual AZ::Transform GetLocalFrame(JointFrame frame) const = 0;
        virtual void SetLocalFrame(JointFrame frame, const AZ::Transform& localFrame) = 0;
        [[nodiscard]] virtual AZ::Transform GetFrameSpace(JointFrame frame) const = 0;

    protected:
        virtual ~IJointManipulatorRequests() = default;
    };

    using JointManipulatorRequestBus = AZ::EBus<IJointManipulatorRequests>;

    class JointComponentBase
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
        , private JointManipulatorRequestBus::Handler
    {
    public:
        AZ_RTTI(JointComponentBase, "{0577A917-FEA9-45EF-A047-0356A8D4F98B}", AzToolsFramework::Components::EditorComponentBase);
        AZ_CLASS_ALLOCATOR(JointComponentBase, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void Activate() override;
        void Deactivate() override;
        void DisplayEntityViewport(const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay) override;
        bool SupportsEditorRayIntersect() override;
        AZ::Aabb GetEditorSelectionBoundsViewport(const AzFramework::ViewportInfo& viewportInfo) override;
        bool EditorSelectionIntersectRayViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            const AZ::Vector3& rayStart,
            const AZ::Vector3& rayDirection,
            float& distance) override;

    protected:
        void AddJointToGameEntity(AZ::Entity* gameEntity, const JointConfiguration& configuration) const;
        [[nodiscard]] virtual JointCommonConfiguration& GetCommonConfiguration() = 0;
        [[nodiscard]] virtual const JointCommonConfiguration& GetCommonConfiguration() const = 0;

        AZ::EntityId m_parentEntity;

    private:
        AZ::Transform GetLocalFrame(JointFrame frame) const override;
        void SetLocalFrame(JointFrame frame, const AZ::Transform& localFrame) override;
        AZ::Transform GetFrameSpace(JointFrame frame) const override;

        AzToolsFramework::ComponentModeFramework::ComponentModeDelegate m_componentModeDelegate;
    };

    class ParallelJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(ParallelJointComponent, ParallelJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        ParallelJointConfiguration m_configuration;
    };

    class DistanceJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(DistanceJointComponent, DistanceJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        DistanceJointConfiguration m_configuration;
    };

    class FilterJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(FilterJointComponent, FilterJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        FilterJointConfiguration m_configuration;
    };

    class MotorJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(MotorJointComponent, MotorJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        MotorJointConfiguration m_configuration;
    };

    class PrismaticJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(PrismaticJointComponent, PrismaticJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        PrismaticJointConfiguration m_configuration;
    };

    class RevoluteJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(RevoluteJointComponent, RevoluteJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        RevoluteJointConfiguration m_configuration;
    };

    class SphericalJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(SphericalJointComponent, SphericalJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        SphericalJointConfiguration m_configuration;
    };

    class WeldJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(WeldJointComponent, WeldJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        WeldJointConfiguration m_configuration;
    };

    class WheelJointComponent final
        : public JointComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(WheelJointComponent, WheelJointComponentTypeId, JointComponentBase);

        static void Reflect(AZ::ReflectContext* context);
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        [[nodiscard]] JointCommonConfiguration& GetCommonConfiguration() override;
        [[nodiscard]] const JointCommonConfiguration& GetCommonConfiguration() const override;
        WheelJointConfiguration m_configuration;
    };
} // namespace Box3D::Editor
