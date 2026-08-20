/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/BodyBus.h>
#include <Jolt/RagdollBus.h>
#include <Jolt/RagdollComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class RuntimeImplementation;

    class JOLT_API RagdollComponent final
        : public AZ::Component
        , public RagdollRequestBus::Handler
        , private BodyNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(RagdollComponent, RagdollComponentTypeId);

        RagdollComponent();
        explicit RagdollComponent(RagdollComponentConfiguration configuration);
        ~RagdollComponent() override;

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
        RagdollHandle GetRagdollHandle() const override;

        [[nodiscard]]
        RagdollState GetState() const override;

        QueryResult QueryBodies(AZStd::span<BodyHandle> bodyHandles) const override;

        QueryResult QueryConstraints(AZStd::span<ConstraintHandle> constraintHandles) const override;

        QueryResult QueryPose(
            WorldPosition& rootPosition,
            AZStd::span<AZ::Transform> modelTransforms) const override;

        [[nodiscard]]
        AZStd::vector<BodyHandle> CopyBodies() const override;

        [[nodiscard]]
        AZStd::vector<ConstraintHandle> CopyConstraints() const override;

        [[nodiscard]]
        AZStd::vector<AZ::Transform> CopyPose() const override;

        bool SetPose(
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms) override;

        bool SetPoseFromTransforms(
            WorldPosition rootPosition,
            const AZStd::vector<AZ::Transform>& modelTransforms) override;

        bool DriveKinematically(
            WorldPosition rootPosition,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) override;

        bool DriveKinematicallyFromTransforms(
            WorldPosition rootPosition,
            const AZStd::vector<AZ::Transform>& modelTransforms,
            float deltaTime) override;

        bool DriveMotors(AZStd::span<const AZ::Transform> modelTransforms) override;

        bool DriveMotorsFromTransforms(const AZStd::vector<AZ::Transform>& modelTransforms) override;

        bool DriveMotorsWithVelocity(
            AZStd::span<const AZ::Transform> previousModelTransforms,
            AZStd::span<const AZ::Transform> modelTransforms,
            float deltaTime) override;

        bool DriveMotorsWithVelocityFromTransforms(
            const AZStd::vector<AZ::Transform>& previousModelTransforms,
            const AZStd::vector<AZ::Transform>& modelTransforms,
            float deltaTime) override;

        bool ResetWarmStart() override;

        bool ActivateRagdoll() override;

        bool SetVelocity(
            const AZ::Vector3& linearVelocity,
            const AZ::Vector3& angularVelocity) override;

        bool SetLinearVelocity(const AZ::Vector3& linearVelocity) override;

        bool SetCollisionGroupId(AZ::u32 collisionGroupId) override;

        bool AddLinearVelocity(const AZ::Vector3& linearVelocity) override;

        bool AddImpulse(const AZ::Vector3& impulse) override;

    private:
        struct RuntimeResources;

        void Activate() override;

        void Deactivate() override;

        void OnBodyMoved(const BodyMoveEvent& event) override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        bool DestroyInstance();

        void ReleaseResources();

        AZStd::unique_ptr<RagdollComponentConfiguration> m_configuration;
        AZStd::unique_ptr<RuntimeResources> m_resources;

        RuntimeImplementation* m_system = nullptr;
        WorldHandle m_worldHandle;
        RagdollHandle m_ragdollHandle;
        BodyHandle m_rootBodyHandle;
        AZ::Transform m_entityTransform = AZ::Transform::CreateIdentity();
        bool m_syncingTransform = false;
    };
} // namespace Jolt
