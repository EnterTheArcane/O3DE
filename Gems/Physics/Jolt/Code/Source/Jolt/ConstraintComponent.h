/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/ComponentDependencyManager.h>
#include <Jolt/ConstraintBus.h>
#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class RuntimeImplementation;

    class IConstraintRuntimeGeometry
    {
    public:
        AZ_RTTI(IConstraintRuntimeGeometry, ConstraintRuntimeGeometryTypeId);

        virtual ~IConstraintRuntimeGeometry() = default;

        [[nodiscard]]
        virtual bool Resolve(
            ConstraintConfiguration& configuration,
            ConstraintHandle& firstDependencyHandle,
            ConstraintHandle& secondDependencyHandle,
            PathHandle& pathHandle) const = 0;

        virtual void RegisterDependencies(
            IComponentDependencyManager& dependencyManager,
            IConstraintDependencyClient& client) const = 0;

        virtual void UnregisterDependencies(
            IComponentDependencyManager& dependencyManager,
            IConstraintDependencyClient& client) const = 0;
    };

    class JOLT_API ConstraintComponent final
        : public AZ::Component
        , public ConstraintRequestBus::Handler
        , private IConstraintDependencyClient
    {
    public:
        AZ_COMPONENT(ConstraintComponent, ConstraintComponentTypeId);

        ConstraintComponent() = default;
        explicit ConstraintComponent(ConstraintComponentConfiguration configuration);
        ~ConstraintComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        ConstraintHandle GetConstraintHandle() const override;

        [[nodiscard]]
        ConstraintState GetState() const override;

        [[nodiscard]]
        AZ::u64 GetUserData() const override;

        bool SetUserData(AZ::u64 userData) override;

        [[nodiscard]]
        float GetDebugDrawSize() const override;

        bool SetDebugDrawSize(float debugDrawSize) override;

        bool SetEnabled(bool enabled) override;

        bool ResetWarmStart() override;

        bool SetHingeTargetOrientation(const AZ::Quaternion& targetOrientation) override;

    private:
        struct RuntimeConfiguration final
        {
            AZ_TYPE_INFO(RuntimeConfiguration, ConstraintRuntimeConfigurationTypeId);

            RuntimeConfiguration() = default;
            ~RuntimeConfiguration() = default;

            AZ_DISABLE_COPY_MOVE(RuntimeConfiguration);

            AZ::EntityId m_firstBodyEntityId;
            AZ::EntityId m_secondBodyEntityId;
            AZStd::unique_ptr<IConstraintRuntimeGeometry> m_geometry;

            AZ::u64 m_userData = 0;
            AZ::u32 m_priority = 0;
            AZ::u8 m_positionStepCount = 0;
            AZ::u8 m_velocityStepCount = 0;
            bool m_enabled = true;
        };

        void Activate() override;

        void Deactivate() override;

        void OnBodyDependencyCreated(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        bool OnBodyDependencyDestroying(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        void OnConstraintDependencyCreated(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        bool OnConstraintDependencyDestroying(
            WorldHandle worldHandle,
            ConstraintHandle constraintHandle) override;

        void OnPathDependencyCreated(PathHandle pathHandle) override;

        bool OnPathDependencyDestroying(PathHandle pathHandle) override;

        RuntimeConfiguration m_configuration;

        RuntimeImplementation* m_system = nullptr;
        IComponentDependencyManager* m_dependencyManager = nullptr;
        WorldHandle m_worldHandle;
        ConstraintHandle m_constraintHandle;
        BodyHandle m_firstBodyHandle;
        BodyHandle m_secondBodyHandle;
        ConstraintHandle m_firstDependencyHandle;
        ConstraintHandle m_secondDependencyHandle;
        PathHandle m_pathHandle;
    };
} // namespace Jolt
