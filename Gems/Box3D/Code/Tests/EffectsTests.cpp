/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Effects.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Math/Transform.h>
#include <AzCore/std/limits.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        BodyHandle CreateEffectBody(System& system, WorldHandle worldHandle, const AZ::Vector3& position, AZ::u64 categoryBits)
        {
            RigidBodyConfiguration bodyConfiguration;
            bodyConfiguration.m_bodyType = BodyType::Dynamic;
            bodyConfiguration.m_gravityScale = 0.0f;
            bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(position);
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
            shapeConfiguration.m_properties.m_collisionFilter.m_categoryBits = categoryBits;
            return system.CreateShape(worldHandle, bodyHandle, shapeConfiguration).IsValid() ? bodyHandle : BodyHandle{};
        }
    } // namespace

    TEST(Box3DEffectsTests, ExplosionHonorsMaskAndAppliesRadialImpulse)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateEffectBody(system, worldHandle, AZ::Vector3::CreateAxisX(), 2);
        ASSERT_TRUE(bodyHandle.IsValid());
        ExplosionConfiguration configuration;
        configuration.m_radius = 3.0f;
        configuration.m_impulsePerArea = 10.0f;
        configuration.m_maskBits = 1;
        ASSERT_TRUE(system.Explode(worldHandle, configuration));
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_TRUE(state.m_linearVelocity.IsZero());

        configuration.m_maskBits = 2;
        ASSERT_TRUE(system.Explode(worldHandle, configuration));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_GT(state.m_linearVelocity.GetX(), 0.0f);
    }

    TEST(Box3DEffectsTests, WindAppliesAerodynamicForceAndRejectsInvalidInput)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateEffectBody(system, worldHandle, AZ::Vector3::CreateZero(), 1);
        ASSERT_TRUE(bodyHandle.IsValid());
        WindConfiguration configuration;
        configuration.m_velocity = 10.0f * AZ::Vector3::CreateAxisX();
        configuration.m_drag = 2.0f;
        configuration.m_maximumSpeed = 20.0f;
        ASSERT_TRUE(system.ApplyWind(worldHandle, bodyHandle, configuration));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_GT(state.m_linearVelocity.GetX(), 0.0f);

        configuration.m_drag = -1.0f;
        EXPECT_FALSE(system.ApplyWind(worldHandle, bodyHandle, configuration));
        configuration.m_drag = 1.0f;
        configuration.m_velocity.SetX(AZStd::numeric_limits<float>::quiet_NaN());
        EXPECT_FALSE(system.ApplyWind(worldHandle, bodyHandle, configuration));
    }
} // namespace Box3D::Tests
