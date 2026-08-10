/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Handle.h>
#include <Box3D/TypeIds.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/base.h>

#include <cstddef>

namespace AZ
{
    class ReflectContext;
}

namespace Box3D
{
    enum class StepEventTypes : AZ::u8
    {
        None = 0,
        BodyMoves = 1 << 0,
        Sensors = 1 << 1,
        Contacts = 1 << 2,
        ContactHits = 1 << 3,
        JointThresholds = 1 << 4,
        All = 0x1f,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(StepEventTypes)

    enum class EventPhase : AZ::u8
    {
        Begin,
        Persist,
        End,
    };

    struct ContactPoint final
    {
        AZ_TYPE_INFO(ContactPoint, ContactPointTypeId);

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_impulse = AZ::Vector3::CreateZero();

        float m_separation = 0.0f;
        AZ::s32 m_faceIndex = -1;
    };

    struct ContactEvent final
    {
        AZ_TYPE_INFO(ContactEvent, ContactEventTypeId);

        BodyHandle m_bodyA;
        BodyHandle m_bodyB;
        ShapeHandle m_shapeA;
        ShapeHandle m_shapeB;

        EventPhase m_phase = EventPhase::Begin;
        AZ::u32 m_firstPoint = 0;
        AZ::u32 m_pointCount = 0;

        AZ::s32 m_childIndexA = -1;
        AZ::s32 m_childIndexB = -1;
    };

    struct ContactHitEvent final
    {
        AZ_TYPE_INFO(ContactHitEvent, ContactHitEventTypeId);

        BodyHandle m_bodyA;
        BodyHandle m_bodyB;
        ShapeHandle m_shapeA;
        ShapeHandle m_shapeB;
        MaterialHandle m_materialA;
        MaterialHandle m_materialB;

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        float m_approachSpeed = 0.0f;

        AZ::s32 m_faceIndex = -1;
        AZ::s32 m_childIndexA = -1;
        AZ::s32 m_childIndexB = -1;
    };

    struct SensorEvent final
    {
        AZ_TYPE_INFO(SensorEvent, SensorEventTypeId);

        BodyHandle m_sensorBody;
        BodyHandle m_visitorBody;
        ShapeHandle m_sensorShape;
        ShapeHandle m_visitorShape;

        EventPhase m_phase = EventPhase::Begin;
    };

    struct BodyMoveEvent final
    {
        AZ_TYPE_INFO(BodyMoveEvent, BodyMoveEventTypeId);

        BodyHandle m_bodyHandle;

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();

        bool m_fellAsleep = false;
    };

    struct JointThresholdEvent final
    {
        AZ_TYPE_INFO(JointThresholdEvent, JointThresholdEventTypeId);

        JointHandle m_jointHandle;
    };

    struct ContactSnapshot final
    {
        BodyHandle m_bodyA;
        BodyHandle m_bodyB;
        ShapeHandle m_shapeA;
        ShapeHandle m_shapeB;

        size_t m_firstPoint = 0;
        size_t m_pointCount = 0;
        size_t m_requiredPointCount = 0;

        AZ::s32 m_childIndexA = -1;
        AZ::s32 m_childIndexB = -1;
    };

    struct ContactSnapshotResult final
    {
        BufferResult m_contacts;
        BufferResult m_points;
    };

    struct SensorOverlap final
    {
        BodyHandle m_sensorBody;
        BodyHandle m_visitorBody;
        ShapeHandle m_sensorShape;
        ShapeHandle m_visitorShape;
    };

    void ReflectEvents(AZ::ReflectContext* context);
} // namespace Box3D
