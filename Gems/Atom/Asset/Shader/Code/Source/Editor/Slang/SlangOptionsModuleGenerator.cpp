/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangOptionsModuleGenerator.h"

#include <AzCore/std/string/conversions.h>

namespace AZ::ShaderBuilder::SlangOptionsModuleGenerator
{
    AZ::Outcome<RPI::Ptr<RPI::ShaderOptionGroupLayout>, AZStd::string> BuildShaderOptionGroupLayout(
        AZStd::span<const ShaderOptionDeclaration> declarations)
    {
        RPI::Ptr<RPI::ShaderOptionGroupLayout> layout = RPI::ShaderOptionGroupLayout::Create();

        uint32_t runningBitOffset = 0;
        uint32_t order = 0;
        for (const ShaderOptionDeclaration& declaration : declarations)
        {
            AZStd::vector<RPI::ShaderOptionValuePair> valuePairs;
            switch (declaration.m_type)
            {
            case RPI::ShaderOptionType::Boolean:
                valuePairs.push_back({Name("false"), RPI::ShaderOptionValue(0)});
                valuePairs.push_back({Name("true"), RPI::ShaderOptionValue(1)});
                break;
            case RPI::ShaderOptionType::Enumeration:
                if (declaration.m_enumValues.empty())
                {
                    return AZ::Failure(AZStd::string::format(
                        "Enumeration option %s declares no values", declaration.m_name.GetCStr()));
                }
                for (size_t valueIndex = 0; valueIndex < declaration.m_enumValues.size(); ++valueIndex)
                {
                    valuePairs.push_back({declaration.m_enumValues[valueIndex], RPI::ShaderOptionValue(valueIndex)});
                }
                break;
            case RPI::ShaderOptionType::IntegerRange:
                if (declaration.m_maxValue < declaration.m_minValue)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Integer range option %s has an empty range", declaration.m_name.GetCStr()));
                }
                valuePairs.push_back({Name(AZStd::to_string(declaration.m_minValue)), RPI::ShaderOptionValue(declaration.m_minValue)});
                valuePairs.push_back({Name(AZStd::to_string(declaration.m_maxValue)), RPI::ShaderOptionValue(declaration.m_maxValue)});
                break;
            default:
                return AZ::Failure(AZStd::string::format("Option %s has an unsupported type", declaration.m_name.GetCStr()));
            }

            const Name defaultValue = declaration.m_defaultValue.IsEmpty() ? valuePairs[0].first : declaration.m_defaultValue;

            // Deterministic packing: sequential bit offsets in declaration order, builder-assigned
            // specialization ids in the same order — the packing AZSLC produces with --sc-options
            const RPI::ShaderOptionDescriptor descriptor(
                declaration.m_name,
                declaration.m_type,
                runningBitOffset,
                order,
                valuePairs,
                defaultValue,
                0,
                aznumeric_cast<int32_t>(order));
            if (!layout->AddShaderOption(descriptor))
            {
                return AZ::Failure(AZStd::string::format("Failed to add option %s to the layout", declaration.m_name.GetCStr()));
            }
            runningBitOffset += descriptor.GetBitCount();
            ++order;
        }

        layout->Finalize();
        return AZ::Success(AZStd::move(layout));
    }

    //! The Slang type an option lowers to: booleans stay bool, everything else is a plain int
    //! (enumeration values are their value indices). Generated code uses raw primitives, not the
    //! prelude aliases, so it compiles in any session regardless of prelude injection.
    static AZStd::string_view GetOptionTypeText(RPI::ShaderOptionType type)
    {
        return type == RPI::ShaderOptionType::Boolean ? "bool" : "int";
    }

    static AZStd::string GetDefaultValueText(const RPI::ShaderOptionDescriptor& option)
    {
        const RPI::ShaderOptionValue defaultValue = option.FindValue(option.GetDefaultValue());
        if (option.GetType() == RPI::ShaderOptionType::Boolean)
        {
            return defaultValue.GetIndex() != 0 ? "true" : "false";
        }
        return AZStd::to_string(aznumeric_cast<int32_t>(defaultValue.GetIndex()));
    }

    AZStd::string GenerateOptionsModule(
        ShaderOptionLoweringMode mode,
        AZStd::string_view moduleName,
        const RPI::ShaderOptionGroupLayout& layout,
        AZStd::string_view fallbackBufferName)
    {
        AZStd::string module = AZStd::string::format("module %.*s;\n\n", AZ_STRING_ARG(moduleName));

        if (mode == ShaderOptionLoweringMode::DynamicFallback)
        {
            // The fallback-key constant buffer carrying the packed ShaderVariantKey bits
            // (feasibility gate 2's proven dynamic form; binding integration follows the
            // variant-fallback ShaderResourceGroup work)
            module += AZStd::string::format(
                "struct %.*s_Key\n"
                "{\n"
                "    uint4 m_keyBits;\n"
                "};\n\n"
                "ConstantBuffer<%.*s_Key> %.*s;\n\n",
                AZ_STRING_ARG(fallbackBufferName),
                AZ_STRING_ARG(fallbackBufferName),
                AZ_STRING_ARG(fallbackBufferName));
        }

        uint32_t specializationId = 0;
        for (const RPI::ShaderOptionDescriptor& option : layout.GetShaderOptions())
        {
            const AZStd::string_view typeText = GetOptionTypeText(option.GetType());
            const AZStd::string defaultText = GetDefaultValueText(option);

            switch (mode)
            {
            case ShaderOptionLoweringMode::Baked:
                // Satisfied by the export in the values module composed at link time
                module += AZStd::string::format(
                    "public extern static const %.*s %s;\n",
                    AZ_STRING_ARG(typeText),
                    option.GetName().GetCStr());
                break;
            case ShaderOptionLoweringMode::SpecializationConstant:
                module += AZStd::string::format(
                    "[vk::constant_id(%u)]\n"
                    "public const %.*s %s = %s;\n",
                    specializationId,
                    AZ_STRING_ARG(typeText),
                    option.GetName().GetCStr(),
                    defaultText.c_str());
                break;
            case ShaderOptionLoweringMode::DynamicFallback:
            {
                // Extract the option's bits from the packed key, handling 32-bit word straddles
                const uint32_t bitOffset = option.GetBitOffset();
                const uint32_t bitCount = option.GetBitCount();
                const uint32_t wordIndex = bitOffset / 32;
                const uint32_t bitInWord = bitOffset % 32;
                const uint32_t mask = bitCount >= 32 ? ~0u : ((1u << bitCount) - 1);

                AZStd::string readExpression = AZStd::string::format(
                    "(%.*s.m_keyBits[%u] >> %uu)",
                    AZ_STRING_ARG(fallbackBufferName), wordIndex, bitInWord);
                if (bitInWord + bitCount > 32)
                {
                    readExpression = AZStd::string::format(
                        "(%s | (%.*s.m_keyBits[%u] << %uu))",
                        readExpression.c_str(),
                        AZ_STRING_ARG(fallbackBufferName), wordIndex + 1, 32 - bitInWord);
                }
                readExpression = AZStd::string::format("(%s & 0x%Xu)", readExpression.c_str(), mask);

                if (option.GetType() == RPI::ShaderOptionType::Boolean)
                {
                    module += AZStd::string::format(
                        "public static bool %s = %s != 0u;\n", option.GetName().GetCStr(), readExpression.c_str());
                }
                else
                {
                    module += AZStd::string::format(
                        "public static int %s = int(%s);\n", option.GetName().GetCStr(), readExpression.c_str());
                }
                break;
            }
            }
            ++specializationId;
        }

        return module;
    }

    AZStd::string GenerateBakedValuesModule(
        AZStd::string_view moduleName,
        const RPI::ShaderOptionGroup& optionGroup)
    {
        const RPI::ShaderOptionGroupLayout* layout = optionGroup.GetShaderOptionLayout();

        AZStd::string module = AZStd::string::format("module %.*s;\n\n", AZ_STRING_ARG(moduleName));
        for (size_t optionIndex = 0; optionIndex < layout->GetShaderOptions().size(); ++optionIndex)
        {
            const RPI::ShaderOptionDescriptor& option = layout->GetShaderOptions()[optionIndex];
            RPI::ShaderOptionValue value = optionGroup.GetValue(RPI::ShaderOptionIndex(optionIndex));
            if (!value.IsValid())
            {
                value = option.FindValue(option.GetDefaultValue());
            }

            if (option.GetType() == RPI::ShaderOptionType::Boolean)
            {
                module += AZStd::string::format(
                    "export static const bool %s = %s;\n",
                    option.GetName().GetCStr(),
                    value.GetIndex() != 0 ? "true" : "false");
            }
            else
            {
                module += AZStd::string::format(
                    "export static const int %s = %d;\n",
                    option.GetName().GetCStr(),
                    aznumeric_cast<int32_t>(value.GetIndex()));
            }
        }
        return module;
    }
} // namespace AZ::ShaderBuilder::SlangOptionsModuleGenerator
