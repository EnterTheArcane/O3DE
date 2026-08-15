/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CharacterControllerComponent.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/Editor/CharacterControllerComponent.h>
#include <Jolt/Editor/ConstraintComponent.h>
#include <Jolt/Editor/DebugDraw.h>
#include <Jolt/Editor/HairComponent.h>
#include <Jolt/Editor/PathComponent.h>
#include <Jolt/Editor/RagdollComponent.h>
#include <Jolt/Editor/ColliderComponent.h>
#include <Jolt/Editor/RigidBodyComponent.h>
#include <Jolt/Editor/SceneComponent.h>
#include <Jolt/Editor/SceneBuilder.h>
#include <Jolt/Editor/SkeletonComponent.h>
#include <Jolt/Editor/SoftBodyComponent.h>
#include <Jolt/Editor/StaticRigidBodyComponent.h>
#include <Jolt/Editor/VehicleComponents.h>
#include <Jolt/Editor/VirtualCharacterControllerComponent.h>
#include <Jolt/RigidBodyComponent.h>
#include <Jolt/HairComponent.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/PathComponent.h>
#include <Jolt/RagdollComponent.h>
#include <Jolt/SceneComponent.h>
#include <Jolt/SkeletonComponent.h>
#include <Jolt/ConstraintComponent.h>
#include <Jolt/StaticRigidBodyComponent.h>
#include <Jolt/SystemComponent.h>
#include <Jolt/VehicleComponents.h>
#include <Jolt/VirtualCharacterControllerComponent.h>

#include <AzTest/AzTest.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Input/System/InputSystemComponent.h>
#include <AzFramework/UnitTest/TestDebugDisplayRequests.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Manipulators/BoxManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CapsuleManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CylinderManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/RadiusManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/ShapeManipulatorRequestBus.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>

AZ_TOOLS_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace Jolt::Editor
{
    namespace
    {
        class NameDictionaryScope final
        {
        public:
            NameDictionaryScope()
            {
                if (!AZ::Interface<AZ::NameDictionary>::Get())
                {
                    AZ::NameDictionary::Create();
                    m_created = true;
                }
            }

            ~NameDictionaryScope()
            {
                if (m_created)
                {
                    AZ::NameDictionary::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(NameDictionaryScope);

        private:
            bool m_created = false;
        };

        class HeadlessComponentModeTestApplication final
            : public UnitTest::ToolsTestApplication
        {
        public:
            HeadlessComponentModeTestApplication()
                : UnitTest::ToolsTestApplication("JoltComponentModeTests")
            {
            }

            [[nodiscard]]
            AZ::ComponentTypeList GetRequiredSystemComponents() const override
            {
                AZ::ComponentTypeList components =
                    UnitTest::ToolsTestApplication::GetRequiredSystemComponents();
                components.erase(
                    AZStd::remove(
                        components.begin(),
                        components.end(),
                        azrtti_typeid<AzFramework::InputSystemComponent>()),
                    components.end());
                return components;
            }
        };

        class ComponentModeTests
            : public UnitTest::ToolsApplicationFixture<false>
        {
        protected:
            AZStd::unique_ptr<UnitTest::ToolsTestApplication> CreateTestApplication() override
            {
                return AZStd::make_unique<HeadlessComponentModeTestApplication>();
            }

            void SetUpEditorFixtureImpl() override
            {
                GetApplication()->RegisterComponentDescriptor(ColliderComponent::CreateDescriptor());
                m_viewportManager.Create();
            }

            void TearDownEditorFixtureImpl() override
            {
                m_viewportManager.Destroy();
            }

            UnitTest::ViewportManagerWrapper m_viewportManager;
        };

        void ExpectColliderComponentMode(
            const AZ::EntityComponentIdPair& pair)
        {
            AzToolsFramework::SelectEntity(pair.GetEntityId());
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

        template<typename Component>
        void ExpectBinaryRoundTrip(
            Component& source,
            AZ::SerializeContext& serializeContext)
        {
            AZStd::vector<char> sourceBuffer;
            AZ::IO::ByteContainerStream sourceStream(&sourceBuffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
                sourceStream,
                AZ::DataStream::ST_BINARY,
                &source,
                &serializeContext));

            sourceStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            auto restored = AZStd::make_unique<Component>();
            ASSERT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(
                sourceStream,
                *restored,
                &serializeContext));

            AZStd::vector<char> restoredBuffer;
            AZ::IO::ByteContainerStream restoredStream(&restoredBuffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
                restoredStream,
                AZ::DataStream::ST_BINARY,
                restored.get(),
                &serializeContext));
            EXPECT_EQ(restoredBuffer, sourceBuffer);
        }
    } // namespace

    TEST(EditorComponentTests, CompleteModuleReflectionRegistersWithoutDuplicateTypes)
    {
        {
            AZ::SerializeContext serializeContext;
            serializeContext.CreateEditContext();

            const auto reflectModule = [&serializeContext]()
            {
                Jolt::SystemComponent::Reflect(&serializeContext);
                Jolt::CharacterControllerComponent::Reflect(&serializeContext);
                Jolt::ConstraintComponent::Reflect(&serializeContext);
                Jolt::ColliderComponent::Reflect(&serializeContext);
                Jolt::HairComponent::Reflect(&serializeContext);
                Jolt::PathComponent::Reflect(&serializeContext);
                Jolt::RagdollComponent::Reflect(&serializeContext);
                Jolt::RigidBodyComponent::Reflect(&serializeContext);
                Jolt::SceneComponent::Reflect(&serializeContext);
                Jolt::SoftBodyComponent::Reflect(&serializeContext);
                Jolt::SkeletonComponent::Reflect(&serializeContext);
                Jolt::StaticRigidBodyComponent::Reflect(&serializeContext);
                Jolt::WheeledVehicleComponent::Reflect(&serializeContext);
                Jolt::MotorcycleComponent::Reflect(&serializeContext);
                Jolt::TrackedVehicleComponent::Reflect(&serializeContext);
                Jolt::VirtualCharacterControllerComponent::Reflect(&serializeContext);
                ColliderComponent::Reflect(&serializeContext);
                HairComponent::Reflect(&serializeContext);
                PathComponent::Reflect(&serializeContext);
                RagdollComponent::Reflect(&serializeContext);
                CharacterControllerComponent::Reflect(&serializeContext);
                ConstraintComponent::Reflect(&serializeContext);
                RigidBodyComponent::Reflect(&serializeContext);
                SceneComponent::Reflect(&serializeContext);
                SoftBodyComponent::Reflect(&serializeContext);
                SkeletonComponent::Reflect(&serializeContext);
                StaticRigidBodyComponent::Reflect(&serializeContext);
                WheeledVehicleComponent::Reflect(&serializeContext);
                MotorcycleComponent::Reflect(&serializeContext);
                TrackedVehicleComponent::Reflect(&serializeContext);
                VirtualCharacterControllerComponent::Reflect(&serializeContext);
                BuilderComponent::Reflect(&serializeContext);
            };
            reflectModule();

            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::SystemComponent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::CollisionGroupConfiguration>()));
            EXPECT_TRUE(
                serializeContext.FindClassData(azrtti_typeid<Jolt::CompoundChildConfiguration>()));
            EXPECT_TRUE(
                serializeContext.FindClassData(azrtti_typeid<Jolt::CompoundShapeConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::ConstraintComponentConfiguration>()));
            EXPECT_TRUE(
                serializeContext.FindClassData(azrtti_typeid<Jolt::DecoratedShapeConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::GroupFilterTableConfiguration>()));
            EXPECT_TRUE(
                serializeContext.FindClassData(azrtti_typeid<Jolt::OffsetCenterOfMassShapeConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::RagdollComponentConfiguration>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::SceneComponent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SceneComponent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<Jolt::SkeletonComponent>()));
            EXPECT_TRUE(serializeContext.FindClassData(azrtti_typeid<SkeletonComponent>()));
            EXPECT_TRUE(
                serializeContext.FindClassData(azrtti_typeid<Jolt::RotatedTranslatedShapeConfiguration>()));
            EXPECT_TRUE(
                serializeContext.FindClassData(azrtti_typeid<Jolt::ScaledShapeConfiguration>()));

            serializeContext.EnableRemoveReflection();
            reflectModule();
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(EditorComponentTests, RuntimeConstraintGeometryRoundTripsThroughSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            AZ::Entity::Reflect(&serializeContext);
            Jolt::WorldPosition::Reflect(&serializeContext);
            Jolt::ConstraintComponent::Reflect(&serializeContext);

            {
                ConstraintComponentConfiguration configuration;
                configuration.m_userData = 0x1234'5678'9abc'def0;
                configuration.m_geometry = HingeConstraintConfiguration{
                    .m_maximumLimit = 0.75f,
                    .m_minimumLimit = -0.5f,
                };
                Jolt::ConstraintComponent source(AZStd::move(configuration));

                AZStd::vector<char> sourceBuffer;
                AZ::IO::ByteContainerStream sourceStream(&sourceBuffer);
                ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
                    sourceStream,
                    AZ::DataStream::ST_BINARY,
                    &source,
                    &serializeContext));

                {
                    sourceStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                    auto restored = AZStd::make_unique<Jolt::ConstraintComponent>();
                    ASSERT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(
                        sourceStream,
                        *restored,
                        &serializeContext));

                    AZStd::vector<char> restoredBuffer;
                    AZ::IO::ByteContainerStream restoredStream(&restoredBuffer);
                    ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
                        restoredStream,
                        AZ::DataStream::ST_BINARY,
                        restored.get(),
                        &serializeContext));

                    EXPECT_TRUE(restoredBuffer == sourceBuffer);
                    restored.reset();
                }
            }
            serializeContext.EnableRemoveReflection();
            Jolt::ConstraintComponent::Reflect(&serializeContext);
            Jolt::WorldPosition::Reflect(&serializeContext);
            AZ::Entity::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(EditorComponentTests, RuntimeColliderMetadataRoundTripsThroughSerialization)
    {
        {
            AZ::SerializeContext serializeContext;
            AZ::Entity::Reflect(&serializeContext);
            Jolt::ColliderComponent::Reflect(&serializeContext);

            ColliderShapeConfiguration firstShape;
            firstShape.m_shape.m_geometry = BoxShapeConfiguration{};
            firstShape.m_shape.m_userData = 101;
            firstShape.m_localTransform =
                AZ::Transform::CreateTranslation(-2.0f * AZ::Vector3::CreateAxisX());
            firstShape.m_compoundUserData = 11;

            ColliderShapeConfiguration secondShape;
            secondShape.m_shape.m_geometry = SphereShapeConfiguration{};
            secondShape.m_shape.m_userData = 202;
            secondShape.m_localTransform =
                AZ::Transform::CreateTranslation(2.0f * AZ::Vector3::CreateAxisX());
            secondShape.m_compoundUserData = 22;

            Jolt::ColliderComponent source(AZStd::vector{firstShape, secondShape});
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream stream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
                stream,
                AZ::DataStream::ST_BINARY,
                &source,
                &serializeContext));

            stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            auto restored = AZStd::make_unique<Jolt::ColliderComponent>();
            ASSERT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(
                stream,
                *restored,
                &serializeContext));

            const AZStd::span<const ColliderShapeConfiguration> configurations =
                restored->GetShapeConfigurations();
            ASSERT_EQ(configurations.size(), 2);
            EXPECT_TRUE(
                AZStd::holds_alternative<BoxShapeConfiguration>(configurations[0].m_shape.m_geometry));
            EXPECT_EQ(configurations[0].m_shape.m_userData, 101);
            EXPECT_TRUE(
                configurations[0].m_localTransform.GetTranslation().IsClose(
                    -2.0f * AZ::Vector3::CreateAxisX()));
            EXPECT_EQ(configurations[0].m_compoundUserData, 11);
            EXPECT_TRUE(
                AZStd::holds_alternative<SphereShapeConfiguration>(configurations[1].m_shape.m_geometry));
            EXPECT_EQ(configurations[1].m_shape.m_userData, 202);
            EXPECT_TRUE(
                configurations[1].m_localTransform.GetTranslation().IsClose(
                    2.0f * AZ::Vector3::CreateAxisX()));
            EXPECT_EQ(configurations[1].m_compoundUserData, 22);

            restored.reset();
            serializeContext.EnableRemoveReflection();
            Jolt::ColliderComponent::Reflect(&serializeContext);
            AZ::Entity::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(EditorComponentTests, RuntimeOwnedConfigurationsDoNotDuplicateDefaultsDuringDeserialization)
    {
        NameDictionaryScope nameDictionaryScope;
        {
            AZ::SerializeContext serializeContext;
            AZ::Entity::Reflect(&serializeContext);
            AZ::Name::Reflect(&serializeContext);
            ObjectLayer::Reflect(&serializeContext);
            Jolt::HairComponent::Reflect(&serializeContext);
            Jolt::RagdollComponent::Reflect(&serializeContext);
            Jolt::SoftBodyComponent::Reflect(&serializeContext);
            Jolt::WheeledVehicleComponent::Reflect(&serializeContext);
            Jolt::MotorcycleComponent::Reflect(&serializeContext);
            Jolt::TrackedVehicleComponent::Reflect(&serializeContext);

            {
                Jolt::RagdollComponent source(RagdollComponentConfiguration::CreateDefault());
                ExpectBinaryRoundTrip(source, serializeContext);
            }

            {
                SoftBodyComponentConfiguration configuration =
                    SoftBodyComponentConfiguration::CreateDefault();
                configuration.m_body.m_userData = 0x1234'5678'9abc'def0;
                configuration.m_definition.m_inverseBinds = {
                    {
                        .m_transform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(0.25f)),
                        .m_jointIndex = 3,
                    },
                };
                configuration.m_definition.m_skinConstraints.resize(
                    configuration.m_definition.m_vertices.size());
                SoftBodySkinConstraint& skinConstraint =
                    configuration.m_definition.m_skinConstraints.front();
                skinConstraint.m_weights[0] = {
                    .m_inverseBindIndex = 0,
                    .m_weight = 1.0f,
                };
                skinConstraint.m_vertex = 0;
                skinConstraint.m_backstopDistance = 0.1f;
                skinConstraint.m_backstopRadius = 0.2f;
                skinConstraint.m_maximumDistance = 0.3f;
                Jolt::SoftBodyComponent source(AZStd::move(configuration));
                ExpectBinaryRoundTrip(source, serializeContext);
            }

            {
                Jolt::HairComponent source(HairComponentConfiguration::CreateDefault());
                ExpectBinaryRoundTrip(source, serializeContext);
            }

            {
                Jolt::WheeledVehicleComponent source(
                    WheeledVehicleComponentConfiguration::CreateDefault());
                ExpectBinaryRoundTrip(source, serializeContext);
            }

            {
                Jolt::MotorcycleComponent source(
                    MotorcycleComponentConfiguration::CreateDefault());
                ExpectBinaryRoundTrip(source, serializeContext);
            }

            {
                Jolt::TrackedVehicleComponent source(
                    TrackedVehicleComponentConfiguration::CreateDefault());
                ExpectBinaryRoundTrip(source, serializeContext);
            }

            serializeContext.EnableRemoveReflection();
            Jolt::TrackedVehicleComponent::Reflect(&serializeContext);
            Jolt::MotorcycleComponent::Reflect(&serializeContext);
            Jolt::WheeledVehicleComponent::Reflect(&serializeContext);
            Jolt::SoftBodyComponent::Reflect(&serializeContext);
            Jolt::RagdollComponent::Reflect(&serializeContext);
            Jolt::HairComponent::Reflect(&serializeContext);
            ObjectLayer::Reflect(&serializeContext);
            AZ::Name::Reflect(&serializeContext);
            AZ::Entity::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(EditorComponentTests, CharacterControllerBuildsRuntimeComponent)
    {
        CharacterComponentConfiguration configuration;
        configuration.m_userData = 0x1234'5678'9abc'def0;
        CharacterControllerComponent editorComponent(AZStd::move(configuration));
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        const auto* runtimeComponent = gameEntity.FindComponent<Jolt::CharacterControllerComponent>();
        ASSERT_TRUE(runtimeComponent);
        EXPECT_EQ(runtimeComponent->GetUserData(), 0x1234'5678'9abc'def0);
    }

    TEST(EditorComponentTests, SceneBuildsRuntimeComponent)
    {
        SceneComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::SceneComponent>());
    }

    TEST(EditorComponentTests, CharacterControllerDrawsAndSelectsSlopeAndSupportGuides)
    {
        CharacterComponentConfiguration configuration;
        configuration.m_maximumSlopeAngle = AZ::Constants::QuarterPi;
        configuration.m_supportingPlaneDistance = -0.5f;
        CharacterControllerComponent editorComponent(AZStd::move(configuration));
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(selectionBounds.Contains(drawnBounds));
        EXPECT_GT(drawnBounds.GetMax().GetZ(), 0.9f);
        float distance = 0.0f;
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            {0},
            AZ::Vector3(-2.0f, 0.0f, 0.5f),
            AZ::Vector3::CreateAxisX(),
            distance));
    }

    TEST(EditorComponentTests, ColliderBuildsRuntimeComponentWithAuthoredDefault)
    {
        ColliderComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        Jolt::ColliderComponent* runtimeComponent = gameEntity.FindComponent<Jolt::ColliderComponent>();
        ASSERT_TRUE(runtimeComponent);
        const AZStd::span<const ColliderShapeConfiguration> configurations =
            runtimeComponent->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        EXPECT_TRUE(AZStd::holds_alternative<BoxShapeConfiguration>(configurations.front().m_shape.m_geometry));
    }

    TEST(EditorComponentTests, ColliderDrawsAndSelectsAuthoredGeometry)
    {
        ColliderComponent editorComponent;
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(-0.5f * AZ::Vector3::CreateOne()));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(0.5f * AZ::Vector3::CreateOne()));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        float distance = 0.0f;
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(drawnBounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(drawnBounds.GetMax()));
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            {0},
            AZ::Vector3::CreateAxisX(-2.0f),
            AZ::Vector3::CreateAxisX(),
            distance));
        EXPECT_NEAR(distance, 1.5f, 1.0e-4f);
    }

    TEST_F(ComponentModeTests, ColliderManipulatorBusesEditTheActiveBox)
    {
        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Jolt collider", &editorEntity);
        ASSERT_TRUE(editorEntity);

        editorEntity->Deactivate();
        ColliderComponent* editorComponent = editorEntity->CreateComponent<ColliderComponent>();
        ASSERT_TRUE(editorComponent);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        AZ::Vector3 dimensions = AZ::Vector3::CreateZero();
        AzToolsFramework::BoxManipulatorRequestBus::EventResult(
            dimensions,
            pair,
            &AzToolsFramework::BoxManipulatorRequestBus::Events::GetDimensions);
        EXPECT_TRUE(dimensions.IsClose(AZ::Vector3::CreateOne()));

        const AZ::Vector3 expectedDimensions(2.0f, 3.0f, 4.0f);
        const AZ::Vector3 expectedOffset(5.0f, 6.0f, 7.0f);
        AzToolsFramework::BoxManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::BoxManipulatorRequestBus::Events::SetDimensions,
            expectedDimensions);
        AzToolsFramework::ShapeManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::ShapeManipulatorRequestBus::Events::SetTranslationOffset,
            expectedOffset);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Jolt::ColliderComponent* collider = gameEntity.FindComponent<Jolt::ColliderComponent>();
        ASSERT_TRUE(collider);

        const AZStd::span<const ColliderShapeConfiguration> configurations =
            collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        const auto* box =
            AZStd::get_if<BoxShapeConfiguration>(&configurations.front().m_shape.m_geometry);
        ASSERT_TRUE(box);
        EXPECT_TRUE(box->m_dimensions.IsClose(expectedDimensions));
        EXPECT_TRUE(configurations.front().m_localTransform.GetTranslation().IsClose(expectedOffset));

        ExpectColliderComponentMode(pair);
    }

    TEST_F(ComponentModeTests, ColliderManipulatorBusesEditTheActiveSphere)
    {
        ColliderShapeConfiguration configuration;
        configuration.m_shape.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};

        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Jolt sphere collider", &editorEntity);
        ASSERT_TRUE(editorEntity);

        editorEntity->Deactivate();
        ColliderComponent* editorComponent = editorEntity->CreateComponent<ColliderComponent>(
            AZStd::vector<ColliderShapeConfiguration>{configuration});
        ASSERT_TRUE(editorComponent);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        float radius = 0.0f;
        AzToolsFramework::RadiusManipulatorRequestBus::EventResult(
            radius,
            pair,
            &AzToolsFramework::RadiusManipulatorRequestBus::Events::GetRadius);
        EXPECT_FLOAT_EQ(radius, 0.5f);

        AzToolsFramework::RadiusManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::RadiusManipulatorRequestBus::Events::SetRadius,
            1.25f);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Jolt::ColliderComponent* collider = gameEntity.FindComponent<Jolt::ColliderComponent>();
        ASSERT_TRUE(collider);

        const AZStd::span<const ColliderShapeConfiguration> configurations =
            collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        const auto* sphere =
            AZStd::get_if<SphereShapeConfiguration>(&configurations.front().m_shape.m_geometry);
        ASSERT_TRUE(sphere);
        EXPECT_FLOAT_EQ(sphere->m_radius, 1.25f);

        ExpectColliderComponentMode(pair);
    }

    TEST_F(ComponentModeTests, ColliderManipulatorBusesPreserveCapsuleHeightAndNativeAxis)
    {
        ColliderShapeConfiguration configuration;
        configuration.m_shape.m_geometry = CapsuleShapeConfiguration{
            .m_cylinderHeight = 1.0f,
            .m_radius = 0.5f,
        };
        configuration.m_localTransform = AZ::Transform::CreateUniformScale(2.0f);

        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Jolt capsule collider", &editorEntity);
        ASSERT_TRUE(editorEntity);

        editorEntity->Deactivate();
        ColliderComponent* editorComponent = editorEntity->CreateComponent<ColliderComponent>(
            AZStd::vector<ColliderShapeConfiguration>{configuration});
        ASSERT_TRUE(editorComponent);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        float height = 0.0f;
        float radius = 0.0f;
        AzToolsFramework::CapsuleManipulatorRequestBus::EventResult(
            height,
            pair,
            &AzToolsFramework::CapsuleManipulatorRequestBus::Events::GetHeight);
        AzToolsFramework::RadiusManipulatorRequestBus::EventResult(
            radius,
            pair,
            &AzToolsFramework::RadiusManipulatorRequestBus::Events::GetRadius);
        EXPECT_FLOAT_EQ(height, 4.0f);
        EXPECT_FLOAT_EQ(radius, 1.0f);

        AzToolsFramework::RadiusManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::RadiusManipulatorRequestBus::Events::SetRadius,
            0.5f);
        AzToolsFramework::CapsuleManipulatorRequestBus::EventResult(
            height,
            pair,
            &AzToolsFramework::CapsuleManipulatorRequestBus::Events::GetHeight);
        EXPECT_FLOAT_EQ(height, 4.0f);

        AzToolsFramework::CapsuleManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::CapsuleManipulatorRequestBus::Events::SetHeight,
            3.0f);

        AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
        AzToolsFramework::ShapeManipulatorRequestBus::EventResult(
            rotation,
            pair,
            &AzToolsFramework::ShapeManipulatorRequestBus::Events::GetRotationOffset);
        EXPECT_TRUE(rotation.TransformVector(AZ::Vector3::CreateAxisZ()).IsClose(AZ::Vector3::CreateAxisY()));

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Jolt::ColliderComponent* collider = gameEntity.FindComponent<Jolt::ColliderComponent>();
        ASSERT_TRUE(collider);

        const AZStd::span<const ColliderShapeConfiguration> configurations =
            collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        const auto* capsule =
            AZStd::get_if<CapsuleShapeConfiguration>(&configurations.front().m_shape.m_geometry);
        ASSERT_TRUE(capsule);
        EXPECT_FLOAT_EQ(capsule->m_cylinderHeight, 1.0f);
        EXPECT_FLOAT_EQ(capsule->m_radius, 0.25f);

        ExpectColliderComponentMode(pair);
    }

    TEST_F(ComponentModeTests, ColliderManipulatorBusesEditTheActiveCylinderUsingTheNativeAxis)
    {
        ColliderShapeConfiguration configuration;
        configuration.m_shape.m_geometry = CylinderShapeConfiguration{
            .m_height = 1.5f,
            .m_radius = 0.4f,
        };
        configuration.m_localTransform = AZ::Transform::CreateUniformScale(2.0f);

        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Jolt cylinder collider", &editorEntity);
        ASSERT_TRUE(editorEntity);

        editorEntity->Deactivate();
        ColliderComponent* editorComponent = editorEntity->CreateComponent<ColliderComponent>(
            AZStd::vector<ColliderShapeConfiguration>{configuration});
        ASSERT_TRUE(editorComponent);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        float height = 0.0f;
        float radius = 0.0f;
        AzToolsFramework::CylinderManipulatorRequestBus::EventResult(
            height,
            pair,
            &AzToolsFramework::CylinderManipulatorRequestBus::Events::GetHeight);
        AzToolsFramework::RadiusManipulatorRequestBus::EventResult(
            radius,
            pair,
            &AzToolsFramework::RadiusManipulatorRequestBus::Events::GetRadius);
        EXPECT_FLOAT_EQ(height, 3.0f);
        EXPECT_FLOAT_EQ(radius, 0.8f);

        AzToolsFramework::CylinderManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::CylinderManipulatorRequestBus::Events::SetHeight,
            4.0f);
        AzToolsFramework::RadiusManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::RadiusManipulatorRequestBus::Events::SetRadius,
            1.0f);

        AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
        AzToolsFramework::ShapeManipulatorRequestBus::EventResult(
            rotation,
            pair,
            &AzToolsFramework::ShapeManipulatorRequestBus::Events::GetRotationOffset);
        EXPECT_TRUE(rotation.TransformVector(AZ::Vector3::CreateAxisZ()).IsClose(AZ::Vector3::CreateAxisY()));

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Jolt::ColliderComponent* collider = gameEntity.FindComponent<Jolt::ColliderComponent>();
        ASSERT_TRUE(collider);

        const AZStd::span<const ColliderShapeConfiguration> configurations =
            collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        const auto* cylinder =
            AZStd::get_if<CylinderShapeConfiguration>(&configurations.front().m_shape.m_geometry);
        ASSERT_TRUE(cylinder);
        EXPECT_FLOAT_EQ(cylinder->m_height, 2.0f);
        EXPECT_FLOAT_EQ(cylinder->m_radius, 0.5f);

        ExpectColliderComponentMode(pair);
    }

    TEST_F(ComponentModeTests, ColliderComponentModeEditsTheOffsetOfNonPrimitiveShapes)
    {
        ColliderShapeConfiguration configuration;
        configuration.m_shape.m_geometry = TriangleShapeConfiguration{};

        AZ::Entity* editorEntity = nullptr;
        UnitTest::CreateDefaultEditorEntity("Jolt triangle collider", &editorEntity);
        ASSERT_TRUE(editorEntity);

        editorEntity->Deactivate();
        ColliderComponent* editorComponent = editorEntity->CreateComponent<ColliderComponent>(
            AZStd::vector<ColliderShapeConfiguration>{configuration});
        ASSERT_TRUE(editorComponent);
        editorEntity->Activate();

        const AZ::EntityComponentIdPair pair(editorEntity->GetId(), editorComponent->GetId());
        const AZ::Vector3 expectedOffset(1.0f, 2.0f, 3.0f);
        AzToolsFramework::ShapeManipulatorRequestBus::Event(
            pair,
            &AzToolsFramework::ShapeManipulatorRequestBus::Events::SetTranslationOffset,
            expectedOffset);

        AZ::Entity gameEntity;
        editorComponent->BuildGameEntity(&gameEntity);
        const Jolt::ColliderComponent* collider = gameEntity.FindComponent<Jolt::ColliderComponent>();
        ASSERT_TRUE(collider);

        const AZStd::span<const ColliderShapeConfiguration> configurations =
            collider->GetShapeConfigurations();
        ASSERT_EQ(configurations.size(), 1);
        EXPECT_TRUE(configurations.front().m_localTransform.GetTranslation().IsClose(expectedOffset));

        ExpectColliderComponentMode(pair);
    }

    TEST(EditorComponentTests, RigidBodyBuildsRuntimeComponent)
    {
        RigidBodyComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::RigidBodyComponent>());
    }

    TEST(EditorComponentTests, HairBuildsRuntimeComponent)
    {
        HairComponent editorComponent;
        editorComponent.Init();
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::HairComponent>());
    }

    TEST(EditorComponentTests, HairDrawsAndSelectsAuthoredStrands)
    {
        HairComponent editorComponent;
        editorComponent.Init();
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3::CreateAxisZ()));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(drawnBounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(drawnBounds.GetMax()));
    }

    TEST(EditorComponentTests, RagdollBuildsRuntimeComponent)
    {
        RagdollComponent editorComponent;
        editorComponent.Init();
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::RagdollComponent>());
    }

    TEST(EditorComponentTests, RagdollDrawsAndSelectsAuthoredParts)
    {
        RagdollComponent editorComponent;
        editorComponent.Init();
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3(-0.25f)));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3(0.25f, 0.25f, 0.75f)));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(drawnBounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(drawnBounds.GetMax()));
    }

    TEST(EditorComponentTests, SkeletonBuildsRuntimeComponent)
    {
        SkeletonComponent editorComponent;
        editorComponent.Init();
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::SkeletonComponent>());
    }

    TEST(EditorComponentTests, SoftBodyBuildsRuntimeComponent)
    {
        SoftBodyComponentConfiguration configuration = SoftBodyComponentConfiguration::CreateDefault();
        configuration.m_body.m_userData = 0x1234'5678'9abc'def0;
        SoftBodyComponent editorComponent(AZStd::move(configuration));
        editorComponent.Init();
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        const auto* runtimeComponent = gameEntity.FindComponent<Jolt::SoftBodyComponent>();
        ASSERT_TRUE(runtimeComponent);
        EXPECT_EQ(runtimeComponent->GetUserData(), 0x1234'5678'9abc'def0);
    }

    TEST(EditorComponentTests, SoftBodyDrawsAndSelectsAuthoredFaces)
    {
        SoftBodyComponent editorComponent;
        editorComponent.Init();
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3(-0.5f, 0.0f, -0.5f)));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3(0.5f, 0.0f, 0.5f)));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(drawnBounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(drawnBounds.GetMax()));
    }

    TEST(EditorComponentTests, SoftBodyVisualizesSkinJointsDistancesAndBackstops)
    {
        SoftBodyComponentConfiguration configuration = SoftBodyComponentConfiguration::CreateDefault();
        configuration.m_definition.m_inverseBinds = {
            {},
        };
        configuration.m_definition.m_skinConstraints.resize(configuration.m_definition.m_vertices.size());
        for (AZ::u32 vertexIndex = 0; vertexIndex < configuration.m_definition.m_skinConstraints.size(); ++vertexIndex)
        {
            SoftBodySkinConstraint& constraint = configuration.m_definition.m_skinConstraints[vertexIndex];
            constraint.m_vertex = vertexIndex;
            constraint.m_weights[0] = {
                .m_inverseBindIndex = 0,
                .m_weight = 1.0f,
            };
        }
        SoftBodySkinConstraint& constrainedVertex = configuration.m_definition.m_skinConstraints[0];
        constrainedVertex.m_backstopDistance = 0.1f;
        constrainedVertex.m_backstopRadius = 0.2f;
        constrainedVertex.m_maximumDistance = 0.25f;

        SoftBodyComponent editorComponent(AZStd::move(configuration));
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        EXPECT_GT(debugDisplay.GetPoints().size(), 12);
        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(AZ::Vector3(-0.75f, -0.25f, -0.5f)));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(AZ::Vector3(0.5f, 0.5f, 0.75f)));
    }

    TEST(EditorComponentTests, PathBuildsRuntimeComponent)
    {
        PathComponent editorComponent;
        editorComponent.Init();
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        Jolt::PathComponent* runtimeComponent = gameEntity.FindComponent<Jolt::PathComponent>();
        ASSERT_TRUE(runtimeComponent);
        EXPECT_EQ(runtimeComponent->GetConfiguration().m_points.size(), 2);
        EXPECT_TRUE(
            runtimeComponent->GetConfiguration().m_points.front().m_position.IsClose(
                AZ::Vector3(-0.5f, 0.0f, 0.0f)));
        EXPECT_TRUE(
            runtimeComponent->GetConfiguration().m_points.back().m_position.IsClose(
                AZ::Vector3(0.5f, 0.0f, 0.0f)));
    }

    TEST(EditorComponentTests, PathDefaultIsVisibleAndSelectable)
    {
        PathComponent editorComponent;
        editorComponent.Init();
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3(-0.5f, 0.0f, 0.0f)));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3(0.5f, 0.0f, 0.0f)));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(drawnBounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(drawnBounds.GetMax()));
    }

    TEST(EditorComponentTests, PathDrawsAuthoredHermiteCurve)
    {
        HermitePathConfiguration configuration;
        configuration.m_points = {
            HermitePathPoint{
                .m_position = AZ::Vector3::CreateZero(),
            },
            HermitePathPoint{
                .m_position = AZ::Vector3::CreateAxisX(2.0f),
            },
        };
        PathComponent editorComponent(configuration);
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3::CreateAxisX(2.0f)));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.GetMin().IsClose(drawnBounds.GetMin()));
        EXPECT_TRUE(selectionBounds.GetMax().IsClose(drawnBounds.GetMax()));
    }

    TEST(EditorComponentTests, ConstraintBuildsRuntimeComponent)
    {
        ConstraintComponentConfiguration configuration;
        configuration.m_userData = 0x1234'5678'9abc'def0;
        ConstraintComponent editorComponent(AZStd::move(configuration));
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        const auto* runtimeComponent = gameEntity.FindComponent<Jolt::ConstraintComponent>();
        ASSERT_TRUE(runtimeComponent);
        EXPECT_EQ(runtimeComponent->GetUserData(), 0x1234'5678'9abc'def0);
    }

    TEST(EditorComponentTests, ConstraintDrawsAndSelectsAuthoredFrames)
    {
        ConstraintComponent editorComponent;
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3::CreateAxisX()));

        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(selectionBounds.IsValid());
        EXPECT_TRUE(selectionBounds.Contains(drawnBounds));
        float distance = 0.0f;
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            {0},
            AZ::Vector3::CreateAxisX(-2.0f),
            AZ::Vector3::CreateAxisX(),
            distance));
        EXPECT_NEAR(distance, 1.9f, 1.0e-4f);
    }

    TEST(EditorComponentTests, ConstraintDrawsEveryAuthoredGeometryWithoutRuntimeState)
    {
        const AZStd::vector<ConstraintComponentGeometry> geometries = {
            ConeConstraintConfiguration{},
            CustomConstraintConfiguration{},
            DistanceConstraintConfiguration{},
            FixedConstraintConfiguration{},
            GearConstraintComponentConfiguration{},
            HingeConstraintConfiguration{},
            PathConstraintComponentConfiguration{},
            PointConstraintConfiguration{},
            PulleyConstraintConfiguration{},
            RackAndPinionConstraintComponentConfiguration{},
            SixDofConstraintConfiguration{},
            SliderConstraintConfiguration{},
            SwingTwistConstraintConfiguration{},
        };

        for (const ConstraintComponentGeometry& geometry : geometries)
        {
            ConstraintComponentConfiguration configuration;
            configuration.m_geometry = geometry;
            ConstraintComponent editorComponent(AZStd::move(configuration));
            UnitTest::TestDebugDisplayRequests debugDisplay;

            editorComponent.DisplayEntityViewport({0}, debugDisplay);

            EXPECT_TRUE(debugDisplay.GetAabb().IsValid());
            EXPECT_TRUE(editorComponent.GetEditorSelectionBoundsViewport({0}).IsValid());
        }
    }

    TEST(EditorComponentTests, PulleyConstraintSelectionOnlyContainsAuthoredRoute)
    {
        ConstraintComponentConfiguration configuration;
        configuration.m_geometry = PulleyConstraintConfiguration{
            .m_firstBodyPoint = {.m_x = 10.0},
            .m_firstFixedPoint = {.m_x = 10.0, .m_y = 1.0},
            .m_secondBodyPoint = {.m_x = 12.0},
            .m_secondFixedPoint = {.m_x = 12.0, .m_y = 1.0},
            .m_space = ConstraintSpace::World,
        };
        ConstraintComponent editorComponent(AZStd::move(configuration));
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        EXPECT_TRUE(drawnBounds.GetMin().IsClose(AZ::Vector3(10.0f, 0.0f, 0.0f)));
        EXPECT_TRUE(drawnBounds.GetMax().IsClose(AZ::Vector3(12.0f, 1.0f, 0.0f)));
        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_GT(selectionBounds.GetMin().GetX(), 9.0f);
    }

    TEST(EditorComponentTests, StaticRigidBodyBuildsRuntimeComponent)
    {
        StaticRigidBodyComponent editorComponent;
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::StaticRigidBodyComponent>());
    }

    TEST(EditorComponentTests, VehicleComponentsBuildSpecializedRuntimeComponents)
    {
        WheeledVehicleComponent wheeled;
        MotorcycleComponent motorcycle;
        TrackedVehicleComponent tracked;
        AZ::Entity gameEntity;

        wheeled.Init();
        motorcycle.Init();
        tracked.Init();
        wheeled.BuildGameEntity(&gameEntity);
        motorcycle.BuildGameEntity(&gameEntity);
        tracked.BuildGameEntity(&gameEntity);

        EXPECT_TRUE(gameEntity.FindComponent<Jolt::WheeledVehicleComponent>());
        EXPECT_TRUE(gameEntity.FindComponent<Jolt::MotorcycleComponent>());
        EXPECT_TRUE(gameEntity.FindComponent<Jolt::TrackedVehicleComponent>());
    }

    TEST(EditorComponentTests, VehicleComponentsDrawUsableAuthoredDefaults)
    {
        WheeledVehicleComponent wheeled;
        MotorcycleComponent motorcycle;
        TrackedVehicleComponent tracked;
        UnitTest::TestDebugDisplayRequests wheeledDisplay;
        UnitTest::TestDebugDisplayRequests motorcycleDisplay;
        UnitTest::TestDebugDisplayRequests trackedDisplay;

        wheeled.Init();
        motorcycle.Init();
        tracked.Init();
        wheeled.DisplayEntityViewport({0}, wheeledDisplay);
        motorcycle.DisplayEntityViewport({0}, motorcycleDisplay);
        tracked.DisplayEntityViewport({0}, trackedDisplay);

        EXPECT_TRUE(wheeledDisplay.GetAabb().IsValid());
        EXPECT_TRUE(motorcycleDisplay.GetAabb().IsValid());
        EXPECT_TRUE(trackedDisplay.GetAabb().IsValid());
        EXPECT_TRUE(wheeled.GetEditorSelectionBoundsViewport({0}).IsValid());
        EXPECT_TRUE(motorcycle.GetEditorSelectionBoundsViewport({0}).IsValid());
        EXPECT_TRUE(tracked.GetEditorSelectionBoundsViewport({0}).IsValid());
    }

    TEST(EditorComponentTests, WheeledVehicleSelectionOnlyContainsAuthoredWheelAssembly)
    {
        WheeledVehicleComponentConfiguration configuration;
        configuration.m_vehicle.m_wheels = {
            WheelConfiguration{
                .m_position = AZ::Vector3(10.0f, 0.0f, 0.0f),
                .m_suspensionMaximumLength = 1.0f,
                .m_suspensionMinimumLength = 0.5f,
            },
        };
        configuration.m_vehicle.m_differentials = {
            VehicleDifferentialConfiguration{
                .m_leftWheel = 0,
                .m_leftRightSplit = 1.0f,
            },
        };
        WheeledVehicleComponent editorComponent(AZStd::move(configuration));
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_GT(drawnBounds.GetMin().GetX(), 9.0f);
        EXPECT_GT(selectionBounds.GetMin().GetX(), 9.0f);
        EXPECT_TRUE(selectionBounds.Contains(drawnBounds));
        float distance = 0.0f;
        EXPECT_TRUE(editorComponent.EditorSelectionIntersectRayViewport(
            {0},
            AZ::Vector3(8.0f, 0.0f, -1.0f),
            AZ::Vector3::CreateAxisX(),
            distance));
    }

    TEST(EditorComponentTests, TrackedVehicleDrawsClosedAuthoredTrackRoutes)
    {
        TrackedVehicleComponentConfiguration configuration;
        configuration.m_vehicle.m_wheels = {
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(10.0f, 1.0f, 0.0f),
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(10.0f, -1.0f, 0.0f),
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(12.0f, 1.0f, 0.0f),
                },
            },
            TrackedWheelConfiguration{
                .m_common = WheelConfiguration{
                    .m_position = AZ::Vector3(12.0f, -1.0f, 0.0f),
                },
            },
        };
        configuration.m_vehicle.m_tracks = {
            VehicleTrackConfiguration{
                .m_wheels = {0, 1},
                .m_drivenWheel = 1,
            },
            VehicleTrackConfiguration{
                .m_wheels = {2, 3},
                .m_drivenWheel = 3,
            },
        };
        TrackedVehicleComponent editorComponent(AZStd::move(configuration));
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_GT(drawnBounds.GetMin().GetX(), 9.0f);
        EXPECT_GT(selectionBounds.GetMin().GetX(), 9.0f);
        EXPECT_TRUE(selectionBounds.Contains(drawnBounds));
    }

    TEST(EditorComponentTests, VirtualCharacterBuildsRuntimeComponent)
    {
        VirtualCharacterComponentConfiguration configuration;
        configuration.m_userData = 0x1234'5678'9abc'def0;
        VirtualCharacterControllerComponent editorComponent(AZStd::move(configuration));
        AZ::Entity gameEntity;

        editorComponent.BuildGameEntity(&gameEntity);

        const auto* runtimeComponent = gameEntity.FindComponent<Jolt::VirtualCharacterControllerComponent>();
        ASSERT_TRUE(runtimeComponent);
        EXPECT_EQ(runtimeComponent->GetUserData(), 0x1234'5678'9abc'def0);
    }

    TEST(EditorComponentTests, VirtualCharacterDrawsShapeOffsetAndStepGuides)
    {
        VirtualCharacterComponentConfiguration configuration;
        configuration.m_shapeOffset = AZ::Vector3::CreateAxisX(2.0f);
        configuration.m_update.m_stickToFloorStepDown = -AZ::Vector3::CreateAxisZ(0.75f);
        configuration.m_update.m_walkStairsStepDownExtra = -AZ::Vector3::CreateAxisZ(0.25f);
        configuration.m_update.m_walkStairsStepUp = AZ::Vector3::CreateAxisZ(0.5f);
        VirtualCharacterControllerComponent editorComponent(AZStd::move(configuration));
        UnitTest::TestDebugDisplayRequests debugDisplay;

        editorComponent.DisplayEntityViewport({0}, debugDisplay);

        const AZ::Aabb drawnBounds = debugDisplay.GetAabb();
        const AZ::Aabb selectionBounds = editorComponent.GetEditorSelectionBoundsViewport({0});
        EXPECT_TRUE(drawnBounds.IsValid());
        EXPECT_TRUE(selectionBounds.Contains(drawnBounds));
        EXPECT_GT(drawnBounds.GetMax().GetX(), 2.0f);
        EXPECT_LT(drawnBounds.GetMin().GetZ(), -0.7f);
        EXPECT_GT(drawnBounds.GetMax().GetZ(), 0.9f);
    }

    TEST(EditorComponentTests, CustomConvexShapeUsesAuthoredEditorBounds)
    {
        const AZ::Aabb localBounds = AZ::Aabb::CreateFromMinMax(
            {-1.0f, -2.0f, -3.0f},
            {1.0f, 2.0f, 3.0f});
        const ShapeGeometry geometry = CustomConvexShapeConfiguration{
            .m_editorBounds = localBounds,
            .m_providerId = CustomConvexShapeConfigurationTypeId,
        };
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateTranslation(
            AZ::Vector3(4.0f, 5.0f, 6.0f));

        const AZ::Aabb bounds = CalculateShapeBounds(geometry, transform);

        EXPECT_TRUE(bounds.IsClose(AZ::Aabb::CreateFromMinMax(
            {3.0f, 3.0f, 3.0f},
            {5.0f, 7.0f, 9.0f})));
    }

} // namespace Jolt::Editor
