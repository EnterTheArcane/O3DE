/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Rollback.h>

#include <Jolt/BodyConfiguration.h>
#include <Jolt/Collision.h>
#include <Jolt/Query.h>
#include <Jolt/TypeIds.h>
#include <Jolt/WorldTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>

namespace Jolt
{
    struct BodyCollisionParticipant final
    {
        AZ_TYPE_INFO(BodyCollisionParticipant, BodyCollisionParticipantTypeId);

        WorldTransform m_centerOfMassTransform;

        AZ::EntityId m_entityId;
        BodyHandle m_bodyHandle;
        ShapeHandle m_shapeHandle;
        ObjectLayer m_objectLayer;
        BodyKind m_kind = BodyKind::None;
        MotionType m_motionType = MotionType::None;

        bool m_isActive = false;
        bool m_isSensor = false;
    };

    struct BodyPairCollisionInput final
    {
        AZ_TYPE_INFO(BodyPairCollisionInput, BodyPairCollisionInputTypeId);

        //! Solver ordering is independent of body creation order.
        BodyCollisionParticipant m_first;
        BodyCollisionParticipant m_second;
    };

    struct BodyPairCollisionSettings final
    {
        AZ_TYPE_INFO(BodyPairCollisionSettings, BodyPairCollisionSettingsTypeId);

        AZ::Vector3 m_activeEdgeMovementDirection = AZ::Vector3::CreateZero();

        float m_collisionTolerance = 1.0e-4f;
        float m_internalEdgeRemovalVertexTolerance = 1.0e-4f;
        float m_maximumSeparationDistance = 0.0f;
        float m_penetrationTolerance = 1.0e-4f;

        ActiveEdgeMode m_activeEdgeMode = ActiveEdgeMode::CollideOnlyWithActive;
        BackFaceMode m_backFaceMode = BackFaceMode::Ignore;
        FaceCollectionMode m_faceCollectionMode = FaceCollectionMode::Collect;
    };

    struct BodyPairCollisionHit final
    {
        AZ_TYPE_INFO(BodyPairCollisionHit, BodyPairCollisionHitTypeId);

        //! Contact and supporting-face positions use absolute world coordinates.
        WorldPosition m_contactPositionOnFirst;
        WorldPosition m_contactPositionOnSecond;
        AZ::Vector3 m_penetrationAxis = AZ::Vector3::CreateZero();

        AZStd::span<const WorldPosition> m_firstFace;
        AZStd::span<const WorldPosition> m_secondFace;

        SubShapeId m_firstSubShapeId;
        SubShapeId m_secondSubShapeId;
        float m_penetrationDepth = 0.0f;
    };

    class IBodyPairCollisionCollector
    {
    public:
        virtual ~IBodyPairCollisionCollector() = default;

        virtual bool CollideDefault(const BodyPairCollisionSettings& settings) = 0;

        virtual bool CollideAny(const BodyPairCollisionSettings& settings) = 0;

        virtual bool CollideAnyPerLeaf(const BodyPairCollisionSettings& settings) = 0;

        virtual bool CollideDeepest(const BodyPairCollisionSettings& settings) = 0;

        virtual bool CollideDeepestPerLeaf(const BodyPairCollisionSettings& settings) = 0;

        virtual bool AddHit(const BodyPairCollisionHit& hit) = 0;

        [[nodiscard]]
        virtual bool ShouldEarlyOut() const = 0;
    };

    class IBodyPairCollider
        : public IRollbackParticipant
    {
    public:
        virtual ~IBodyPairCollider() = default;

        //! Called concurrently under simulation locks. Implementations must not call runtime capabilities.
        //! The state hash must include behavior-affecting mutable state, but exclude observational state.

        [[nodiscard]]
        virtual AZ::u64 GetStateHash() const = 0;

        virtual void Collide(
            const BodyPairCollisionInput& input,
            const BodyPairCollisionSettings& settings,
            IBodyPairCollisionCollector& collector) = 0;
    };
} // namespace Jolt
