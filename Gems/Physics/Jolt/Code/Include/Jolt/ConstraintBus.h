/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Constraint.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/base.h>

namespace Jolt
{
    class IConstraintRequests
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
        virtual ConstraintHandle GetConstraintHandle() const = 0;

        [[nodiscard]]
        virtual ConstraintState GetState() const = 0;

        [[nodiscard]]
        virtual AZ::u64 GetUserData() const = 0;

        virtual bool SetUserData(AZ::u64 userData) = 0;

        [[nodiscard]]
        virtual float GetDebugDrawSize() const = 0;

        virtual bool SetDebugDrawSize(float debugDrawSize) = 0;

        virtual bool SetEnabled(bool enabled) = 0;

        virtual bool ResetWarmStart() = 0;

        //! Sets the target orientation of a hinge motor relative to its first body.
        virtual bool SetHingeTargetOrientation(const AZ::Quaternion& targetOrientation) = 0;
    };

    using ConstraintRequestBus = AZ::EBus<IConstraintRequests>;

    class IConstraintNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnConstraintCreated(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] ConstraintHandle constraintHandle)
        {
        }

        virtual void OnConstraintDestroying(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] ConstraintHandle constraintHandle)
        {
        }

        virtual void OnConstraintDestroyed(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] ConstraintHandle constraintHandle)
        {
        }
    };

    using ConstraintNotificationBus = AZ::EBus<IConstraintNotifications>;
} // namespace Jolt
