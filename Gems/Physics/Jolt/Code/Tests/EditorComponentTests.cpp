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
#include <Jolt/Editor/AssetBuilder.h>
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
#include <AzTest/Utils.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Input/System/InputSystemComponent.h>
#include <AzFramework/UnitTest/TestDebugDisplayRequests.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Manipulators/BoxManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CapsuleManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/CylinderManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/RadiusManipulatorRequestBus.h>
#include <AzToolsFramework/Manipulators/ShapeManipulatorRequestBus.h>
#include <AzToolsFramework/Prefab/Instance/Instance.h>
#include <AzToolsFramework/Prefab/Instance/InstanceToTemplateInterface.h>
#include <AzToolsFramework/Prefab/Instance/InstanceUpdateExecutorInterface.h>
#include <AzToolsFramework/Prefab/PrefabLoaderInterface.h>
#include <AzToolsFramework/Prefab/PrefabSystemComponent.h>
#include <AzToolsFramework/Prefab/Undo/PrefabUndo.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>

namespace Jolt::Editor
{
    namespace
    {
        char TestExecutableName[] = "JoltEditorTests";
        char TestEnginePathOption[] = "--engine-path";
        char TestEngineRoot[] = JOLT_TEST_ENGINE_ROOT;
        char* TestApplicationArguments[] = {
            TestExecutableName,
            TestEnginePathOption,
            TestEngineRoot,
        };
        constexpr int TestApplicationArgumentCount = 3;

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

        class GeometryDrawRecorder final
            : public UnitTest::NullDebugDisplayRequests
        {
        public:
            using UnitTest::NullDebugDisplayRequests::DrawLine;

            void DrawLine(
                [[maybe_unused]] const AZ::Vector3& first,
                [[maybe_unused]] const AZ::Vector3& second) override
            {
                ++m_lineCount;
            }

            void DrawPoint(
                const AZ::Vector3& point,
                [[maybe_unused]] const int size) override
            {
                m_points.push_back(point);
            }

            AZStd::vector<AZ::Vector3> m_points;
            AZ::u32 m_lineCount = 0;
        };

        class HeadlessComponentModeTestApplication final
            : public UnitTest::ToolsTestApplication
        {
        public:
            HeadlessComponentModeTestApplication()
                : UnitTest::ToolsTestApplication(
                    "JoltComponentModeTests",
                    TestApplicationArgumentCount,
                    TestApplicationArguments)
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

        class PrefabComponentTests
            : public UnitTest::ToolsApplicationFixture<>
        {
        public:
            ~PrefabComponentTests() override
            {
                // A tools application can release process caches created by earlier suites,
                // so only net growth indicates a fixture leak.
                AZ::GetGlobalSerializeContextModule().Cleanup();
                AZ::AllocatorManager::Instance().GarbageCollect();

                AllocatedSizesMap allocatedSizesAfterTest = GetAllocatedSizes();
                for (const auto& [allocator, sizeAfterTestRan] : allocatedSizesAfterTest)
                {
                    size_t sizeBeforeTestRan = 0;
                    if (const auto allocationIterator = m_allocatedSizes.find(allocator);
                        allocationIterator != m_allocatedSizes.end())
                    {
                        sizeBeforeTestRan = allocationIterator->second;
                    }

                    EXPECT_LE(sizeAfterTestRan, sizeBeforeTestRan)
                        << "for allocator " << allocator->GetName();
                }

                m_allocatedSizes = AZStd::move(allocatedSizesAfterTest);
            }

        protected:
            AZStd::unique_ptr<UnitTest::ToolsTestApplication> CreateTestApplication() override
            {
                return AZStd::make_unique<UnitTest::ToolsTestApplication>(
                    "JoltPrefabTests",
                    TestApplicationArgumentCount,
                    TestApplicationArguments);
            }

            void SetUpEditorFixtureImpl() override
            {
                AZ::SerializeContext* serializeContext =
                    GetApplication()->GetSerializeContext();
                SystemComponent::Reflect(serializeContext);
                Jolt::CharacterControllerComponent::Reflect(serializeContext);
                Jolt::ColliderComponent::Reflect(serializeContext);
                Jolt::ConstraintComponent::Reflect(serializeContext);
                Jolt::HairComponent::Reflect(serializeContext);
                Jolt::PathComponent::Reflect(serializeContext);
                Jolt::RagdollComponent::Reflect(serializeContext);
                Jolt::RigidBodyComponent::Reflect(serializeContext);
                Jolt::SceneComponent::Reflect(serializeContext);
                Jolt::SkeletonComponent::Reflect(serializeContext);
                Jolt::SoftBodyComponent::Reflect(serializeContext);
                Jolt::StaticRigidBodyComponent::Reflect(serializeContext);
                Jolt::WheeledVehicleComponent::Reflect(serializeContext);
                Jolt::MotorcycleComponent::Reflect(serializeContext);
                Jolt::TrackedVehicleComponent::Reflect(serializeContext);
                Jolt::VirtualCharacterControllerComponent::Reflect(serializeContext);
                SystemComponent::Reflect(GetApplication()->GetBehaviorContext());
                SystemComponent::Reflect(GetApplication()->GetJsonRegistrationContext());
                GetApplication()->RegisterComponentDescriptor(
                    CharacterControllerComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    ColliderComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    ConstraintComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    HairComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    PathComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    RagdollComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    RigidBodyComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    SceneComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    SkeletonComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    SoftBodyComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    StaticRigidBodyComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    WheeledVehicleComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    MotorcycleComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    TrackedVehicleComponent::CreateDescriptor());
                GetApplication()->RegisterComponentDescriptor(
                    VirtualCharacterControllerComponent::CreateDescriptor());

                AZ::Entity* systemEntity = GetApplication()->FindEntity(AZ::SystemEntityId);
                ASSERT_TRUE(systemEntity);
                m_prefabSystem = systemEntity->FindComponent<AzToolsFramework::Prefab::PrefabSystemComponent>();
                ASSERT_TRUE(m_prefabSystem);
                m_prefabLoader = AZ::Interface<AzToolsFramework::Prefab::PrefabLoaderInterface>::Get();
                ASSERT_TRUE(m_prefabLoader);
                m_instanceToTemplate =
                    AZ::Interface<AzToolsFramework::Prefab::InstanceToTemplateInterface>::Get();
                ASSERT_TRUE(m_instanceToTemplate);
                m_instanceUpdateExecutor =
                    AZ::Interface<AzToolsFramework::Prefab::InstanceUpdateExecutorInterface>::Get();
                ASSERT_TRUE(m_instanceUpdateExecutor);
            }

            void TearDownEditorFixtureImpl() override
            {
                AZ::JsonRegistrationContext* jsonContext = GetApplication()->GetJsonRegistrationContext();
                jsonContext->EnableRemoveReflection();
                SystemComponent::Reflect(jsonContext);
                jsonContext->DisableRemoveReflection();
            }

            AzToolsFramework::Prefab::PrefabSystemComponent* m_prefabSystem = nullptr;
            AzToolsFramework::Prefab::PrefabLoaderInterface* m_prefabLoader = nullptr;
            AzToolsFramework::Prefab::InstanceToTemplateInterface* m_instanceToTemplate = nullptr;
            AzToolsFramework::Prefab::InstanceUpdateExecutorInterface* m_instanceUpdateExecutor = nullptr;
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

        template<typename Component>
        void ExpectInitPreservesSerializedState(
            Component& component,
            AZ::SerializeContext& serializeContext)
        {
            const auto serialize = [&serializeContext](Component& value)
            {
                AZStd::vector<char> buffer;
                AZ::IO::ByteContainerStream stream(&buffer);
                EXPECT_TRUE(AZ::Utils::SaveObjectToStream(
                    stream,
                    AZ::DataStream::ST_BINARY,
                    &value,
                    &serializeContext));
                return buffer;
            };

            const AZStd::vector<char> beforeInit = serialize(component);
            component.Init();
            EXPECT_EQ(serialize(component), beforeInit);
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

    TEST(EditorComponentTests, EveryAuthorableComponentPreservesNonDefaultConfiguration)
    {
        NameDictionaryScope nameDictionaryScope;
        {
            AZ::SerializeContext serializeContext;
            serializeContext.CreateEditContext();
            AZ::Data::AssetData::Reflect(&serializeContext);
            AZ::Entity::Reflect(&serializeContext);
            AZ::Name::Reflect(&serializeContext);
            AzToolsFramework::Components::EditorComponentBase::Reflect(&serializeContext);
            AzToolsFramework::ComponentModeFramework::ComponentModeDelegate::Reflect(&serializeContext);
            SystemComponent::Reflect(&serializeContext);
            Jolt::CharacterControllerComponent::Reflect(&serializeContext);
            Jolt::ColliderComponent::Reflect(&serializeContext);
            Jolt::ConstraintComponent::Reflect(&serializeContext);
            Jolt::HairComponent::Reflect(&serializeContext);
            Jolt::PathComponent::Reflect(&serializeContext);
            Jolt::RagdollComponent::Reflect(&serializeContext);
            Jolt::RigidBodyComponent::Reflect(&serializeContext);
            Jolt::SceneComponent::Reflect(&serializeContext);
            Jolt::SkeletonComponent::Reflect(&serializeContext);
            Jolt::SoftBodyComponent::Reflect(&serializeContext);
            Jolt::StaticRigidBodyComponent::Reflect(&serializeContext);
            Jolt::WheeledVehicleComponent::Reflect(&serializeContext);
            Jolt::MotorcycleComponent::Reflect(&serializeContext);
            Jolt::TrackedVehicleComponent::Reflect(&serializeContext);
            Jolt::VirtualCharacterControllerComponent::Reflect(&serializeContext);
            CharacterControllerComponent::Reflect(&serializeContext);
            ColliderComponent::Reflect(&serializeContext);
            ConstraintComponent::Reflect(&serializeContext);
            HairComponent::Reflect(&serializeContext);
            PathComponent::Reflect(&serializeContext);
            RagdollComponent::Reflect(&serializeContext);
            RigidBodyComponent::Reflect(&serializeContext);
            SceneComponent::Reflect(&serializeContext);
            SkeletonComponent::Reflect(&serializeContext);
            SoftBodyComponent::Reflect(&serializeContext);
            StaticRigidBodyComponent::Reflect(&serializeContext);
            WheeledVehicleComponent::Reflect(&serializeContext);
            MotorcycleComponent::Reflect(&serializeContext);
            TrackedVehicleComponent::Reflect(&serializeContext);
            VirtualCharacterControllerComponent::Reflect(&serializeContext);

            CharacterComponentConfiguration characterConfiguration;
            characterConfiguration.m_userData = 0x0102'0304'0506'0708;
            characterConfiguration.m_mass = 91.0f;
            characterConfiguration.m_enhancedInternalEdgeRemoval = true;
            CharacterControllerComponent character(AZStd::move(characterConfiguration));
            ExpectBinaryRoundTrip(character, serializeContext);

            ColliderShapeConfiguration colliderShape;
            colliderShape.m_shape.m_geometry = CapsuleShapeConfiguration{
                .m_cylinderHeight = 2.5f,
                .m_radius = 0.75f,
            };
            colliderShape.m_shape.m_userData = 0x1112'1314'1516'1718;
            colliderShape.m_localTransform =
                AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f));
            ColliderComponent collider({colliderShape});
            ExpectBinaryRoundTrip(collider, serializeContext);

            ConstraintComponentConfiguration constraintConfiguration;
            constraintConfiguration.m_geometry = HingeConstraintConfiguration{
                .m_maximumLimit = 0.75f,
                .m_minimumLimit = -0.5f,
            };
            constraintConfiguration.m_userData = 0x2122'2324'2526'2728;
            constraintConfiguration.m_priority = 7;
            ConstraintComponent constraint(AZStd::move(constraintConfiguration));
            ExpectBinaryRoundTrip(constraint, serializeContext);

            HairComponentConfiguration hairConfiguration = HairComponentConfiguration::CreateDefault();
            hairConfiguration.m_jointModelTransforms = {
                AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(0.25f)),
            };
            hairConfiguration.m_jointToHair =
                AZ::Transform::CreateRotationZ(AZ::Constants::QuarterPi);
            hairConfiguration.m_autoUpdate = false;
            HairComponent hair(AZStd::move(hairConfiguration));
            ExpectBinaryRoundTrip(hair, serializeContext);

            HermitePathConfiguration pathConfiguration;
            pathConfiguration.m_points = {
                {
                    .m_position = AZ::Vector3(-1.0f, 0.0f, 0.5f),
                    .m_tangent = AZ::Vector3::CreateAxisX(2.0f),
                },
                {
                    .m_position = AZ::Vector3(2.0f, 1.0f, 0.5f),
                    .m_tangent = AZ::Vector3::CreateAxisY(2.0f),
                },
            };
            pathConfiguration.m_isLooping = true;
            PathComponent path(AZStd::move(pathConfiguration));
            ExpectBinaryRoundTrip(path, serializeContext);

            RagdollComponentConfiguration ragdollConfiguration =
                RagdollComponentConfiguration::CreateDefault();
            ragdollConfiguration.m_baseConstraintPriority = 5;
            ragdollConfiguration.m_minimumCollisionSeparation = 0.125f;
            ragdollConfiguration.m_stabilize = false;
            const auto appendRagdollConstraint = [&ragdollConfiguration](RagdollConstraintComponentGeometry geometry)
            {
                AdditionalRagdollConstraintComponentConfiguration constraint;
                constraint.m_constraint.m_geometry = AZStd::move(geometry);
                constraint.m_firstPartIndex = 0;
                constraint.m_secondPartIndex = 1;
                ragdollConfiguration.m_additionalConstraints.push_back(AZStd::move(constraint));
            };
            appendRagdollConstraint(ConeConstraintConfiguration{.m_halfConeAngle = 0.25f});
            appendRagdollConstraint(CustomConstraintConfiguration{
                .m_data = {1, 2, 3},
                .m_providerId = CustomConstraintConfigurationTypeId,
            });
            appendRagdollConstraint(DistanceConstraintConfiguration{
                .m_maximumDistance = 2.0f,
                .m_minimumDistance = 0.5f,
            });
            appendRagdollConstraint(FixedConstraintConfiguration{.m_autoDetectPoint = false});
            appendRagdollConstraint(RagdollGearConstraintConfiguration{.m_ratio = 2.0f});
            appendRagdollConstraint(HingeConstraintConfiguration{
                .m_maximumLimit = 0.75f,
                .m_minimumLimit = -0.5f,
            });
            appendRagdollConstraint(PathConstraintComponentConfiguration{
                .m_pathEntityId = AZ::EntityId(0x1234),
                .m_pathPosition = AZ::Vector3::CreateAxisX(),
            });
            appendRagdollConstraint(PointConstraintConfiguration{
                .m_firstPoint = {.m_x = 1.0},
                .m_secondPoint = {.m_y = 2.0},
            });
            appendRagdollConstraint(PulleyConstraintConfiguration{
                .m_maximumLength = 5.0f,
                .m_minimumLength = 1.0f,
            });
            appendRagdollConstraint(RagdollRackAndPinionConstraintConfiguration{.m_ratio = 3.0f});
            appendRagdollConstraint(SixDofConstraintConfiguration{
                .m_translationX = {
                    .m_maximumLimit = 1.0f,
                    .m_minimumLimit = -1.0f,
                    .m_mode = SixDofAxisMode::Limited,
                },
            });
            appendRagdollConstraint(SliderConstraintConfiguration{
                .m_maximumLimit = 1.5f,
                .m_minimumLimit = -1.5f,
            });
            appendRagdollConstraint(SwingTwistConstraintConfiguration{
                .m_normalHalfConeAngle = 0.25f,
                .m_planeHalfConeAngle = 0.5f,
            });
            RagdollComponent ragdoll(AZStd::move(ragdollConfiguration));
            ExpectBinaryRoundTrip(ragdoll, serializeContext);

            RigidBodyConfiguration rigidBodyConfiguration;
            rigidBodyConfiguration.m_initialLinearVelocity = AZ::Vector3(1.0f, 2.0f, 3.0f);
            rigidBodyConfiguration.m_initialAngularVelocity = AZ::Vector3(-1.0f, -2.0f, -3.0f);
            rigidBodyConfiguration.m_userData = 0x3132'3334'3536'3738;
            rigidBodyConfiguration.m_allowDynamicOrKinematic = true;
            RigidBodyComponent rigidBody(AZStd::move(rigidBodyConfiguration));
            ExpectBinaryRoundTrip(rigidBody, serializeContext);

            SceneComponentConfiguration sceneConfiguration;
            sceneConfiguration.m_asset = AZ::Data::Asset<SceneAsset>(
                AZ::Data::AssetId(AZ::Uuid("{23BB23A5-AD82-449E-9652-AF9BB3A14091}"), 3),
                SceneAssetTypeId,
                "Physics/Jolt/EditorScene.jolt");
            sceneConfiguration.m_asset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::NoLoad);
            SceneComponent scene(AZStd::move(sceneConfiguration));
            ExpectBinaryRoundTrip(scene, serializeContext);

            SkeletonComponentConfiguration skeletonConfiguration;
            skeletonConfiguration.m_asset = AZ::Data::Asset<SkeletonAsset>(
                AZ::Data::AssetId(AZ::Uuid("{51F05DB1-64F0-4759-B954-289F95DE9FE8}"), 4),
                SkeletonAssetTypeId,
                "Physics/Jolt/EditorSkeleton.jolt");
            skeletonConfiguration.m_asset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::NoLoad);
            SkeletonComponent skeleton(AZStd::move(skeletonConfiguration));
            ExpectBinaryRoundTrip(skeleton, serializeContext);

            SoftBodyComponentConfiguration softBodyConfiguration =
                SoftBodyComponentConfiguration::CreateDefault();
            softBodyConfiguration.m_body.m_userData = 0x4142'4344'4546'4748;
            softBodyConfiguration.m_body.m_pressure = 0.25f;
            softBodyConfiguration.m_enabled = false;
            SoftBodyComponent softBody(AZStd::move(softBodyConfiguration));
            ExpectBinaryRoundTrip(softBody, serializeContext);

            StaticRigidBodyConfiguration staticRigidBodyConfiguration;
            staticRigidBodyConfiguration.m_userData = 0x5152'5354'5556'5758;
            staticRigidBodyConfiguration.m_friction = 0.7f;
            staticRigidBodyConfiguration.m_restitution = 0.4f;
            staticRigidBodyConfiguration.m_isSensor = true;
            StaticRigidBodyComponent staticRigidBody(AZStd::move(staticRigidBodyConfiguration));
            ExpectBinaryRoundTrip(staticRigidBody, serializeContext);

            WheeledVehicleComponentConfiguration wheeledConfiguration =
                WheeledVehicleComponentConfiguration::CreateDefault();
            wheeledConfiguration.m_enabled = false;
            wheeledConfiguration.m_vehicle.m_collisionSphereRadius = 0.45f;
            wheeledConfiguration.m_vehicle.m_gravityOverride = AZ::Vector3(1.0f, 2.0f, 3.0f);
            wheeledConfiguration.m_vehicle.m_overrideGravity = true;
            WheeledVehicleComponent wheeled(AZStd::move(wheeledConfiguration));
            ExpectBinaryRoundTrip(wheeled, serializeContext);

            MotorcycleComponentConfiguration motorcycleConfiguration =
                MotorcycleComponentConfiguration::CreateDefault();
            motorcycleConfiguration.m_enabled = false;
            motorcycleConfiguration.m_motorcycle.m_wheeled.m_gravityOverride = AZ::Vector3(-2.0f, -3.0f, -4.0f);
            motorcycleConfiguration.m_motorcycle.m_wheeled.m_overrideGravity = true;
            motorcycleConfiguration.m_motorcycle.m_controller.m_maximumLeanAngle = 0.5f;
            MotorcycleComponent motorcycle(AZStd::move(motorcycleConfiguration));
            ExpectBinaryRoundTrip(motorcycle, serializeContext);

            TrackedVehicleComponentConfiguration trackedConfiguration =
                TrackedVehicleComponentConfiguration::CreateDefault();
            trackedConfiguration.m_enabled = false;
            trackedConfiguration.m_vehicle.m_collisionSphereRadius = 0.55f;
            trackedConfiguration.m_vehicle.m_gravityOverride = AZ::Vector3(5.0f, 6.0f, 7.0f);
            trackedConfiguration.m_vehicle.m_overrideGravity = true;
            TrackedVehicleComponent tracked(AZStd::move(trackedConfiguration));
            ExpectBinaryRoundTrip(tracked, serializeContext);

            HairComponentConfiguration emptyHairConfiguration;
            emptyHairConfiguration.m_autoUpdate = false;
            HairComponent emptyHair(AZStd::move(emptyHairConfiguration));
            ExpectInitPreservesSerializedState(emptyHair, serializeContext);

            HermitePathConfiguration emptyPathConfiguration;
            emptyPathConfiguration.m_isLooping = true;
            PathComponent emptyPath(AZStd::move(emptyPathConfiguration));
            ExpectInitPreservesSerializedState(emptyPath, serializeContext);

            RagdollComponentConfiguration emptyRagdollConfiguration;
            emptyRagdollConfiguration.m_stabilize = false;
            RagdollComponent emptyRagdoll(AZStd::move(emptyRagdollConfiguration));
            ExpectInitPreservesSerializedState(emptyRagdoll, serializeContext);

            SoftBodyComponentConfiguration emptySoftBodyConfiguration;
            emptySoftBodyConfiguration.m_enabled = false;
            SoftBodyComponent emptySoftBody(AZStd::move(emptySoftBodyConfiguration));
            ExpectInitPreservesSerializedState(emptySoftBody, serializeContext);

            WheeledVehicleComponentConfiguration emptyWheeledConfiguration;
            emptyWheeledConfiguration.m_enabled = false;
            WheeledVehicleComponent emptyWheeled(AZStd::move(emptyWheeledConfiguration));
            ExpectInitPreservesSerializedState(emptyWheeled, serializeContext);

            MotorcycleComponentConfiguration emptyMotorcycleConfiguration;
            emptyMotorcycleConfiguration.m_enabled = false;
            MotorcycleComponent emptyMotorcycle(AZStd::move(emptyMotorcycleConfiguration));
            ExpectInitPreservesSerializedState(emptyMotorcycle, serializeContext);

            TrackedVehicleComponentConfiguration emptyTrackedConfiguration;
            emptyTrackedConfiguration.m_enabled = false;
            TrackedVehicleComponent emptyTracked(AZStd::move(emptyTrackedConfiguration));
            ExpectInitPreservesSerializedState(emptyTracked, serializeContext);

            VirtualCharacterComponentConfiguration virtualCharacterConfiguration;
            virtualCharacterConfiguration.m_userData = 0x9192'9394'9596'9798;
            virtualCharacterConfiguration.m_mass = 82.0f;
            virtualCharacterConfiguration.m_createInnerBody = true;
            VirtualCharacterControllerComponent virtualCharacter(
                AZStd::move(virtualCharacterConfiguration));
            ExpectBinaryRoundTrip(virtualCharacter, serializeContext);

            serializeContext.EnableRemoveReflection();
            VirtualCharacterControllerComponent::Reflect(&serializeContext);
            TrackedVehicleComponent::Reflect(&serializeContext);
            MotorcycleComponent::Reflect(&serializeContext);
            WheeledVehicleComponent::Reflect(&serializeContext);
            StaticRigidBodyComponent::Reflect(&serializeContext);
            SoftBodyComponent::Reflect(&serializeContext);
            SkeletonComponent::Reflect(&serializeContext);
            SceneComponent::Reflect(&serializeContext);
            RigidBodyComponent::Reflect(&serializeContext);
            RagdollComponent::Reflect(&serializeContext);
            PathComponent::Reflect(&serializeContext);
            HairComponent::Reflect(&serializeContext);
            ConstraintComponent::Reflect(&serializeContext);
            ColliderComponent::Reflect(&serializeContext);
            CharacterControllerComponent::Reflect(&serializeContext);
            Jolt::VirtualCharacterControllerComponent::Reflect(&serializeContext);
            Jolt::TrackedVehicleComponent::Reflect(&serializeContext);
            Jolt::MotorcycleComponent::Reflect(&serializeContext);
            Jolt::WheeledVehicleComponent::Reflect(&serializeContext);
            Jolt::StaticRigidBodyComponent::Reflect(&serializeContext);
            Jolt::SoftBodyComponent::Reflect(&serializeContext);
            Jolt::SkeletonComponent::Reflect(&serializeContext);
            Jolt::SceneComponent::Reflect(&serializeContext);
            Jolt::RigidBodyComponent::Reflect(&serializeContext);
            Jolt::RagdollComponent::Reflect(&serializeContext);
            Jolt::PathComponent::Reflect(&serializeContext);
            Jolt::HairComponent::Reflect(&serializeContext);
            Jolt::ConstraintComponent::Reflect(&serializeContext);
            Jolt::ColliderComponent::Reflect(&serializeContext);
            Jolt::CharacterControllerComponent::Reflect(&serializeContext);
            SystemComponent::Reflect(&serializeContext);
            AzToolsFramework::ComponentModeFramework::ComponentModeDelegate::Reflect(&serializeContext);
            AzToolsFramework::Components::EditorComponentBase::Reflect(&serializeContext);
            AZ::Name::Reflect(&serializeContext);
            AZ::Entity::Reflect(&serializeContext);
            AZ::Data::AssetData::Reflect(&serializeContext);
            serializeContext.DisableRemoveReflection();
        }

        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST_F(PrefabComponentTests, NonDefaultRigidBodySupportsPrefabUndoRedoAndSaveReload)
    {
        RigidBodyConfiguration configuration;
        configuration.m_initialLinearVelocity = AZ::Vector3(1.0f, 2.0f, 3.0f);
        configuration.m_initialAngularVelocity = AZ::Vector3(-1.0f, -2.0f, -3.0f);
        configuration.m_userData = 0x1234'5678'9abc'def0;
        configuration.m_allowDynamicOrKinematic = true;

        AZ::Entity* sourceEntity = aznew AZ::Entity("Jolt prefab body");
        sourceEntity->CreateComponent<AzToolsFramework::Components::TransformComponent>();
        sourceEntity->CreateComponent<ColliderComponent>();
        sourceEntity->CreateComponent<RigidBodyComponent>(AZStd::move(configuration));
        sourceEntity->Init();
        sourceEntity->Activate();
        const AZ::EntityId sourceEntityId = sourceEntity->GetId();

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZ::IO::FixedMaxPath prefabPath =
            AZ::IO::FixedMaxPath(temporaryDirectory.GetDirectory()) / "JoltPhase3.prefab";
        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> sourceInstance =
            m_prefabSystem->CreatePrefab(
                {sourceEntity},
                {},
                prefabPath);
        ASSERT_TRUE(sourceInstance);
        const AzToolsFramework::Prefab::TemplateId sourceTemplateId =
            sourceInstance->GetTemplateId();
        const AzToolsFramework::Prefab::EntityAliasOptionalReference entityAliasReference =
            sourceInstance->GetEntityAlias(sourceEntityId);
        ASSERT_TRUE(entityAliasReference);
        const AzToolsFramework::Prefab::EntityAlias entityAlias = entityAliasReference.value();

        const auto expectConfiguration =
            [](AZ::Entity& entity, const AZ::u64 expectedUserData)
            {
                const RigidBodyComponent* editorComponent =
                    entity.FindComponent<RigidBodyComponent>();
                ASSERT_TRUE(editorComponent);

                AZ::Entity runtimeEntity;
                const_cast<RigidBodyComponent*>(editorComponent)->BuildGameEntity(&runtimeEntity);
                const Jolt::RigidBodyComponent* runtimeComponent =
                    runtimeEntity.FindComponent<Jolt::RigidBodyComponent>();
                ASSERT_TRUE(runtimeComponent);
                EXPECT_EQ(runtimeComponent->GetUserData(), expectedUserData);
            };

        const AzToolsFramework::Prefab::EntityOptionalReference sourceEntityReference =
            sourceInstance->GetEntity(entityAlias);
        ASSERT_TRUE(sourceEntityReference);
        AZ::Entity& ownedSourceEntity = sourceEntityReference->get();
        AzToolsFramework::Prefab::PrefabDom entityBeforeUpdate;
        m_instanceToTemplate->GenerateEntityDomBySerializing(
            entityBeforeUpdate,
            ownedSourceEntity);

        ownedSourceEntity.Deactivate();
        RigidBodyComponent* originalComponent =
            ownedSourceEntity.FindComponent<RigidBodyComponent>();
        ASSERT_TRUE(originalComponent);
        ASSERT_TRUE(ownedSourceEntity.RemoveComponent(originalComponent));
        delete originalComponent;
        RigidBodyConfiguration changedConfiguration;
        changedConfiguration.m_userData = 0xfedc'ba98'7654'3210;
        ownedSourceEntity.CreateComponent<RigidBodyComponent>(AZStd::move(changedConfiguration));
        ownedSourceEntity.Activate();

        AzToolsFramework::Prefab::PrefabDom entityAfterUpdate;
        m_instanceToTemplate->GenerateEntityDomBySerializing(
            entityAfterUpdate,
            ownedSourceEntity);
        AzToolsFramework::Prefab::PrefabDom patch;
        m_instanceToTemplate->GeneratePatch(
            patch,
            entityBeforeUpdate,
            entityAfterUpdate);
        AzToolsFramework::Prefab::PrefabUndoEntityUpdate undoNode("Jolt component configuration");
        undoNode.Capture(
            entityBeforeUpdate,
            entityAfterUpdate,
            ownedSourceEntity.GetId());
        ASSERT_TRUE(m_instanceToTemplate->PatchEntityInTemplate(
            patch,
            ownedSourceEntity.GetId()));

        undoNode.Undo();
        m_instanceUpdateExecutor->UpdateTemplateInstancesInQueue();
        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> undoInstance =
            m_prefabSystem->InstantiatePrefab(sourceTemplateId);
        ASSERT_TRUE(undoInstance);
        const AzToolsFramework::Prefab::EntityOptionalReference undoEntity =
            undoInstance->GetEntity(entityAlias);
        ASSERT_TRUE(undoEntity);
        expectConfiguration(undoEntity->get(), 0x1234'5678'9abc'def0);
        undoInstance.reset();

        undoNode.Redo();
        m_instanceUpdateExecutor->UpdateTemplateInstancesInQueue();
        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> redoInstance =
            m_prefabSystem->InstantiatePrefab(sourceTemplateId);
        ASSERT_TRUE(redoInstance);
        const AzToolsFramework::Prefab::EntityOptionalReference redoEntity =
            redoInstance->GetEntity(entityAlias);
        ASSERT_TRUE(redoEntity);
        expectConfiguration(redoEntity->get(), 0xfedc'ba98'7654'3210);
        redoInstance.reset();

        undoNode.Undo();
        m_instanceUpdateExecutor->UpdateTemplateInstancesInQueue();

        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> copiedInstance =
            m_prefabSystem->InstantiatePrefab(sourceTemplateId);
        ASSERT_TRUE(copiedInstance);
        const AzToolsFramework::Prefab::EntityOptionalReference copiedEntity =
            copiedInstance->GetEntity(entityAlias);
        ASSERT_TRUE(copiedEntity);
        expectConfiguration(copiedEntity->get(), 0x1234'5678'9abc'def0);

        ASSERT_TRUE(m_prefabLoader->SaveTemplateToFile(sourceTemplateId, prefabPath));
        copiedInstance.reset();
        sourceInstance.reset();
        m_prefabSystem->RemoveTemplate(sourceTemplateId);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const AzToolsFramework::Prefab::TemplateId reloadedTemplateId =
            m_prefabLoader->LoadTemplateFromFile(prefabPath);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        ASSERT_NE(reloadedTemplateId, AzToolsFramework::Prefab::InvalidTemplateId);
        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> reloadedInstance =
            m_prefabSystem->InstantiatePrefab(reloadedTemplateId);
        ASSERT_TRUE(reloadedInstance);
        const AzToolsFramework::Prefab::EntityOptionalReference reloadedEntity =
            reloadedInstance->GetEntity(entityAlias);
        ASSERT_TRUE(reloadedEntity);
        expectConfiguration(reloadedEntity->get(), 0x1234'5678'9abc'def0);
    }

    TEST_F(PrefabComponentTests, EveryAuthorableComponentSurvivesPrefabCopyAndSaveReload)
    {
        struct PrefabExpectation final
        {
            const char* m_name = nullptr;
            AZ::EntityId m_sourceEntityId;
            AZ::TypeId m_editorComponentType;
            AZ::TypeId m_runtimeComponentType;
            AzToolsFramework::Prefab::EntityAlias m_entityAlias;
            AZStd::string m_expectedConfiguration;
        };

        AZ::SerializeContext* serializeContext = GetApplication()->GetSerializeContext();
        ASSERT_TRUE(serializeContext);

        const auto serializeComponent =
            [serializeContext](
                const AZ::Component& component,
                const AZ::TypeId& componentType)
            {
                AZ::Component& mutableComponent = const_cast<AZ::Component&>(component);
                const AZ::ComponentId componentId = mutableComponent.GetId();
                const AZStd::string serializedIdentifier =
                    mutableComponent.GetSerializedIdentifier();
                mutableComponent.SetId(AZ::InvalidComponentId);
                mutableComponent.SetSerializedIdentifier({});

                AZStd::vector<char> serializedComponent;
                AZ::IO::ByteContainerStream stream(&serializedComponent);
                EXPECT_TRUE(AZ::Utils::SaveObjectToStream(
                    stream,
                    AZ::DataStream::ST_BINARY,
                    &component,
                    componentType,
                    serializeContext));
                mutableComponent.SetSerializedIdentifier(serializedIdentifier);
                mutableComponent.SetId(componentId);
                return serializedComponent;
            };

        AZStd::vector<AZ::Entity*> sourceEntities;
        AZStd::vector<PrefabExpectation> expectations;
        const auto addComponent =
            [&sourceEntities, &expectations]<class Component, class RuntimeComponent, class Configuration>(
                const char* entityName,
                Configuration configuration)
            {
                AZ::Entity* entity = aznew AZ::Entity(entityName);
                entity->CreateComponent<AzToolsFramework::Components::TransformComponent>();
                Component* component = entity->CreateComponent<Component>(AZStd::move(configuration));
                expectations.push_back(PrefabExpectation{
                    .m_name = entityName,
                    .m_sourceEntityId = entity->GetId(),
                    .m_editorComponentType = azrtti_typeid<Component>(),
                    .m_runtimeComponentType = azrtti_typeid<RuntimeComponent>(),
                });
                AZ_UNUSED(component);
                sourceEntities.push_back(entity);
            };

        CharacterComponentConfiguration characterConfiguration;
        characterConfiguration.m_mass = 91.0f;
        characterConfiguration.m_userData = 0x0102'0304'0506'0708;
        addComponent.operator()<CharacterControllerComponent, Jolt::CharacterControllerComponent>(
            "Jolt character",
            AZStd::move(characterConfiguration));

        ColliderShapeConfiguration colliderShape;
        colliderShape.m_shape.m_geometry = CapsuleShapeConfiguration{
            .m_cylinderHeight = 2.5f,
            .m_radius = 0.75f,
        };
        colliderShape.m_shape.m_userData = 0x1112'1314'1516'1718;
        addComponent.operator()<ColliderComponent, Jolt::ColliderComponent>(
            "Jolt collider",
            AZStd::vector{colliderShape});

        ConstraintComponentConfiguration constraintConfiguration;
        constraintConfiguration.m_geometry = HingeConstraintConfiguration{
            .m_maximumLimit = 0.75f,
            .m_minimumLimit = -0.5f,
        };
        constraintConfiguration.m_priority = 7;
        addComponent.operator()<ConstraintComponent, Jolt::ConstraintComponent>(
            "Jolt constraint",
            AZStd::move(constraintConfiguration));

        HairComponentConfiguration hairConfiguration = HairComponentConfiguration::CreateDefault();
        hairConfiguration.m_autoUpdate = false;
        hairConfiguration.m_jointToHair =
            AZ::Transform::CreateRotationZ(AZ::Constants::QuarterPi);
        addComponent.operator()<HairComponent, Jolt::HairComponent>(
            "Jolt Hair",
            AZStd::move(hairConfiguration));

        HermitePathConfiguration pathConfiguration = HermitePathConfiguration::CreateDefault();
        pathConfiguration.m_isLooping = true;
        pathConfiguration.m_points[0].m_position = AZ::Vector3(-1.0f, 0.0f, 0.5f);
        pathConfiguration.m_points[1].m_position = AZ::Vector3(2.0f, 1.0f, 0.5f);
        addComponent.operator()<PathComponent, Jolt::PathComponent>(
            "Jolt path",
            AZStd::move(pathConfiguration));

        RagdollComponentConfiguration ragdollConfiguration =
            RagdollComponentConfiguration::CreateDefault();
        ragdollConfiguration.m_baseConstraintPriority = 5;
        ragdollConfiguration.m_stabilize = false;
        addComponent.operator()<RagdollComponent, Jolt::RagdollComponent>(
            "Jolt ragdoll",
            AZStd::move(ragdollConfiguration));

        RigidBodyConfiguration rigidBodyConfiguration;
        rigidBodyConfiguration.m_initialLinearVelocity = AZ::Vector3(1.0f, 2.0f, 3.0f);
        rigidBodyConfiguration.m_userData = 0x3132'3334'3536'3738;
        addComponent.operator()<RigidBodyComponent, Jolt::RigidBodyComponent>(
            "Jolt rigid body",
            AZStd::move(rigidBodyConfiguration));

        SceneComponentConfiguration sceneConfiguration;
        sceneConfiguration.m_asset = AZ::Data::Asset<SceneAsset>(
            AZ::Data::AssetId(AZ::Uuid("{26C17D7A-B1B4-4A90-BF27-E10287247A12}"), 3),
            SceneAssetTypeId,
            "Physics/Jolt/PrefabScene.jolt");
        sceneConfiguration.m_asset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::NoLoad);
        addComponent.operator()<SceneComponent, Jolt::SceneComponent>(
            "Jolt scene",
            AZStd::move(sceneConfiguration));

        SkeletonComponentConfiguration skeletonConfiguration;
        skeletonConfiguration.m_asset = AZ::Data::Asset<SkeletonAsset>(
            AZ::Data::AssetId(AZ::Uuid("{F3E742F7-77D7-4C8E-8A49-BC17E712B847}"), 4),
            SkeletonAssetTypeId,
            "Physics/Jolt/PrefabSkeleton.jolt");
        skeletonConfiguration.m_asset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::NoLoad);
        addComponent.operator()<SkeletonComponent, Jolt::SkeletonComponent>(
            "Jolt skeleton",
            AZStd::move(skeletonConfiguration));

        SoftBodyComponentConfiguration softBodyConfiguration =
            SoftBodyComponentConfiguration::CreateDefault();
        softBodyConfiguration.m_body.m_pressure = 0.25f;
        softBodyConfiguration.m_enabled = false;
        addComponent.operator()<SoftBodyComponent, Jolt::SoftBodyComponent>(
            "Jolt soft body",
            AZStd::move(softBodyConfiguration));

        StaticRigidBodyConfiguration staticBodyConfiguration;
        staticBodyConfiguration.m_friction = 0.7f;
        staticBodyConfiguration.m_isSensor = true;
        staticBodyConfiguration.m_userData = 0x5152'5354'5556'5758;
        addComponent.operator()<StaticRigidBodyComponent, Jolt::StaticRigidBodyComponent>(
            "Jolt static rigid body",
            AZStd::move(staticBodyConfiguration));

        WheeledVehicleComponentConfiguration wheeledConfiguration =
            WheeledVehicleComponentConfiguration::CreateDefault();
        wheeledConfiguration.m_enabled = false;
        wheeledConfiguration.m_vehicle.m_collisionSphereRadius = 0.45f;
        addComponent.operator()<WheeledVehicleComponent, Jolt::WheeledVehicleComponent>(
            "Jolt wheeled vehicle",
            AZStd::move(wheeledConfiguration));

        MotorcycleComponentConfiguration motorcycleConfiguration =
            MotorcycleComponentConfiguration::CreateDefault();
        motorcycleConfiguration.m_enabled = false;
        motorcycleConfiguration.m_motorcycle.m_controller.m_maximumLeanAngle = 0.5f;
        addComponent.operator()<MotorcycleComponent, Jolt::MotorcycleComponent>(
            "Jolt motorcycle",
            AZStd::move(motorcycleConfiguration));

        TrackedVehicleComponentConfiguration trackedConfiguration =
            TrackedVehicleComponentConfiguration::CreateDefault();
        trackedConfiguration.m_enabled = false;
        trackedConfiguration.m_vehicle.m_collisionSphereRadius = 0.55f;
        addComponent.operator()<TrackedVehicleComponent, Jolt::TrackedVehicleComponent>(
            "Jolt tracked vehicle",
            AZStd::move(trackedConfiguration));

        VirtualCharacterComponentConfiguration virtualCharacterConfiguration;
        virtualCharacterConfiguration.m_createInnerBody = true;
        virtualCharacterConfiguration.m_mass = 82.0f;
        virtualCharacterConfiguration.m_userData = 0x9192'9394'9596'9798;
        addComponent.operator()<VirtualCharacterControllerComponent, Jolt::VirtualCharacterControllerComponent>(
            "Jolt virtual character",
            AZStd::move(virtualCharacterConfiguration));

        AZ::Test::ScopedAutoTempDirectory temporaryDirectory;
        const AZ::IO::FixedMaxPath prefabPath =
            AZ::IO::FixedMaxPath(temporaryDirectory.GetDirectory()) / "JoltAuthorableComponents.prefab";
        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> sourceInstance =
            m_prefabSystem->CreatePrefab(
                sourceEntities,
                {},
                prefabPath);
        ASSERT_TRUE(sourceInstance);
        const AzToolsFramework::Prefab::TemplateId sourceTemplateId =
            sourceInstance->GetTemplateId();

        const auto serializeRuntimeExport =
            [&serializeComponent](
                AZ::Entity& entity,
                const PrefabExpectation& expectation)
            {
                AZ::Component* component =
                    entity.FindComponent(expectation.m_editorComponentType);
                auto* editorComponent =
                    azrtti_cast<AzToolsFramework::Components::EditorComponentBase*>(component);
                EXPECT_TRUE(editorComponent);
                if (!editorComponent)
                {
                    return AZStd::vector<char>{};
                }

                AZ::Entity runtimeEntity;
                editorComponent->BuildGameEntity(&runtimeEntity);
                const AZ::Component* runtimeComponent =
                    runtimeEntity.FindComponent(expectation.m_runtimeComponentType);
                EXPECT_TRUE(runtimeComponent);
                if (!runtimeComponent)
                {
                    return AZStd::vector<char>{};
                }

                return serializeComponent(
                    *runtimeComponent,
                    expectation.m_runtimeComponentType);
            };

        const auto serializeEditorConfiguration =
            [this](
                AZ::Entity& entity,
                const PrefabExpectation& expectation)
            {
                AzToolsFramework::Prefab::PrefabDom entityDom;
                m_instanceToTemplate->GenerateEntityDomBySerializing(entityDom, entity);
                const auto components = entityDom.FindMember("Components");
                EXPECT_NE(components, entityDom.MemberEnd());
                if (components == entityDom.MemberEnd() || !components->value.IsObject())
                {
                    return AZStd::string{};
                }

                const AZStd::string typeId = expectation.m_editorComponentType.ToString<AZStd::string>();
                for (auto component = components->value.MemberBegin(); component != components->value.MemberEnd(); ++component)
                {
                    if (!component->value.IsObject())
                    {
                        continue;
                    }

                    const auto type = component->value.FindMember("$type");
                    if (type == component->value.MemberEnd()
                        || !type->value.IsString()
                        || AZStd::string_view(type->value.GetString(), type->value.GetStringLength()).find(typeId)
                            == AZStd::string_view::npos)
                    {
                        continue;
                    }

                    rapidjson::Document configuration;
                    configuration.CopyFrom(component->value, configuration.GetAllocator());
                    configuration.RemoveMember("Id");
                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    configuration.Accept(writer);
                    return AZStd::string(buffer.GetString(), buffer.GetSize());
                }

                ADD_FAILURE() << "Editor component was absent from the serialized prefab entity";
                return AZStd::string{};
            };

        for (PrefabExpectation& expectation : expectations)
        {
            const AzToolsFramework::Prefab::EntityAliasOptionalReference entityAlias =
                sourceInstance->GetEntityAlias(expectation.m_sourceEntityId);
            ASSERT_TRUE(entityAlias);
            expectation.m_entityAlias = entityAlias->get();
            const AzToolsFramework::Prefab::EntityOptionalReference sourceEntity =
                sourceInstance->GetEntity(expectation.m_entityAlias);
            ASSERT_TRUE(sourceEntity);
            expectation.m_expectedConfiguration = serializeEditorConfiguration(sourceEntity->get(), expectation);
            EXPECT_FALSE(expectation.m_expectedConfiguration.empty());
            EXPECT_FALSE(serializeRuntimeExport(sourceEntity->get(), expectation).empty());
        }

        const auto expectInstance =
            [&expectations, &serializeEditorConfiguration, &serializeRuntimeExport](
                AzToolsFramework::Prefab::Instance& instance)
            {
                for (const PrefabExpectation& expectation : expectations)
                {
                    SCOPED_TRACE(expectation.m_name);
                    const AzToolsFramework::Prefab::EntityOptionalReference entity =
                        instance.GetEntity(expectation.m_entityAlias);
                    ASSERT_TRUE(entity);
                    EXPECT_EQ(
                        serializeEditorConfiguration(entity->get(), expectation),
                        expectation.m_expectedConfiguration);
                    EXPECT_FALSE(serializeRuntimeExport(entity->get(), expectation).empty());
                }
            };

        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> copiedInstance =
            m_prefabSystem->InstantiatePrefab(sourceTemplateId);
        ASSERT_TRUE(copiedInstance);
        {
            SCOPED_TRACE("Copied instance");
            expectInstance(*copiedInstance);
        }

        ASSERT_TRUE(m_prefabLoader->SaveTemplateToFile(sourceTemplateId, prefabPath));
        copiedInstance.reset();
        sourceInstance.reset();
        m_prefabSystem->RemoveTemplate(sourceTemplateId);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const AzToolsFramework::Prefab::TemplateId reloadedTemplateId =
            m_prefabLoader->LoadTemplateFromFile(prefabPath);
        AZ_TEST_STOP_TRACE_SUPPRESSION(15);
        ASSERT_NE(reloadedTemplateId, AzToolsFramework::Prefab::InvalidTemplateId);
        AZStd::unique_ptr<AzToolsFramework::Prefab::Instance> reloadedInstance =
            m_prefabSystem->InstantiatePrefab(reloadedTemplateId);
        ASSERT_TRUE(reloadedInstance);
        {
            SCOPED_TRACE("Reloaded instance");
            expectInstance(*reloadedInstance);
        }
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

    TEST(EditorComponentTests, ConvexHullPreviewDoesNotInventEdgesBetweenSourcePoints)
    {
        const AZStd::vector<AZ::Vector3> points = {
            AZ::Vector3(1.0f, 0.0f, 0.0f),
            AZ::Vector3(0.0f, 0.0f, 1.0f),
            AZ::Vector3(0.0f, 1.0f, 0.0f),
            AZ::Vector3(-1.0f, -1.0f, -1.0f),
        };
        const ShapeGeometry geometry = ConvexHullShapeConfiguration{.m_points = points};
        GeometryDrawRecorder debugDisplay;

        DrawShapeGeometry(debugDisplay, geometry, AZ::Matrix3x4::CreateIdentity());

        EXPECT_EQ(debugDisplay.m_points, points);
        EXPECT_EQ(debugDisplay.m_lineCount, 0);
    }

    TEST(EditorComponentTests, CustomShapeUsesAuthoredEditorBounds)
    {
        const AZ::Aabb localBounds = AZ::Aabb::CreateFromMinMax(
            {-2.0f, -3.0f, -4.0f},
            {2.0f, 3.0f, 4.0f});
        const ShapeGeometry geometry = CustomShapeConfiguration{
            .m_editorBounds = localBounds,
            .m_providerId = CustomShapeConfigurationTypeId,
        };
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateTranslation(
            AZ::Vector3(5.0f, 6.0f, 7.0f));

        const AZ::Aabb bounds = CalculateShapeBounds(geometry, transform);

        EXPECT_TRUE(bounds.IsClose(AZ::Aabb::CreateFromMinMax(
            {3.0f, 3.0f, 3.0f},
            {7.0f, 9.0f, 11.0f})));
    }

} // namespace Jolt::Editor
