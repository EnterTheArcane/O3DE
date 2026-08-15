/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyBus.h>
#include <Jolt/BodyConfiguration.h>
#include <Jolt/Query.h>
#include <Jolt/SoftBody.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class ISoftBodyRequests
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
        virtual BodyHandle GetBodyHandle() const = 0;

        [[nodiscard]]
        virtual SoftBodyDefinitionHandle GetDefinitionHandle() const = 0;

        [[nodiscard]]
        virtual WorldTransform GetCenterOfMassTransform() const = 0;

        [[nodiscard]]
        virtual SoftBodyDefinitionState GetDefinitionState() const = 0;

        [[nodiscard]]
        virtual BodyState GetState() const = 0;

        virtual QueryResult QueryFaces(AZStd::span<SoftBodyFace> faces) const = 0;

        virtual QueryResult QueryMaterials(AZStd::span<MaterialHandle> materials) const = 0;

        virtual QueryResult QueryRodStates(AZStd::span<SoftBodyRodState> rods) const = 0;

        virtual QueryResult QueryVertices(AZStd::span<SoftBodyVertex> vertices) const = 0;

        virtual QueryResult QueryDefinitionDihedralBendConstraints(
            AZStd::span<SoftBodyDihedralBendConstraint> constraints) const = 0;

        virtual QueryResult QueryDefinitionEdgeConstraints(
            AZStd::span<SoftBodyEdgeConstraint> constraints) const = 0;

        virtual QueryResult QueryDefinitionFaces(AZStd::span<SoftBodyFace> faces) const = 0;

        virtual QueryResult QueryDefinitionInverseBinds(
            AZStd::span<SoftBodyInverseBind> inverseBinds) const = 0;

        virtual QueryResult QueryDefinitionLongRangeConstraints(
            AZStd::span<SoftBodyLongRangeConstraint> constraints) const = 0;

        virtual QueryResult QueryDefinitionMaterials(AZStd::span<MaterialHandle> materials) const = 0;

        virtual QueryResult QueryDefinitionRodBendTwistConstraints(
            AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const = 0;

        virtual QueryResult QueryDefinitionRodStretchShearConstraints(
            AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const = 0;

        virtual QueryResult QueryDefinitionSkinConstraints(
            AZStd::span<SoftBodySkinConstraint> constraints) const = 0;

        virtual QueryResult QueryDefinitionVertices(AZStd::span<SoftBodyVertex> vertices) const = 0;

        virtual QueryResult QueryDefinitionVolumeConstraints(
            AZStd::span<SoftBodyVolumeConstraint> constraints) const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyFace> CopyFaces() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<MaterialHandle> CopyMaterials() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyRodState> CopyRodStates() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyVertex> CopyVertices() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyDihedralBendConstraint> CopyDefinitionDihedralBendConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyEdgeConstraint> CopyDefinitionEdgeConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyFace> CopyDefinitionFaces() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyInverseBind> CopyDefinitionInverseBinds() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyLongRangeConstraint> CopyDefinitionLongRangeConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<MaterialHandle> CopyDefinitionMaterials() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyRodBendTwistConstraint> CopyDefinitionRodBendTwistConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyRodStretchShearConstraint> CopyDefinitionRodStretchShearConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodySkinConstraint> CopyDefinitionSkinConstraints() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyVertex> CopyDefinitionVertices() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<SoftBodyVolumeConstraint> CopyDefinitionVolumeConstraints() const = 0;

        virtual bool GetInverseInertia(AZ::Matrix3x3& inverseInertia) const = 0;

        virtual bool GetInverseMass(float& inverseMass) const = 0;

        virtual bool GetLocalBounds(AZ::Aabb& bounds) const = 0;

        virtual bool GetRuntimeConfiguration(SoftBodyRuntimeConfiguration& configuration) const = 0;

        virtual bool GetVolume(float& volume) const = 0;

        virtual bool ApplySkinPose(
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll) = 0;

        virtual bool ApplySkinPoseFromTransforms(
            const AZStd::vector<AZ::Transform>& jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll) = 0;

        virtual bool RecalculateMassProperties(bool activate) = 0;

        virtual bool SetVertexInverseMass(
            AZ::u32 vertexIndex,
            float inverseMass) = 0;

        virtual bool SetVertexInverseMasses(
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses) = 0;

        virtual bool SetVertexInverseMassesFromValues(
            AZ::u32 startVertexIndex,
            const AZStd::vector<float>& inverseMasses) = 0;

        virtual bool SetVertexVelocity(
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity) = 0;

        virtual bool SetVertexVelocities(
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities) = 0;

        virtual bool SetVertexVelocitiesFromVectors(
            AZ::u32 startVertexIndex,
            const AZStd::vector<AZ::Vector3>& velocities) = 0;

        virtual bool UpdateManually(float deltaTime) = 0;

        virtual bool UpdateRuntimeConfiguration(const SoftBodyRuntimeConfiguration& configuration) = 0;
    };

    using SoftBodyRequestBus = AZ::EBus<ISoftBodyRequests>;
} // namespace Jolt
