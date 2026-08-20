/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>

#include <Jolt/Rollback.h>

#include <Jolt/Collision.h>
#include <Jolt/Handle.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>

namespace AZ
{
    class ReflectContext;
} // namespace AZ

namespace Jolt
{
    inline constexpr size_t MaximumSoftBodySkinWeights = 4;

    enum class SoftBodyBendType : AZ::u8
    {
        None = 0,
        Dihedral,
        Distance,
    };

    enum class SoftBodyLongRangeAttachmentType : AZ::u8
    {
        None = 0,
        EuclideanDistance,
        GeodesicDistance,
    };

    enum class SoftBodyContactDecision : AZ::u8
    {
        None = 0,
        Accept,
        Reject,
    };

    struct SoftBodyContactSettings final
    {
        AZ_TYPE_INFO(SoftBodyContactSettings, SoftBodyContactSettingsTypeId);

        float m_softBodyInverseMassScale = 1.0f;
        float m_otherBodyInverseMassScale = 1.0f;
        float m_otherBodyInverseInertiaScale = 1.0f;
        bool m_isSensor = false;
    };

    struct SoftBodyContactPoint final
    {
        AZ_TYPE_INFO(SoftBodyContactPoint, SoftBodyContactPointTypeId);

        WorldPosition m_position;
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        BodyHandle m_otherBodyHandle;
        AZ::u32 m_vertexIndex = 0;
    };

    class ISoftBodyContactManifold
    {
    public:
        virtual ~ISoftBodyContactManifold() = default;

        //! This view is valid only for the duration of OnContactAdded.

        [[nodiscard]]
        virtual AZ::u32 GetVertexCount() const = 0;

        [[nodiscard]]
        virtual bool GetContact(
            AZ::u32 vertexIndex,
            SoftBodyContactPoint& contact) const = 0;

        [[nodiscard]]
        virtual AZ::u32 GetSensorCount() const = 0;

        [[nodiscard]]
        virtual BodyHandle GetSensorBody(AZ::u32 sensorIndex) const = 0;
    };

    class ISoftBodyContactCallbacks
        : public IRollbackParticipant
    {
    public:
        virtual ~ISoftBodyContactCallbacks() = default;

        //! Callbacks run under simulation locks and may run concurrently. They must not call ISystem.
        //! The state hash must include behavior-affecting mutable state, but exclude observational state.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        virtual SoftBodyContactDecision OnContactValidate(
            BodyHandle softBodyHandle,
            BodyHandle otherBodyHandle,
            SoftBodyContactSettings& settings) = 0;

        virtual void OnContactAdded(
            BodyHandle softBodyHandle,
            const ISoftBodyContactManifold& manifold) = 0;
    };

    struct SoftBodyVertex final
    {
        AZ_TYPE_INFO(SoftBodyVertex, SoftBodyVertexTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_velocity = AZ::Vector3::CreateZero();
        float m_inverseMass = 1.0f;
    };

    struct SoftBodyFace final
    {
        AZ_TYPE_INFO(SoftBodyFace, SoftBodyFaceTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::u32 m_thirdVertex = 0;
        AZ::u32 m_materialIndex = 0;
    };

    struct SoftBodyVertexAttributes final
    {
        AZ_TYPE_INFO(SoftBodyVertexAttributes, SoftBodyVertexAttributesTypeId);

        float m_bendCompliance = AZStd::numeric_limits<float>::max();
        float m_compliance = 0.0f;
        float m_longRangeMaximumDistanceMultiplier = 1.0f;
        float m_shearCompliance = 0.0f;
        SoftBodyLongRangeAttachmentType m_longRangeAttachmentType = SoftBodyLongRangeAttachmentType::None;
    };

    struct SoftBodyEdgeConstraint final
    {
        AZ_TYPE_INFO(SoftBodyEdgeConstraint, SoftBodyEdgeConstraintTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        float m_compliance = 0.0f;
        float m_restLength = -1.0f;
    };

    struct SoftBodyDihedralBendConstraint final
    {
        AZ_TYPE_INFO(SoftBodyDihedralBendConstraint, SoftBodyDihedralBendConstraintTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::u32 m_thirdVertex = 0;
        AZ::u32 m_fourthVertex = 0;
        float m_compliance = 0.0f;
        float m_initialAngle = 0.0f;
        bool m_calculateInitialAngle = true;
    };

    struct SoftBodyVolumeConstraint final
    {
        AZ_TYPE_INFO(SoftBodyVolumeConstraint, SoftBodyVolumeConstraintTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::u32 m_thirdVertex = 0;
        AZ::u32 m_fourthVertex = 0;
        float m_compliance = 0.0f;
        float m_restVolume = 0.0f;
        bool m_calculateRestVolume = true;
    };

    struct SoftBodyLongRangeConstraint final
    {
        AZ_TYPE_INFO(SoftBodyLongRangeConstraint, SoftBodyLongRangeConstraintTypeId);

        AZ::u32 m_fixedVertex = 0;
        AZ::u32 m_dynamicVertex = 0;
        float m_maximumDistance = 0.0f;
    };

    struct SoftBodyRodStretchShearConstraint final
    {
        AZ_TYPE_INFO(SoftBodyRodStretchShearConstraint, SoftBodyRodStretchShearConstraintTypeId);

        AZ::u32 m_firstVertex = 0;
        AZ::u32 m_secondVertex = 0;
        AZ::Quaternion m_bishopRotation = AZ::Quaternion::CreateIdentity();
        float m_compliance = 0.0f;
        float m_inverseMass = -1.0f;
        float m_restLength = -1.0f;
        bool m_calculateBishopRotation = true;
    };

    struct SoftBodyRodBendTwistConstraint final
    {
        AZ_TYPE_INFO(SoftBodyRodBendTwistConstraint, SoftBodyRodBendTwistConstraintTypeId);

        AZ::u32 m_firstRod = 0;
        AZ::u32 m_secondRod = 0;
        AZ::Quaternion m_initialRotation = AZ::Quaternion::CreateIdentity();
        float m_compliance = 0.0f;
        bool m_calculateInitialRotation = true;
    };

    struct SoftBodyInverseBind final
    {
        AZ_TYPE_INFO(SoftBodyInverseBind, SoftBodyInverseBindTypeId);

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        AZ::u32 m_jointIndex = 0;
    };

    struct SoftBodySkinWeight final
    {
        AZ_TYPE_INFO(SoftBodySkinWeight, SoftBodySkinWeightTypeId);

        AZ::u32 m_inverseBindIndex = 0;
        float m_weight = 0.0f;
    };

    struct SoftBodySkinConstraint final
    {
        AZ_TYPE_INFO(SoftBodySkinConstraint, SoftBodySkinConstraintTypeId);

        [[nodiscard]]
        SoftBodySkinWeight GetWeight(const AZ::u32 index) const
        {
            if (index >= m_weights.size())
            {
                return {};
            }
            return m_weights[index];
        }

        bool SetWeight(
            const AZ::u32 index,
            const SoftBodySkinWeight& weight)
        {
            if (index >= m_weights.size())
            {
                return false;
            }
            m_weights[index] = weight;
            return true;
        }

        AZStd::array<SoftBodySkinWeight, MaximumSoftBodySkinWeights> m_weights;
        AZ::u32 m_vertex = 0;
        float m_backstopDistance = AZStd::numeric_limits<float>::max();
        float m_backstopRadius = 40.0f;
        float m_maximumDistance = AZStd::numeric_limits<float>::max();
    };

    struct SoftBodyOptimizationRemap final
    {
        AZ_TYPE_INFO(SoftBodyOptimizationRemap, SoftBodyOptimizationRemapTypeId);

        AZStd::vector<AZ::u32> m_dihedralBendConstraints;
        AZStd::vector<AZ::u32> m_edgeConstraints;
        AZStd::vector<AZ::u32> m_longRangeConstraints;
        AZStd::vector<AZ::u32> m_rodBendTwistConstraints;
        AZStd::vector<AZ::u32> m_rodStretchShearConstraints;
        AZStd::vector<AZ::u32> m_skinConstraints;
        AZStd::vector<AZ::u32> m_volumeConstraints;
    };

    struct SoftBodyDefinitionArchive final
    {
        AZ_TYPE_INFO(SoftBodyDefinitionArchive, SoftBodyDefinitionArchiveTypeId);

        JOLT_API static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<AZ::u8> m_binaryState;
        AZ::u64 m_buildFingerprint = 0;
        AZ::u64 m_contentHash = 0;
        AZ::u32 m_formatVersion = 0;
        AZ::u32 m_materialCount = 0;
    };

    struct SoftBodyDefinitionConfiguration final
    {
        AZ_TYPE_INFO(SoftBodyDefinitionConfiguration, SoftBodyDefinitionConfigurationTypeId);

        AZStd::vector<SoftBodyVertex> m_vertices;
        AZStd::vector<SoftBodyFace> m_faces;
        AZStd::vector<MaterialHandle> m_materials;
        AZStd::vector<SoftBodyVertexAttributes> m_vertexAttributes;

        AZStd::vector<SoftBodyEdgeConstraint> m_edgeConstraints;
        AZStd::vector<SoftBodyDihedralBendConstraint> m_dihedralBendConstraints;
        AZStd::vector<SoftBodyLongRangeConstraint> m_longRangeConstraints;
        AZStd::vector<SoftBodyRodStretchShearConstraint> m_rodStretchShearConstraints;
        AZStd::vector<SoftBodyRodBendTwistConstraint> m_rodBendTwistConstraints;
        AZStd::vector<SoftBodyVolumeConstraint> m_volumeConstraints;
        AZStd::vector<SoftBodyInverseBind> m_inverseBinds;
        AZStd::vector<SoftBodySkinConstraint> m_skinConstraints;

        float m_shearAngleTolerance = 0.13962634f;
        SoftBodyBendType m_bendType = SoftBodyBendType::Distance;
        bool m_createFaceConstraints = true;
        bool m_optimize = true;
    };

    struct SoftBodyDefinitionState final
    {
        AZ_TYPE_INFO(SoftBodyDefinitionState, SoftBodyDefinitionStateTypeId);

        AZ::u32 m_vertexCount = 0;
        AZ::u32 m_faceCount = 0;
        AZ::u32 m_materialCount = 0;

        AZ::u32 m_dihedralBendConstraintCount = 0;
        AZ::u32 m_edgeConstraintCount = 0;
        AZ::u32 m_longRangeConstraintCount = 0;
        AZ::u32 m_rodBendTwistConstraintCount = 0;
        AZ::u32 m_rodStretchShearConstraintCount = 0;
        AZ::u32 m_skinConstraintCount = 0;
        AZ::u32 m_volumeConstraintCount = 0;

        AZ::u32 m_inverseBindCount = 0;
    };

    struct SoftBodyConfiguration final
    {
        AZ_TYPE_INFO(SoftBodyConfiguration, SoftBodyConfigurationTypeId);

        SoftBodyDefinitionHandle m_definitionHandle;
        WorldTransform m_transform;

        AZ::EntityId m_entityId = AZ::EntityId();
        AZ::Name m_name;
        AZ::u64 m_userData = 0;
        CollisionGroupConfiguration m_collisionGroup;
        ObjectLayer m_objectLayer = DefaultLayers::Moving;

        float m_friction = 0.2f;
        float m_gravityFactor = 1.0f;
        float m_linearDamping = 0.1f;
        float m_maximumLinearVelocity = 500.0f;
        float m_pressure = 0.0f;
        float m_restitution = 0.0f;
        float m_skinnedMaximumDistanceMultiplier = 1.0f;
        float m_vertexRadius = 0.0f;
        AZ::u32 m_iterationCount = 5;

        bool m_activate = true;
        bool m_allowSleeping = true;
        bool m_enableSkinConstraints = true;
        bool m_facesDoubleSided = false;
        bool m_makeRotationIdentity = true;
        bool m_manualUpdate = false;
        bool m_updatePosition = true;
    };

    struct SoftBodyRuntimeConfiguration final
    {
        AZ_TYPE_INFO(SoftBodyRuntimeConfiguration, SoftBodyRuntimeConfigurationTypeId);

        float m_friction = 0.2f;
        float m_gravityFactor = 1.0f;
        float m_linearDamping = 0.1f;
        float m_maximumLinearVelocity = 500.0f;
        float m_pressure = 0.0f;
        float m_restitution = 0.0f;
        float m_skinnedMaximumDistanceMultiplier = 1.0f;
        float m_vertexRadius = 0.0f;
        AZ::u32 m_iterationCount = 5;
        bool m_allowSleeping = true;
        bool m_enableSkinConstraints = true;
        bool m_facesDoubleSided = false;
        bool m_updatePosition = true;
    };

    struct SoftBodyRodState final
    {
        AZ_TYPE_INFO(SoftBodyRodState, SoftBodyRodStateTypeId);

        AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();
    };
} // namespace Jolt

AZ_TYPE_INFO_SPECIALIZE(Jolt::SoftBodyBendType, "{94DCFF13-D1FC-40F0-B3E5-E6ECE418A41C}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::SoftBodyContactDecision, "{306F6E14-A186-44C4-8DB6-049DCE6F2F5C}");
AZ_TYPE_INFO_SPECIALIZE(Jolt::SoftBodyLongRangeAttachmentType, "{DDA7E439-F81F-4207-8D78-EAD2511FF900}");
