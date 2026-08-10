/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Events.h>
#include <Box3D/Joints.h>

#include <AzCore/Component/ComponentBus.h>

namespace Box3D
{
    class JointRequests
        : public AZ::ComponentBus
    {
    public:
        virtual bool EnableSimulation() = 0;

        virtual bool DisableSimulation() = 0;

        [[nodiscard]]
        virtual bool IsSimulationEnabled() const = 0;

        [[nodiscard]]
        virtual WorldHandle GetWorldHandle() const = 0;

        [[nodiscard]]
        virtual JointHandle GetJointHandle() const = 0;

        [[nodiscard]]
        virtual JointConfiguration GetConfiguration() const = 0;

        virtual bool UpdateConfiguration(const JointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual JointMeasurements GetMeasurements() const = 0;

        virtual bool WakeBodies() = 0;

        [[nodiscard]]
        virtual DistanceJointConfiguration GetDistanceConfiguration() const = 0;

        virtual bool UpdateDistanceConfiguration(const DistanceJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual FilterJointConfiguration GetFilterConfiguration() const = 0;

        virtual bool UpdateFilterConfiguration(const FilterJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual MotorJointConfiguration GetMotorConfiguration() const = 0;

        virtual bool UpdateMotorConfiguration(const MotorJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual ParallelJointConfiguration GetParallelConfiguration() const = 0;

        virtual bool UpdateParallelConfiguration(const ParallelJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual PrismaticJointConfiguration GetPrismaticConfiguration() const = 0;

        virtual bool UpdatePrismaticConfiguration(const PrismaticJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual RevoluteJointConfiguration GetRevoluteConfiguration() const = 0;

        virtual bool UpdateRevoluteConfiguration(const RevoluteJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual SphericalJointConfiguration GetSphericalConfiguration() const = 0;

        virtual bool UpdateSphericalConfiguration(const SphericalJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual WeldJointConfiguration GetWeldConfiguration() const = 0;

        virtual bool UpdateWeldConfiguration(const WeldJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual WheelJointConfiguration GetWheelConfiguration() const = 0;

        virtual bool UpdateWheelConfiguration(const WheelJointConfiguration& configuration) = 0;

        [[nodiscard]]
        virtual DistanceJointState GetDistanceState() const = 0;

        [[nodiscard]]
        virtual PrismaticJointState GetPrismaticState() const = 0;

        [[nodiscard]]
        virtual RevoluteJointState GetRevoluteState() const = 0;

        [[nodiscard]]
        virtual SphericalJointState GetSphericalState() const = 0;

        [[nodiscard]]
        virtual WheelJointState GetWheelState() const = 0;
    };

    using JointRequestBus = AZ::EBus<JointRequests>;

    class JointNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnThresholdExceeded(
            [[maybe_unused]] const JointThresholdEvent& event)
        {
        }
    };

    using JointNotificationBus = AZ::EBus<JointNotifications>;
} // namespace Box3D
