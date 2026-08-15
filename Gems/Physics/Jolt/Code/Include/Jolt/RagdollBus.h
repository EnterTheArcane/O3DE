/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Query.h>
#include <Jolt/Ragdoll.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class IRagdollRequests
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
        virtual RagdollHandle GetRagdollHandle() const = 0;

        [[nodiscard]]
        virtual RagdollState GetState() const = 0;

        virtual QueryResult QueryBodies(AZStd::span<BodyHandle> bodyHandles) const = 0;

        virtual QueryResult QueryConstraints(AZStd::span<ConstraintHandle> constraintHandles) const = 0;

        virtual QueryResult QueryPose(
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<BodyHandle> CopyBodies() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<ConstraintHandle> CopyConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<AZ::Transform> CopyPose() const = 0;

        virtual bool SetPose(
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms) = 0;

        virtual bool SetPoseFromTransforms(
            WorldPosition rootPosition,
            const AZStd::vector<AZ::Transform>& modelTransforms) = 0;

        virtual bool DriveKinematically(
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) = 0;

        virtual bool DriveKinematicallyFromTransforms(
            WorldPosition rootPosition,
            const AZStd::vector<AZ::Transform>& modelTransforms,
            float deltaTime) = 0;

        virtual bool DriveMotors(AZStd::span<const AZ::Transform> modelTransforms) = 0;

        virtual bool DriveMotorsFromTransforms(const AZStd::vector<AZ::Transform>& modelTransforms) = 0;

        virtual bool DriveMotorsWithVelocity(
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) = 0;

        virtual bool DriveMotorsWithVelocityFromTransforms(
            const AZStd::vector<AZ::Transform>& previousModelTransforms,
            const AZStd::vector<AZ::Transform>& modelTransforms,
            float deltaTime) = 0;

        virtual bool ResetWarmStart() = 0;

        virtual bool ActivateRagdoll() = 0;

        virtual bool SetVelocity(
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) = 0;

        virtual bool SetLinearVelocity(const AZ::Vector3& linearVelocity) = 0;

        virtual bool SetCollisionGroupId(AZ::u32 collisionGroupId) = 0;

        virtual bool AddLinearVelocity(const AZ::Vector3& linearVelocity) = 0;

        virtual bool AddImpulse(const AZ::Vector3& impulse) = 0;
    };

    using RagdollRequestBus = AZ::EBus<IRagdollRequests>;

    class IRagdollNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual void OnRagdollCreated(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] RagdollHandle ragdollHandle)
        {
        }

        virtual void OnRagdollDestroying(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] RagdollHandle ragdollHandle)
        {
        }

        virtual void OnRagdollDestroyed(
            [[maybe_unused]] WorldHandle worldHandle,
            [[maybe_unused]] RagdollHandle ragdollHandle)
        {
        }
    };

    using RagdollNotificationBus = AZ::EBus<IRagdollNotifications>;
} // namespace Jolt
