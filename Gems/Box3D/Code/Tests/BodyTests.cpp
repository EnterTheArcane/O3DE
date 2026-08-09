/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        BodyHandle CreateDynamicSphere(System& system, WorldHandle worldHandle, const AZ::Transform& transform)
        {
            RigidBodyConfiguration bodyConfiguration;
            bodyConfiguration.m_bodyType = BodyType::Dynamic;
            bodyConfiguration.m_transform = transform;
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
            if (!bodyHandle.IsValid())
            {
                return {};
            }

            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
            return system.CreateShape(worldHandle, bodyHandle, shapeConfiguration).IsValid() ? bodyHandle : BodyHandle{};
        }
    } // namespace

    TEST(Box3DBodyTests, ConvertsPointsVectorsAndPointVelocities)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(
            AZ::Quaternion::CreateRotationZ(AZ::Constants::HalfPi), AZ::Vector3(3.0f, 4.0f, 5.0f));
        const BodyHandle bodyHandle = CreateDynamicSphere(system, worldHandle, transform);
        ASSERT_TRUE(bodyHandle.IsValid());
        ASSERT_TRUE(system.SetLinearVelocity(worldHandle, bodyHandle, AZ::Vector3(2.0f, 3.0f, 4.0f)));
        ASSERT_TRUE(system.SetAngularVelocity(worldHandle, bodyHandle, AZ::Vector3(0.0f, 0.0f, 2.0f)));

        const AZ::Vector3 localPoint(0.5f, -0.25f, 0.75f);
        AZ::Vector3 worldPoint;
        ASSERT_TRUE(system.GetBodyWorldPoint(worldHandle, bodyHandle, localPoint, worldPoint));
        AZ::Vector3 roundTripPoint;
        ASSERT_TRUE(system.GetBodyLocalPoint(worldHandle, bodyHandle, worldPoint, roundTripPoint));
        EXPECT_TRUE(roundTripPoint.IsClose(localPoint));

        const AZ::Vector3 localVector(1.0f, 2.0f, 3.0f);
        AZ::Vector3 worldVector;
        ASSERT_TRUE(system.GetBodyWorldVector(worldHandle, bodyHandle, localVector, worldVector));
        AZ::Vector3 roundTripVector;
        ASSERT_TRUE(system.GetBodyLocalVector(worldHandle, bodyHandle, worldVector, roundTripVector));
        EXPECT_TRUE(roundTripVector.IsClose(localVector));

        const AZ::Vector3 localVelocity = system.GetLinearVelocityAtLocalPoint(worldHandle, bodyHandle, localPoint);
        const AZ::Vector3 worldVelocity = system.GetLinearVelocityAtWorldPoint(worldHandle, bodyHandle, worldPoint);
        EXPECT_TRUE(localVelocity.IsClose(worldVelocity));
    }

    TEST(Box3DBodyTests, MutatesPropertiesMassAndWakePolicy)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateDynamicSphere(system, worldHandle, AZ::Transform::CreateIdentity());
        ASSERT_TRUE(bodyHandle.IsValid());
        EXPECT_TRUE(system.SetBodyName(worldHandle, bodyHandle, AZ_NAME_LITERAL("Renamed body")));
        EXPECT_EQ(system.GetBodyName(worldHandle, bodyHandle), AZ_NAME_LITERAL("Renamed body"));

        BodyProperties properties;
        ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, properties));
        properties.m_motionLocks.m_linearZ = true;
        properties.m_linearDamping = 0.25f;
        properties.m_angularDamping = 0.5f;
        properties.m_gravityScale = 0.0f;
        properties.m_sleepThreshold = 0.125f;
        properties.m_isBullet = true;
        properties.m_enableContactRecycling = false;
        ASSERT_TRUE(system.SetBodyProperties(worldHandle, bodyHandle, properties));

        BodyProperties updatedProperties;
        ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, updatedProperties));
        EXPECT_TRUE(updatedProperties.m_motionLocks.m_linearZ);
        EXPECT_FLOAT_EQ(updatedProperties.m_linearDamping, 0.25f);
        EXPECT_FLOAT_EQ(updatedProperties.m_angularDamping, 0.5f);
        EXPECT_FLOAT_EQ(updatedProperties.m_gravityScale, 0.0f);
        EXPECT_FLOAT_EQ(updatedProperties.m_sleepThreshold, 0.125f);
        EXPECT_TRUE(updatedProperties.m_isBullet);
        EXPECT_FALSE(updatedProperties.m_enableContactRecycling);

        MassProperties massProperties;
        ASSERT_TRUE(system.GetMassProperties(worldHandle, bodyHandle, massProperties));
        EXPECT_GT(massProperties.m_mass, 0.0f);
        massProperties.m_mass = 3.0f;
        massProperties.m_center = AZ::Vector3(0.1f, 0.0f, 0.0f);
        massProperties.m_inertia = AZ::Matrix3x3::CreateDiagonal(AZ::Vector3(2.0f, 3.0f, 4.0f));
        ASSERT_TRUE(system.SetMassProperties(worldHandle, bodyHandle, massProperties));
        MassProperties updatedMassProperties;
        ASSERT_TRUE(system.GetMassProperties(worldHandle, bodyHandle, updatedMassProperties));
        EXPECT_FLOAT_EQ(updatedMassProperties.m_mass, 3.0f);
        EXPECT_TRUE(updatedMassProperties.m_center.IsClose(massProperties.m_center));

        ASSERT_TRUE(system.SetBodyAwake(worldHandle, bodyHandle, false));
        ASSERT_TRUE(system.ApplyLinearImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(), false));
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_FALSE(state.m_isAwake);
        ASSERT_TRUE(system.ApplyLinearImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(), true));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_TRUE(state.m_isAwake);

        EXPECT_TRUE(system.RecomputeMassFromShapes(worldHandle, bodyHandle));
        EXPECT_TRUE(system.GetWorldInverseInertia(worldHandle, bodyHandle).IsFinite());
        EXPECT_TRUE(system.GetWorldCenterOfMass(worldHandle, bodyHandle).IsFinite());
    }

    TEST(Box3DBodyTests, AppliesForcesImpulsesTorqueAndKinematicTargets)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle dynamicBody = CreateDynamicSphere(system, worldHandle, AZ::Transform::CreateIdentity());
        ASSERT_TRUE(dynamicBody.IsValid());

        EXPECT_TRUE(system.ApplyForce(worldHandle, dynamicBody, 10.0f * AZ::Vector3::CreateAxisX(), true));
        EXPECT_TRUE(
            system.ApplyForceAtWorldPoint(worldHandle, dynamicBody, 5.0f * AZ::Vector3::CreateAxisY(), AZ::Vector3::CreateAxisZ(), true));
        EXPECT_TRUE(system.ApplyTorque(worldHandle, dynamicBody, 2.0f * AZ::Vector3::CreateAxisZ(), true));
        EXPECT_TRUE(
            system.ApplyLinearImpulseAtWorldPoint(worldHandle, dynamicBody, AZ::Vector3::CreateAxisX(), AZ::Vector3::CreateAxisY()));
        EXPECT_TRUE(system.ApplyAngularImpulse(worldHandle, dynamicBody, AZ::Vector3::CreateAxisZ()));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        BodyState dynamicState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBody, dynamicState));
        EXPECT_GT(dynamicState.m_linearVelocity.GetLengthSq(), 0.0f);
        EXPECT_GT(dynamicState.m_angularVelocity.GetLengthSq(), 0.0f);

        RigidBodyConfiguration kinematicConfiguration;
        kinematicConfiguration.m_bodyType = BodyType::Kinematic;
        const BodyHandle kinematicBody = system.CreateBody(worldHandle, kinematicConfiguration);
        ASSERT_TRUE(kinematicBody.IsValid());
        const AZ::Transform target = AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f));
        EXPECT_TRUE(system.SetKinematicTarget(worldHandle, kinematicBody, target, 1.0f / 60.0f));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        BodyState kinematicState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, kinematicBody, kinematicState));
        EXPECT_TRUE(kinematicState.m_transform.GetTranslation().IsClose(target.GetTranslation(), 1.0e-3f));
    }

    TEST(Box3DBodyTests, DefersMassUpdatesForBulkShapeChanges)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Dynamic;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle.IsValid());

        MassProperties initialMass;
        ASSERT_TRUE(system.GetMassProperties(worldHandle, bodyHandle, initialMass));
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 1.0f };
        shapeConfiguration.m_properties.m_density = 1000.0f;
        shapeConfiguration.m_properties.m_createContactsImmediately = false;
        shapeConfiguration.m_properties.m_updateBodyMass = false;
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle.IsValid());

        MassProperties deferredMass;
        ASSERT_TRUE(system.GetMassProperties(worldHandle, bodyHandle, deferredMass));
        EXPECT_FLOAT_EQ(deferredMass.m_mass, initialMass.m_mass);
        ASSERT_TRUE(system.RecomputeMassFromShapes(worldHandle, bodyHandle));
        MassProperties computedMass;
        ASSERT_TRUE(system.GetMassProperties(worldHandle, bodyHandle, computedMass));
        EXPECT_GT(computedMass.m_mass, deferredMass.m_mass);

        ASSERT_TRUE(system.DestroyShape(worldHandle, shapeHandle, false));
        MassProperties massAfterDeferredDestroy;
        ASSERT_TRUE(system.GetMassProperties(worldHandle, bodyHandle, massAfterDeferredDestroy));
        EXPECT_FLOAT_EQ(massAfterDeferredDestroy.m_mass, computedMass.m_mass);
        EXPECT_TRUE(system.RecomputeMassFromShapes(worldHandle, bodyHandle));
    }

    TEST(Box3DBodyTests, EnumeratesShapesAndJointsWithoutAllocatingOutputContainers)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Dynamic;
        const BodyHandle bodyA = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(2.0f));
        const BodyHandle bodyB = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyA.IsValid());
        ASSERT_TRUE(bodyB.IsValid());

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyA, shapeConfiguration).IsValid());
        shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.25f };
        ASSERT_TRUE(system.CreateShape(worldHandle, bodyA, shapeConfiguration).IsValid());
        DistanceJointConfiguration jointConfiguration;
        jointConfiguration.m_common.m_parentBody = bodyA;
        jointConfiguration.m_common.m_childBody = bodyB;
        const JointHandle jointHandle = system.CreateJoint(worldHandle, jointConfiguration);
        ASSERT_TRUE(jointHandle.IsValid());

        AZStd::array<ShapeHandle, 1> shapes;
        const BufferResult shapeResult = system.GetBodyShapes(worldHandle, bodyA, shapes);
        EXPECT_EQ(shapeResult.m_count, 1);
        EXPECT_EQ(shapeResult.m_requiredCount, 2);
        EXPECT_TRUE(shapeResult.HasOverflow());
        EXPECT_TRUE(shapes.front().IsValid());

        AZStd::array<JointHandle, 1> joints;
        const BufferResult jointResult = system.GetBodyJoints(worldHandle, bodyA, joints);
        EXPECT_EQ(jointResult.m_count, 1);
        EXPECT_EQ(jointResult.m_requiredCount, 1);
        EXPECT_FALSE(jointResult.HasOverflow());
        EXPECT_EQ(joints.front(), jointHandle);
    }

    TEST(Box3DBodyTests, ReturnsCurrentBodyAndShapeContactsWithExplicitBufferSizing)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.SetWorldGravity(worldHandle, AZ::Vector3::CreateZero()));
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        const BodyHandle bodyA = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_bodyType = BodyType::Dynamic;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(0.5f));
        const BodyHandle bodyB = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 1.0f };
        const ShapeHandle shapeA = system.CreateShape(worldHandle, bodyA, shapeConfiguration);
        const ShapeHandle shapeB = system.CreateShape(worldHandle, bodyB, shapeConfiguration);
        ASSERT_TRUE(shapeA.IsValid());
        ASSERT_TRUE(shapeB.IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const ContactSnapshotResult required = system.GetBodyContacts(worldHandle, bodyB, {}, {});
        ASSERT_GT(required.m_contacts.m_requiredCount, 0);
        ASSERT_GT(required.m_points.m_requiredCount, 0);
        EXPECT_TRUE(required.m_contacts.HasOverflow());
        EXPECT_TRUE(required.m_points.HasOverflow());

        AZStd::array<ContactSnapshot, 4> contacts;
        AZStd::array<ContactPoint, 16> points;
        const ContactSnapshotResult bodyContacts = system.GetBodyContacts(worldHandle, bodyB, contacts, points);
        ASSERT_GT(bodyContacts.m_contacts.m_count, 0);
        EXPECT_FALSE(bodyContacts.m_contacts.HasOverflow());
        EXPECT_FALSE(bodyContacts.m_points.HasOverflow());
        const ContactSnapshot& contact = contacts.front();
        EXPECT_TRUE(contact.m_bodyA == bodyA || contact.m_bodyB == bodyA);
        EXPECT_TRUE(contact.m_bodyA == bodyB || contact.m_bodyB == bodyB);
        EXPECT_EQ(contact.m_pointCount, contact.m_requiredPointCount);

        const ContactSnapshotResult shapeContacts = system.GetShapeContacts(worldHandle, shapeB, contacts, points);
        EXPECT_EQ(shapeContacts.m_contacts.m_requiredCount, bodyContacts.m_contacts.m_requiredCount);
    }

    TEST(Box3DBodyTests, RejectsUnsupportedBodyTypeChangesWithoutLockingTheWorld)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;

        const BodyHandle compoundBody = system.CreateBody(worldHandle, bodyConfiguration);
        CompoundShapeConfiguration compound;
        compound.m_children.push_back({ SphereShapeConfiguration{}, AZ::Transform::CreateIdentity(), 0 });
        ShapeConfiguration compoundShape;
        compoundShape.m_geometry = AZStd::move(compound);
        ASSERT_TRUE(system.CreateShape(worldHandle, compoundBody, compoundShape).IsValid());

        const BodyHandle heightfieldBody = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration heightfieldShape;
        heightfieldShape.m_geometry =
            HeightfieldShapeConfiguration{ { 0.0f, 0.0f, 0.0f, 0.0f }, {}, 2, 2, AZ::Vector2::CreateOne(), 1.0f, false };
        ASSERT_TRUE(system.CreateShape(worldHandle, heightfieldBody, heightfieldShape).IsValid());

        for (BodyHandle bodyHandle : AZStd::array{ compoundBody, heightfieldBody })
        {
            BodyProperties properties;
            ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, properties));
            properties.m_bodyType = BodyType::Dynamic;
            EXPECT_FALSE(system.SetBodyProperties(worldHandle, bodyHandle, properties));

            ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, properties));
            EXPECT_EQ(properties.m_bodyType, BodyType::Static);
        }
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
    }

    TEST(Box3DBodyTests, ReturnsCurrentSensorOverlapsForBodiesAndShapes)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.SetWorldGravity(worldHandle, AZ::Vector3::CreateZero()));
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        const BodyHandle sensorBody = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_bodyType = BodyType::Dynamic;
        const BodyHandle visitorBody = system.CreateBody(worldHandle, bodyConfiguration);
        ShapeConfiguration sensorConfiguration;
        sensorConfiguration.m_geometry = SphereShapeConfiguration{ 1.0f };
        sensorConfiguration.m_properties.m_isSensor = true;
        sensorConfiguration.m_properties.m_enableSensorEvents = true;
        const ShapeHandle sensorShape = system.CreateShape(worldHandle, sensorBody, sensorConfiguration);
        ShapeConfiguration visitorConfiguration;
        visitorConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
        visitorConfiguration.m_properties.m_enableSensorEvents = true;
        const ShapeHandle visitorShape = system.CreateShape(worldHandle, visitorBody, visitorConfiguration);
        ASSERT_TRUE(sensorShape.IsValid());
        ASSERT_TRUE(visitorShape.IsValid());
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        AZStd::array<SensorOverlap, 4> overlaps;
        const BufferResult bodyResult = system.GetBodySensorOverlaps(worldHandle, sensorBody, overlaps);
        ASSERT_EQ(bodyResult.m_count, 1);
        EXPECT_EQ(bodyResult.m_requiredCount, 1);
        EXPECT_EQ(overlaps.front().m_sensorBody, sensorBody);
        EXPECT_EQ(overlaps.front().m_visitorBody, visitorBody);
        EXPECT_EQ(overlaps.front().m_sensorShape, sensorShape);
        EXPECT_EQ(overlaps.front().m_visitorShape, visitorShape);

        const BufferResult shapeResult = system.GetShapeSensorOverlaps(worldHandle, sensorShape, overlaps);
        EXPECT_EQ(shapeResult.m_count, 1);
        EXPECT_EQ(shapeResult.m_requiredCount, 1);
    }

    TEST(Box3DBodyTests, InspectsAndMutatesShapeStateWithoutRecookingGeometry)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_bodyType = BodyType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_friction = 0.2f;
        materialConfiguration.m_restitution = 0.1f;
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(materialHandle.IsValid());
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.5f };
        shapeConfiguration.m_properties.m_materials.push_back(materialHandle);
        shapeConfiguration.m_properties.m_collisionFilter = { 4, 8 };
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle.IsValid());

        ShapeState state;
        ASSERT_TRUE(system.GetShapeState(worldHandle, shapeHandle, state));
        EXPECT_EQ(state.m_bodyHandle, bodyHandle);
        EXPECT_EQ(state.m_type, ShapeType::Hull);
        EXPECT_EQ(state.m_collisionFilter.m_categoryBits, 4);
        EXPECT_EQ(state.m_collisionFilter.m_maskBits, 8);
        EXPECT_FLOAT_EQ(state.m_friction, 0.2f);
        EXPECT_FLOAT_EQ(state.m_restitution, 0.1f);

        AZStd::array<MaterialHandle, 1> materials;
        const BufferResult materialResult = system.GetShapeMaterials(worldHandle, shapeHandle, materials);
        EXPECT_EQ(materialResult.m_count, 1);
        EXPECT_EQ(materialResult.m_requiredCount, 1);
        EXPECT_EQ(materials.front(), materialHandle);

        EXPECT_TRUE(system.SetShapeDensity(worldHandle, shapeHandle, 2.0f));
        EXPECT_TRUE(system.SetShapeFriction(worldHandle, shapeHandle, 0.25f));
        EXPECT_TRUE(system.SetShapeRestitution(worldHandle, shapeHandle, 0.75f));
        EXPECT_TRUE(system.SetShapeEventSubscriptions(worldHandle, shapeHandle, true, false, false, true));
        EXPECT_FALSE(system.SetShapeDensity(worldHandle, shapeHandle, -1.0f));
        ASSERT_TRUE(system.GetShapeState(worldHandle, shapeHandle, state));
        EXPECT_FLOAT_EQ(state.m_density, 2.0f);
        EXPECT_FLOAT_EQ(state.m_friction, 0.25f);
        EXPECT_FLOAT_EQ(state.m_restitution, 0.75f);
        EXPECT_TRUE(state.m_enableSensorEvents);
        EXPECT_FALSE(state.m_enableContactEvents);
        EXPECT_FALSE(state.m_enableHitEvents);
        EXPECT_TRUE(state.m_enablePreSolveEvents);
        EXPECT_TRUE(system.SetBodyHitEventsEnabled(worldHandle, bodyHandle, true));
        ASSERT_TRUE(system.GetShapeState(worldHandle, shapeHandle, state));
        EXPECT_TRUE(state.m_enableHitEvents);

        MassProperties massProperties;
        ASSERT_TRUE(system.GetShapeMassProperties(worldHandle, shapeHandle, massProperties));
        EXPECT_GT(massProperties.m_mass, 0.0f);
        AZ::Vector3 closestPoint;
        float closestDistance = 0.0f;
        ASSERT_TRUE(system.GetShapeClosestPoint(worldHandle, shapeHandle, AZ::Vector3::CreateAxisX(2.0f), closestPoint, closestDistance));
        EXPECT_NEAR(closestDistance, 1.5f, 1.0e-3f);

        QueryHit hit;
        ASSERT_TRUE(
            system.RaycastShape(worldHandle, shapeHandle, -2.0f * AZ::Vector3::CreateAxisX(), AZ::Vector3::CreateAxisX(), 4.0f, hit));
        EXPECT_EQ(hit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(hit.m_shapeHandle, shapeHandle);
        EXPECT_EQ(hit.m_materialHandle, materialHandle);
        EXPECT_NEAR(hit.m_distance, 1.5f, 1.0e-3f);
    }

    TEST(Box3DBodyTests, ReportsEntityNameAndCreationOnlyPolicy)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RigidBodyConfiguration configuration;
        configuration.m_entityId = AZ::EntityId(42);
        configuration.m_name = AZ_NAME_LITERAL("RobotArm");
        configuration.m_allowFastRotation = true;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, configuration);
        ASSERT_TRUE(bodyHandle.IsValid());

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_EQ(state.m_entityId, configuration.m_entityId);
        EXPECT_EQ(state.m_name.GetStringView(), configuration.m_name.GetStringView());
        EXPECT_TRUE(state.m_allowFastRotation);
    }
} // namespace Box3D::Tests
