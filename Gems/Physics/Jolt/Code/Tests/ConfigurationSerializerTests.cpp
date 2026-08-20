/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ConfigurationSerializer.h>

#include <Jolt/CharacterControllerComponent.h>
#include <Jolt/ColliderComponent.h>
#include <Jolt/Constraint.h>
#include <Jolt/ConstraintComponent.h>
#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/Cooking.h>
#include <Jolt/HairComponent.h>
#include <Jolt/PathComponent.h>
#include <Jolt/Query.h>
#include <Jolt/RagdollComponent.h>
#include <Jolt/RigidBodyComponent.h>
#include <Jolt/Scene.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/SceneComponent.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/SkeletonComponent.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/StaticRigidBodyComponent.h>
#include <Jolt/SystemComponent.h>
#include <Jolt/VehicleComponents.h>
#include <Jolt/VirtualCharacterControllerComponent.h>

#include <AzTest/AzTest.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/JSON/document.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

namespace Jolt
{
    namespace
    {
        template<typename Variant>
        void ExpectTaggedJsonRoundTrip(
            const char* name,
            const Variant& source,
            AZ::SerializeContext& serializeContext,
            AZ::JsonRegistrationContext& jsonContext)
        {
            SCOPED_TRACE(name);
            AZ::JsonSerializerSettings serializerSettings;
            serializerSettings.m_serializeContext = &serializeContext;
            serializerSettings.m_registrationContext = &jsonContext;
            AZStd::string report;
            serializerSettings.m_reporting = [&report](
                const AZStd::string_view message,
                const AZ::JsonSerializationResult::ResultCode result,
                const AZStd::string_view path)
            {
                report.append(path);
                report.append(": ");
                report.append(message);
                report.push_back('\n');
                return result;
            };

            rapidjson::Document sourceDocument;
            const AZ::JsonSerializationResult::ResultCode storeResult = AZ::JsonSerialization::Store(
                sourceDocument,
                sourceDocument.GetAllocator(),
                source,
                serializerSettings);
            ASSERT_NE(
                storeResult.GetProcessing(),
                AZ::JsonSerializationResult::Processing::Halted)
                << report.c_str();

            Variant restored;
            AZ::JsonDeserializerSettings deserializerSettings;
            deserializerSettings.m_serializeContext = &serializeContext;
            deserializerSettings.m_registrationContext = &jsonContext;
            deserializerSettings.m_reporting = serializerSettings.m_reporting;
            report.clear();
            const AZ::JsonSerializationResult::ResultCode loadResult = AZ::JsonSerialization::Load(
                restored,
                sourceDocument,
                deserializerSettings);
            ASSERT_NE(
                loadResult.GetProcessing(),
                AZ::JsonSerializationResult::Processing::Halted)
                << report.c_str();
            if constexpr (requires { restored.index(); source.index(); })
            {
                EXPECT_EQ(restored.index(), source.index());
            }
            else if constexpr (requires { restored.has_value(); source.has_value(); })
            {
                EXPECT_EQ(restored.has_value(), source.has_value());
            }

            rapidjson::Document restoredDocument;
            report.clear();
            const AZ::JsonSerializationResult::ResultCode restoredStoreResult = AZ::JsonSerialization::Store(
                restoredDocument,
                restoredDocument.GetAllocator(),
                restored,
                serializerSettings);
            ASSERT_NE(
                restoredStoreResult.GetProcessing(),
                AZ::JsonSerializationResult::Processing::Halted)
                << report.c_str();
            EXPECT_EQ(restoredDocument, sourceDocument);
        }
    } // namespace

    TEST(ConfigurationSerializerTests, EveryTaggedVariantRoundTripsThroughJson)
    {
        auto serializeContext = AZStd::make_unique<AZ::SerializeContext>();
        AZ::Entity::Reflect(serializeContext.get());
        AZ::Name::Reflect(serializeContext.get());
        SystemComponent::Reflect(serializeContext.get());
        ColliderComponent::Reflect(serializeContext.get());
        ConstraintComponent::Reflect(serializeContext.get());
        SoftBodyComponent::Reflect(serializeContext.get());

        auto jsonContext = AZStd::make_unique<AZ::JsonRegistrationContext>();
        SystemComponent::Reflect(jsonContext.get());

        ExpectTaggedJsonRoundTrip(
            "BroadPhaseCastGeometry",
            BroadPhaseCastGeometry{BroadPhaseRay{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "BroadPhaseOverlapGeometry",
            BroadPhaseOverlapGeometry{BroadPhaseSphere{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "ConstraintComponentGeometry",
            ConstraintComponentGeometry{SwingTwistConstraintConfiguration{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "ConstraintGeometry",
            ConstraintGeometry{SwingTwistConstraintConfiguration{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "ConstraintMeasurements",
            ConstraintMeasurements{SwingTwistMeasurements{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "CookedDecoratedShapeGeometry",
            CookedDecoratedShapeGeometry{CookedScaledShapeConfiguration{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "DecoratedShapeGeometry",
            DecoratedShapeGeometry{ScaledShapeConfiguration{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "PrimitiveShapeGeometry",
            PrimitiveShapeGeometry{TriangleShapeConfiguration{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "SceneAssetBody",
            SceneAssetBody{SceneAssetSoftBody{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "SceneBodyConfiguration",
            SceneBodyConfiguration{SoftBodyConfiguration{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "SceneSourceShape",
            SceneSourceShape{SceneSourceScaledShape{}},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "ShapeGeometry",
            ShapeGeometry{CustomConvexShapeConfiguration{
                .m_data = {1, 2, 3},
                .m_editorBounds = AZ::Aabb::CreateCenterRadius(AZ::Vector3::CreateZero(), 2.0f),
                .m_providerId = CustomConvexShapeConfigurationTypeId,
            }},
            *serializeContext,
            *jsonContext);

        ExpectTaggedJsonRoundTrip(
            "GeneralCustomShapeGeometry",
            ShapeGeometry{CustomShapeConfiguration{
                .m_data = {4, 5, 6},
                .m_editorBounds = AZ::Aabb::CreateCenterRadius(AZ::Vector3::CreateOne(), 3.0f),
                .m_providerId = CustomShapeConfigurationTypeId,
            }},
            *serializeContext,
            *jsonContext);

        using OptionalConstraintGeometry = AZStd::optional<ConstraintGeometry>;
        ExpectTaggedJsonRoundTrip(
            "EngagedOptionalConstraintGeometry",
            OptionalConstraintGeometry{ConstraintGeometry{HingeConstraintConfiguration{}}},
            *serializeContext,
            *jsonContext);
        ExpectTaggedJsonRoundTrip(
            "EmptyOptionalConstraintGeometry",
            OptionalConstraintGeometry{},
            *serializeContext,
            *jsonContext);

        jsonContext->EnableRemoveReflection();
        SystemComponent::Reflect(jsonContext.get());
        jsonContext->DisableRemoveReflection();

        serializeContext->EnableRemoveReflection();
        SoftBodyComponent::Reflect(serializeContext.get());
        ConstraintComponent::Reflect(serializeContext.get());
        ColliderComponent::Reflect(serializeContext.get());
        SystemComponent::Reflect(serializeContext.get());
        AZ::Name::Reflect(serializeContext.get());
        AZ::Entity::Reflect(serializeContext.get());
        serializeContext->DisableRemoveReflection();

        jsonContext.reset();
        serializeContext.reset();
        AZ::GetGlobalSerializeContextModule().Cleanup();
    }

    TEST(ConfigurationSerializerTests, EveryAuthorableConfigurationRoundTripsNonDefaultJson)
    {
        auto serializeContext = AZStd::make_unique<AZ::SerializeContext>();
        AZ::Data::AssetData::Reflect(serializeContext.get());
        AZ::Entity::Reflect(serializeContext.get());
        AZ::Name::Reflect(serializeContext.get());
        SystemComponent::Reflect(serializeContext.get());
        CharacterControllerComponent::Reflect(serializeContext.get());
        ColliderComponent::Reflect(serializeContext.get());
        ConstraintComponent::Reflect(serializeContext.get());
        HairComponent::Reflect(serializeContext.get());
        PathComponent::Reflect(serializeContext.get());
        RagdollComponent::Reflect(serializeContext.get());
        RigidBodyComponent::Reflect(serializeContext.get());
        SceneComponent::Reflect(serializeContext.get());
        SkeletonComponent::Reflect(serializeContext.get());
        SoftBodyComponent::Reflect(serializeContext.get());
        StaticRigidBodyComponent::Reflect(serializeContext.get());
        WheeledVehicleComponent::Reflect(serializeContext.get());
        MotorcycleComponent::Reflect(serializeContext.get());
        TrackedVehicleComponent::Reflect(serializeContext.get());
        VirtualCharacterControllerComponent::Reflect(serializeContext.get());

        auto jsonContext = AZStd::make_unique<AZ::JsonRegistrationContext>();
        SystemComponent::Reflect(jsonContext.get());

        CharacterComponentConfiguration characterConfiguration;
        characterConfiguration.m_userData = 0x0102'0304'0506'0708;
        characterConfiguration.m_mass = 91.0f;
        characterConfiguration.m_enhancedInternalEdgeRemoval = true;
        ExpectTaggedJsonRoundTrip(
            "CharacterComponentConfiguration",
            characterConfiguration,
            *serializeContext,
            *jsonContext);

        ColliderShapeConfiguration colliderConfiguration;
        colliderConfiguration.m_shape.m_geometry = CapsuleShapeConfiguration{
            .m_cylinderHeight = 2.5f,
            .m_radius = 0.75f,
        };
        colliderConfiguration.m_shape.m_userData = 0x1112'1314'1516'1718;
        colliderConfiguration.m_localTransform =
            AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f));
        ExpectTaggedJsonRoundTrip(
            "ColliderShapeConfiguration",
            colliderConfiguration,
            *serializeContext,
            *jsonContext);

        ConstraintComponentConfiguration constraintConfiguration;
        constraintConfiguration.m_geometry = HingeConstraintConfiguration{
            .m_maximumLimit = 0.75f,
            .m_minimumLimit = -0.5f,
        };
        constraintConfiguration.m_userData = 0x2122'2324'2526'2728;
        constraintConfiguration.m_priority = 7;
        ExpectTaggedJsonRoundTrip(
            "ConstraintComponentConfiguration",
            constraintConfiguration,
            *serializeContext,
            *jsonContext);

        HairComponentConfiguration hairConfiguration = HairComponentConfiguration::CreateDefault();
        hairConfiguration.m_jointModelTransforms = {
            AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(0.25f)),
        };
        hairConfiguration.m_autoUpdate = false;
        ExpectTaggedJsonRoundTrip(
            "HairComponentConfiguration",
            hairConfiguration,
            *serializeContext,
            *jsonContext);

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
        ExpectTaggedJsonRoundTrip(
            "HermitePathConfiguration",
            pathConfiguration,
            *serializeContext,
            *jsonContext);

        RagdollComponentConfiguration ragdollConfiguration =
            RagdollComponentConfiguration::CreateDefault();
        ragdollConfiguration.m_baseConstraintPriority = 5;
        ragdollConfiguration.m_minimumCollisionSeparation = 0.125f;
        ExpectTaggedJsonRoundTrip(
            "RagdollComponentConfiguration",
            ragdollConfiguration,
            *serializeContext,
            *jsonContext);

        RigidBodyConfiguration rigidBodyConfiguration;
        rigidBodyConfiguration.m_initialLinearVelocity = AZ::Vector3(1.0f, 2.0f, 3.0f);
        rigidBodyConfiguration.m_userData = 0x3132'3334'3536'3738;
        rigidBodyConfiguration.m_allowDynamicOrKinematic = true;
        ExpectTaggedJsonRoundTrip(
            "RigidBodyConfiguration",
            rigidBodyConfiguration,
            *serializeContext,
            *jsonContext);

        SceneComponentConfiguration sceneConfiguration;
        sceneConfiguration.m_asset = AZ::Data::Asset<SceneAsset>(
            AZ::Data::AssetId(AZ::Uuid("{23BB23A5-AD82-449E-9652-AF9BB3A14091}"), 3),
            SceneAssetTypeId,
            "Physics/Jolt/JsonScene.jolt");
        sceneConfiguration.m_asset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::NoLoad);
        ExpectTaggedJsonRoundTrip(
            "SceneComponentConfiguration",
            sceneConfiguration,
            *serializeContext,
            *jsonContext);

        SkeletonComponentConfiguration skeletonConfiguration;
        skeletonConfiguration.m_asset = AZ::Data::Asset<SkeletonAsset>(
            AZ::Data::AssetId(AZ::Uuid("{51F05DB1-64F0-4759-B954-289F95DE9FE8}"), 4),
            SkeletonAssetTypeId,
            "Physics/Jolt/JsonSkeleton.jolt");
        skeletonConfiguration.m_asset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::NoLoad);
        ExpectTaggedJsonRoundTrip(
            "SkeletonComponentConfiguration",
            skeletonConfiguration,
            *serializeContext,
            *jsonContext);

        SoftBodyComponentConfiguration softBodyConfiguration =
            SoftBodyComponentConfiguration::CreateDefault();
        softBodyConfiguration.m_body.m_userData = 0x4142'4344'4546'4748;
        softBodyConfiguration.m_body.m_pressure = 0.25f;
        softBodyConfiguration.m_enabled = false;
        ExpectTaggedJsonRoundTrip(
            "SoftBodyComponentConfiguration",
            softBodyConfiguration,
            *serializeContext,
            *jsonContext);

        StaticRigidBodyConfiguration staticRigidBodyConfiguration;
        staticRigidBodyConfiguration.m_userData = 0x5152'5354'5556'5758;
        staticRigidBodyConfiguration.m_friction = 0.7f;
        staticRigidBodyConfiguration.m_restitution = 0.4f;
        staticRigidBodyConfiguration.m_isSensor = true;
        ExpectTaggedJsonRoundTrip(
            "StaticRigidBodyConfiguration",
            staticRigidBodyConfiguration,
            *serializeContext,
            *jsonContext);

        WheeledVehicleComponentConfiguration wheeledConfiguration =
            WheeledVehicleComponentConfiguration::CreateDefault();
        wheeledConfiguration.m_enabled = false;
        wheeledConfiguration.m_vehicle.m_collisionSphereRadius = 0.45f;
        wheeledConfiguration.m_vehicle.m_gravityOverride = AZ::Vector3(1.0f, 2.0f, 3.0f);
        wheeledConfiguration.m_vehicle.m_overrideGravity = true;
        ExpectTaggedJsonRoundTrip(
            "WheeledVehicleComponentConfiguration",
            wheeledConfiguration,
            *serializeContext,
            *jsonContext);

        MotorcycleComponentConfiguration motorcycleConfiguration =
            MotorcycleComponentConfiguration::CreateDefault();
        motorcycleConfiguration.m_enabled = false;
        motorcycleConfiguration.m_motorcycle.m_wheeled.m_gravityOverride = AZ::Vector3(-2.0f, -3.0f, -4.0f);
        motorcycleConfiguration.m_motorcycle.m_wheeled.m_overrideGravity = true;
        motorcycleConfiguration.m_motorcycle.m_controller.m_maximumLeanAngle = 0.5f;
        ExpectTaggedJsonRoundTrip(
            "MotorcycleComponentConfiguration",
            motorcycleConfiguration,
            *serializeContext,
            *jsonContext);

        TrackedVehicleComponentConfiguration trackedConfiguration =
            TrackedVehicleComponentConfiguration::CreateDefault();
        trackedConfiguration.m_enabled = false;
        trackedConfiguration.m_vehicle.m_collisionSphereRadius = 0.55f;
        trackedConfiguration.m_vehicle.m_gravityOverride = AZ::Vector3(5.0f, 6.0f, 7.0f);
        trackedConfiguration.m_vehicle.m_overrideGravity = true;
        ExpectTaggedJsonRoundTrip(
            "TrackedVehicleComponentConfiguration",
            trackedConfiguration,
            *serializeContext,
            *jsonContext);

        VirtualCharacterComponentConfiguration virtualCharacterConfiguration;
        virtualCharacterConfiguration.m_userData = 0x9192'9394'9596'9798;
        virtualCharacterConfiguration.m_mass = 82.0f;
        virtualCharacterConfiguration.m_createInnerBody = true;
        ExpectTaggedJsonRoundTrip(
            "VirtualCharacterComponentConfiguration",
            virtualCharacterConfiguration,
            *serializeContext,
            *jsonContext);

        jsonContext->EnableRemoveReflection();
        SystemComponent::Reflect(jsonContext.get());
        jsonContext->DisableRemoveReflection();

        serializeContext->EnableRemoveReflection();
        VirtualCharacterControllerComponent::Reflect(serializeContext.get());
        TrackedVehicleComponent::Reflect(serializeContext.get());
        MotorcycleComponent::Reflect(serializeContext.get());
        WheeledVehicleComponent::Reflect(serializeContext.get());
        StaticRigidBodyComponent::Reflect(serializeContext.get());
        SoftBodyComponent::Reflect(serializeContext.get());
        SkeletonComponent::Reflect(serializeContext.get());
        SceneComponent::Reflect(serializeContext.get());
        RigidBodyComponent::Reflect(serializeContext.get());
        RagdollComponent::Reflect(serializeContext.get());
        PathComponent::Reflect(serializeContext.get());
        HairComponent::Reflect(serializeContext.get());
        ConstraintComponent::Reflect(serializeContext.get());
        ColliderComponent::Reflect(serializeContext.get());
        CharacterControllerComponent::Reflect(serializeContext.get());
        SystemComponent::Reflect(serializeContext.get());
        AZ::Name::Reflect(serializeContext.get());
        AZ::Entity::Reflect(serializeContext.get());
        AZ::Data::AssetData::Reflect(serializeContext.get());
        serializeContext->DisableRemoveReflection();

        jsonContext.reset();
        serializeContext.reset();
        AZ::GetGlobalSerializeContextModule().Cleanup();
    }
} // namespace Jolt
