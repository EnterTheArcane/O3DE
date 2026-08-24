/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Symbol/SymbolJsonSerializer.h>

#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    AZ_CLASS_ALLOCATOR_IMPL(SymbolJsonSerializer, SystemAllocator);

    JsonSerializationResult::Result SymbolJsonSerializer::Load(
        void* outputValue,
        [[maybe_unused]] const Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue,
        JsonDeserializerContext& context)
    {
        namespace JSR = JsonSerializationResult;

        AZ_Assert(
            azrtti_typeid<Symbol>() == outputValueTypeId,
            "Unable to deserialize AZ::Symbol because the provided type is %s",
            outputValueTypeId.ToString<AZStd::string>().c_str());

        Symbol* symbol = reinterpret_cast<Symbol*>(outputValue);
        AZ_Assert(symbol, "Output value for SymbolJsonSerializer cannot be null");

        if (!inputValue.IsString())
        {
            return context.Report(
                JSR::Tasks::ReadField,
                JSR::Outcomes::Invalid,
                "AZ::Symbol JSON value must be a string");
        }

        const AZStd::string_view value{inputValue.GetString(), inputValue.GetStringLength()};
        const AZStd::optional<Symbol> loadedSymbol = Symbol::TryCreate(value);
        if (!loadedSymbol)
        {
            const Internal::SymbolValidationError error = Internal::ValidateSymbolValue(value, Symbol::MaxStringSize);
            const char* message = "AZ::Symbol storage budget or allocation exhausted";
            if (error != Internal::SymbolValidationError::None)
            {
                message = Internal::GetSymbolValidationErrorMessage(error);
            }
            return context.Report(
                JSR::Tasks::ReadField,
                JSR::Outcomes::Invalid,
                message);
        }

        *symbol = *loadedSymbol;
        return context.Report(
            JSR::Tasks::ReadField,
            JSR::Outcomes::Success,
            "Successfully loaded AZ::Symbol");
    }

    JsonSerializationResult::Result SymbolJsonSerializer::Store(
        rapidjson::Value& outputValue,
        const void* inputValue,
        const void* defaultValue,
        [[maybe_unused]] const Uuid& valueTypeId,
        JsonSerializerContext& context)
    {
        namespace JSR = JsonSerializationResult;

        AZ_Assert(
            azrtti_typeid<Symbol>() == valueTypeId,
            "Unable to serialize AZ::Symbol because the provided type is %s",
            valueTypeId.ToString<AZStd::string>().c_str());

        const Symbol* symbol = reinterpret_cast<const Symbol*>(inputValue);
        AZ_Assert(symbol, "Input value for SymbolJsonSerializer cannot be null");
        const Symbol* defaultSymbol = reinterpret_cast<const Symbol*>(defaultValue);
        if (!context.ShouldKeepDefaults() && defaultSymbol && *symbol == *defaultSymbol)
        {
            return context.Report(
                JSR::Tasks::WriteValue,
                JSR::Outcomes::DefaultsUsed,
                "Default AZ::Symbol used");
        }

        const AZStd::string_view value = symbol->GetStringView();
        outputValue.SetString(symbol->GetCStr(), static_cast<rapidjson::SizeType>(value.size()), context.GetJsonAllocator());
        return context.Report(JSR::Tasks::WriteValue, JSR::Outcomes::Success, "AZ::Symbol successfully stored");
    }

    BaseJsonSerializer::OperationFlags SymbolJsonSerializer::GetOperationsFlags() const
    {
        return OperationFlags::ManualDefault;
    }
} // namespace AZ
