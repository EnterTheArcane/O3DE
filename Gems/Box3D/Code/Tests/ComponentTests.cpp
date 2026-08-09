/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/CharacterBus.h>
#include <Box3D/ColliderComponent.h>
#include <Box3D/HeightfieldColliderComponent.h>
#include <Box3D/JointBus.h>
#include <Box3D/RigidBodyComponent.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzTest/AzTest.h>

namespace Box3D::Tests
{
    namespace
    {
        class NameDictionaryScope final
        {
        public:
            NameDictionaryScope()
            {
                AZ::NameDictionary::Create();
            }

            ~NameDictionaryScope()
            {
                AZ::NameDictionary::Destroy();
            }
        };

        class CharacterMoveRecorder final
            : public CharacterNotificationBus::Handler
        {
        public:
            explicit CharacterMoveRecorder(AZ::EntityId entityId)
            {
                BusConnect(entityId);
            }

            ~CharacterMoveRecorder()
            {
                BusDisconnect();
            }

            void OnCharacterMoved(const CharacterState& state) override
            {
                ++m_moveCount;
                m_state = state;
            }

            CharacterState m_state;
            AZ::u32 m_moveCount = 0;
        };

        class JointThresholdRecorder final
            : public JointNotificationBus::Handler
        {
        public:
            explicit JointThresholdRecorder(AZ::EntityId entityId)
            {
                BusConnect(entityId);
            }

            ~JointThresholdRecorder()
            {
                BusDisconnect();
            }

            void OnThresholdExceeded(const JointThresholdEvent& event) override
            {
                ++m_eventCount;
                m_event = event;
            }

            JointThresholdEvent m_event;
            AZ::u32 m_eventCount = 0;
        };
    } // namespace

    TEST(Box3DComponentTests, RigidBodyOwnsItsAssociationAndSynchronizesTransforms)
    {
        NameDictionaryScope nameDictionaryScope;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = RigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, bodyDescriptor);

        System system;
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(2.0f));
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{ 0.5f };
        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_name = AZ::Name("Rubber");
        materialConfiguration.m_friction = 0.9f;
        shapeConfiguration.m_materialConfigurations.push_back(materialConfiguration);

        AZ::Entity entity("Box3D body");
        entity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* collider = entity.CreateComponent<ColliderComponent>(shapeConfiguration);
        RigidBodyComponent* body = entity.CreateComponent<RigidBodyComponent>(bodyConfiguration);
        entity.Init();
        AZ::Transform initialTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(2.0f));
        initialTransform.SetUniformScale(2.0f);
        AZ::TransformBus::Event(entity.GetId(), &AZ::TransformInterface::SetWorldTM, initialTransform);
        entity.Activate();

        ASSERT_TRUE(body->IsSimulationEnabled());
        const WorldHandle worldHandle = body->GetWorldHandle();
        const BodyHandle bodyHandle = body->GetBodyHandle();
        ASSERT_TRUE(worldHandle.IsValid());
        ASSERT_TRUE(bodyHandle.IsValid());
        ASSERT_EQ(collider->GetShapeHandles().size(), 1);

        RaycastRequest request;
        request.m_start = AZ::Vector3::CreateAxisZ(5.0f);
        request.m_direction = -AZ::Vector3::CreateAxisZ();
        request.m_distance = 10.0f;
        QueryHit materialHit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, materialHit));
        EXPECT_NEAR(materialHit.m_distance, 2.0f, 0.01f);
        ASSERT_TRUE(materialHit.m_materialHandle.IsValid());
        MaterialConfiguration resolvedMaterial;
        ASSERT_TRUE(system.GetMaterial(materialHit.m_materialHandle, resolvedMaterial));
        EXPECT_FLOAT_EQ(resolvedMaterial.m_friction, 0.9f);
        const MaterialHandle ownedMaterial = materialHit.m_materialHandle;

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        AZ::Transform entityTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(entityTransform, entity.GetId(), &AZ::TransformInterface::GetWorldTM);
        EXPECT_LT(entityTransform.GetTranslation().GetZ(), 2.0f);
        EXPECT_FLOAT_EQ(entityTransform.GetUniformScale(), 2.0f);

        AZ::Transform teleported = AZ::Transform::CreateTranslation(AZ::Vector3(3.0f, 4.0f, 5.0f));
        teleported.SetUniformScale(3.0f);
        AZ::TransformBus::Event(entity.GetId(), &AZ::TransformInterface::SetWorldTM, teleported);
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_TRUE(state.m_transform.GetTranslation().IsClose(teleported.GetTranslation()));
        EXPECT_FLOAT_EQ(state.m_transform.GetUniformScale(), 1.0f);
        EXPECT_NEAR(collider->GetAabb().GetExtents().GetX(), 3.0f, 0.05f);

        entity.Deactivate();
        EXPECT_FALSE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_FALSE(system.GetMaterial(ownedMaterial, resolvedMaterial));

        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(Box3DComponentTests, HeightfieldUpdatesPreserveTheComponentHandles)
    {
        NameDictionaryScope nameDictionaryScope;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* heightfieldDescriptor = HeightfieldColliderComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, heightfieldDescriptor);

        System system;
        HeightfieldShapeConfiguration heightfield;
        heightfield.m_columnCount = 3;
        heightfield.m_rowCount = 3;
        heightfield.m_samples.resize(9);
        heightfield.m_materialIndices.resize(4);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = heightfield;

        AZ::Entity entity("Box3D heightfield");
        entity.CreateComponent<AzFramework::TransformComponent>();
        HeightfieldColliderComponent* component = entity.CreateComponent<HeightfieldColliderComponent>(shapeConfiguration);
        entity.Init();
        entity.Activate();

        ASSERT_TRUE(component->IsSimulationEnabled());
        const WorldHandle worldHandle = component->GetWorldHandle();
        const BodyHandle bodyHandle = component->GetBodyHandle();
        const ShapeHandle shapeHandle = component->GetShapeHandle();
        ASSERT_TRUE(worldHandle.IsValid());
        ASSERT_TRUE(bodyHandle.IsValid());
        ASSERT_TRUE(shapeHandle.IsValid());

        const AZStd::array<float, 1> raisedCorner{ 3.0f };
        EXPECT_TRUE(component->UpdateHeights(2, 2, 1, 1, raisedCorner));
        EXPECT_EQ(component->GetBodyHandle(), bodyHandle);
        EXPECT_EQ(component->GetShapeHandle(), shapeHandle);
        EXPECT_GT(component->GetAabb().GetMax().GetZ(), 2.9f);
        EXPECT_FALSE(component->UpdateHeights(3, 3, 1, 1, raisedCorner));

        const AZStd::array<AZ::u8, 1> materialIndex{ 2 };
        EXPECT_TRUE(component->UpdateMaterials(0, 0, 1, 1, materialIndex));
        ASSERT_EQ(component->GetMaterialIndices().size(), 4);
        EXPECT_EQ(component->GetMaterialIndices()[0], 2);

        entity.Deactivate();
        BodyState state;
        EXPECT_FALSE(system.GetBodyState(worldHandle, bodyHandle, state));

        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, heightfieldDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, transformDescriptor);
        heightfieldDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(Box3DComponentTests, UniformScaleChangesRecookColliderGeometry)
    {
        NameDictionaryScope nameDictionaryScope;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = RigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, bodyDescriptor);

        System system;
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_gravityScale = 0.0f;
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.5f };

        AZ::Entity entity("Box3D scaled body");
        entity.CreateComponent<AzFramework::TransformComponent>();
        ColliderComponent* collider = entity.CreateComponent<ColliderComponent>(shapeConfiguration);
        entity.CreateComponent<RigidBodyComponent>(bodyConfiguration);
        entity.Init();
        entity.Activate();

        EXPECT_TRUE(collider->GetAabb().GetExtents().IsClose(AZ::Vector3::CreateOne(), 0.05f));
        AZ::TransformBus::Event(entity.GetId(), &AZ::TransformInterface::SetLocalUniformScale, 2.0f);
        EXPECT_TRUE(collider->GetAabb().GetExtents().IsClose(AZ::Vector3::CreateOne() * 2.0f, 0.05f));

        entity.Deactivate();
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }

    TEST(Box3DComponentTests, StepDispatchesQueuedCharacterMovesByEntity)
    {
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const AZ::EntityId entityId(101);
        CharacterMoveRecorder recorder(entityId);

        CharacterConfiguration configuration;
        configuration.m_entityId = entityId;
        configuration.m_groundStickDistance = 0.0f;
        configuration.m_stepHeight = 0.0f;
        configuration.m_applyMoveOnFixedTick = true;
        const CharacterHandle characterHandle = system.CreateCharacter(worldHandle, configuration);
        ASSERT_TRUE(characterHandle.IsValid());
        ASSERT_TRUE(system.MoveCharacter(worldHandle, characterHandle, 2.0f * AZ::Vector3::CreateAxisX(), 1.0f / 60.0f));
        EXPECT_EQ(recorder.m_moveCount, 0);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_EQ(recorder.m_moveCount, 1);
        EXPECT_GT(recorder.m_state.m_basePosition.GetX(), 0.0f);
    }

    TEST(Box3DComponentTests, StepDispatchesJointThresholdsByEntity)
    {
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_workerCount = 1;
        System system(systemConfiguration);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const AZ::EntityId entityId(102);
        JointThresholdRecorder recorder(entityId);

        RigidBodyConfiguration parentConfiguration;
        parentConfiguration.m_bodyType = BodyType::Dynamic;
        const BodyHandle parentBody = system.CreateBody(worldHandle, parentConfiguration);
        RigidBodyConfiguration childConfiguration;
        childConfiguration.m_bodyType = BodyType::Dynamic;
        childConfiguration.m_transform = AZ::Transform::CreateTranslation(2.0f * AZ::Vector3::CreateAxisX());
        const BodyHandle childBody = system.CreateBody(worldHandle, childConfiguration);
        ASSERT_TRUE(parentBody.IsValid());
        ASSERT_TRUE(childBody.IsValid());

        DistanceJointConfiguration jointConfiguration;
        jointConfiguration.m_common.m_parentBody = parentBody;
        jointConfiguration.m_common.m_childBody = childBody;
        jointConfiguration.m_common.m_forceThreshold = 0.0f;
        jointConfiguration.m_length = 1.0f;
        const JointHandle jointHandle = system.CreateJoint(worldHandle, jointConfiguration);
        ASSERT_TRUE(jointHandle.IsValid());
        ASSERT_TRUE(system.SetJointEntityId(worldHandle, jointHandle, entityId));
        ASSERT_TRUE(system.ApplyLinearImpulse(worldHandle, childBody, 100.0f * AZ::Vector3::CreateAxisX()));

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_EQ(recorder.m_eventCount, 1);
        EXPECT_EQ(recorder.m_event.m_jointHandle, jointHandle);
    }
} // namespace Box3D::Tests
