/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Configuration.h>
#include <Jolt/Query.h>
#include <Jolt/SoftBody.h>
#include <AzCore/Math/Aabb.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API SoftBodies
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static SoftBodies* Get();

        [[nodiscard]]
        SoftBodyDefinitionHandle CreateSoftBodyDefinition(
            const SoftBodyDefinitionConfiguration& configuration,
            SoftBodyOptimizationRemap* optimizationRemap = nullptr);

        [[nodiscard]]
        bool ExportSoftBodyDefinition(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionArchive& archive,
            AZStd::vector<MaterialHandle>& materialHandles) const;

        [[nodiscard]]
        SoftBodyDefinitionHandle ImportSoftBodyDefinition(
            const SoftBodyDefinitionArchive& archive,
            AZStd::span<const MaterialHandle> materialHandles);

        bool DestroySoftBodyDefinition(SoftBodyDefinitionHandle definitionHandle);

        [[nodiscard]]
        bool IsValid(SoftBodyDefinitionHandle definitionHandle) const;

        [[nodiscard]]
        bool GetSoftBodyDefinitionState(
            SoftBodyDefinitionHandle definitionHandle,
            SoftBodyDefinitionState& state) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionDihedralBendConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyDihedralBendConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionEdgeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyEdgeConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionFaces(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyFace> faces) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionInverseBinds(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyInverseBind> inverseBinds) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionLongRangeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyLongRangeConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionMaterials(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<MaterialHandle> materials) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionRodBendTwistConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionRodStretchShearConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionSkinConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodySkinConstraint> constraints) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionVertices(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVertex> vertices) const;

        [[nodiscard]]
        QueryResult GetSoftBodyDefinitionVolumeConstraints(
            SoftBodyDefinitionHandle definitionHandle,
            AZStd::span<SoftBodyVolumeConstraint> constraints) const;

        [[nodiscard]]
        BodyHandle CreateSoftBody(
            WorldHandle worldHandle,
            const SoftBodyConfiguration& configuration);

        //! Creates a soft body with an explicit simulation identity for synchronized peers.
        [[nodiscard]]
        BodyHandle CreateSoftBodyWithId(
            WorldHandle worldHandle,
            BodyId bodyId,
            const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        QueryResult GetSoftBodyFaces(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyFace> faces) const;

        [[nodiscard]]
        bool GetSoftBodyLocalBounds(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::Aabb& bounds) const;

        [[nodiscard]]
        QueryResult GetSoftBodyMaterials(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<MaterialHandle> materials) const;

        [[nodiscard]]
        QueryResult GetSoftBodyRodStates(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyRodState> rods) const;

        [[nodiscard]]
        bool GetSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            SoftBodyRuntimeConfiguration& configuration) const;

        //! Replaces soft-body creation settings and resets its simulated vertices.
        //! The body must remain manually updated and cannot belong to a scene instance.
        bool ApplySoftBodyConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyConfiguration& configuration);

        [[nodiscard]]
        QueryResult GetSoftBodyVertices(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<SoftBodyVertex> vertices) const;

        [[nodiscard]]
        bool GetSoftBodyVolume(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float& volume) const;

        bool RecalculateSoftBodyMassProperties(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            bool activate);

        bool SkinSoftBody(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
            bool hardSkinAll);

        bool UpdateSoftBodyManually(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            float deltaTime);

        bool UpdateSoftBodyRuntimeConfiguration(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            const SoftBodyRuntimeConfiguration& configuration);

        bool SetSoftBodyVertexInverseMass(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            float inverseMass);

        bool SetSoftBodyVertexInverseMasses(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const float> inverseMasses);

        bool SetSoftBodyVertexVelocity(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 vertexIndex,
            const AZ::Vector3& velocity);

        bool SetSoftBodyVertexVelocities(
            WorldHandle worldHandle,
            BodyHandle bodyHandle,
            AZ::u32 startVertexIndex,
            AZStd::span<const AZ::Vector3> velocities);

    private:
        friend class Runtime;

        SoftBodies() = default;
        ~SoftBodies() = default;
    };
} // namespace Jolt
