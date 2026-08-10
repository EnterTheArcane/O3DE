/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/ConfigurationSerializer.h>

#include <Box3D/Joints.h>
#include <Box3D/ShapeConfiguration.h>

#include <AzCore/Serialization/Json/JsonSerialization.h>

namespace Box3D
{
    namespace
    {
        const rapidjson::Value::StringRefType ValueField = rapidjson::StringRef("Value");
    } // namespace

    template<typename Variant, size_t Index>
    AZ::JsonSerializationResult::ResultCode JsonTaggedVariantSerializerBase::LoadAlternative(
        Variant& output,
        const AZ::TypeId& alternativeTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        if constexpr (Index == AZStd::variant_size_v<Variant>)
        {
            return context.Report(
                AZ::JsonSerializationResult::Tasks::ReadField,
                AZ::JsonSerializationResult::Outcomes::Unsupported,
                "The serialized configuration type is not an alternative of this variant.");
        }
        else
        {
            using Alternative = AZStd::variant_alternative_t<Index, Variant>;
            if (alternativeTypeId == azrtti_typeid<Alternative>())
            {
                Alternative alternative;
                AZ::JsonSerializationResult::ResultCode result =
                    ContinueLoadingFromJsonObjectField(&alternative, azrtti_typeid<Alternative>(), inputValue, ValueField, context);
                if (result.GetProcessing() != AZ::JsonSerializationResult::Processing::Halted)
                {
                    output = AZStd::move(alternative);
                }
                return result;
            }

            return LoadAlternative<Variant, Index + 1>(output, alternativeTypeId, inputValue, context);
        }
    }

    template<typename Variant>
    AZ::JsonSerializationResult::Result JsonTaggedVariantSerializerBase::LoadVariant(
        void* outputValue,
        [[maybe_unused]] const AZ::Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        AZ_Assert(outputValueTypeId == azrtti_typeid<Variant>(), "Configuration serializer received an unexpected output type.");
        AZ_Assert(outputValue, "Configuration serializer received a null output value.");

        if (!inputValue.IsObject())
        {
            return context.Report(JSR::Tasks::ReadField, JSR::Outcomes::Unsupported, "A tagged configuration must be an object.");
        }

        const auto typeMember = inputValue.FindMember(AZ::JsonSerialization::TypeIdFieldIdentifier);
        if (typeMember == inputValue.MemberEnd())
        {
            return context.Report(JSR::Tasks::ReadField, JSR::Outcomes::Missing, "A tagged configuration requires a $type field.");
        }

        AZ::TypeId alternativeTypeId;
        JSR::ResultCode result = LoadTypeId(alternativeTypeId, typeMember->value, context);
        if (result.GetProcessing() == JSR::Processing::Halted)
        {
            return context.Report(result, "Failed to read the tagged configuration type.");
        }

        result.Combine(LoadAlternative(*static_cast<Variant*>(outputValue), alternativeTypeId, inputValue, context));
        if (result.GetProcessing() != JSR::Processing::Halted)
        {
            return context.Report(result, "Loaded the tagged configuration.");
        }

        return context.Report(result, "Failed to load the tagged configuration.");
    }

    template<typename Variant>
    AZ::JsonSerializationResult::Result JsonTaggedVariantSerializerBase::StoreVariant(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        [[maybe_unused]] const AZ::Uuid& valueTypeId,
        AZ::JsonSerializerContext& context)
    {
        namespace JSR = AZ::JsonSerializationResult;

        AZ_Assert(valueTypeId == azrtti_typeid<Variant>(), "Configuration serializer received an unexpected input type.");
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
                    rapidjson::StringRef(AZ::JsonSerialization::TypeIdFieldIdentifier), AZStd::move(typeValue), context.GetJsonAllocator());

                const Alternative* defaultAlternative = nullptr;
                if (defaults)
                {
                    defaultAlternative = AZStd::get_if<Alternative>(defaults);
                }
                result.Combine(
                    ContinueStoringToJsonObjectField(
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

    AZ_CLASS_ALLOCATOR_IMPL(JsonShapeGeometrySerializer, AZ::SystemAllocator);
    AZ_CLASS_ALLOCATOR_IMPL(JsonCompoundChildGeometrySerializer, AZ::SystemAllocator);
    AZ_CLASS_ALLOCATOR_IMPL(JsonJointConfigurationSerializer, AZ::SystemAllocator);

    AZ::JsonSerializationResult::Result JsonShapeGeometrySerializer::Load(
        void* outputValue,
        const AZ::Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        return LoadVariant<ShapeGeometry>(outputValue, outputValueTypeId, inputValue, context);
    }

    AZ::JsonSerializationResult::Result JsonShapeGeometrySerializer::Store(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        const AZ::Uuid& valueTypeId,
        AZ::JsonSerializerContext& context)
    {
        return StoreVariant<ShapeGeometry>(outputValue, inputValue, defaultValue, valueTypeId, context);
    }

    AZ::JsonSerializationResult::Result JsonCompoundChildGeometrySerializer::Load(
        void* outputValue,
        const AZ::Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        return LoadVariant<CompoundChildGeometry>(outputValue, outputValueTypeId, inputValue, context);
    }

    AZ::JsonSerializationResult::Result JsonCompoundChildGeometrySerializer::Store(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        const AZ::Uuid& valueTypeId,
        AZ::JsonSerializerContext& context)
    {
        return StoreVariant<CompoundChildGeometry>(outputValue, inputValue, defaultValue, valueTypeId, context);
    }

    AZ::JsonSerializationResult::Result JsonJointConfigurationSerializer::Load(
        void* outputValue,
        const AZ::Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue,
        AZ::JsonDeserializerContext& context)
    {
        return LoadVariant<JointConfiguration>(outputValue, outputValueTypeId, inputValue, context);
    }

    AZ::JsonSerializationResult::Result JsonJointConfigurationSerializer::Store(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        const AZ::Uuid& valueTypeId,
        AZ::JsonSerializerContext& context)
    {
        return StoreVariant<JointConfiguration>(outputValue, inputValue, defaultValue, valueTypeId, context);
    }
} // namespace Box3D
