/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "Emitter.h"
#include "PlatformEmitter_CommonVulkan.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace AZ::ShaderCompiler
{
    std::string PlatformEmitter_CommonVulkan::GetSpecializationConstant(const CodeEmitter& codeEmitter, const IdentifierUID& symbolUid, const Options& options) const
    {
        std::stringstream stream;
        auto* ir = codeEmitter.GetIR();
        auto* varInfo = ir->GetSymbolSubAs<VarInfo>(symbolUid.GetName());
        auto retInfo = varInfo->GetTypeRefInfo();
        std::string typeAsStr = codeEmitter.GetTranslatedName(retInfo, UsageContext::ReferenceSite);
        std::string defaultValue = codeEmitter.GetInitializerClause(varInfo);
        Modifiers forbidden = StorageFlag::Static;
        assert(varInfo->m_specializationId >= 0);
        stream << "[[vk::constant_id(" << varInfo->m_specializationId << ")]]\n";
        if (retInfo.m_typeClass == TypeClass::Enum)
        {
            // Enums are not a valid type for specialization constant, so we use the underlaying scalar type.
            // The we add a global static variable with the enum type that cast from the specialization constant.
            auto* enumClassInfo = ir->GetSymbolSubAs<ClassInfo>(retInfo.m_typeId.GetName());
            auto& enumerators = enumClassInfo->GetOrderedMembers();
            auto scalarType = enumClassInfo->Get<EnumerationInfo>()->m_underlyingType.m_arithmeticInfo.UnderlyingScalarToStr();
            std::string scName = JoinAllNestedNamesWithUnderscore(symbolUid.m_name) + "_SC_OPTION";
            stream << "const " << scalarType << " " << scName;
            // TODO: if using a default value, emit it as the underlying scalar type, since enums are not valid default values for
            // specialization constants (casting is also not allowed). We set default values for shader options at runtime so it's not
            // a problem.
            stream << " = (" << scalarType << ")" << "0;\n";
            // Set the default value for the global static variable as the value of the specialization constant.
            defaultValue = std::move(scName);
            forbidden = Modifiers{};
        }
        stream << codeEmitter.GetTranslatedName(varInfo->m_typeInfoExt, UsageContext::ReferenceSite, options, forbidden) + " ";
        stream << codeEmitter.GetTranslatedName(symbolUid.m_name, UsageContext::DeclarationSite) + " = ";
        std::string emittedDefaultValue = defaultValue;
        if (defaultValue.empty())
        {
            emittedDefaultValue = "0";
        }

        stream << "(" << typeAsStr << ")" << emittedDefaultValue << "; \n";
        return stream.str();
    }

    std::pair<std::string, std::string> PlatformEmitter_CommonVulkan::GetDataViewHeaderFooter(
        const CodeEmitter& codeEmitter,
        const IdentifierUID& symbol,
        uint32_t bindInfoRegisterIndex,
        std::string_view registerTypeLetter,
        std::optional<std::string> stringifiedLogicalSpace,
        const Options& options) const
    {
        std::stringstream stream;
        std::optional<AttributeInfo> inputAttachmentIndexAttribute;
        auto varInfo = codeEmitter.GetIR()->GetSymbolSubAs<VarInfo>(symbol.GetName());
        bool isSubpassInput = varInfo->m_typeInfoExt.m_coreType.m_typeId.GetName().starts_with("?SubpassInput");
        if (isSubpassInput)
        {
            inputAttachmentIndexAttribute = codeEmitter.GetIR()->m_symbols.GetAttribute(symbol, "input_attachment_index");
            if (inputAttachmentIndexAttribute)
            {
                inputAttachmentIndexAttribute->m_namespace = "vk";
                inputAttachmentIndexAttribute->m_category = AttributeCategory::Sequence;
                inputAttachmentIndexAttribute->m_argList[0] = static_cast<ConstNumericVal>(ExtractValueAs<int32_t>(std::get<ConstNumericVal>(inputAttachmentIndexAttribute->m_argList[0]), 0) + options.m_subpassInputsOffset);
                MakeOStreamStreamable soss(stream);
                CodeEmitter::EmitAttribute(*inputAttachmentIndexAttribute, soss);
                stream << "[[vk::binding(" << bindInfoRegisterIndex;
                if (stringifiedLogicalSpace)
                {
                    stream << ", " << *stringifiedLogicalSpace;
                }
                stream << ")]]\n";
            }
        }

        std::string registerString;
        if (!inputAttachmentIndexAttribute)
        {
            // fallback to the base behavior in non-input-attachment cases for the `.. : register();` syntax.
            registerString = PlatformEmitter::GetDataViewHeaderFooter(
                codeEmitter,
                symbol,
                bindInfoRegisterIndex,
                registerTypeLetter,
                stringifiedLogicalSpace,
                options).second;
        }
        return {stream.str(), registerString};
    }
}
