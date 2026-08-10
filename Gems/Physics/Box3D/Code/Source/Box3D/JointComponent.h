/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/JointBus.h>
#include <Box3D/RigidBodyBus.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/Component.h>

namespace Box3D
{
    class System;

    class JointComponent final
        : public AZ::Component
        , public JointRequestBus::Handler
        , private RigidBodyNotificationBus::MultiHandler
    {
    public:
        AZ_COMPONENT(JointComponent, JointComponentTypeId);

        JointComponent() = default;

        JointComponent(
            JointConfiguration configuration,
            AZ::EntityId parentEntityId = AZ::EntityId());

        ~JointComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        bool EnableSimulation() override;

        bool DisableSimulation() override;

        [[nodiscard]]
        bool IsSimulationEnabled() const override;

        [[nodiscard]]
        WorldHandle GetWorldHandle() const override;

        [[nodiscard]]
        JointHandle GetJointHandle() const override;

        [[nodiscard]]
        JointConfiguration GetConfiguration() const override;

        bool UpdateConfiguration(const JointConfiguration& configuration) override;

        [[nodiscard]]
        JointMeasurements GetMeasurements() const override;

        bool WakeBodies() override;

        [[nodiscard]]
        DistanceJointConfiguration GetDistanceConfiguration() const override;

        bool UpdateDistanceConfiguration(const DistanceJointConfiguration& configuration) override;

        [[nodiscard]]
        FilterJointConfiguration GetFilterConfiguration() const override;

        bool UpdateFilterConfiguration(const FilterJointConfiguration& configuration) override;

        [[nodiscard]]
        MotorJointConfiguration GetMotorConfiguration() const override;

        bool UpdateMotorConfiguration(const MotorJointConfiguration& configuration) override;

        [[nodiscard]]
        ParallelJointConfiguration GetParallelConfiguration() const override;

        bool UpdateParallelConfiguration(const ParallelJointConfiguration& configuration) override;

        [[nodiscard]]
        PrismaticJointConfiguration GetPrismaticConfiguration() const override;

        bool UpdatePrismaticConfiguration(const PrismaticJointConfiguration& configuration) override;

        [[nodiscard]]
        RevoluteJointConfiguration GetRevoluteConfiguration() const override;

        bool UpdateRevoluteConfiguration(const RevoluteJointConfiguration& configuration) override;

        [[nodiscard]]
        SphericalJointConfiguration GetSphericalConfiguration() const override;

        bool UpdateSphericalConfiguration(const SphericalJointConfiguration& configuration) override;

        [[nodiscard]]
        WeldJointConfiguration GetWeldConfiguration() const override;

        bool UpdateWeldConfiguration(const WeldJointConfiguration& configuration) override;

        [[nodiscard]]
        WheelJointConfiguration GetWheelConfiguration() const override;

        bool UpdateWheelConfiguration(const WheelJointConfiguration& configuration) override;

        [[nodiscard]]
        DistanceJointState GetDistanceState() const override;

        [[nodiscard]]
        PrismaticJointState GetPrismaticState() const override;

        [[nodiscard]]
        RevoluteJointState GetRevoluteState() const override;

        [[nodiscard]]
        SphericalJointState GetSphericalState() const override;

        [[nodiscard]]
        WheelJointState GetWheelState() const override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnBodyCreated(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        void OnBodyDestroyed(
            WorldHandle worldHandle,
            BodyHandle bodyHandle) override;

        [[nodiscard]]
        bool ResolveConfiguration(
            const JointConfiguration& source,
            JointConfiguration& resolved,
            WorldHandle& worldHandle) const;

        JointConfiguration m_configuration;
        AZ::EntityId m_parentEntityId{};

        System* m_system = nullptr;

        WorldHandle m_worldHandle;
        JointHandle m_jointHandle;
    };
} // namespace Box3D
