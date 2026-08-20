/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Query.h>
#include <Jolt/Ragdoll.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Ragdolls
    {
    public:
        [[nodiscard]]
        static Ragdolls* Get();

        [[nodiscard]]
        RagdollDefinitionHandle CreateRagdollDefinition(
            WorldHandle worldHandle,
            const RagdollDefinitionConfiguration& configuration);

        bool DestroyRagdollDefinition(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        QueryResult GetRagdollBodyConstraintIndices(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<AZ::s32> constraintIndices) const;

        [[nodiscard]]
        QueryResult GetRagdollConstraintBodyPairs(
            WorldHandle worldHandle,
            RagdollDefinitionHandle definitionHandle,
            AZStd::span<RagdollConstraintBodyPair> bodyPairs) const;

        [[nodiscard]]
        RagdollHandle CreateRagdoll(
            WorldHandle worldHandle,
            const RagdollConfiguration& configuration);

        bool AddRagdollToSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            bool activate);

        bool RemoveRagdollFromSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        bool DestroyRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        [[nodiscard]]
        bool IsValid(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        bool IsRagdollInSimulation(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle) const;

        [[nodiscard]]
        bool GetRagdollState(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            RagdollState& state) const;

        bool SetRagdollCollisionGroupId(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::u32 collisionGroupId);

        [[nodiscard]]
        QueryResult GetRagdollBodies(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<BodyHandle> bodyHandles) const;

        [[nodiscard]]
        QueryResult GetRagdollConstraints(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<ConstraintHandle> constraintHandles) const;

        bool ActivateRagdoll(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        bool SetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms);

        [[nodiscard]]
        QueryResult GetRagdollPose(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const;

        bool DriveRagdollKinematically(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime);

        bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> modelTransforms);

        bool DriveRagdollMotors(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime);

        bool ResetRagdollWarmStart(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle);

        bool SetRagdollVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity,
            AZ::Vector3 angularVelocity);

        bool SetRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity);

        bool AddRagdollLinearVelocity(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 linearVelocity);

        bool AddRagdollImpulse(
            WorldHandle worldHandle,
            RagdollHandle ragdollHandle,
            AZ::Vector3 impulse);

    private:
        friend class Runtime;

        Ragdolls() = default;
        ~Ragdolls() = default;

        static AZStd::atomic<Ragdolls*> s_instance;
    };
} // namespace Jolt
