/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/System.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Component/ComponentBus.h>

namespace Box3D
{
    struct ClosestPoint final
    {
        AZ_TYPE_INFO(ClosestPoint, ClosestPointTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        float m_distance = 0.0f;
        bool m_found = false;
    };

    class RigidBodyRequests
        : public AZ::ComponentBus
    {
    public:
        virtual bool EnableSimulation() = 0;
        virtual bool DisableSimulation() = 0;
        [[nodiscard]] virtual bool IsSimulationEnabled() const = 0;
        [[nodiscard]] virtual WorldHandle GetWorldHandle() const = 0;
        [[nodiscard]] virtual BodyHandle GetBodyHandle() const = 0;
        [[nodiscard]] virtual BodyState GetState() const = 0;
        [[nodiscard]] virtual AZ::Name GetName() const = 0;
        virtual bool SetName(AZ::Name name) = 0;
        [[nodiscard]] virtual BodyProperties GetProperties() const = 0;
        virtual bool SetProperties(const BodyProperties& properties) = 0;
        [[nodiscard]] virtual AZ::Aabb GetAabb() const = 0;
        [[nodiscard]] virtual ClosestPoint GetClosestPoint(const AZ::Vector3& target) const = 0;
        [[nodiscard]] virtual MassProperties GetMassProperties() const = 0;
        virtual bool SetMassProperties(const MassProperties& properties) = 0;
        virtual bool RecomputeMassFromShapes() = 0;
        virtual bool SetTransform(const AZ::Transform& transform) = 0;
        virtual bool SetLinearVelocity(const AZ::Vector3& velocity) = 0;
        virtual bool SetAngularVelocity(const AZ::Vector3& velocity) = 0;
        [[nodiscard]] virtual AZ::Vector3 GetLinearVelocityAtWorldPoint(const AZ::Vector3& worldPoint) const = 0;
        virtual bool SetKinematicTarget(const AZ::Transform& transform, float fixedTimeStep) = 0;
        virtual bool ApplyLinearImpulse(const AZ::Vector3& impulse) = 0;
        virtual bool ApplyLinearImpulseAtWorldPoint(const AZ::Vector3& impulse, const AZ::Vector3& worldPoint) = 0;
        virtual bool ApplyAngularImpulse(const AZ::Vector3& impulse) = 0;
        virtual bool ApplyForce(const AZ::Vector3& force, bool wake) = 0;
        virtual bool ApplyForceAtWorldPoint(const AZ::Vector3& force, const AZ::Vector3& worldPoint, bool wake) = 0;
        virtual bool ApplyTorque(const AZ::Vector3& torque, bool wake) = 0;
        virtual bool SetAwake(bool awake) = 0;
        virtual bool SetHitEventsEnabled(bool enabled) = 0;
    };

    using RigidBodyRequestBus = AZ::EBus<RigidBodyRequests>;

    class RigidBodyNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnBodyCreated([[maybe_unused]] WorldHandle worldHandle, [[maybe_unused]] BodyHandle bodyHandle)
        {
        }
        virtual void OnBodyDestroyed([[maybe_unused]] WorldHandle worldHandle, [[maybe_unused]] BodyHandle bodyHandle)
        {
        }
    };

    using RigidBodyNotificationBus = AZ::EBus<RigidBodyNotifications>;
} // namespace Box3D
