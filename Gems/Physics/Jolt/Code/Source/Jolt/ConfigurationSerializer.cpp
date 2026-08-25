/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/ConfigurationSerializer.h>

#include <Jolt/Constraint.h>
#include <Jolt/ConstraintComponentConfiguration.h>
#include <Jolt/Cooking.h>
#include <Jolt/Query.h>
#include <Jolt/RagdollComponentConfiguration.h>
#include <Jolt/Scene.h>
#include <Jolt/SceneAsset.h>
#include <Jolt/ShapeConfiguration.h>

#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>

namespace Jolt
{
    namespace
    {
        const rapidjson::Value::StringRefType ValueField = rapidjson::StringRef("Value");

        using OptionalConstraintGeometry = AZStd::optional<ConstraintGeometry>;
    } // namespace

    AZ_CLASS_ALLOCATOR_IMPL(JsonConfigurationSerializer, AZ::SystemAllocator);

    template<typename Variant, size_t Index>
    AZ::JsonSerializationResult::ResultCode JsonConfigurationSerializer::LoadAlternative(
        Variant& output,
        const AZ::TypeId& alternativeTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        if constexpr (Index == AZStd::variant_size_v<Variant>)
        {
            return context.Report(
                JSR::Tasks::ReadField,
                JSR::Outcomes::Unsupported,
                "The serialized configuration type is not an alternative of this variant.");
        }
        else
        {
            using Alternative = AZStd::variant_alternative_t<Index, Variant>;
            if (alternativeTypeId == azrtti_typeid<Alternative>())
            {
                Alternative alternative;
                JSR::ResultCode result = ContinueLoadingFromJsonObjectField(
                    &alternative,
                    azrtti_typeid<Alternative>(),
                    inputValue,
                    ValueField,
                    context);
                if (result.GetProcessing() != JSR::Processing::Halted)
                {
                    output = AZStd::move(alternative);
                }
                return result;
            }

            return LoadAlternative<Variant, Index + 1>(
                output,
                alternativeTypeId,
                inputValue,
                context);
        }
    }

    template<typename Variant>
    AZ::JsonSerializationResult::Result JsonConfigurationSerializer::LoadVariant(
        void* outputValue,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        AZ_Assert(outputValue, "Configuration serializer received a null output value.");
        if (!inputValue.IsObject())
        {
            return context.Report(
                JSR::Tasks::ReadField,
                JSR::Outcomes::Unsupported,
                "A tagged configuration must be an object.");
        }

        const auto typeMember = inputValue.FindMember(AZ::JsonSerialization::TypeIdFieldIdentifier);
        if (typeMember == inputValue.MemberEnd())
        {
            return context.Report(
                JSR::Tasks::ReadField,
                JSR::Outcomes::Missing,
                "A tagged configuration requires a $type field.");
        }

        AZ::TypeId alternativeTypeId;
        JSR::ResultCode result = LoadTypeId(alternativeTypeId, typeMember->value, context);
        if (result.GetProcessing() == JSR::Processing::Halted)
        {
            return context.Report(result, "Failed to read the tagged configuration type.");
        }

        result.Combine(LoadAlternative(
            *static_cast<Variant*>(outputValue),
            alternativeTypeId,
            inputValue,
            context));
        if (result.GetProcessing() != JSR::Processing::Halted)
        {
            return context.Report(result, "Loaded the tagged configuration.");
        }

        return context.Report(result, "Failed to load the tagged configuration.");
    }

    template<typename Variant>
    AZ::JsonSerializationResult::Result JsonConfigurationSerializer::StoreVariant(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        AZ::JsonSerializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        AZ_Assert(inputValue, "Configuration serializer received a null input value.");
        const auto& input = *static_cast<const Variant*>(inputValue);
        const auto* defaults = static_cast<const Variant*>(defaultValue);
        JSR::ResultCode result(JSR::Tasks::WriteValue);
        outputValue.SetObject();

        AZStd::visit(
            [this, &outputValue, defaults, &context, &result](const auto& alternative)
            {
                using Alternative = AZStd::remove_cvref_t<decltype(alternative)>;
                rapidjson::Value typeValue;
                result.Combine(StoreTypeId(typeValue, azrtti_typeid<Alternative>(), context));
                outputValue.AddMember(
                    rapidjson::StringRef(AZ::JsonSerialization::TypeIdFieldIdentifier),
                    AZStd::move(typeValue),
                    context.GetJsonAllocator());

                const Alternative* defaultAlternative = nullptr;
                if (defaults)
                {
                    defaultAlternative = AZStd::get_if<Alternative>(defaults);
                }
                result.Combine(ContinueStoringToJsonObjectField(
                    outputValue,
                    ValueField,
                    &alternative,
                    defaultAlternative,
                    azrtti_typeid<Alternative>(),
                    context));
            },
            input);

        if (result.GetProcessing() != JSR::Processing::Halted)
        {
            return context.Report(result, "Stored the tagged configuration.");
        }

        return context.Report(result, "Failed to store the tagged configuration.");
    }

    AZ::JsonSerializationResult::Result JsonConfigurationSerializer::LoadOptionalConstraint(
        void* outputValue,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        AZ_Assert(outputValue, "Configuration serializer received a null optional output value.");
        auto& output = *static_cast<OptionalConstraintGeometry*>(outputValue);
        if (inputValue.IsNull())
        {
            output.reset();
            return context.Report(
                JSR::Tasks::ReadField,
                JSR::Outcomes::Success,
                "Loaded an empty optional constraint.");
        }

        ConstraintGeometry geometry;
        JSR::ResultCode result = ContinueLoading(
            &geometry,
            azrtti_typeid<ConstraintGeometry>(),
            inputValue,
            context);
        if (result.GetProcessing() != JSR::Processing::Halted)
        {
            output = AZStd::move(geometry);
            return context.Report(result, "Loaded an optional constraint.");
        }

        return context.Report(result, "Failed to load an optional constraint.");
    }

    AZ::JsonSerializationResult::Result JsonConfigurationSerializer::StoreOptionalConstraint(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        AZ::JsonSerializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        AZ_Assert(inputValue, "Configuration serializer received a null optional input value.");
        const auto& input = *static_cast<const OptionalConstraintGeometry*>(inputValue);
        if (!input)
        {
            outputValue.SetNull();
            return context.Report(
                JSR::Tasks::WriteValue,
                JSR::Outcomes::Success,
                "Stored an empty optional constraint.");
        }

        const ConstraintGeometry* defaultGeometry = nullptr;
        if (const auto* defaults = static_cast<const OptionalConstraintGeometry*>(defaultValue))
        {
            if (*defaults)
            {
                defaultGeometry = &defaults->value();
            }
        }
        JSR::ResultCode result = ContinueStoring(
            outputValue,
            &input.value(),
            defaultGeometry,
            azrtti_typeid<ConstraintGeometry>(),
            context);
        return context.Report(result, "Stored an optional constraint.");
    }

    AZ::JsonSerializationResult::Result JsonConfigurationSerializer::Load(
        void* outputValue,
        const AZ::Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        if (outputValueTypeId == azrtti_typeid<BroadPhaseCastGeometry>())
        {
            return LoadVariant<BroadPhaseCastGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<BroadPhaseOverlapGeometry>())
        {
            return LoadVariant<BroadPhaseOverlapGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<ConstraintComponentGeometry>())
        {
            return LoadVariant<ConstraintComponentGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<ConstraintGeometry>())
        {
            return LoadVariant<ConstraintGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<ConstraintMeasurements>())
        {
            return LoadVariant<ConstraintMeasurements>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<CookedDecoratedShapeGeometry>())
        {
            return LoadVariant<CookedDecoratedShapeGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<DecoratedShapeGeometry>())
        {
            return LoadVariant<DecoratedShapeGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<PrimitiveShapeGeometry>())
        {
            return LoadVariant<PrimitiveShapeGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<RagdollConstraintComponentGeometry>())
        {
            return LoadVariant<RagdollConstraintComponentGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<SceneAssetBody>())
        {
            return LoadVariant<SceneAssetBody>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<SceneBodyConfiguration>())
        {
            return LoadVariant<SceneBodyConfiguration>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<SceneSourceShape>())
        {
            return LoadVariant<SceneSourceShape>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<ShapeGeometry>())
        {
            return LoadVariant<ShapeGeometry>(outputValue, inputValue, context);
        }

        if (outputValueTypeId == azrtti_typeid<OptionalConstraintGeometry>())
        {
            return LoadOptionalConstraint(outputValue, inputValue, context);
        }

        return context.Report(
            AZ::JsonSerializationResult::Tasks::ReadField,
            AZ::JsonSerializationResult::Outcomes::Unsupported,
            "The configuration serializer received an unsupported type.");
    }

    AZ::JsonSerializationResult::Result JsonConfigurationSerializer::Store(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        const AZ::Uuid& valueTypeId,
        AZ::JsonSerializerContext& context)
    {
        if (valueTypeId == azrtti_typeid<BroadPhaseCastGeometry>())
        {
            return StoreVariant<BroadPhaseCastGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<BroadPhaseOverlapGeometry>())
        {
            return StoreVariant<BroadPhaseOverlapGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<ConstraintComponentGeometry>())
        {
            return StoreVariant<ConstraintComponentGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<ConstraintGeometry>())
        {
            return StoreVariant<ConstraintGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<ConstraintMeasurements>())
        {
            return StoreVariant<ConstraintMeasurements>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<CookedDecoratedShapeGeometry>())
        {
            return StoreVariant<CookedDecoratedShapeGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<DecoratedShapeGeometry>())
        {
            return StoreVariant<DecoratedShapeGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<PrimitiveShapeGeometry>())
        {
            return StoreVariant<PrimitiveShapeGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<RagdollConstraintComponentGeometry>())
        {
            return StoreVariant<RagdollConstraintComponentGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<SceneAssetBody>())
        {
            return StoreVariant<SceneAssetBody>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<SceneBodyConfiguration>())
        {
            return StoreVariant<SceneBodyConfiguration>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<SceneSourceShape>())
        {
            return StoreVariant<SceneSourceShape>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<ShapeGeometry>())
        {
            return StoreVariant<ShapeGeometry>(outputValue, inputValue, defaultValue, context);
        }

        if (valueTypeId == azrtti_typeid<OptionalConstraintGeometry>())
        {
            return StoreOptionalConstraint(outputValue, inputValue, defaultValue, context);
        }

        return context.Report(
            AZ::JsonSerializationResult::Tasks::WriteValue,
            AZ::JsonSerializationResult::Outcomes::Unsupported,
            "The configuration serializer received an unsupported type.");
    }

    void ReflectConfigurationSerializers(
        AZ::ReflectContext* context)
    {
        if (auto* jsonContext = azrtti_cast<AZ::JsonRegistrationContext*>(context))
        {
            jsonContext
                ->Serializer<JsonConfigurationSerializer>()
                ->HandlesType<BroadPhaseCastGeometry>()
                ->HandlesType<BroadPhaseOverlapGeometry>()
                ->HandlesType<ConstraintComponentGeometry>()
                ->HandlesType<ConstraintGeometry>()
                ->HandlesType<ConstraintMeasurements>()
                ->HandlesType<CookedDecoratedShapeGeometry>()
                ->HandlesType<DecoratedShapeGeometry>()
                ->HandlesType<OptionalConstraintGeometry>()
                ->HandlesType<PrimitiveShapeGeometry>()
                ->HandlesType<RagdollConstraintComponentGeometry>()
                ->HandlesType<SceneAssetBody>()
                ->HandlesType<SceneBodyConfiguration>()
                ->HandlesType<SceneSourceShape>()
                ->HandlesType<ShapeGeometry>();
        }
    }
} // namespace Jolt
