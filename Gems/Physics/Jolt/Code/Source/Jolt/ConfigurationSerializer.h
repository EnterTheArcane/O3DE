/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/Json/BaseJsonSerializer.h>

namespace Jolt
{
    class JsonConfigurationSerializer final
        : public AZ::BaseJsonSerializer
    {
    public:
        AZ_RTTI(JsonConfigurationSerializer, "{F90666AD-0DCA-4EEB-868C-5B71A50B5E00}", AZ::BaseJsonSerializer);
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

    private:
        template<typename Variant, size_t Index = 0>
        AZ::JsonSerializationResult::ResultCode LoadAlternative(
            Variant& output,
            const AZ::TypeId& alternativeTypeId,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context);

        template<typename Variant>
        AZ::JsonSerializationResult::Result LoadVariant(
            void* outputValue,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context);

        template<typename Variant>
        AZ::JsonSerializationResult::Result StoreVariant(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            AZ::JsonSerializerContext& context);

        AZ::JsonSerializationResult::Result LoadOptionalConstraint(
            void* outputValue,
            const rapidjson::Value& inputValue,
            AZ::JsonDeserializerContext& context);

        AZ::JsonSerializationResult::Result StoreOptionalConstraint(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            AZ::JsonSerializerContext& context);
    };

    void ReflectConfigurationSerializers(AZ::ReflectContext* context);
} // namespace Jolt
