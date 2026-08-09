/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/ColliderComponent.h>
#include <Box3D/HeightfieldColliderComponent.h>
#include <Box3D/RigidBodyComponent.h>
#include <Box3D/SystemInternal.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Components/NonUniformScaleComponent.h>
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
        shapeConfiguration.m_properties.m_materialConfigurations.push_back(materialConfiguration);

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
        AZ::TickBus::Broadcast(&AZ::TickEvents::OnTick, 1.0f / 60.0f, AZ::ScriptTimePoint{});
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

    TEST(Box3DComponentTests, NonUniformScaleChangesRecookColliderGeometry)
    {
        NameDictionaryScope nameDictionaryScope;
        AZ::ComponentDescriptor* transformDescriptor = AzFramework::TransformComponent::CreateDescriptor();
        AZ::ComponentDescriptor* scaleDescriptor = AzFramework::NonUniformScaleComponent::CreateDescriptor();
        AZ::ComponentDescriptor* colliderDescriptor = ColliderComponent::CreateDescriptor();
        AZ::ComponentDescriptor* bodyDescriptor = RigidBodyComponent::CreateDescriptor();
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, transformDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, scaleDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, bodyDescriptor);

        System system;
        RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_gravityScale = 0.0f;
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{ AZ::Vector3::CreateOne() * 0.5f };

        AZ::Entity entity("Box3D scaled body");
        entity.CreateComponent<AzFramework::TransformComponent>();
        entity.CreateComponent<AzFramework::NonUniformScaleComponent>();
        ColliderComponent* collider = entity.CreateComponent<ColliderComponent>(shapeConfiguration);
        entity.CreateComponent<RigidBodyComponent>(bodyConfiguration);
        entity.Init();
        entity.Activate();

        EXPECT_TRUE(collider->GetAabb().GetExtents().IsClose(AZ::Vector3::CreateOne(), 0.05f));
        AZ::NonUniformScaleRequestBus::Event(entity.GetId(), &AZ::NonUniformScaleRequests::SetScale, AZ::Vector3(2.0f, 1.0f, 0.5f));
        EXPECT_TRUE(collider->GetAabb().GetExtents().IsClose(AZ::Vector3(2.0f, 1.0f, 0.5f), 0.05f));

        entity.Deactivate();
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, bodyDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, colliderDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, scaleDescriptor);
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, transformDescriptor);
        bodyDescriptor->ReleaseDescriptor();
        colliderDescriptor->ReleaseDescriptor();
        scaleDescriptor->ReleaseDescriptor();
        transformDescriptor->ReleaseDescriptor();
    }
} // namespace Box3D::Tests
