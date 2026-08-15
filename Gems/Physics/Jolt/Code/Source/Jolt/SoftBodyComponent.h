/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/SoftBodyBus.h>
#include <Jolt/SoftBodyComponentConfiguration.h>
#include <Jolt/TypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Jolt
{
    class ISystem;

    class SoftBodyComponent final
        : public AZ::Component
        , public SoftBodyRequestBus::Handler
        , public BodyRequestBus::Handler
        , private BodyNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(SoftBodyComponent, SoftBodyComponentTypeId);

        SoftBodyComponent();
        explicit SoftBodyComponent(SoftBodyComponentConfiguration configuration);
        ~SoftBodyComponent() override = default;

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
        BodyHandle GetBodyHandle() const override;

        [[nodiscard]]
        AZ::u64 GetUserData() const override;

        bool SetUserData(AZ::u64 userData) override;

        [[nodiscard]]
        SoftBodyDefinitionHandle GetDefinitionHandle() const override;

        [[nodiscard]]
        WorldTransform GetCenterOfMassTransform() const override;

        [[nodiscard]]
        SoftBodyDefinitionState GetDefinitionState() const override;

        [[nodiscard]]
        BodyState GetState() const override;

        QueryResult QueryFaces(AZStd::span<SoftBodyFace> faces) const override;

        QueryResult QueryMaterials(AZStd::span<MaterialHandle> materials) const override;

        QueryResult QueryRodStates(AZStd::span<SoftBodyRodState> rods) const override;

        QueryResult QueryVertices(AZStd::span<SoftBodyVertex> vertices) const override;

        QueryResult QueryDefinitionDihedralBendConstraints(
            AZStd::span<SoftBodyDihedralBendConstraint> constraints) const override;

        QueryResult QueryDefinitionEdgeConstraints(
            AZStd::span<SoftBodyEdgeConstraint> constraints) const override;

        QueryResult QueryDefinitionFaces(AZStd::span<SoftBodyFace> faces) const override;

        QueryResult QueryDefinitionInverseBinds(
            AZStd::span<SoftBodyInverseBind> inverseBinds) const override;

        QueryResult QueryDefinitionLongRangeConstraints(
            AZStd::span<SoftBodyLongRangeConstraint> constraints) const override;

        QueryResult QueryDefinitionMaterials(AZStd::span<MaterialHandle> materials) const override;

        QueryResult QueryDefinitionRodBendTwistConstraints(
            AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const override;

        QueryResult QueryDefinitionRodStretchShearConstraints(
            AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const override;

        QueryResult QueryDefinitionSkinConstraints(
            AZStd::span<SoftBodySkinConstraint> constraints) const override;

        QueryResult QueryDefinitionVertices(AZStd::span<SoftBodyVertex> vertices) const override;

        QueryResult QueryDefinitionVolumeConstraints(
            AZStd::span<SoftBodyVolumeConstraint> constraints) const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyFace> CopyFaces() const override;

        [[nodiscard]]
        AZStd::vector<MaterialHandle> CopyMaterials() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyRodState> CopyRodStates() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyVertex> CopyVertices() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyDihedralBendConstraint> CopyDefinitionDihedralBendConstraints() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyEdgeConstraint> CopyDefinitionEdgeConstraints() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyFace> CopyDefinitionFaces() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyInverseBind> CopyDefinitionInverseBinds() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyLongRangeConstraint> CopyDefinitionLongRangeConstraints() const override;

        [[nodiscard]]
        AZStd::vector<MaterialHandle> CopyDefinitionMaterials() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyRodBendTwistConstraint> CopyDefinitionRodBendTwistConstraints() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyRodStretchShearConstraint> CopyDefinitionRodStretchShearConstraints() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodySkinConstraint> CopyDefinitionSkinConstraints() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyVertex> CopyDefinitionVertices() const override;

        [[nodiscard]]
        AZStd::vector<SoftBodyVolumeConstraint> CopyDefinitionVolumeConstraints() const override;

        bool GetInverseInertia(AZ::Matrix3x3& inverseInertia) const override;

        bool GetInverseMass(float& inverseMass) const override;

        bool GetLocalBounds(AZ::Aabb& bounds) const override;

        bool GetRuntimeConfiguration(SoftBodyRuntimeConfiguration& configuration) const override;

        bool GetVolume(float& volume) const override;

        bool ApplySkinPose(
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll) override;

        bool ApplySkinPoseFromTransforms(
            const AZStd::vector<AZ::Transform>& jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll) override;

        bool RecalculateMassProperties(bool activate) override;

        bool SetVertexInverseMass(
            AZ::u32 vertexIndex,
            float inverseMass) override;

        bool SetVertexInverseMasses(
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses) override;

        bool SetVertexInverseMassesFromValues(
            AZ::u32 startVertexIndex,
            const AZStd::vector<float>& inverseMasses) override;

        bool SetVertexVelocity(
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity) override;

        bool SetVertexVelocities(
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities) override;

        bool SetVertexVelocitiesFromVectors(
            AZ::u32 startVertexIndex,
            const AZStd::vector<AZ::Vector3>& velocities) override;

        bool UpdateManually(float deltaTime) override;

        bool UpdateRuntimeConfiguration(const SoftBodyRuntimeConfiguration& configuration) override;

    private:
        void Activate() override;

        void Deactivate() override;

        void OnBodyMoved(const BodyMoveEvent& event) override;

        void OnTransformChanged(
            const AZ::Transform& local,
            const AZ::Transform& world) override;

        void ReleaseDefinition();

        AZStd::unique_ptr<SoftBodyComponentConfiguration> m_configuration;
        AZStd::unique_ptr<AZStd::vector<MaterialHandle>> m_materialHandles;

        ISystem* m_system = nullptr;
        WorldHandle m_worldHandle;
        BodyHandle m_bodyHandle;
        SoftBodyDefinitionHandle m_definitionHandle;
        float m_uniformScale = 1.0f;
        bool m_syncingTransform = false;
    };
} // namespace Jolt
