/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Serialization/Json/BaseJsonSerializer.h>

namespace Box3D
{
    class JsonTaggedVariantSerializerBase
        : public AZ::BaseJsonSerializer
    {
    public:
        AZ_RTTI(JsonTaggedVariantSerializerBase, "{37D78582-F743-4F54-AE69-FCC7D704622B}", AZ::BaseJsonSerializer);

    protected:
        template<typename Variant, size_t Index = 0>
        AZ::JsonSerializationResult::ResultCode LoadAlternative(
            Variant& output,
            const AZ::TypeId& alternativeTypeId,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context);

        template<typename Variant>
        AZ::JsonSerializationResult::Result LoadVariant(
            void* outputValue,
            const AZ::Uuid& outputValueTypeId,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context);

        template<typename Variant>
        AZ::JsonSerializationResult::Result StoreVariant(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            const AZ::Uuid& valueTypeId,
            AZ::JsonSerializerContext& context);
    };

    class JsonShapeGeometrySerializer final
        : public JsonTaggedVariantSerializerBase
    {
    public:
        AZ_RTTI(JsonShapeGeometrySerializer, "{FA3026F9-3349-4D12-9E43-D831A65FA63D}", JsonTaggedVariantSerializerBase);
        AZ_CLASS_ALLOCATOR_DECL;

        AZ::JsonSerializationResult::Result Load(
            void* outputValue,
            const AZ::Uuid& outputValueTypeId,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context) override;

        AZ::JsonSerializationResult::Result Store(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            const AZ::Uuid& valueTypeId,
            AZ::JsonSerializerContext& context) override;

    };

    class JsonCompoundChildGeometrySerializer final
        : public JsonTaggedVariantSerializerBase
    {
    public:
        AZ_RTTI(JsonCompoundChildGeometrySerializer, "{4B6A8D88-E3EA-4696-B3CE-6C132597AE3C}", JsonTaggedVariantSerializerBase);
        AZ_CLASS_ALLOCATOR_DECL;

        AZ::JsonSerializationResult::Result Load(
            void* outputValue,
            const AZ::Uuid& outputValueTypeId,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context) override;

        AZ::JsonSerializationResult::Result Store(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            const AZ::Uuid& valueTypeId,
            AZ::JsonSerializerContext& context) override;
    };

    class JsonJointConfigurationSerializer final
        : public JsonTaggedVariantSerializerBase
    {
    public:
        AZ_RTTI(JsonJointConfigurationSerializer, "{7203F295-7ED4-4D2F-A08A-863DE54C397F}", JsonTaggedVariantSerializerBase);
        AZ_CLASS_ALLOCATOR_DECL;

        AZ::JsonSerializationResult::Result Load(
            void* outputValue,
            const AZ::Uuid& outputValueTypeId,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context) override;

        AZ::JsonSerializationResult::Result Store(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            const AZ::Uuid& valueTypeId,
            AZ::JsonSerializerContext& context) override;
    };
} // namespace Box3D
