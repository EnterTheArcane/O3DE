/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Joints.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/variant.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        BodyHandle CreateDynamicBody(System& system, WorldHandle worldHandle, const AZ::Vector3& position)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = BodyType::Dynamic;
            configuration.m_transform = AZ::Transform::CreateTranslation(position);
            return system.CreateBody(worldHandle, configuration);
        }
    } // namespace

    TEST(Box3DJointTests, ReturnsLatestConfigurationAndTypedMeasurements)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle parentBody = CreateDynamicBody(system, worldHandle, AZ::Vector3::CreateZero());
        const BodyHandle childBody = CreateDynamicBody(system, worldHandle, 2.0f * AZ::Vector3::CreateAxisX());
        ASSERT_TRUE(parentBody.IsValid());
        ASSERT_TRUE(childBody.IsValid());

        DistanceJointConfiguration configuration;
        configuration.m_common.m_parentBody = parentBody;
        configuration.m_common.m_childBody = childBody;
        configuration.m_length = 2.0f;
        configuration.m_minLength = 1.0f;
        configuration.m_maxLength = 3.0f;
        configuration.m_enableLimit = true;
        const JointHandle jointHandle = system.CreateJoint(worldHandle, configuration);
        ASSERT_TRUE(jointHandle.IsValid());

        configuration.m_length = 1.5f;
        configuration.m_hertz = 5.0f;
        configuration.m_dampingRatio = 0.75f;
        configuration.m_enableSpring = true;
        ASSERT_TRUE(system.UpdateJoint(worldHandle, jointHandle, configuration));
        JointConfiguration retainedConfiguration;
        ASSERT_TRUE(system.GetJointConfiguration(worldHandle, jointHandle, retainedConfiguration));
        const auto* retainedDistance = AZStd::get_if<DistanceJointConfiguration>(&retainedConfiguration);
        ASSERT_NE(retainedDistance, nullptr);
        EXPECT_FLOAT_EQ(retainedDistance->m_length, 1.5f);
        EXPECT_FLOAT_EQ(retainedDistance->m_hertz, 5.0f);
        EXPECT_TRUE(retainedDistance->m_enableSpring);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        JointMeasurements measurements;
        ASSERT_TRUE(system.GetJointMeasurements(worldHandle, jointHandle, measurements));
        EXPECT_NE(AZStd::get_if<DistanceJointState>(&measurements.m_state), nullptr);
        EXPECT_TRUE(measurements.m_constraintForce.IsFinite());
        EXPECT_TRUE(measurements.m_constraintTorque.IsFinite());

        ASSERT_TRUE(system.SetBodyAwake(worldHandle, parentBody, false));
        ASSERT_TRUE(system.SetBodyAwake(worldHandle, childBody, false));
        ASSERT_TRUE(system.WakeJointBodies(worldHandle, jointHandle));
        BodyState parentState;
        BodyState childState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, parentBody, parentState));
        ASSERT_TRUE(system.GetBodyState(worldHandle, childBody, childState));
        EXPECT_TRUE(parentState.m_isAwake);
        EXPECT_TRUE(childState.m_isAwake);
    }

    TEST(Box3DJointTests, BodyDestructionInvalidatesAttachedJointHandles)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle parentBody = CreateDynamicBody(system, worldHandle, AZ::Vector3::CreateZero());
        const BodyHandle childBody = CreateDynamicBody(system, worldHandle, AZ::Vector3::CreateAxisX());
        DistanceJointConfiguration configuration;
        configuration.m_common.m_parentBody = parentBody;
        configuration.m_common.m_childBody = childBody;
        const JointHandle staleJoint = system.CreateJoint(worldHandle, configuration);
        ASSERT_TRUE(staleJoint.IsValid());

        ASSERT_TRUE(system.DestroyBody(worldHandle, childBody));
        JointMeasurements measurements;
        EXPECT_FALSE(system.GetJointMeasurements(worldHandle, staleJoint, measurements));

        const BodyHandle replacementChild = CreateDynamicBody(system, worldHandle, AZ::Vector3::CreateAxisX());
        configuration.m_common.m_childBody = replacementChild;
        const JointHandle replacementJoint = system.CreateJoint(worldHandle, configuration);
        ASSERT_TRUE(replacementJoint.IsValid());
        EXPECT_NE(replacementJoint, staleJoint);
    }

    TEST(Box3DJointTests, ThresholdEventsIdentifyTheProviderJoint)
    {
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle parentBody = CreateDynamicBody(system, worldHandle, AZ::Vector3::CreateZero());
        const BodyHandle childBody = CreateDynamicBody(system, worldHandle, 2.0f * AZ::Vector3::CreateAxisX());

        DistanceJointConfiguration configuration;
        configuration.m_common.m_parentBody = parentBody;
        configuration.m_common.m_childBody = childBody;
        configuration.m_common.m_forceThreshold = 0.0f;
        configuration.m_length = 1.0f;
        const JointHandle jointHandle = system.CreateJoint(worldHandle, configuration);
        ASSERT_TRUE(jointHandle.IsValid());
        ASSERT_TRUE(system.ApplyLinearImpulse(worldHandle, childBody, 100.0f * AZ::Vector3::CreateAxisX()));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const StepEvents events = system.GetStepEvents(worldHandle);
        const auto event = AZStd::find_if(
            events.m_jointThresholds.begin(),
            events.m_jointThresholds.end(),
            [jointHandle](const JointThresholdEvent& thresholdEvent)
            {
                return thresholdEvent.m_jointHandle == jointHandle;
            });
        EXPECT_NE(event, events.m_jointThresholds.end());
    }
} // namespace Box3D::Tests
