/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Cooking.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/vector.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        BodyHandle CreateStaticBody(System& system, WorldHandle worldHandle)
        {
            RigidBodyConfiguration configuration;
            configuration.m_bodyType = BodyType::Static;
            return system.CreateBody(worldHandle, configuration);
        }

        AZStd::vector<ShapeConfiguration> CreateCookingConfigurations()
        {
            AZStd::vector<ShapeConfiguration> configurations;
            configurations.resize(8);
            configurations[0].m_geometry = SphereShapeConfiguration{ 0.5f };
            configurations[1].m_geometry = CapsuleShapeConfiguration{ 1.0f, 0.25f };
            configurations[2].m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.5f };
            configurations[3].m_geometry = CylinderShapeConfiguration{ 1.0f, 0.5f, 12 };
            configurations[4].m_geometry = ConvexHullShapeConfiguration{ {
                AZ::Vector3(-0.5f, -0.5f, -0.5f),
                AZ::Vector3(0.5f, -0.5f, -0.5f),
                AZ::Vector3(0.0f, 0.5f, -0.5f),
                AZ::Vector3(0.0f, 0.0f, 0.5f),
            } };
            configurations[5].m_geometry = TriangleMeshShapeConfiguration{
                { AZ::Vector3(-1.0f, -1.0f, 0.0f), AZ::Vector3(1.0f, -1.0f, 0.0f), AZ::Vector3(0.0f, 1.0f, 0.0f) }, { 0, 1, 2 }
            };
            configurations[6].m_geometry = HeightfieldShapeConfiguration{ { 0.0f, 0.1f, 0.2f, 0.3f }, {}, 2, 2, AZ::Vector3::CreateOne() };
            CompoundShapeConfiguration compound;
            compound.m_children.push_back({ SphereShapeConfiguration{ 0.5f } });
            configurations[7].m_geometry = AZStd::move(compound);
            return configurations;
        }
    } // namespace

    TEST(Box3DCookingTests, RegistersProviderOwnedInterfaceAndCooksEveryGeometryFamily)
    {
        EXPECT_EQ(AZ::Interface<ICooking>::Get(), nullptr);
        System system;
        EXPECT_EQ(AZ::Interface<ICooking>::Get(), &system);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateStaticBody(system, worldHandle);
        ASSERT_TRUE(bodyHandle.IsValid());

        for (const ShapeConfiguration& configuration : CreateCookingConfigurations())
        {
            const CookedShapeHandle cookedShapeHandle = system.CookShape(configuration);
            ASSERT_TRUE(cookedShapeHandle.IsValid());
            EXPECT_TRUE(system.IsValid(cookedShapeHandle));
            const ShapeHandle shapeHandle = system.CreateShapeFromCooked(worldHandle, bodyHandle, cookedShapeHandle, ShapeProperties{});
            EXPECT_TRUE(shapeHandle.IsValid());
            EXPECT_TRUE(system.GetShapeAabb(worldHandle, shapeHandle).IsValid());
            EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
            EXPECT_FALSE(system.IsValid(cookedShapeHandle));
            EXPECT_TRUE(system.GetShapeAabb(worldHandle, shapeHandle).IsValid());
            EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        }
    }

    TEST(Box3DCookingTests, RejectsStaleHandlesAndInstanceOverridesOfBakedData)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateStaticBody(system, worldHandle);
        ShapeConfiguration configuration;
        configuration.m_geometry = BoxShapeConfiguration{};
        configuration.m_properties.m_localTransform = AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f));
        const CookedShapeHandle firstHandle = system.CookShape(configuration);
        ASSERT_TRUE(firstHandle.IsValid());
        const AZ::Aabb bounds = system.GetAabb(firstHandle);
        ASSERT_TRUE(bounds.IsValid());
        EXPECT_NEAR(bounds.GetMin().GetX(), 0.5f, 1.0e-3f);
        EXPECT_NEAR(bounds.GetMax().GetX(), 1.5f, 1.0e-3f);
        GeometryHit hit;
        ASSERT_TRUE(system.Raycast(
            firstHandle, -2.0f * AZ::Vector3::CreateAxisX() + AZ::Vector3(0.0f, 2.0f, 3.0f), AZ::Vector3::CreateAxisX(), 4.0f, hit));
        EXPECT_NEAR(hit.m_distance, 2.5f, 1.0e-3f);
        ASSERT_TRUE(system.DestroyCookedShape(firstHandle));
        EXPECT_FALSE(system.GetAabb(firstHandle).IsValid());
        EXPECT_FALSE(system.Raycast(firstHandle, AZ::Vector3::CreateZero(), AZ::Vector3::CreateAxisX(), 1.0f, hit));
        EXPECT_FALSE(system.DestroyCookedShape(firstHandle));
        EXPECT_FALSE(system.CreateShapeFromCooked(worldHandle, bodyHandle, firstHandle, ShapeProperties{}).IsValid());

        const CookedShapeHandle secondHandle = system.CookShape(configuration);
        ASSERT_TRUE(secondHandle.IsValid());
        EXPECT_NE(firstHandle, secondHandle);
        ShapeProperties invalidProperties;
        invalidProperties.m_localTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX());
        EXPECT_FALSE(system.CreateShapeFromCooked(worldHandle, bodyHandle, secondHandle, invalidProperties).IsValid());
        invalidProperties.m_localTransform = AZ::Transform::CreateIdentity();
        invalidProperties.m_materials.push_back(MaterialHandle{});
        EXPECT_FALSE(system.CreateShapeFromCooked(worldHandle, bodyHandle, secondHandle, invalidProperties).IsValid());
    }

    TEST(Box3DCookingTests, KeepsMaterialsStableAcrossCookedAndLiveShapeLifetimes)
    {
        System system;
        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_surfaceTypeId = 42;
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(materialHandle.IsValid());

        ShapeConfiguration configuration;
        CompoundShapeConfiguration compound;
        compound.m_children.push_back({ SphereShapeConfiguration{ 0.5f } });
        configuration.m_geometry = AZStd::move(compound);
        configuration.m_properties.m_materials.push_back(materialHandle);
        const CookedShapeHandle cookedShapeHandle = system.CookShape(configuration);
        ASSERT_TRUE(cookedShapeHandle.IsValid());
        materialConfiguration.m_friction = 0.25f;
        EXPECT_FALSE(system.UpdateMaterial(materialHandle, materialConfiguration));
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateStaticBody(system, worldHandle);
        const ShapeHandle shapeHandle = system.CreateShapeFromCooked(worldHandle, bodyHandle, cookedShapeHandle, ShapeProperties{});
        ASSERT_TRUE(shapeHandle.IsValid());
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_TRUE(system.UpdateMaterial(materialHandle, materialConfiguration));
        ShapeState state;
        ASSERT_TRUE(system.GetShapeState(worldHandle, shapeHandle, state));
        EXPECT_FLOAT_EQ(state.m_friction, materialConfiguration.m_friction);
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.UpdateMaterial(materialHandle, materialConfiguration));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(Box3DCookingTests, UpdatesCompoundFrictionAndRestitutionAcrossEveryMaterialSlot)
    {
        System system;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const BodyHandle bodyHandle = CreateStaticBody(system, worldHandle);

        ShapeConfiguration configuration;
        CompoundShapeConfiguration compound;
        compound.m_children.push_back({ SphereShapeConfiguration{ 0.5f } });
        compound.m_children.push_back(
            { SphereShapeConfiguration{ 0.5f }, AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(2.0f)) });
        configuration.m_geometry = AZStd::move(compound);
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, bodyHandle, configuration);
        ASSERT_TRUE(shapeHandle.IsValid());

        EXPECT_TRUE(system.SetShapeFriction(worldHandle, shapeHandle, 0.125f));
        EXPECT_TRUE(system.SetShapeRestitution(worldHandle, shapeHandle, 0.875f));
        ShapeState state;
        ASSERT_TRUE(system.GetShapeState(worldHandle, shapeHandle, state));
        EXPECT_FLOAT_EQ(state.m_friction, 0.125f);
        EXPECT_FLOAT_EQ(state.m_restitution, 0.875f);

        BodyProperties properties;
        ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, properties));
        properties.m_bodyType = BodyType::Dynamic;
        EXPECT_FALSE(system.SetBodyProperties(worldHandle, bodyHandle, properties));
        ASSERT_TRUE(system.GetBodyProperties(worldHandle, bodyHandle, properties));
        EXPECT_EQ(properties.m_bodyType, BodyType::Static);
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
    }
} // namespace Box3D::Tests
