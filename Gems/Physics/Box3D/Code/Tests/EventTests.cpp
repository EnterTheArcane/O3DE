/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/SystemInternal.h>
#include <Box3D/WorldBus.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        BodyHandle CreateEventBody(
            System& system,
            WorldHandle worldHandle,
            BodyType bodyType,
            const AZ::Vector3& position)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = bodyType;
            configuration.m_transform = AZ::Transform::CreateTranslation(position);
            configuration.m_gravityScale = 0.0f;
            return system.CreateBody(worldHandle, configuration);
        }

        class WorldEventHandler final
            : public WorldNotificationBus::Handler
        {
        public:
            void OnBodyMoved(
                const BodyMoveEvent&) override
            {
                ++m_bodyMoveCount;
            }

            void OnSensor(
                const SensorEvent& event) override
            {
                ++m_sensorCount;
                m_lastSensor = event;
            }

            void OnContact(
                const ContactEvent& event) override
            {
                ++m_contactCount;
                m_lastContact = event;
            }

            void OnContactHit(
                const ContactHitEvent& event) override
            {
                ++m_contactHitCount;
                m_lastContactHit = event;
            }

            void OnJointThresholdExceeded(
                const JointThresholdEvent& event) override
            {
                ++m_jointThresholdCount;
                m_lastJointThreshold = event;
            }

            size_t m_bodyMoveCount = 0;
            size_t m_sensorCount = 0;
            size_t m_contactCount = 0;
            size_t m_contactHitCount = 0;
            size_t m_jointThresholdCount = 0;
            SensorEvent m_lastSensor;
            ContactEvent m_lastContact;
            ContactHitEvent m_lastContactHit;
            JointThresholdEvent m_lastJointThreshold;
        };
    } // namespace

    TEST(
        Box3DEventTests,
        DestroyedContactShapeRetainsEndEventIdentity)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyA = CreateEventBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        const BodyHandle bodyB = CreateEventBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisZ(0.5f));
        ShapeConfiguration configuration;
        configuration.m_geometry = SphereShapeConfiguration{1.0f};
        const ShapeHandle shapeA = system.CreateShape(worldHandle, bodyA, configuration);
        const ShapeHandle shapeB = system.CreateShape(worldHandle, bodyB, configuration);
        ASSERT_TRUE(shapeA.IsValid());
        ASSERT_TRUE(shapeB.IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        ASSERT_TRUE(system.DestroyShape(worldHandle, shapeB));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const StepEvents events = system.GetStepEvents(worldHandle);
        const auto ended = AZStd::find_if(
            events.m_contactEvents.begin(),
            events.m_contactEvents.end(),
            [bodyA, bodyB, shapeA, shapeB](const ContactEvent& event)
            {
                return event.m_phase == EventPhase::End
                    && event.m_bodyA == bodyA
                    && event.m_bodyB == bodyB
                    && event.m_shapeA == shapeA
                    && event.m_shapeB == shapeB;
            });
        EXPECT_NE(ended, events.m_contactEvents.end());
    }

    TEST(
        Box3DEventTests,
        WorldNotificationBusDispatchesCollectedEventsOncePerStep)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        WorldEventHandler handler;
        handler.BusConnect(worldHandle);

        const BodyHandle staticBody = CreateEventBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        const BodyHandle dynamicBody = CreateEventBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateAxisZ(0.5f));
        ShapeConfiguration configuration;
        configuration.m_geometry = SphereShapeConfiguration{1.0f};
        ASSERT_TRUE(system.CreateShape(worldHandle, staticBody, configuration).IsValid());
        ASSERT_TRUE(system.CreateShape(worldHandle, dynamicBody, configuration).IsValid());

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(handler.m_bodyMoveCount, 0);
        ASSERT_GT(handler.m_contactCount, 0);
        EXPECT_EQ(handler.m_lastContact.m_bodyA, staticBody);
        EXPECT_EQ(handler.m_lastContact.m_bodyB, dynamicBody);

        const size_t contactCount = handler.m_contactCount;
        handler.BusDisconnect();
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_EQ(handler.m_contactCount, contactCount);
    }

    TEST(
        Box3DEventTests,
        WorldNotificationBusDispatchesEveryEventFamily)
    {
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        WorldEventHandler handler;
        handler.BusConnect(worldHandle);

        const BodyHandle sensorBody = CreateEventBody(system, worldHandle, BodyType::Static, -10.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle sensorVisitor = CreateEventBody(system, worldHandle, BodyType::Dynamic, -14.0f * AZ::Vector3::CreateAxisX());
        ShapeConfiguration sensorConfiguration;
        sensorConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
        sensorConfiguration.m_properties.m_isSensor = true;
        sensorConfiguration.m_properties.m_enableSensorEvents = true;
        ShapeConfiguration visitorConfiguration;
        visitorConfiguration.m_geometry = SphereShapeConfiguration{0.5f};
        visitorConfiguration.m_properties.m_enableSensorEvents = true;
        const ShapeHandle sensorShape = system.CreateShape(worldHandle, sensorBody, sensorConfiguration);
        const ShapeHandle visitorShape = system.CreateShape(worldHandle, sensorVisitor, visitorConfiguration);
        ASSERT_TRUE(sensorShape.IsValid());
        ASSERT_TRUE(visitorShape.IsValid());
        ASSERT_TRUE(system.SetLinearVelocity(worldHandle, sensorVisitor, 10.0f * AZ::Vector3::CreateAxisX()));

        const BodyHandle contactBody = CreateEventBody(system, worldHandle, BodyType::Static, 10.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle contactVisitor = CreateEventBody(
            system, worldHandle, BodyType::Dynamic, 10.0f * AZ::Vector3::CreateAxisX() + 5.0f * AZ::Vector3::CreateAxisZ());
        ShapeConfiguration contactConfiguration;
        contactConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
        contactConfiguration.m_properties.m_enableContactEvents = true;
        contactConfiguration.m_properties.m_enableHitEvents = true;
        const ShapeHandle contactShape = system.CreateShape(worldHandle, contactBody, contactConfiguration);
        const ShapeHandle contactVisitorShape = system.CreateShape(worldHandle, contactVisitor, contactConfiguration);
        ASSERT_TRUE(contactShape.IsValid());
        ASSERT_TRUE(contactVisitorShape.IsValid());
        ASSERT_TRUE(system.SetLinearVelocity(worldHandle, contactVisitor, -20.0f * AZ::Vector3::CreateAxisZ()));

        const BodyHandle jointParent = CreateEventBody(system, worldHandle, BodyType::Dynamic, 20.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle jointChild = CreateEventBody(system, worldHandle, BodyType::Dynamic, 22.0f * AZ::Vector3::CreateAxisX());
        DistanceJointConfiguration jointConfiguration;
        jointConfiguration.m_common.m_parentBody = jointParent;
        jointConfiguration.m_common.m_childBody = jointChild;
        jointConfiguration.m_common.m_forceThreshold = 0.0f;
        jointConfiguration.m_length = 1.0f;
        const JointHandle jointHandle = system.CreateJoint(worldHandle, jointConfiguration);
        ASSERT_TRUE(jointHandle.IsValid());
        ASSERT_TRUE(system.ApplyLinearImpulse(worldHandle, jointChild, 100.0f * AZ::Vector3::CreateAxisX()));

        for (AZ::u32 step = 0; step < 120
             && (handler.m_bodyMoveCount == 0
             || handler.m_sensorCount == 0
             || handler.m_contactCount == 0
             || handler.m_contactHitCount == 0
                 || handler.m_jointThresholdCount == 0);
             ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        EXPECT_GT(handler.m_bodyMoveCount, 0);
        EXPECT_GT(handler.m_sensorCount, 0);
        EXPECT_GT(handler.m_contactCount, 0);
        EXPECT_GT(handler.m_contactHitCount, 0);
        EXPECT_GT(handler.m_jointThresholdCount, 0);
        EXPECT_EQ(handler.m_lastSensor.m_sensorBody, sensorBody);
        EXPECT_EQ(handler.m_lastSensor.m_sensorShape, sensorShape);
        EXPECT_EQ(handler.m_lastSensor.m_visitorBody, sensorVisitor);
        EXPECT_EQ(handler.m_lastSensor.m_visitorShape, visitorShape);
        EXPECT_TRUE(
        (handler.m_lastContactHit.m_bodyA == contactBody
            && handler.m_lastContactHit.m_bodyB == contactVisitor)
            || (handler.m_lastContactHit.m_bodyA == contactVisitor
            && handler.m_lastContactHit.m_bodyB == contactBody));
        EXPECT_EQ(handler.m_lastJointThreshold.m_jointHandle, jointHandle);
    }

    TEST(
        Box3DEventTests,
        RuntimeSensorMutationReportsTheNextOverlap)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle sensorBody = CreateEventBody(system, worldHandle, BodyType::Static, 10.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle visitorBody = CreateEventBody(system, worldHandle, BodyType::Dynamic, 6.0f * AZ::Vector3::CreateAxisX());
        ShapeConfiguration configuration;
        configuration.m_geometry = SphereShapeConfiguration{0.5f};
        const ShapeHandle sensorShape = system.CreateShape(worldHandle, sensorBody, configuration);
        const ShapeHandle visitorShape = system.CreateShape(worldHandle, visitorBody, configuration);
        ASSERT_TRUE(sensorShape.IsValid());
        ASSERT_TRUE(visitorShape.IsValid());

        configuration.m_geometry = SphereShapeConfiguration{1.0f};
        configuration.m_properties.m_isSensor = true;
        configuration.m_properties.m_enableSensorEvents = true;
        configuration.m_properties.m_enableContactEvents = false;
        configuration.m_properties.m_enableHitEvents = false;
        ASSERT_TRUE(system.UpdateShape(worldHandle, sensorShape, configuration));
        configuration.m_geometry = SphereShapeConfiguration{0.5f};
        configuration.m_properties.m_isSensor = false;
        ASSERT_TRUE(system.UpdateShape(worldHandle, visitorShape, configuration));
        ASSERT_TRUE(system.SetBodyTransform(worldHandle, visitorBody, AZ::Transform::CreateTranslation(9.0f * AZ::Vector3::CreateAxisX())));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const StepEvents events = system.GetStepEvents(worldHandle);
        const auto sensorEvent = AZStd::find_if(
            events.m_sensorEvents.begin(),
            events.m_sensorEvents.end(),
            [sensorBody, visitorBody, sensorShape, visitorShape](const SensorEvent& event)
            {
                return event.m_phase == EventPhase::Begin
                    && event.m_sensorBody == sensorBody
                    && event.m_visitorBody == visitorBody
                    && event.m_sensorShape == sensorShape
                    && event.m_visitorShape == visitorShape;
            });
        EXPECT_NE(sensorEvent, events.m_sensorEvents.end());
    }

    TEST(
        Box3DEventTests,
        DestroyedSensorBodyCannotBeAttributedToARecycledSlot)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle sensorBody = CreateEventBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        const BodyHandle visitorBody = CreateEventBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        ShapeConfiguration sensorConfiguration;
        sensorConfiguration.m_geometry = SphereShapeConfiguration{1.0f};
        sensorConfiguration.m_properties.m_isSensor = true;
        sensorConfiguration.m_properties.m_enableSensorEvents = true;
        ShapeConfiguration visitorConfiguration;
        visitorConfiguration.m_geometry = SphereShapeConfiguration{0.5f};
        visitorConfiguration.m_properties.m_enableSensorEvents = true;
        const ShapeHandle sensorShape = system.CreateShape(worldHandle, sensorBody, sensorConfiguration);
        const ShapeHandle visitorShape = system.CreateShape(worldHandle, visitorBody, visitorConfiguration);
        ASSERT_TRUE(sensorShape.IsValid());
        ASSERT_TRUE(visitorShape.IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        ASSERT_TRUE(system.DestroyBody(worldHandle, visitorBody));
        const BodyHandle replacementBody = CreateEventBody(system, worldHandle, BodyType::Dynamic, 10.0f * AZ::Vector3::CreateAxisX());
        const ShapeHandle replacementShape = system.CreateShape(worldHandle, replacementBody, visitorConfiguration);
        ASSERT_TRUE(replacementBody.IsValid());
        ASSERT_TRUE(replacementShape.IsValid());
        EXPECT_NE(replacementBody, visitorBody);
        EXPECT_NE(replacementShape, visitorShape);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const StepEvents events = system.GetStepEvents(worldHandle);
        const auto ended = AZStd::find_if(
            events.m_sensorEvents.begin(),
            events.m_sensorEvents.end(),
            [sensorBody, visitorBody, sensorShape, visitorShape](const SensorEvent& event)
            {
                return event.m_phase == EventPhase::End
                    && event.m_sensorBody == sensorBody
                    && event.m_visitorBody == visitorBody
                    && event.m_sensorShape == sensorShape
                    && event.m_visitorShape == visitorShape;
            });
        EXPECT_NE(ended, events.m_sensorEvents.end());
    }

    TEST(
        Box3DEventTests,
        AutomaticCatchUpRetainsEventsFromEveryFixedStep)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.SetWorldGravity(worldHandle, AZ::Vector3::CreateZero()));
        const BodyHandle sensorBody = CreateEventBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        const BodyHandle visitorBody = CreateEventBody(system, worldHandle, BodyType::Dynamic, -1.25f * AZ::Vector3::CreateAxisX());
        ShapeConfiguration sensorConfiguration;
        sensorConfiguration.m_geometry = SphereShapeConfiguration{0.5f};
        sensorConfiguration.m_properties.m_isSensor = true;
        sensorConfiguration.m_properties.m_enableSensorEvents = true;
        ShapeConfiguration visitorConfiguration;
        visitorConfiguration.m_geometry = SphereShapeConfiguration{0.5f};
        visitorConfiguration.m_properties.m_enableSensorEvents = true;
        const ShapeHandle sensorShape = system.CreateShape(worldHandle, sensorBody, sensorConfiguration);
        const ShapeHandle visitorShape = system.CreateShape(worldHandle, visitorBody, visitorConfiguration);
        ASSERT_TRUE(sensorShape.IsValid());
        ASSERT_TRUE(visitorShape.IsValid());
        ASSERT_TRUE(system.SetLinearVelocity(worldHandle, visitorBody, 60.0f * AZ::Vector3::CreateAxisX()));

        system.StepAutoSimulatedWorlds(4.0f / 60.0f);
        const StepEvents events = system.GetStepEvents(worldHandle);
        const bool sawBegin = AZStd::any_of(
            events.m_sensorEvents.begin(),
            events.m_sensorEvents.end(),
            [](const SensorEvent& event)
            {
                return event.m_phase == EventPhase::Begin;
            });
        const bool sawEnd = AZStd::any_of(
            events.m_sensorEvents.begin(),
            events.m_sensorEvents.end(),
            [](const SensorEvent& event)
            {
                return event.m_phase == EventPhase::End;
            });
        EXPECT_TRUE(sawBegin);
        EXPECT_TRUE(sawEnd);
    }

    TEST(
        Box3DEventTests,
        DisabledCollectionAvoidsEveryEventStream)
    {
        System system;
        WorldConfiguration worldConfiguration;
        worldConfiguration.m_name = AZ::Name::FromStringLiteral("No events", nullptr);
        worldConfiguration.m_collectedEventTypes = StepEventTypes::None;
        const WorldHandle worldHandle = system.CreateWorld(worldConfiguration);
        ASSERT_TRUE(worldHandle.IsValid());

        const BodyHandle bodyA = CreateEventBody(system, worldHandle, BodyType::Static, AZ::Vector3::CreateZero());
        const BodyHandle bodyB = CreateEventBody(system, worldHandle, BodyType::Dynamic, AZ::Vector3::CreateZero());
        ShapeConfiguration configuration;
        configuration.m_geometry = SphereShapeConfiguration{1.0f};
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyA, configuration).IsValid());
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyB, configuration).IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const StepEvents events = system.GetStepEvents(worldHandle);
        EXPECT_TRUE(events.m_bodyMoves.empty());
        EXPECT_TRUE(events.m_sensorEvents.empty());
        EXPECT_TRUE(events.m_contactEvents.empty());
        EXPECT_TRUE(events.m_contactPoints.empty());
        EXPECT_TRUE(events.m_contactHits.empty());
        EXPECT_TRUE(events.m_jointThresholds.empty());
    }
} // namespace Box3D::Tests
