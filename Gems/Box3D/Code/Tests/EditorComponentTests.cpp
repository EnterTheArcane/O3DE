/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/Component/Entity.h>
#include <AzFramework/UnitTest/TestDebugDisplayRequests.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Manipulators/BoxManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CapsuleManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/RadiusManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/ShapeManipulatorRequestBus.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#include <Box3D/CharacterControllerComponent.h>
#include <Box3D/ColliderComponent.h>
#include <Box3D/Editor/CharacterControllerComponent.h>
#include <Box3D/Editor/ColliderComponent.h>
#include <Box3D/Editor/EffectComponents.h>
#include <Box3D/Editor/HeightfieldColliderComponent.h>
#include <Box3D/Editor/JointComponents.h>
#include <Box3D/Editor/RigidBodyComponent.h>
#include <Box3D/Editor/StaticRigidBodyComponent.h>
#include <Box3D/EffectComponents.h>
#include <Box3D/HeightfieldColliderComponent.h>
#include <Box3D/JointComponent.h>
#include <Box3D/RigidBodyComponent.h>
#include <Box3D/StaticRigidBodyComponent.h>

AZ_TOOLS_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace Box3D::Editor
{
    namespace
    {
        class ComponentModeTests
            : public UnitTest::ToolsApplicationFixture<false>
        {
        protected:
            void SetUpEditorFixtureImpl() override
            {
                ReflectJoints(GetApplication()->GetSerializeContext());
                GetApplication()->RegisterComponentDescriptor(ColliderComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(CharacterControllerComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(DistanceJointComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(ExplosionComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(RigidBodyComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(WindComponent::CreateDescriptor());
                m_viewportManager.Create();
            }

            void TearDownEditorFixtureImpl() override
            {
                m_viewportManager.Destroy();
            }

            UnitTest::ViewportManagerWrapper m_viewportManager;
        };

        class ViewportDebugDisplay final
            : public UnitTest::TestDebugDisplayRequests
        {
        public:
            void DrawWireCapsule(const AZ::Vector3& center, const AZ::Vector3& axis, float radius, float heightStraightSection) override
            {
                ++m_capsuleCount;
                m_center = center;
                m_axis = axis;
                m_radius = radius;
                m_heightStraightSection = heightStraightSection;
            }

            AZ::u32 m_capsuleCount = 0;
            AZ::Vector3 m_center = AZ::Vector3::CreateZero();
            AZ::Vector3 m_axis = AZ::Vector3::CreateZero();
            float m_radius = 0.0f;
            float m_heightStraightSection = 0.0f;
        };
    } // namespace

    TEST(EditorComponentTests, BuildGameEntityEmitsRuntimeCollider)
    {
        ColliderComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        Box3D::ColliderComponent* collider = gameEntity.FindComponent<Box3D::ColliderComponent>();
        ASSERT_NE(collider, nullptr);
        const AZStd::span<const ShapeConfiguration> configurations = collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        EXPECT_TRUE(AZStd::holds_alternative<BoxShapeConfiguration>(configurations.front().m_geometry));
    }

    TEST(EditorComponentTests, ColliderDrawsAuthoredGeometryInTheViewport)
    {
        ColliderComponent editorComponent;
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({ 0 }, debugDisplay);

        const AZ::Aabb bounds = debugDisplay.GetAabb();
        EXPECT_TRUE(bounds.IsValid());
        EXPECT_TRUE(bounds.GetMin().IsClose(-0.5f * AZ::Vector3::CreateOne()));
        EXPECT_TRUE(bounds.GetMax().IsClose(0.5f * AZ::Vector3::CreateOne()));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({ 0 });
        float distance = 0.0f;
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(bounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(bounds.GetMax()));
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            { 0 }, AZ::Vector3::CreateAxisX(-2.0f), AZ::Vector3::CreateAxisX(), distance));
        EXPECT_NEAR(distance, 1.5f, 1.0e-4f);
    }

    TEST_F(ComponentModeTests, ColliderManipulatorBusesEditTheActiveShape)
    {
        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Box3D collider", &editorEntity);
        ASSERT_NE(editorEntity, nullptr);
        editorEntity->Deactivate();
        ColliderComponent* editorComponent = editorEntity->CreateComponent<ColliderComponent>();
        ASSERT_NE(editorComponent, nullptr);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        AZ::Vector3 dimensions = AZ::Vector3::CreateZero();
        AzToolsFramework::BoxManipulatorRequestBus::EventResult(
            dimensions, pair, &AzToolsFramework::BoxManipulatorRequestBus::Events::GetDimensions);
        EXPECT_TRUE(dimensions.IsClose(AZ::Vector3::CreateOne()));

        const AZ::Vector3 expectedDimensions(2.0f, 3.0f, 4.0f);
        const AZ::Vector3 expectedOffset(5.0f, 6.0f, 7.0f);
        AzToolsFramework::BoxManipulatorRequestBus::Event(
            pair, &AzToolsFramework::BoxManipulatorRequestBus::Events::SetDimensions, expectedDimensions);
        AzToolsFramework::ShapeManipulatorRequestBus::Event(
            pair, &AzToolsFramework::ShapeManipulatorRequestBus::Events::SetTranslationOffset, expectedOffset);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Box3D::ColliderComponent* collider = gameEntity.FindComponent<Box3D::ColliderComponent>();
        ASSERT_NE(collider, nullptr);
        const AZStd::span<const ShapeConfiguration> configurations = collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        const auto* box = AZStd::get_if<BoxShapeConfiguration>(&configurations.front().m_geometry);
        ASSERT_NE(box, nullptr);
        EXPECT_TRUE((2.0f * box->m_halfExtents).IsClose(expectedDimensions));
        EXPECT_TRUE(configurations.front().m_properties.m_localTransform.GetTranslation().IsClose(expectedOffset));

        AzToolsFramework::SelectEntity(editorEntity->GetId());
        UnitTest::EnterComponentMode<ColliderComponent>();
        bool componentModeInstantiated = false;
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::BroadcastResult(
            componentModeInstantiated,
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::ComponentModeInstantiated,
            pair);
        EXPECT_TRUE(componentModeInstantiated);
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::EndComponentMode);
    }

    TEST(EditorComponentTests, BuildGameEntityEmitsRuntimeCharacterController)
    {
        CharacterControllerComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_NE(gameEntity.FindComponent<Box3D::CharacterControllerComponent>(), nullptr);
    }

    TEST(EditorComponentTests, CharacterControllerDrawsAuthoredCapsuleInTheViewport)
    {
        CharacterControllerComponent editorComponent;
        ViewportDebugDisplay debugDisplay;

        editorComponent.DisplayEntityViewport({ 0 }, debugDisplay);

        EXPECT_EQ(debugDisplay.m_capsuleCount, 1);
        EXPECT_TRUE(debugDisplay.m_center.IsClose(AZ::Vector3::CreateAxisZ(0.5f)));
        EXPECT_TRUE(debugDisplay.m_axis.IsClose(AZ::Vector3::CreateAxisZ()));
        EXPECT_FLOAT_EQ(debugDisplay.m_radius, 0.25f);
        EXPECT_FLOAT_EQ(debugDisplay.m_heightStraightSection, 0.5f);

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({ 0 });
        float distance = 0.0f;
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(AZ::Vector3(-0.25f, -0.25f, 0.0f)));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(AZ::Vector3(0.25f, 0.25f, 1.0f)));
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            { 0 }, AZ::Vector3::CreateAxisZ(-1.0f), AZ::Vector3::CreateAxisZ(), distance));
        EXPECT_NEAR(distance, 1.0f, 1.0e-4f);
    }

    TEST_F(ComponentModeTests, CharacterManipulatorBusesPreserveValidCapsuleDimensions)
    {
        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Box3D character", &editorEntity);
        ASSERT_NE(editorEntity, nullptr);
        editorEntity->Deactivate();
        CharacterControllerComponent* editorComponent = editorEntity->CreateComponent<CharacterControllerComponent>();
        ASSERT_NE(editorComponent, nullptr);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        AzToolsFramework::RadiusManipulatorRequestBus::Event(
            pair, &AzToolsFramework::RadiusManipulatorRequestBus::Events::SetRadius, 0.75f);
        float height = 0.0f;
        AzToolsFramework::CapsuleManipulatorRequestBus::EventResult(
            height, pair, &AzToolsFramework::CapsuleManipulatorRequestBus::Events::GetHeight);
        EXPECT_FLOAT_EQ(height, 1.5f);
        AzToolsFramework::CapsuleManipulatorRequestBus::Event(
            pair, &AzToolsFramework::CapsuleManipulatorRequestBus::Events::SetHeight, 2.0f);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Box3D::CharacterControllerComponent* character = gameEntity.FindComponent<Box3D::CharacterControllerComponent>();
        ASSERT_NE(character, nullptr);
        EXPECT_FLOAT_EQ(character->GetConfiguration().m_radius, 0.75f);
        EXPECT_FLOAT_EQ(character->GetConfiguration().m_height, 2.0f);

        AzToolsFramework::SelectEntity(editorEntity->GetId());
        UnitTest::EnterComponentMode<CharacterControllerComponent>();
        bool componentModeInstantiated = false;
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::BroadcastResult(
            componentModeInstantiated,
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::ComponentModeInstantiated,
            pair);
        EXPECT_TRUE(componentModeInstantiated);
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::EndComponentMode);
    }

    TEST(EditorComponentTests, BuildGameEntityEmitsRuntimeHeightfieldCollider)
    {
        HeightfieldColliderComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_NE(gameEntity.FindComponent<Box3D::HeightfieldColliderComponent>(), nullptr);
    }

    TEST(EditorComponentTests, HeightfieldDrawsAuthoredGridInTheViewport)
    {
        HeightfieldColliderComponent editorComponent;
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({ 0 }, debugDisplay);

        const AZ::Aabb bounds = debugDisplay.GetAabb();
        EXPECT_TRUE(bounds.IsValid());
        EXPECT_TRUE(bounds.GetMin().IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(bounds.GetMax().IsClose(AZ::Vector3(1.0f, 1.0f, 0.0f)));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({ 0 });
        float distance = 0.0f;
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(bounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(bounds.GetMax()));
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            { 0 }, AZ::Vector3(0.5f, 0.5f, 2.0f), -AZ::Vector3::CreateAxisZ(), distance));
        EXPECT_NEAR(distance, 2.0f, 1.0e-4f);
    }

    TEST(EditorComponentTests, BuildGameEntityEmitsRuntimeRigidBody)
    {
        RigidBodyComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_NE(gameEntity.FindComponent<Box3D::RigidBodyComponent>(), nullptr);
    }

    TEST(EditorComponentTests, BuildGameEntityEmitsRuntimeStaticRigidBody)
    {
        StaticRigidBodyComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_NE(gameEntity.FindComponent<Box3D::StaticRigidBodyComponent>(), nullptr);
    }

    TEST(EditorComponentTests, BuildGameEntityEmitsRuntimeEffects)
    {
        ExplosionComponent editorExplosion;
        WindComponent editorWind;
        AZ::Entity gameEntity;

        editorExplosion.BuildGameEntity(&gameEntity);
        editorWind.BuildGameEntity(&gameEntity);

        EXPECT_NE(gameEntity.FindComponent<Box3D::ExplosionComponent>(), nullptr);
        EXPECT_NE(gameEntity.FindComponent<Box3D::WindComponent>(), nullptr);
    }

    TEST(EditorComponentTests, EffectsDrawAuthoredInfluenceInTheViewport)
    {
        ExplosionComponent editorExplosion;
        WindComponent editorWind;
        UnitTest::TestDebugDisplayRequests explosionDisplay;
        UnitTest::TestDebugDisplayRequests windDisplay;

        editorExplosion.DisplayEntityViewport({ 0 }, explosionDisplay);
        editorWind.DisplayEntityViewport({ 0 }, windDisplay);

        const AZ::Aabb explosionBounds = explosionDisplay.GetAabb();
        EXPECT_TRUE(explosionBounds.IsValid());
        EXPECT_TRUE(explosionBounds.GetMin().IsClose(-AZ::Vector3::CreateOne()));
        EXPECT_TRUE(explosionBounds.GetMax().IsClose(AZ::Vector3::CreateOne()));
        EXPECT_TRUE(windDisplay.GetAabb().IsValid());

        float distance = 0.0f;
        const AZ::Aabb explosionSelectionBounds = editorExplosion.GetEditorSelectionBoundsViewport({ 0 });
        EXPECT_TRUE(explosionSelectionBounds.GetMin().IsClose(explosionBounds.GetMin()));
        EXPECT_TRUE(explosionSelectionBounds.GetMax().IsClose(explosionBounds.GetMax()));
        EXPECT_TRUE(editorExplosion.EditorSelectionIntersectRayViewport(
            { 0 }, AZ::Vector3::CreateAxisX(-2.0f), AZ::Vector3::CreateAxisX(), distance));
        EXPECT_NEAR(distance, 1.0f, 1.0e-4f);
        EXPECT_TRUE(editorWind.GetEditorSelectionBoundsViewport({ 0 }).IsValid());
    }

    TEST_F(ComponentModeTests, ExplosionManipulatorBusesEditLocalPositionAndRadius)
    {
        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Box3D explosion", &editorEntity);
        ASSERT_NE(editorEntity, nullptr);
        editorEntity->Deactivate();
        ExplosionComponent* editorComponent = editorEntity->CreateComponent<ExplosionComponent>();
        ASSERT_NE(editorComponent, nullptr);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        const AZ::Vector3 expectedPosition(1.0f, 2.0f, 3.0f);
        AzToolsFramework::RadiusManipulatorRequestBus::Event(pair, &AzToolsFramework::RadiusManipulatorRequestBus::Events::SetRadius, 4.0f);
        AzToolsFramework::ShapeManipulatorRequestBus::Event(
            pair, &AzToolsFramework::ShapeManipulatorRequestBus::Events::SetTranslationOffset, expectedPosition);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Box3D::ExplosionComponent* explosion = gameEntity.FindComponent<Box3D::ExplosionComponent>();
        ASSERT_NE(explosion, nullptr);
        EXPECT_FLOAT_EQ(explosion->GetConfiguration().m_radius, 4.0f);
        EXPECT_TRUE(explosion->GetConfiguration().m_position.IsClose(expectedPosition));

        AzToolsFramework::SelectEntity(editorEntity->GetId());
        UnitTest::EnterComponentMode<ExplosionComponent>();
        bool componentModeInstantiated = false;
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::BroadcastResult(
            componentModeInstantiated,
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::ComponentModeInstantiated,
            pair);
        EXPECT_TRUE(componentModeInstantiated);
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::EndComponentMode);
    }

    TEST_F(ComponentModeTests, WindManipulatorBusEditsWorldSpaceVelocity)
    {
        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Box3D wind", &editorEntity);
        ASSERT_NE(editorEntity, nullptr);
        editorEntity->Deactivate();
        ASSERT_NE(editorEntity->CreateComponent<ColliderComponent>(), nullptr);
        ASSERT_NE(editorEntity->CreateComponent<RigidBodyComponent>(), nullptr);
        WindComponent* editorComponent = editorEntity->CreateComponent<WindComponent>();
        ASSERT_NE(editorComponent, nullptr);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        const AZ::Vector3 expectedVelocity(3.0f, -2.0f, 1.0f);
        AzToolsFramework::ShapeManipulatorRequestBus::Event(
            pair, &AzToolsFramework::ShapeManipulatorRequestBus::Events::SetTranslationOffset, expectedVelocity);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Box3D::WindComponent* wind = gameEntity.FindComponent<Box3D::WindComponent>();
        ASSERT_NE(wind, nullptr);
        EXPECT_TRUE(wind->GetConfiguration().m_velocity.IsClose(expectedVelocity));

        AzToolsFramework::SelectEntity(editorEntity->GetId());
        UnitTest::EnterComponentMode<WindComponent>();
        bool componentModeInstantiated = false;
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::BroadcastResult(
            componentModeInstantiated,
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::ComponentModeInstantiated,
            pair);
        EXPECT_TRUE(componentModeInstantiated);
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::EndComponentMode);
    }

    TEST_F(ComponentModeTests, JointManipulatorBusEditsBothLocalFrames)
    {
        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Box3D joint", &editorEntity);
        ASSERT_NE(editorEntity, nullptr);
        editorEntity->Deactivate();
        ASSERT_NE(editorEntity->CreateComponent<ColliderComponent>(), nullptr);
        ASSERT_NE(editorEntity->CreateComponent<RigidBodyComponent>(), nullptr);
        DistanceJointComponent* editorComponent = editorEntity->CreateComponent<DistanceJointComponent>();
        ASSERT_NE(editorComponent, nullptr);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        const AZ::Transform expectedParent = AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f));
        const AZ::Transform expectedChild = AZ::Transform::CreateFromQuaternionAndTranslation(
            AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi), AZ::Vector3(-1.0f, -2.0f, -3.0f));
        JointManipulatorRequestBus::Event(pair, &JointManipulatorRequestBus::Events::SetLocalFrame, JointFrame::Parent, expectedParent);
        JointManipulatorRequestBus::Event(pair, &JointManipulatorRequestBus::Events::SetLocalFrame, JointFrame::Child, expectedChild);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Box3D::JointComponent* joint = gameEntity.FindComponent<Box3D::JointComponent>();
        ASSERT_NE(joint, nullptr);
        const JointConfiguration jointConfiguration = joint->GetConfiguration();
        const auto* configuration = AZStd::get_if<DistanceJointConfiguration>(&jointConfiguration);
        ASSERT_NE(configuration, nullptr);
        EXPECT_TRUE(configuration->m_common.m_parentLocalFrame.IsClose(expectedParent));
        EXPECT_TRUE(configuration->m_common.m_childLocalFrame.IsClose(expectedChild));

        AzToolsFramework::SelectEntity(editorEntity->GetId());
        UnitTest::EnterComponentMode<DistanceJointComponent>();
        bool componentModeInstantiated = false;
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::BroadcastResult(
            componentModeInstantiated,
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::ComponentModeInstantiated,
            pair);
        EXPECT_TRUE(componentModeInstantiated);
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::EndComponentMode);
    }

    template<class EditorComponent, class Configuration>
    struct JointComponentTypes final
    {
        using Editor = EditorComponent;
        using Config = Configuration;
    };

    template<class Types>
    class EditorJointComponentTests
        : public testing::Test
    {
    };

    using JointTypes = testing::Types<
        JointComponentTypes<ParallelJointComponent, ParallelJointConfiguration>,
        JointComponentTypes<DistanceJointComponent, DistanceJointConfiguration>,
        JointComponentTypes<FilterJointComponent, FilterJointConfiguration>,
        JointComponentTypes<MotorJointComponent, MotorJointConfiguration>,
        JointComponentTypes<PrismaticJointComponent, PrismaticJointConfiguration>,
        JointComponentTypes<RevoluteJointComponent, RevoluteJointConfiguration>,
        JointComponentTypes<SphericalJointComponent, SphericalJointConfiguration>,
        JointComponentTypes<WeldJointComponent, WeldJointConfiguration>,
        JointComponentTypes<WheelJointComponent, WheelJointConfiguration>>;
    TYPED_TEST_SUITE(EditorJointComponentTests, JointTypes);

    TYPED_TEST(EditorJointComponentTests, BuildGameEntityPreservesConcreteJointKind)
    {
        typename TypeParam::Editor editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        Box3D::JointComponent* joint = gameEntity.FindComponent<Box3D::JointComponent>();
        ASSERT_NE(joint, nullptr);
        EXPECT_TRUE(AZStd::holds_alternative<typename TypeParam::Config>(joint->GetConfiguration()));
    }

    TYPED_TEST(EditorJointComponentTests, DrawsJointFrameInTheViewport)
    {
        typename TypeParam::Editor editorComponent;
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({ 0 }, debugDisplay);

        const AZ::Aabb bounds = debugDisplay.GetAabb();
        EXPECT_TRUE(bounds.IsValid());
        EXPECT_TRUE(bounds.GetMin().IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(bounds.GetMax().IsClose(AZ::Vector3::CreateOne()));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({ 0 });
        float distance = 0.0f;
        EXPECT_TRUE(selectionBounds.IsValid());
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            { 0 }, AZ::Vector3::CreateAxisX(-1.0f), AZ::Vector3::CreateAxisX(), distance));
        EXPECT_NEAR(distance, 0.9f, 1.0e-4f);
    }
} // namespace Box3D::Editor
