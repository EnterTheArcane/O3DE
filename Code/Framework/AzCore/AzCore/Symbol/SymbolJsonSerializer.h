/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Serialization/Json/BaseJsonSerializer.h>

namespace AZ
{
    class AZCORE_API SymbolJsonSerializer final
        : public BaseJsonSerializer
    {
    public:
        AZ_RTTI(SymbolJsonSerializer, "{BC99786D-D557-4381-B23B-0647DA66E735}", BaseJsonSerializer);
        AZ_CLASS_ALLOCATOR_DECL;

        JsonSerializationResult::Result Load(
            void* outputValue,
            const Uuid& outputValueTypeId,
            const rapidjson::Value& inputValue,
            JsonDeserializerContext& context) override;

        JsonSerializationResult::Result Store(
            rapidjson::Value& outputValue,
            const void* inputValue,
            const void* defaultValue,
            const Uuid& valueTypeId,
            JsonSerializerContext& context) override;

        OperationFlags GetOperationsFlags() const override;
    };
} // namespace AZ
