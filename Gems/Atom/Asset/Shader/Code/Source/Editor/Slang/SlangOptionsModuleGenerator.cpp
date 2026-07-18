/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "SlangOptionsModuleGenerator.h"

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/string/conversions.h>

namespace AZ::ShaderBuilder::SlangOptionsModuleGenerator
{
    template<typename Reflection>
    static slang::UserAttribute* FindAttributeByName(Reflection* reflection, const char* attributeName)
    {
        for (unsigned attributeIndex = 0; attributeIndex < reflection->getUserAttributeCount(); ++attributeIndex)
        {
            slang::UserAttribute* attribute = reflection->getUserAttributeByIndex(attributeIndex);
            if (azstricmp(attribute->getName(), attributeName) == 0)
            {
                return attribute;
            }
        }
        return nullptr;
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

    //! Reads one option declaration off an [AtomOption]-attributed interface requirement.
    static AZ::Outcome<ShaderOptionDeclaration, AZStd::string> ReadOptionDeclaration(
        slang::FunctionReflection* requirement,
        AZStd::string_view interfaceName,
        const AZStd::unordered_map<AZStd::string, AZStd::vector<Name>>& enumCasesByName)
    {
        ShaderOptionDeclaration declaration;
        declaration.m_methodName = requirement->getName();

        // [AtomOptionAlias] decouples the runtime name (layout, .materialtype references,
        // variant lists) from the Slang method name, so ports keep legacy AZSL option names
        // without carrying the prefix into the API
        if (slang::UserAttribute* aliasAttribute = FindAttributeByName(requirement, "AtomOptionAlias"))
        {
            size_t aliasLength = 0;
            const char* aliasText = aliasAttribute->getArgumentValueString(0, &aliasLength);
            const AZStd::string_view alias = aliasText ? AZStd::string_view(aliasText, aliasLength) : AZStd::string_view();
            const bool aliasIsIdentifier = !alias.empty()
                && !isdigit(static_cast<unsigned char>(alias.front()))
                && AZStd::all_of(alias.begin(), alias.end(), [](char character)
                    {
                        return isalnum(static_cast<unsigned char>(character)) || character == '_';
                    });
            if (!aliasIsIdentifier)
            {
                return AZ::Failure(AZStd::string::format(
                    "Option %s has an [AtomOptionAlias] runtime name that is not a valid identifier", requirement->getName()));
            }
            declaration.m_name = Name(alias);
        }
        else
        {
            declaration.m_name = Name(declaration.m_methodName);
        }

        slang::UserAttribute* optionAttribute = FindAttributeByName(requirement, "AtomOption");
        if (!optionAttribute)
        {
            return AZ::Failure(AZStd::string::format(
                "Options interface %.*s requirement %s has no [AtomOption(default)] attribute",
                AZ_STRING_ARG(interfaceName), requirement->getName()));
        }
        int defaultValue = 0;
        if (SLANG_FAILED(optionAttribute->getArgumentValueInt(0, &defaultValue)))
        {
            return AZ::Failure(AZStd::string::format(
                "Option %s has an [AtomOption] default that does not evaluate to a constant", requirement->getName()));
        }

        slang::TypeReflection* returnType = requirement->getReturnType();
        if (!returnType || !returnType->getName())
        {
            return AZ::Failure(AZStd::string::format("Option %s has no readable return type", requirement->getName()));
        }
        declaration.m_typeText = returnType->getName();

        if (returnType->getKind() == slang::TypeReflection::Kind::Scalar
            && returnType->getScalarType() == slang::TypeReflection::ScalarType::Bool)
        {
            declaration.m_type = RPI::ShaderOptionType::Boolean;
            declaration.m_defaultValue = Name(defaultValue != 0 ? "true" : "false");
        }
        else if (returnType->getKind() == slang::TypeReflection::Kind::Scalar
            && (returnType->getScalarType() == slang::TypeReflection::ScalarType::Int32
                || returnType->getScalarType() == slang::TypeReflection::ScalarType::UInt32))
        {
            slang::UserAttribute* rangeAttribute = FindAttributeByName(requirement, "AtomOptionRange");
            int minValue = 0;
            int maxValue = 0;
            if (!rangeAttribute
                || SLANG_FAILED(rangeAttribute->getArgumentValueInt(0, &minValue))
                || SLANG_FAILED(rangeAttribute->getArgumentValueInt(1, &maxValue)))
            {
                return AZ::Failure(AZStd::string::format(
                    "Integer option %s needs an [AtomOptionRange(min, max)] attribute", requirement->getName()));
            }
            if (defaultValue < minValue || defaultValue > maxValue)
            {
                return AZ::Failure(AZStd::string::format(
                    "Integer option %s has default %d outside its range [%d, %d]",
                    requirement->getName(), defaultValue, minValue, maxValue));
            }
            declaration.m_type = RPI::ShaderOptionType::IntegerRange;
            declaration.m_minValue = minValue;
            declaration.m_maxValue = maxValue;
            declaration.m_defaultValue = Name(AZStd::to_string(defaultValue));
        }
        else if (returnType->getKind() == slang::TypeReflection::Kind::Enum)
        {
            auto enumIterator = enumCasesByName.find(declaration.m_typeText);
            if (enumIterator == enumCasesByName.end() || enumIterator->second.empty())
            {
                return AZ::Failure(AZStd::string::format(
                    "Option %s returns enum %s, whose declaration was not found in the shader's modules",
                    requirement->getName(), declaration.m_typeText.c_str()));
            }
            if (defaultValue < 0 || defaultValue >= aznumeric_cast<int>(enumIterator->second.size()))
            {
                return AZ::Failure(AZStd::string::format(
                    "Option %s has default %d, which is not a case index of enum %s",
                    requirement->getName(), defaultValue, declaration.m_typeText.c_str()));
            }
            declaration.m_type = RPI::ShaderOptionType::Enumeration;
            declaration.m_enumValues = enumIterator->second;
            declaration.m_defaultValue = enumIterator->second[defaultValue];
        }
        else
        {
            return AZ::Failure(AZStd::string::format(
                "Option %s has type %s; shader options are bool, int/uint, or an enum",
                requirement->getName(), declaration.m_typeText.c_str()));
        }

        return AZ::Success(AZStd::move(declaration));
    }

    AZ::Outcome<DiscoveredShaderOptions, AZStd::string> DiscoverShaderOptions(slang::ISession* session)
    {
        DiscoveredShaderOptions discovered;

        struct NamedDecl
        {
            AZStd::string m_name;
            slang::DeclReflection* m_decl = nullptr;
            SlangInt m_moduleIndex = 0;
        };
        AZStd::vector<NamedDecl> interfaceRecords;
        AZStd::vector<NamedDecl> providerCandidates;
        AZStd::unordered_map<AZStd::string, AZStd::vector<Name>> enumCasesByName;

        // Pass 1 over all loaded modules: enum declarations, the [AtomVariantFallback] member,
        // and every named top-level declaration classified through the module layout's type
        // reflection (interface decls surface as Kind::Unsupported, so the layout is the only
        // reliable classifier).
        const SlangInt moduleCount = session->getLoadedModuleCount();
        for (SlangInt moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            slang::IModule* currentModule = session->getLoadedModule(moduleIndex);
            slang::DeclReflection* moduleDecl = currentModule->getModuleReflection();
            slang::ProgramLayout* moduleLayout = currentModule->getLayout();
            if (!moduleDecl || !moduleLayout)
            {
                continue;
            }
            for (unsigned childIndex = 0; childIndex < moduleDecl->getChildrenCount(); ++childIndex)
            {
                slang::DeclReflection* child = moduleDecl->getChild(childIndex);
                if (!child || !child->getName() || child->getName()[0] == '\0' || child->getName()[0] == '$')
                {
                    continue;
                }

                if (child->getKind() == slang::DeclReflection::Kind::Enum)
                {
                    if (!enumCasesByName.contains(child->getName()))
                    {
                        enumCasesByName[child->getName()] = CollectEnumCaseNames(child);
                    }
                    continue;
                }

                if (child->getKind() == slang::DeclReflection::Kind::Variable)
                {
                    // A ParameterBlock variable whose element struct has an [AtomVariantFallback]
                    // member designates the ShaderVariantKey fallback
                    slang::VariableReflection* variable = child->asVariable();
                    slang::TypeReflection* variableType = variable ? variable->getType() : nullptr;
                    if (variableType && variableType->getKind() == slang::TypeReflection::Kind::ParameterBlock)
                    {
                        slang::TypeReflection* elementType = variableType->getElementType();
                        for (unsigned fieldIndex = 0; elementType && fieldIndex < elementType->getFieldCount(); ++fieldIndex)
                        {
                            slang::VariableReflection* field = elementType->getFieldByIndex(fieldIndex);
                            if (field && FindAttributeByName(field, "AtomVariantFallback"))
                            {
                                if (!discovered.m_fallbackMemberName.empty())
                                {
                                    return AZ::Failure(AZStd::string(
                                        "[AtomVariantFallback] is declared on more than one ShaderResourceGroup member; exactly one can hold the ShaderVariantKey fallback"));
                                }
                                discovered.m_fallbackShaderResourceGroupName = child->getName();
                                discovered.m_fallbackMemberName = field->getName();
                                discovered.m_fallbackDeclaringModuleName = currentModule->getName();
                            }
                        }
                    }
                    continue;
                }

                slang::TypeReflection* namedType = moduleLayout->findTypeByName(child->getName());
                if (!namedType)
                {
                    continue;
                }
                if (namedType->getKind() == slang::TypeReflection::Kind::Interface)
                {
                    interfaceRecords.push_back({child->getName(), child, moduleIndex});
                }
                else if (namedType->getKind() == slang::TypeReflection::Kind::Struct
                    && FindAttributeByName(namedType, "AtomOptions"))
                {
                    providerCandidates.push_back({child->getName(), child, moduleIndex});
                }
            }
        }

        // Pass 2: match each [AtomOptions] struct to the one options interface it conforms to and
        // read the option set off that interface's requirements
        AZStd::unordered_set<AZStd::string> seenOptionNames;
        for (const NamedDecl& providerCandidate : providerCandidates)
        {
            slang::ProgramLayout* providerLayout = session->getLoadedModule(providerCandidate.m_moduleIndex)->getLayout();
            slang::TypeReflection* providerType = providerLayout ? providerLayout->findTypeByName(providerCandidate.m_name.c_str()) : nullptr;
            if (!providerType)
            {
                continue;
            }

            const NamedDecl* matchedInterface = nullptr;
            for (const NamedDecl& interfaceRecord : interfaceRecords)
            {
                slang::TypeReflection* interfaceType = providerLayout->findTypeByName(interfaceRecord.m_name.c_str());
                if (interfaceType && providerLayout->isSubType(providerType, interfaceType))
                {
                    if (matchedInterface)
                    {
                        return AZ::Failure(AZStd::string::format(
                            "[AtomOptions] struct %s conforms to more than one interface (%s and %s); options need exactly one",
                            providerCandidate.m_name.c_str(), matchedInterface->m_name.c_str(), interfaceRecord.m_name.c_str()));
                    }
                    matchedInterface = &interfaceRecord;
                }
            }
            if (!matchedInterface)
            {
                return AZ::Failure(AZStd::string::format(
                    "[AtomOptions] struct %s conforms to no interface declared in the shader's modules",
                    providerCandidate.m_name.c_str()));
            }

            DiscoveredOptionsProvider provider;
            provider.m_structName = providerCandidate.m_name;
            provider.m_interfaceName = matchedInterface->m_name;
            provider.m_declaringModuleName = session->getLoadedModule(providerCandidate.m_moduleIndex)->getName();

            for (unsigned memberIndex = 0; memberIndex < matchedInterface->m_decl->getChildrenCount(); ++memberIndex)
            {
                slang::DeclReflection* member = matchedInterface->m_decl->getChild(memberIndex);
                if (!member || !member->getName() || member->getName()[0] == '\0' || member->getName()[0] == '$'
                    || azstricmp(member->getName(), "This") == 0)
                {
                    continue;
                }
                slang::FunctionReflection* requirement = member->asFunction();
                if (!requirement)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Options interface %s member %s must be a static function requirement",
                        matchedInterface->m_name.c_str(), member->getName()));
                }

                auto declarationOutcome = ReadOptionDeclaration(requirement, matchedInterface->m_name, enumCasesByName);
                if (!declarationOutcome.IsSuccess())
                {
                    return AZ::Failure(declarationOutcome.TakeError());
                }
                ShaderOptionDeclaration declaration = declarationOutcome.TakeValue();
                if (!seenOptionNames.insert(declaration.m_name.GetStringView()).second)
                {
                    return AZ::Failure(AZStd::string::format(
                        "Shader option %s is declared more than once", declaration.m_name.GetCStr()));
                }
                provider.m_declarationIndices.push_back(discovered.m_declarations.size());
                discovered.m_declarations.push_back(AZStd::move(declaration));
            }

            if (provider.m_declarationIndices.empty())
            {
                return AZ::Failure(AZStd::string::format(
                    "[AtomOptions] struct %s conforms to interface %s, which declares no [AtomOption] requirements",
                    provider.m_structName.c_str(), provider.m_interfaceName.c_str()));
            }
            discovered.m_providers.push_back(AZStd::move(provider));
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

    //! The accessor's return expression for a known integer value: value indices are the integer
    //! values for every option type (bool 0/1, enum case index, integer value).
    static AZStd::string MakeValueExpression(const ShaderOptionDeclaration& declaration, int32_t value)
    {
        switch (declaration.m_type)
        {
        case RPI::ShaderOptionType::Boolean:
            return value != 0 ? "true" : "false";
        case RPI::ShaderOptionType::Enumeration:
            return AZStd::string::format("(%s)%d", declaration.m_typeText.c_str(), value);
        default:
            return AZStd::to_string(value);
        }
    }

    static int32_t GetDefaultValueAsInt(const RPI::ShaderOptionDescriptor& option)
    {
        return aznumeric_cast<int32_t>(option.FindValue(option.GetDefaultValue()).GetIndex());
    }

    //! Emits the provider structs, with @makeBody producing each option's return expression.
    template<typename MakeBody>
    static void AppendProviderStructs(
        AZStd::string& module,
        const DiscoveredShaderOptions& discovered,
        MakeBody&& makeBody)
    {
        for (const DiscoveredOptionsProvider& provider : discovered.m_providers)
        {
            module += AZStd::string::format("export struct %s : %s\n{\n", provider.m_structName.c_str(), provider.m_interfaceName.c_str());
            for (const size_t declarationIndex : provider.m_declarationIndices)
            {
                const ShaderOptionDeclaration& declaration = discovered.m_declarations[declarationIndex];
                module += AZStd::string::format(
                    "    static %s %s() { return %s; }\n",
                    declaration.m_typeText.c_str(),
                    declaration.m_methodName.c_str(),
                    makeBody(declaration).c_str());
            }
            module += "}\n\n";
        }
    }

    //! Module header plus the imports the generated structs need: every provider's declaring
    //! module (for the interface and any enum types) and the fallback member's module.
    static AZStd::string MakeModuleHeader(
        AZStd::string_view moduleName,
        const DiscoveredShaderOptions& discovered,
        bool importFallbackModule)
    {
        AZStd::string module = AZStd::string::format("module %.*s;\n\n", AZ_STRING_ARG(moduleName));
        AZStd::unordered_set<AZStd::string> importedModules;
        for (const DiscoveredOptionsProvider& provider : discovered.m_providers)
        {
            if (importedModules.insert(provider.m_declaringModuleName).second)
            {
                module += AZStd::string::format("import %s;\n", provider.m_declaringModuleName.c_str());
            }
        }
        if (importFallbackModule && !discovered.m_fallbackDeclaringModuleName.empty()
            && importedModules.insert(discovered.m_fallbackDeclaringModuleName).second)
        {
            module += AZStd::string::format("import %s;\n", discovered.m_fallbackDeclaringModuleName.c_str());
        }
        module += '\n';
        return module;
    }

    //! Builds a name → descriptor lookup for the layout's options.
    static AZStd::unordered_map<AZStd::string_view, const RPI::ShaderOptionDescriptor*> MakeOptionLookup(
        const RPI::ShaderOptionGroupLayout& layout)
    {
        AZStd::unordered_map<AZStd::string_view, const RPI::ShaderOptionDescriptor*> lookup;
        for (const RPI::ShaderOptionDescriptor& option : layout.GetShaderOptions())
        {
            lookup[option.GetName().GetStringView()] = &option;
        }
        return lookup;
    }

    AZStd::string GenerateImplementationModule(
        ShaderOptionLoweringMode mode,
        RHI::ShaderTargetFormat targetFormat,
        AZStd::string_view moduleName,
        const DiscoveredShaderOptions& discovered,
        const RPI::ShaderOptionGroupLayout& layout)
    {
        AZ_Assert(mode != ShaderOptionLoweringMode::Baked, "Baked accessor values come from GenerateBakedValuesModule");

        AZStd::string module = MakeModuleHeader(moduleName, discovered, mode == ShaderOptionLoweringMode::DynamicFallback);
        const auto optionLookup = MakeOptionLookup(layout);

        if (mode == ShaderOptionLoweringMode::SpecializationConstant && targetFormat == RHI::ShaderTargetFormat::Spirv)
        {
            // One native specialization constant per option, ids in layout order, holding the
            // actual (unbiased) integer value, defaulted to the option default
            uint32_t specializationId = 0;
            for (const RPI::ShaderOptionDescriptor& option : layout.GetShaderOptions())
            {
                module += AZStd::string::format(
                    "[vk::constant_id(%u)]\n"
                    "const int AtomOptionSpecialization_%s = %d;\n",
                    specializationId,
                    option.GetName().GetCStr(),
                    GetDefaultValueAsInt(option));
                ++specializationId;
            }
            module += '\n';

            AppendProviderStructs(module, discovered,
                [](const ShaderOptionDeclaration& declaration)
                {
                    const AZStd::string constantName = AZStd::string::format("AtomOptionSpecialization_%s", declaration.m_name.GetCStr());
                    switch (declaration.m_type)
                    {
                    case RPI::ShaderOptionType::Boolean:
                        return AZStd::string::format("%s != 0", constantName.c_str());
                    case RPI::ShaderOptionType::Enumeration:
                        return AZStd::string::format("(%s)%s", declaration.m_typeText.c_str(), constantName.c_str());
                    default:
                        return AZStd::string::format("(%s)%s", declaration.m_typeText.c_str(), constantName.c_str());
                    }
                });
        }
        else if (mode == ShaderOptionLoweringMode::SpecializationConstant)
        {
            // Targets without native specialization constants (Dxil): each specialization id is
            // routed through the compiler service's HLSL-prelude volatile read, so it survives
            // into the bytecode as a discrete dword the dxsc patch (PostProcessStage) can find
            module +=
                "int AtomReadSpecializationConstantRaw(int specializationId)\n"
                "{\n"
                "    __intrinsic_asm \"AtomReadSpecializationConstant($0)\";\n"
                "}\n\n";

            AppendProviderStructs(module, discovered,
                [&optionLookup](const ShaderOptionDeclaration& declaration)
                {
                    const RPI::ShaderOptionDescriptor& option = *optionLookup.find(declaration.m_name.GetStringView())->second;
                    const AZStd::string readExpression =
                        AZStd::string::format("AtomReadSpecializationConstantRaw(%d)", option.GetSpecializationId());
                    switch (declaration.m_type)
                    {
                    case RPI::ShaderOptionType::Boolean:
                        return AZStd::string::format("%s != 0", readExpression.c_str());
                    default:
                        return AZStd::string::format("(%s)%s", declaration.m_typeText.c_str(), readExpression.c_str());
                    }
                });
        }
        else
        {
            AppendProviderStructs(module, discovered,
                [&discovered, &optionLookup](const ShaderOptionDeclaration& declaration)
                {
                    // Extract the option's bits from the packed key member, handling 32-bit word
                    // straddles; integer options are stored biased by their range minimum
                    const RPI::ShaderOptionDescriptor& option = *optionLookup.find(declaration.m_name.GetStringView())->second;
                    const AZStd::string keyReference = AZStd::string::format(
                        "%s.%s", discovered.m_fallbackShaderResourceGroupName.c_str(), discovered.m_fallbackMemberName.c_str());
                    const uint32_t bitOffset = option.GetBitOffset();
                    const uint32_t bitCount = option.GetBitCount();
                    const uint32_t wordIndex = bitOffset / 32;
                    const uint32_t bitInWord = bitOffset % 32;
                    const uint32_t mask = bitCount >= 32 ? ~0u : ((1u << bitCount) - 1);

                    AZStd::string readExpression = AZStd::string::format(
                        "(%s[%u] >> %uu)", keyReference.c_str(), wordIndex, bitInWord);
                    if (bitInWord + bitCount > 32)
                    {
                        readExpression = AZStd::string::format(
                            "(%s | (%s[%u] << %uu))",
                            readExpression.c_str(), keyReference.c_str(), wordIndex + 1, 32 - bitInWord);
                    }
                    readExpression = AZStd::string::format("(%s & 0x%Xu)", readExpression.c_str(), mask);

                    switch (declaration.m_type)
                    {
                    case RPI::ShaderOptionType::Boolean:
                        return AZStd::string::format("%s != 0u", readExpression.c_str());
                    case RPI::ShaderOptionType::Enumeration:
                        return AZStd::string::format("(%s)%s", declaration.m_typeText.c_str(), readExpression.c_str());
                    default:
                        return AZStd::string::format(
                            "(%s)(int(%s) + %d)",
                            declaration.m_typeText.c_str(), readExpression.c_str(), declaration.m_minValue);
                    }
                });
        }

        return module;
    }

    AZStd::string GenerateBakedValuesModule(
        AZStd::string_view moduleName,
        const DiscoveredShaderOptions& discovered,
        const RPI::ShaderOptionGroup& optionGroup)
    {
        const RPI::ShaderOptionGroupLayout* layout = optionGroup.GetShaderOptionLayout();
        AZStd::string module = MakeModuleHeader(moduleName, discovered, false);

        AppendProviderStructs(module, discovered,
            [layout, &optionGroup](const ShaderOptionDeclaration& declaration)
            {
                const RPI::ShaderOptionIndex optionIndex = layout->FindShaderOptionIndex(declaration.m_name);
                RPI::ShaderOptionValue value = optionGroup.GetValue(optionIndex);
                if (!value.IsValid())
                {
                    const RPI::ShaderOptionDescriptor& option = layout->GetShaderOptions()[optionIndex.GetIndex()];
                    value = option.FindValue(option.GetDefaultValue());
                }
                return MakeValueExpression(declaration, aznumeric_cast<int32_t>(value.GetIndex()));
            });
        return module;
    }
} // namespace AZ::ShaderBuilder::SlangOptionsModuleGenerator
