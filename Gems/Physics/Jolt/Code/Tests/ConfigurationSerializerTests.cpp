/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ConfigurationSerializer.h>

#include <Jolt/ColliderComponent.h>
#include <Jolt/Constraint.h>
#include <Jolt/ConstraintComponent.h>
#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/Cooking.h>
#include <Jolt/Query.h>
#include <Jolt/Scene.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/ShapeConfiguration.h>
#include <Jolt/SoftBodyComponent.h>
#include <Jolt/SystemComponent.h>

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
            else
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
} // namespace Jolt
