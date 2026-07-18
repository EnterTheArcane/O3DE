/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangOptionsModuleGenerator.h"

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/string/conversions.h>

namespace AZ::ShaderBuilder::SlangOptionsModuleGenerator
{
    AZStd::span<const AZStd::string_view> GetAuthoringMacroLines()
    {
        // The accessor ABI is always `int AtomOptionImpl_<name>()` so implementation modules stay
        // context-free: the authored property casts the int back to the authored type (bool and
        // Slang enums both accept the explicit cast). Names must match AccessorNamePrefix and
        // FallbackKeyGetterName.
        static constexpr AZStd::string_view macroLines[] = {
            "#define ATOM_OPTION(type, name, defaultValue) [AtomOption(#type, #defaultValue)] public extern int AtomOptionImpl_##name(); public property type name { get { return (type)AtomOptionImpl_##name(); } }",
            "#define ATOM_OPTION_RANGE(type, name, defaultValue, minValue, maxValue) [AtomOption(#type, #defaultValue)] [AtomOptionRange(minValue, maxValue)] public extern int AtomOptionImpl_##name(); public property type name { get { return (type)AtomOptionImpl_##name(); } }",
            "#define ATOM_VARIANT_FALLBACK(shaderResourceGroup, memberName) [AtomVariantFallback(#shaderResourceGroup, #memberName)] export uint4 Atom_GetShaderVariantKeyFallback() { return shaderResourceGroup.memberName; }",
        };
        return macroLines;
    }

    static AZStd::string GetAttributeArgumentString(slang::UserAttribute* attribute, unsigned argumentIndex)
    {
        size_t length = 0;
        const char* text = attribute->getArgumentValueString(argumentIndex, &length);
        return text ? AZStd::string(text, length) : AZStd::string();
    }

    //! Enum cases surface in declaration order as Kind::Unsupported children of the enum decl,
    //! before its synthesized `$__syn_*` operator functions.
    static AZStd::vector<Name> CollectEnumCaseNames(slang::DeclReflection* enumDecl)
    {
        AZStd::vector<Name> caseNames;
        for (unsigned childIndex = 0; childIndex < enumDecl->getChildrenCount(); ++childIndex)
        {
            slang::DeclReflection* child = enumDecl->getChild(childIndex);
            if (child
                && child->getKind() == slang::DeclReflection::Kind::Unsupported
                && child->getName() && child->getName()[0] != '\0' && child->getName()[0] != '$')
            {
                caseNames.push_back(Name(child->getName()));
            }
        }
        return caseNames;
    }

    AZ::Outcome<DiscoveredShaderOptions, AZStd::string> DiscoverShaderOptions(slang::ISession* session)
    {
        struct AccessorCandidate
        {
            AZStd::string m_functionName;
            AZStd::string m_typeName;
            AZStd::string m_defaultValue;
            bool m_hasRange = false;
            int m_minValue = 0;
            int m_maxValue = 0;
        };
        AZStd::vector<AccessorCandidate> candidates;
        AZStd::unordered_map<AZStd::string, AZStd::vector<Name>> enumCasesByName;
        DiscoveredShaderOptions discovered;

        // First pass over all loaded modules collects enum declarations so enumeration options
        // can reference enums from any module regardless of walk order
        const SlangInt moduleCount = session->getLoadedModuleCount();
        for (SlangInt moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            slang::DeclReflection* moduleDecl = session->getLoadedModule(moduleIndex)->getModuleReflection();
            if (!moduleDecl)
            {
                continue;
            }
            for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
            {
                slang::DeclReflection* child = moduleDecl->getChild(childIndex);
                if (child && child->getKind() == slang::DeclReflection::Kind::Enum && child->getName())
                {
                    enumCasesByName[child->getName()] = CollectEnumCaseNames(child);
                }
            }
        }

        // Second pass reads the attributed accessor functions in module-load order, then
        // declaration order — the deterministic packing order of the option layout
        for (SlangInt moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            slang::DeclReflection* moduleDecl = session->getLoadedModule(moduleIndex)->getModuleReflection();
            if (!moduleDecl)
            {
                continue;
            }
            for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
            {
                slang::DeclReflection* child = moduleDecl->getChild(childIndex);
                slang::FunctionReflection* function =
                    (child && child->getKind() == slang::DeclReflection::Kind::Func) ? child->asFunction() : nullptr;
                if (!function || !function->getName())
                {
                    continue;
                }

                AccessorCandidate candidate;
                bool isOptionAccessor = false;
                for (unsigned attributeIndex = 0; attributeIndex < function->getUserAttributeCount(); ++attributeIndex)
                {
                    slang::UserAttribute* attribute = function->getUserAttributeByIndex(attributeIndex);
                    if (azstricmp(attribute->getName(), "AtomOption") == 0)
                    {
                        isOptionAccessor = true;
                        candidate.m_functionName = function->getName();
                        candidate.m_typeName = GetAttributeArgumentString(attribute, 0);
                        candidate.m_defaultValue = GetAttributeArgumentString(attribute, 1);
                    }
                    else if (azstricmp(attribute->getName(), "AtomOptionRange") == 0)
                    {
                        candidate.m_hasRange =
                            SLANG_SUCCEEDED(attribute->getArgumentValueInt(0, &candidate.m_minValue))
                            && SLANG_SUCCEEDED(attribute->getArgumentValueInt(1, &candidate.m_maxValue));
                    }
                    else if (azstricmp(attribute->getName(), "AtomVariantFallback") == 0)
                    {
                        if (!discovered.m_fallbackMemberName.empty())
                        {
                            return AZ::Failure(AZStd::string(
                                "ATOM_VARIANT_FALLBACK is declared more than once; exactly one ShaderResourceGroup member can hold the ShaderVariantKey fallback"));
                        }
                        discovered.m_fallbackShaderResourceGroupName = GetAttributeArgumentString(attribute, 0);
                        discovered.m_fallbackMemberName = GetAttributeArgumentString(attribute, 1);
                    }
                }
                if (isOptionAccessor)
                {
                    candidates.push_back(AZStd::move(candidate));
                }
            }
        }

        for (const AccessorCandidate& candidate : candidates)
        {
            if (!candidate.m_functionName.starts_with(AccessorNamePrefix))
            {
                return AZ::Failure(AZStd::string::format(
                    "[AtomOption] is applied to %s; the attribute is reserved for the accessor declarations the ATOM_OPTION macros generate",
                    candidate.m_functionName.c_str()));
            }

            ShaderOptionDeclaration declaration;
            declaration.m_name = Name(candidate.m_functionName.substr(AccessorNamePrefix.size()));

            for (const ShaderOptionDeclaration& existing :
                AZStd::span<const ShaderOptionDeclaration>(discovered.m_declarations))
            {
                if (existing.m_name == declaration.m_name)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Shader option %s is declared more than once", declaration.m_name.GetCStr()));
                }
            }

            if (candidate.m_typeName == "bool")
            {
                declaration.m_type = RPI::ShaderOptionType::Boolean;
                if (candidate.m_defaultValue != "true" && candidate.m_defaultValue != "false")
                {
                    return AZ::Failure(AZStd::string::format(
                        "Boolean option %s has default '%s'; expected true or false",
                        declaration.m_name.GetCStr(), candidate.m_defaultValue.c_str()));
                }
                declaration.m_defaultValue = Name(candidate.m_defaultValue);
            }
            else if (
                candidate.m_typeName == "int" || candidate.m_typeName == "uint"
                || candidate.m_typeName == "i32" || candidate.m_typeName == "u32"
                || candidate.m_typeName == "int32_t" || candidate.m_typeName == "uint32_t")
            {
                if (!candidate.m_hasRange)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Integer option %s needs a value range; declare it with ATOM_OPTION_RANGE(type, name, default, min, max)",
                        declaration.m_name.GetCStr()));
                }
                declaration.m_type = RPI::ShaderOptionType::IntegerRange;
                declaration.m_minValue = candidate.m_minValue;
                declaration.m_maxValue = candidate.m_maxValue;

                char* parseEnd = nullptr;
                const long defaultValue = strtol(candidate.m_defaultValue.c_str(), &parseEnd, 10);
                if (parseEnd == candidate.m_defaultValue.c_str() || *parseEnd != '\0'
                    || defaultValue < candidate.m_minValue || defaultValue > candidate.m_maxValue)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Integer option %s has default '%s' outside its range [%d, %d]",
                        declaration.m_name.GetCStr(), candidate.m_defaultValue.c_str(),
                        candidate.m_minValue, candidate.m_maxValue));
                }
                declaration.m_defaultValue = Name(candidate.m_defaultValue);
            }
            else
            {
                auto enumIterator = enumCasesByName.find(candidate.m_typeName);
                if (enumIterator == enumCasesByName.end() || enumIterator->second.empty())
                {
                    return AZ::Failure(AZStd::string::format(
                        "Option %s has type %s, which is not bool, int, or an enum declared in the shader's modules",
                        declaration.m_name.GetCStr(), candidate.m_typeName.c_str()));
                }
                declaration.m_type = RPI::ShaderOptionType::Enumeration;
                declaration.m_enumValues = enumIterator->second;

                // The default is spelled as authored, e.g. "QualityT.Medium"; the value name is
                // the bare case label
                const size_t lastDot = candidate.m_defaultValue.find_last_of('.');
                const AZStd::string defaultLabel = lastDot == AZStd::string::npos
                    ? candidate.m_defaultValue
                    : candidate.m_defaultValue.substr(lastDot + 1);
                const Name defaultName(defaultLabel);
                if (AZStd::find(declaration.m_enumValues.begin(), declaration.m_enumValues.end(), defaultName)
                    == declaration.m_enumValues.end())
                {
                    return AZ::Failure(AZStd::string::format(
                        "Option %s has default '%s', which is not a case of enum %s",
                        declaration.m_name.GetCStr(), candidate.m_defaultValue.c_str(), candidate.m_typeName.c_str()));
                }
                declaration.m_defaultValue = defaultName;
            }

            discovered.m_declarations.push_back(AZStd::move(declaration));
        }

        return AZ::Success(AZStd::move(discovered));
    }

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

    //! The accessor's int result for an option's default: value indices are the integer values
    //! for every option type (bool 0/1, enum case index, integer value).
    static int32_t GetDefaultValueAsInt(const RPI::ShaderOptionDescriptor& option)
    {
        return aznumeric_cast<int32_t>(option.FindValue(option.GetDefaultValue()).GetIndex());
    }

    //! Appends one exported accessor whose body is @valueExpression.
    static void AppendAccessor(AZStd::string& module, const RPI::ShaderOptionDescriptor& option, AZStd::string_view valueExpression)
    {
        module += AZStd::string::format(
            "export int %.*s%s() { return %.*s; }\n",
            AZ_STRING_ARG(AccessorNamePrefix),
            option.GetName().GetCStr(),
            AZ_STRING_ARG(valueExpression));
    }

    AZStd::string GenerateImplementationModule(
        ShaderOptionLoweringMode mode,
        AZStd::string_view moduleName,
        const RPI::ShaderOptionGroupLayout& layout)
    {
        AZ_Assert(mode != ShaderOptionLoweringMode::Baked, "Baked accessor values come from GenerateBakedValuesModule");

        AZStd::string module = AZStd::string::format("module %.*s;\n\n", AZ_STRING_ARG(moduleName));

        if (mode == ShaderOptionLoweringMode::DynamicFallback)
        {
            // Satisfied by the export the ATOM_VARIANT_FALLBACK macro generated in the authored
            // module; the extern/export pair links in both directions across the composition
            module += AZStd::string::format("extern uint4 %.*s();\n\n", AZ_STRING_ARG(FallbackKeyGetterName));
        }

        uint32_t specializationId = 0;
        for (const RPI::ShaderOptionDescriptor& option : layout.GetShaderOptions())
        {
            switch (mode)
            {
            case ShaderOptionLoweringMode::SpecializationConstant:
                module += AZStd::string::format(
                    "[vk::constant_id(%u)]\n"
                    "const int AtomOptionSpecialization_%s = %d;\n",
                    specializationId,
                    option.GetName().GetCStr(),
                    GetDefaultValueAsInt(option));
                AppendAccessor(module, option, AZStd::string::format("AtomOptionSpecialization_%s", option.GetName().GetCStr()));
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
                    "(%.*s()[%u] >> %uu)",
                    AZ_STRING_ARG(FallbackKeyGetterName), wordIndex, bitInWord);
                if (bitInWord + bitCount > 32)
                {
                    readExpression = AZStd::string::format(
                        "(%s | (%.*s()[%u] << %uu))",
                        readExpression.c_str(),
                        AZ_STRING_ARG(FallbackKeyGetterName), wordIndex + 1, 32 - bitInWord);
                }
                AppendAccessor(module, option, AZStd::string::format("int(%s & 0x%Xu)", readExpression.c_str(), mask));
                break;
            }
            default:
                break;
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
            AppendAccessor(module, option, AZStd::to_string(aznumeric_cast<int32_t>(value.GetIndex())));
        }
        return module;
    }
} // namespace AZ::ShaderBuilder::SlangOptionsModuleGenerator
