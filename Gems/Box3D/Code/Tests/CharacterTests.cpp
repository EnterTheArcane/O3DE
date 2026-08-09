/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/CharacterConfiguration.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        BodyHandle CreateBody(System& system, WorldHandle worldHandle, BodyType bodyType, const AZ::Transform& transform)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = bodyType;
            configuration.m_transform = transform;
            return system.CreateBody(worldHandle, configuration);
        }

        ShapeHandle CreateBox(System& system, WorldHandle worldHandle, BodyHandle bodyHandle, const AZ::Vector3& halfExtents)
        {
            ShapeConfiguration configuration;
            configuration.m_geometry = BoxShapeConfiguration{ halfExtents };
            return system.CreateShape(worldHandle, bodyHandle, configuration);
        }

        void CreateGround(System& system, WorldHandle worldHandle)
        {
            const BodyHandle ground =
                CreateBody(system, worldHandle, BodyType::Static, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(-0.5f)));
            ASSERT_TRUE(CreateBox(system, worldHandle, ground, AZ::Vector3(5.0f, 5.0f, 0.5f)).IsValid());
        }

        CharacterState MoveOverStep(float stepHeight)
        {
            System system;
            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            CreateGround(system, worldHandle);
            const BodyHandle step =
                CreateBody(system, worldHandle, BodyType::Static, AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 0.0f, 0.15f)));
            EXPECT_TRUE(CreateBox(system, worldHandle, step, AZ::Vector3(0.25f, 2.0f, 0.15f)).IsValid());

            CharacterConfiguration configuration;
            configuration.m_basePosition = AZ::Vector3(-0.8f, 0.0f, 0.0f);
            configuration.m_height = 1.0f;
            configuration.m_radius = 0.2f;
            configuration.m_stepHeight = stepHeight;
            configuration.m_groundStickDistance = 0.1f;
            configuration.m_applyMoveOnFixedTick = false;
            const CharacterHandle character = system.CreateCharacter(worldHandle, configuration);
            EXPECT_TRUE(character.IsValid());
            EXPECT_TRUE(system.MoveCharacter(worldHandle, character, 2.0f * AZ::Vector3::CreateAxisX(), 0.5f));
            CharacterState state;
            EXPECT_TRUE(system.GetCharacterState(worldHandle, character, state));
            return state;
        }
    } // namespace

    TEST(Box3DCharacterTests, GroundStickSnapsToWalkableSupport)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        CreateGround(system, worldHandle);

        CharacterConfiguration configuration;
        configuration.m_basePosition = AZ::Vector3::CreateAxisZ(0.1f);
        configuration.m_groundStickDistance = 0.2f;
        configuration.m_applyMoveOnFixedTick = false;
        const CharacterHandle character = system.CreateCharacter(worldHandle, configuration);
        ASSERT_TRUE(character.IsValid());
        ASSERT_TRUE(system.MoveCharacter(worldHandle, character, AZ::Vector3::CreateAxisX(), 0.1f));

        CharacterState state;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, character, state));
        EXPECT_NEAR(state.m_basePosition.GetZ(), 0.0f, 0.01f);
        EXPECT_EQ(state.m_support.m_state, CharacterSupportState::Supported);
        EXPECT_TRUE(state.m_support.m_bodyHandle.IsValid());
    }

    TEST(Box3DCharacterTests, StepHeightControlsObstacleTraversal)
    {
        const CharacterState blocked = MoveOverStep(0.0f);
        const CharacterState stepped = MoveOverStep(0.4f);
        EXPECT_LT(blocked.m_basePosition.GetX(), -0.4f);
        EXPECT_GT(stepped.m_basePosition.GetX(), 0.1f);
        EXPECT_GT(stepped.m_basePosition.GetZ(), 0.2f);
        EXPECT_EQ(stepped.m_support.m_state, CharacterSupportState::Supported);
    }

    TEST(Box3DCharacterTests, SteepSurfaceReportsSlidingSupport)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const AZ::Transform slopeTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
            AZ::Quaternion::CreateRotationY(AZ::DegToRad(45.0f)), AZ::Vector3::CreateAxisZ(-0.1f));
        const BodyHandle slope = CreateBody(system, worldHandle, BodyType::Static, slopeTransform);
        ASSERT_TRUE(CreateBox(system, worldHandle, slope, AZ::Vector3(2.0f, 2.0f, 0.1f)).IsValid());

        CharacterConfiguration configuration;
        configuration.m_basePosition = AZ::Vector3::CreateAxisZ(1.0f);
        configuration.m_maximumSlopeAngle = 30.0f;
        configuration.m_groundStickDistance = 0.0f;
        configuration.m_applyMoveOnFixedTick = false;
        const CharacterHandle character = system.CreateCharacter(worldHandle, configuration);
        ASSERT_TRUE(character.IsValid());
        ASSERT_TRUE(system.MoveCharacter(worldHandle, character, -2.0f * AZ::Vector3::CreateAxisZ(), 1.0f));

        CharacterState state;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, character, state));
        EXPECT_EQ(state.m_support.m_state, CharacterSupportState::Sliding);
    }

    TEST(Box3DCharacterTests, InteractionScalePushesDynamicBodies)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle pushedBody =
            CreateBody(system, worldHandle, BodyType::Dynamic, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(0.75f)));
        ASSERT_TRUE(CreateBox(system, worldHandle, pushedBody, AZ::Vector3::CreateOne() * 0.25f).IsValid());

        CharacterConfiguration configuration;
        configuration.m_interactionScale = 1.0f;
        configuration.m_groundStickDistance = 0.0f;
        configuration.m_stepHeight = 0.0f;
        configuration.m_applyMoveOnFixedTick = false;
        const CharacterHandle character = system.CreateCharacter(worldHandle, configuration);
        ASSERT_TRUE(character.IsValid());
        ASSERT_TRUE(system.MoveCharacter(worldHandle, character, 2.0f * AZ::Vector3::CreateAxisX(), 0.5f));

        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, pushedBody, bodyState));
        EXPECT_GT(bodyState.m_linearVelocity.GetX(), 0.0f);
    }

    TEST(Box3DCharacterTests, ReturnsRetainedProviderConfiguration)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        CharacterConfiguration configuration;
        configuration.m_entityId = AZ::EntityId(73);
        configuration.m_basePosition = AZ::Vector3(1.0f, 2.0f, 3.0f);
        configuration.m_maximumSpeed = 12.0f;
        const CharacterHandle characterHandle = system.CreateCharacter(worldHandle, configuration);
        ASSERT_TRUE(characterHandle.IsValid());

        CharacterConfiguration retainedConfiguration;
        ASSERT_TRUE(system.GetCharacterConfiguration(worldHandle, characterHandle, retainedConfiguration));
        EXPECT_EQ(retainedConfiguration.m_entityId, configuration.m_entityId);
        EXPECT_TRUE(retainedConfiguration.m_basePosition.IsClose(configuration.m_basePosition));
        EXPECT_FLOAT_EQ(retainedConfiguration.m_maximumSpeed, configuration.m_maximumSpeed);
    }

    TEST(Box3DCharacterTests, RecycledSlotsRejectStaleHandlesAndResetColdState)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        CharacterConfiguration firstConfiguration;
        firstConfiguration.m_basePosition = AZ::Vector3(1.0f, 2.0f, 3.0f);
        firstConfiguration.m_maximumSpeed = 12.0f;
        const CharacterHandle staleHandle = system.CreateCharacter(worldHandle, firstConfiguration);
        ASSERT_TRUE(staleHandle.IsValid());
        ASSERT_TRUE(system.DestroyCharacter(worldHandle, staleHandle));

        CharacterConfiguration replacementConfiguration;
        replacementConfiguration.m_basePosition = AZ::Vector3(-4.0f, -5.0f, -6.0f);
        replacementConfiguration.m_maximumSpeed = 3.0f;
        const CharacterHandle replacementHandle = system.CreateCharacter(worldHandle, replacementConfiguration);
        ASSERT_TRUE(replacementHandle.IsValid());
        EXPECT_NE(replacementHandle, staleHandle);

        CharacterState state;
        CharacterConfiguration retainedConfiguration;
        EXPECT_FALSE(system.GetCharacterState(worldHandle, staleHandle, state));
        EXPECT_FALSE(system.GetCharacterConfiguration(worldHandle, staleHandle, retainedConfiguration));
        ASSERT_TRUE(system.GetCharacterConfiguration(worldHandle, replacementHandle, retainedConfiguration));
        EXPECT_TRUE(retainedConfiguration.m_basePosition.IsClose(replacementConfiguration.m_basePosition));
        EXPECT_FLOAT_EQ(retainedConfiguration.m_maximumSpeed, replacementConfiguration.m_maximumSpeed);
    }
} // namespace Box3D::Tests
