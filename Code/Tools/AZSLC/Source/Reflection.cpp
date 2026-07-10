/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "Reflection.h"
#include "PlatformEmitter.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace AZ::ShaderCompiler
{
    bool CodeReflection::BuildIAElement(
        rapidjson::Value& jsonVal,
        const IdentifierUID& uid,
        const bool allowStruct,
        JsonBuilder& json) const
    {
        const auto& varInfo = *m_ir->GetSymbolSubAs<VarInfo>(uid.m_name);

        if (IsPredefinedType(varInfo.GetTypeClass()))
        {
            if (const auto semOpt = varInfo.m_declNode->SemanticOpt)
            {
                // Semantic name and index
                auto semanticName = semOpt->Name->getText();
                const size_t index = semanticName.find_last_not_of("0123456789") + 1;
                uint32_t semanticIndex = 0;
                if (index != semanticName.length())
                {
                    semanticIndex = static_cast<uint32_t>(std::stoi(semanticName.substr(index)));
                }
                semanticName = semanticName.substr(0, index);
                const bool isSystemValue = (semOpt->Name->HLSLSemanticSystem() != nullptr);

                // Array dimensions
                rapidjson::Value varArrayDimensions = json.MakeArray();
                for (const auto dim : varInfo.m_typeInfoExt.GetDimensions())
                {
                    json.PushValue(varArrayDimensions, dim);
                }

                rapidjson::Value semStream = json.MakeObject();
                json.AddString(semStream, "name", ExtractLeaf(uid.m_name));
                json.AddString(semStream, "fullType", varInfo.GetTypeId().m_name);
                json.AddString(semStream, "baseType", varInfo.m_typeInfoExt.m_coreType.m_arithmeticInfo.UnderlyingScalarToStr());
                json.AddString(semStream, "semanticName", semanticName);
                json.AddValue(semStream, "semanticIndex", semanticIndex);
                json.AddValue(semStream, "systemValue", isSystemValue);
                json.AddValue(semStream, "dimensions", std::move(varArrayDimensions));
                json.AddValue(semStream, "rows", varInfo.m_typeInfoExt.m_coreType.m_arithmeticInfo.m_rows);
                json.AddValue(semStream, "cols", varInfo.m_typeInfoExt.m_coreType.m_arithmeticInfo.m_cols);

                json.PushValue(jsonVal, std::move(semStream));

                return true;
            }
            return false;
        }

        if (!allowStruct)
        {
            return false;
        }

        // From the non-predefineds we only support POD (struct) as valid element for the Input Assembler
        auto idKind = m_ir->GetIdAndKindInfo(varInfo.GetTypeId().GetName());
        if (!idKind)
        {
            return false;
        }

        auto& [typeId, typeKind] = *idKind;
        if (typeKind.GetKind() != Kind::Struct)
        {
            // It's ok not to have POD attributes in other methods, but those are not valid vs entries so return false here
            return false;
        }

        // Structs are PODs, so all member fields can only be variables
        const auto& classInfo = typeKind.GetSubRefAs<ClassInfo>();
        for (const auto& memberVar : classInfo.GetMemberFields())
        {
            if (!BuildIAElement(jsonVal, memberVar, false, json)) // Second pass prevents structs in structs
            {
                return false;
            }
        }

        return jsonVal.Size() > 0;
    }

    bool CodeReflection::BuildOMStruct(
        const ExtendedTypeInfo& returnTypeRef,
        const std::string_view semanticOverride,
        rapidjson::Value& jsonVal,
        int& semanticIndex,
        JsonBuilder& json) const
    {
        auto idKind = m_ir->GetIdAndKindInfo(returnTypeRef.m_coreType.m_typeId.GetName());
        if (!idKind)
        {
            return false;
        }

        auto& [typeId, typeKind] = *idKind;
        if (typeKind.GetKind() != Kind::Struct)
        {
            // It's ok not to have POD attributes in other methods, but those are not valid ps entries so return false here
            return false;
        }

        // Structs are PODs, so all member fields can only be variables
        const auto& classInfo = typeKind.GetSubRefAs<ClassInfo>();
        for (const auto& memberVar : classInfo.GetMemberFields())
        {
            auto& attrVarInfo = *m_ir->GetSymbolSubAs<VarInfo>(memberVar.m_name);

            bool buildOMResult = false;
            if (!semanticOverride.empty())
            {
                if (!BuildOMElement(jsonVal, attrVarInfo.m_typeInfoExt, semanticOverride, semanticIndex, false, json))
                {
                    return false;
                }
            }
            else
            {
                // Semantic name and index
                const auto hlslSemantic = attrVarInfo.m_declNode->hlslSemantic();
                if (hlslSemantic == nullptr || hlslSemantic->Name->HLSLSemanticSystem() == nullptr)
                {
                    return false;
                }
                auto [semanticName, semanticIndex, isSystemValue] = ExtractHlslSemantic(hlslSemantic);
                if (!BuildOMElement(jsonVal, attrVarInfo.m_typeInfoExt, semanticName.c_str(), semanticIndex, isSystemValue, json))
                {
                    return false;
                }
            }
        }

        return jsonVal.Size() > 0;
    }

    bool ValidOutputSemanticIndex(const std::string_view semanticName, const int semanticIndex)
    {
        if (EqualNoCase(semanticName, "SV_Target"))
        {
            return (semanticIndex >= 0 && semanticIndex < kMaxRenderTargets);
        }

        return (semanticIndex == 0);
    }

    bool IsValidPSOutput(
        const std::string_view& semanticOverride,
        const int& semanticIndex,
        rapidjson::Value& jsonVal,
        const ExtendedTypeInfo& returnTypeRef)
    {
        if (semanticOverride.empty())
        {
            // dxc error: Semantic must be defined for all parameters of an entry function or patch constant function
            return false;
        }

        if (!ValidOutputSemanticIndex(semanticOverride, semanticIndex))
        {
            // dxc error: [semanticName] semantic index exceeds maximum (7)
            return false;
        }

        for (const rapidjson::Value& existingSemantic : jsonVal.GetArray())
        {
            if (semanticOverride.find(existingSemantic["semanticName"].GetString()) != std::string::npos &&
                semanticIndex == existingSemantic["semanticIndex"].GetInt())
            {
                // dxc error: Parameter with semantic [semanticName] has overlapping semantic index at [semanticIndex]
                return false;
            }
        }

        if (!IsArithmetic(returnTypeRef.m_coreType.m_typeClass))
        {
            // fxc internal error: compilation aborted unexpectedly
            // dxc hangs
            return false;
        }

        if (returnTypeRef.m_coreType.m_arithmeticInfo.m_rows > 0)
        {
            // dxc error: Pixel shader output registers are not indexable.
            return false;
        }

        return true;
    }

    bool CodeReflection::BuildOMElement(
        rapidjson::Value& jsonVal,
        const ExtendedTypeInfo& returnTypeRef,
        std::string_view semanticOverride,
        int& semanticIndex,
        const bool isSystemValue,
        JsonBuilder& json) const
    {
        // Structs are allowed as pixel shader output, if the type is registered walk over its attributes here
        if (!IsPredefinedType(returnTypeRef.m_coreType.m_typeClass))
        {
            return BuildOMStruct(returnTypeRef, semanticOverride, jsonVal, semanticIndex, json);
        }

        if (!IsValidPSOutput(semanticOverride, semanticIndex, jsonVal, returnTypeRef))
        {
            // We only want to emit a list of potential pixel shader entry points
            //  so give up here if any
            return false;
        }

        rapidjson::Value semStream = json.MakeObject();

        const bool isVoid = returnTypeRef.m_coreType.m_typeClass == TypeClass::Void;

        if (isVoid)
        {
            json.AddString(semStream, "baseType", "void");
        }
        else
        {
            json.AddString(semStream, "baseType", returnTypeRef.m_coreType.m_arithmeticInfo.UnderlyingScalarToStr());
        }

        const auto numElements = returnTypeRef.m_coreType.m_arithmeticInfo.m_cols;
        json.AddValue(semStream, "cols", numElements);
        json.AddString(semStream, "semanticName", semanticOverride);

        if (EqualNoCase(semanticOverride, "SV_Target"))
        {
            if (isVoid)
            {
                json.AddString(semStream, "format", "None");
            }
            else
            {
                json.AddString(semStream, "format", OutputFormat::ToStr(m_ir->m_metaData.m_outputFormatHint[semanticIndex]));
            }
        }
        else if (StartsWithNoCase(semanticOverride, "SV_Depth"))
        {
            json.AddString(semStream, "format", OutputFormat::ToStr(OutputFormat::R32));
        }
        else if (EqualNoCase(semanticOverride, "SV_Coverage") || EqualNoCase(semanticOverride, "SV_StencilRef"))
        {
            json.AddString(semStream, "format", OutputFormat::ToStr(OutputFormat::R32));
        }
        else if (!(StartsWithNoCase(semanticOverride, "SV_")))
        {
            // Can't end up here, which means there is newly added grammar - it should be handled properly
            assert(false);
        }

        // This behavior conforms to how dxc and fxc layout output semantics.
        // Any output semantic declaration in a structure will override any member variable declarations recursively.
        // The end result is semantic index in order of declaration rather than what is specified per member variable.
        json.AddValue(semStream, "semanticIndex", semanticIndex++);

        json.AddValue(semStream, "systemValue", isSystemValue);

        json.PushValue(jsonVal, std::move(semStream));

        return true;
    }

    rapidjson::Value CodeReflection::GetShaderEntries(std::string_view sEntry, JsonBuilder& json) const
    {
        rapidjson::Value inputLayouts = json.MakeArray();

        // Emit all vertex and compute shader entry functions
        for (const auto& symId : m_ir->m_symbols.GetOrderedSymbols())
        {
            const auto& [uid, sym] = *m_ir->m_symbols.GetIdAndKindInfo(symId.m_name);
            const UnqualifiedName funcName{ExtractLeaf(uid.m_name)};

            bool validFunc = ((sym.GetKind() == Kind::Function) && IsGlobal(uid.m_name) &&
                (sEntry.empty() || funcName == sEntry));
            if (!validFunc)
            {
                // It's not a function or at least not the function we are looking for
                continue;
            }

            rapidjson::Value functionEntry = json.MakeObject();
            json.AddString(functionEntry, "entry", RemoveLastParenthesisGroup(funcName));

            rapidjson::Value semanticStreams = json.MakeArray();
            const auto& funcSubInfo = sym.GetSubRefAs<FunctionInfo>();
            for (const auto it : funcSubInfo.GetParameters(false))
            {
                if (!it.m_varId.IsEmpty())
                {
                    validFunc &= BuildIAElement(semanticStreams, it.m_varId, true/*Allow structs on the first level*/, json);
                }
            }

            // Optional - compute shader entries also have [numthreads] attribute
            auto attrInfo = m_ir->m_symbols.GetAttribute(uid, "numthreads");
            if (attrInfo)
            {
                rapidjson::Value numThreads = json.MakeArray();
                for (auto arg : attrInfo->m_argList)
                {
                    if (!std::holds_alternative<ConstNumericVal>(arg))
                    {
                        continue;
                    }

                    const auto& constVal = std::get<ConstNumericVal>(arg);
                    if (!std::holds_alternative<int32_t>(constVal) && !std::holds_alternative<uint32_t>(constVal))
                    {
                        continue;
                    }

                    auto threadCount = ExtractValueAsInt64(constVal, std::numeric_limits<int64_t>::min());
                    if (threadCount <= 0)
                    {
                        continue;
                    }

                    json.PushValue(numThreads, static_cast<int>(threadCount));
                }

                if (numThreads.Size() == 3)
                {
                    // We expect exactly 3 numbers that specify the ThreadGroup dimensions
                    json.AddValue(functionEntry, "numthreads", std::move(numThreads));
                }
            }

            // the shader attribute is the way to declare raytracing entrypoints so they are valid entries
            if (validFunc || m_ir->m_symbols.GetAttribute(uid, "shader"))
            {
                json.AddValue(functionEntry, "streams", std::move(semanticStreams));
                json.PushValue(inputLayouts, std::move(functionEntry));
            }
        }

        return inputLayouts;
    }

    rapidjson::Value CodeReflection::GetOutputMergerLayout(const std::string_view psEntry, JsonBuilder& json) const
    {
        rapidjson::Value outputLayouts = json.MakeArray();

        // Emit all pixel shader entry functions
        for (const auto& [uid, sym] : m_ir->GetOrderedSymbolsOfSubType_2<FunctionInfo>())
        {
            const UnqualifiedNameView funcName = ExtractLeaf(uid.m_name);

            bool validFunc = (IsGlobal(uid.m_name)
                && (psEntry.empty() || funcName == psEntry)
                && sym->m_defNode); // Regarding OM data, only defined functions are relevant.
            if (!validFunc)
            {
                // It's not the function we are looking for
                continue;
            }

            rapidjson::Value functionEntry = json.MakeObject();
            json.AddString(functionEntry, "entry", RemoveLastParenthesisGroup(funcName));

            rapidjson::Value renderTargets = json.MakeArray();
            const auto& funcSubInfo = *sym;

            std::string semanticName("");
            int semanticIndex = 0;
            bool isSystemValue = false;
            if (funcSubInfo.m_defNode->hlslSemantic())
            {
                tie(semanticName, semanticIndex, isSystemValue) = ExtractHlslSemantic(funcSubInfo.m_defNode->hlslSemantic());
            }

            validFunc &= BuildOMElement(
                renderTargets,
                funcSubInfo.m_returnType,
                semanticName,
                semanticIndex,
                isSystemValue,
                json);

            int depthFoundNTimes = 0;
            for (const rapidjson::Value& existingSemantic : renderTargets.GetArray())
            {
                std::string_view semanticNameView{
                    existingSemantic["semanticName"].GetString(),
                    existingSemantic["semanticName"].GetStringLength()
                };
                if (semanticNameView.find("Depth") != std::string::npos)
                {
                    depthFoundNTimes++;
                }
            }
            // dxc error: Pixel Shader only allows one type of depth semantic to be declared
            validFunc &= (depthFoundNTimes <= 1);

            if (validFunc)
            {
                json.AddValue(functionEntry, "renderTargets", std::move(renderTargets));
                json.PushValue(outputLayouts, std::move(functionEntry));
            }
        }

        return outputLayouts;
    }

    void CodeReflection::DumpOutputMergerLayout(const std::string_view psEntry) const
    {
        rapidjson::Document iaRoot;
        iaRoot.SetObject();
        JsonBuilder json(iaRoot);
        json.AddString(iaRoot, "meta", "Output Merger Layout exported by AZSLC");
        json.AddString(iaRoot, "source", m_ir->OriginalSource());
        json.AddString(iaRoot, "material", "Output Merger Layout exported by AZSLC");

        json.AddValue(iaRoot, "outputLayouts", GetOutputMergerLayout(psEntry, json));
        m_out << json.Serialize(iaRoot);
    }

    void CodeReflection::DumpShaderEntries() const
    {
        rapidjson::Document entries;
        entries.SetObject();
        JsonBuilder json(entries);
        json.AddString(entries, "meta", "Shader entries exported by AZSLC");
        json.AddString(entries, "source", m_ir->OriginalSource());
        json.AddString(entries, "material", "Unknown material");

        json.AddValue(entries, "inputLayouts", GetShaderEntries({}, json));
        m_out << json.Serialize(entries);
    }

    uint32_t CodeReflection::GetViewStride(
        const IdentifierUID& memberId,
        const AZ::ShaderCompiler::Packing::Layout& layoutPacking,
        const Options& options,
        JsonBuilder& json) const
    {
        const auto* varInfoPtr = m_ir->GetSymbolSubAs<VarInfo>(memberId.m_name);
        if (!varInfoPtr)
        {
            return 0;
        }

        assert(IsViewType(varInfoPtr->GetTypeClass()));

        if (varInfoPtr->m_typeInfoExt.m_genericParameter.IsEmpty())
        {
            if (varInfoPtr->GetTypeClass() == TypeClass::ByteAddressBuffer)
            {
                return 4;
            }
            // Unspecified generic types default to float4
            return 16;
        }

        const auto genericType = varInfoPtr->m_typeInfoExt.m_genericParameter;
        if (!IsUserDefined(genericType.m_typeClass))
        {
            assert(!genericType.m_arithmeticInfo.IsEmpty());
            return genericType.m_arithmeticInfo.GetTotalSize();
        }

        rapidjson::Value emptyLayout = json.MakeArray();
        const auto& structMember = genericType.m_typeId;
        uint32_t startAt = 0;
        return BuildUserDefinedMemberLayout(emptyLayout, structMember, options, layoutPacking, startAt, "", json);
    }

    uint32_t CodeReflection::BuildUserDefinedMemberLayout(
        rapidjson::Value& membersContainer,
        const IdentifierUID& exportedTypeId,
        const Options& options,
        const AZ::ShaderCompiler::Packing::Layout layoutPacking,
        uint32_t& startAt,
        const std::string_view namePrefix,
        JsonBuilder& json) const
    {
        // Alignment start
        uint32_t tempOffset = startAt = Packing::AlignOffset(layoutPacking, startAt, Packing::Alignment::asStructStart, 0, 0);

        uint32_t largestMemberSize = 0;

        const auto* classInfo = m_ir->GetSymbolSubAs<ClassInfo>(exportedTypeId.m_name);
        for (const auto& memberField : classInfo->GetMemberFields())
        {
            const auto currentStride = tempOffset;
            BuildMemberLayout(membersContainer, namePrefix, memberField, false, options, GetExtendedLayout(layoutPacking), tempOffset, json);
            largestMemberSize = std::max(largestMemberSize, tempOffset - currentStride);
        }

        tempOffset = Packing::AlignStructToLargestMember(layoutPacking, tempOffset, largestMemberSize);

        // Alignment end
        tempOffset = Packing::AlignOffset(layoutPacking, tempOffset, Packing::Alignment::asStructEnd, 0, 0);

        // Total size equals the end offset less the starting address
        return tempOffset - startAt;
    }

    uint32_t CodeReflection::BuildMemberLayout(
        rapidjson::Value& membersContainer,
        std::string_view namePrefix,
        const IdentifierUID& memberId,
        const bool isArrayItr,
        const Options& options,
        const AZ::ShaderCompiler::Packing::Layout layoutPacking,
        uint32_t& offset,
        JsonBuilder& json) const
    {
        const auto* varInfoPtr = m_ir->GetSymbolSubAs<VarInfo>(memberId.m_name);
        uint32_t size = 0;

        if (varInfoPtr)
        {
            const auto& varInfo = *varInfoPtr;

            // View types should only be called from GetViewStride until we decide to support them as struct constants
            assert(!IsChameleon(varInfo.GetTypeClass()));

            auto exportedType = varInfo.m_typeInfoExt.m_coreType;

            if (!exportedType.IsPackable())
            {
                throw std::logic_error{
                    (std::string("reflection error: unpackable type (")
                        + exportedType.m_typeId.m_name
                        + ") in layout member "
                        + memberId.m_name).c_str()
                };
            }
            TypeClass varClass = exportedType.m_typeClass;
            bool isPrefedined = IsPredefinedType(varClass);
            rapidjson::Value memberLayout = json.MakeObject();
            const auto& shortName = memberId.GetNameLeaf();
            std::string constantId = std::string(namePrefix) + shortName;
            if (isArrayItr)
            {
                constantId = std::string(namePrefix);
            }
            json.AddString(memberLayout, "constantId", constantId);
            json.AddString(memberLayout, "qualifiedName", memberId.m_name);
            if (isPrefedined)
            {
                json.AddString(memberLayout, "typeKind", "Predefined");
            }
            else if (IsProductType(varClass))
            {
                json.AddString(memberLayout, "typeKind", "Struct");
            }
            else
            {
                json.AddString(memberLayout, "typeKind", TypeClass::ToStr(varClass));
            }
            json.AddString(memberLayout, "typeName", varInfo.GetTypeId().m_name);

            size = varInfo.m_typeInfoExt.GetTotalSize(layoutPacking, options.m_forceMatrixRowMajor);
            auto startAt = offset;

            // Alignment start
            if (exportedType.m_arithmeticInfo.IsMatrix() || exportedType.m_arithmeticInfo.IsVector())
            {
                const auto rows = exportedType.m_arithmeticInfo.m_rows;
                const auto cols = exportedType.m_arithmeticInfo.m_cols;
                auto packAlignment = Packing::Alignment::asVectorStart;
                if (exportedType.m_arithmeticInfo.IsMatrix())
                {
                    packAlignment = Packing::Alignment::asMatrixStart;
                }
                startAt = offset = Packing::AlignOffset(layoutPacking, offset, packAlignment, rows, cols);
            }

            uint32_t totalArraySize = 1;
            ArrayDimensions listOfArrayDim = varInfo.m_typeInfoExt.GetDimensions();
            std::reverse(listOfArrayDim.m_dimensions.begin(), listOfArrayDim.m_dimensions.end());
            for (const auto dim : varInfo.m_typeInfoExt.GetDimensions())
            {
                totalArraySize *= dim;
            }

            if (varInfo.m_typeInfoExt.IsArray() && !isArrayItr)
            {
                startAt = offset = Packing::AlignOffset(layoutPacking, offset, Packing::Alignment::asArrayStart, 0, 0);
                uint32_t arrayOffset = startAt;
                for (uint32_t i = 0; i < totalArraySize; i++)
                {
                    // Construct dimension, [m][n], [p][q][r], etc
                    std::string strDimIndex = "";
                    int prevDim = 1;
                    for (const auto dim : listOfArrayDim)
                    {
                        // If the array dimension is 3, construct index for an element i in array [x][y][z]
                        strDimIndex = "[" + std::to_string((i / prevDim) % dim) + "]" + strDimIndex;
                        prevDim *= dim;
                    }

                    if (!m_ir->GetIdAndKindInfo(varInfo.GetTypeId().m_name))
                    {
                        continue;
                    }

                    // If array is a structure
                    if (IsProductType(varClass))
                    {
                        startAt = offset;
                        size = BuildUserDefinedMemberLayout(
                            membersContainer,
                            exportedType.m_typeId,
                            options,
                            layoutPacking,
                            startAt,
                            (std::string(namePrefix) + shortName + strDimIndex + "."),
                            json);

                        offset = Packing::PackNextChunk(layoutPacking, size, startAt);

                        // Add packing into array
                        size = Packing::PackIntoArray(layoutPacking, size, varInfo.m_typeInfoExt.GetDimensions());
                    }
                    else
                    {
                        // Alignment start
                        startAt = offset = Packing::AlignOffset(layoutPacking, offset, Packing::Alignment::asArrayStart, 0, 0);

                        // We want to calculate the offset for each array element
                        uint32_t tempOffset = startAt;

                        BuildMemberLayout(membersContainer, (std::string(namePrefix) + shortName + strDimIndex), memberId, true, options, layoutPacking, tempOffset, json);

                        // Alignment end
                        tempOffset = Packing::AlignOffset(layoutPacking, tempOffset, Packing::Alignment::asArrayEnd, 0, 0);
                        size = tempOffset - startAt;

                        offset = Packing::PackNextChunk(layoutPacking, size, startAt);

                        // Add packing into array
                        size = Packing::PackIntoArray(layoutPacking, size, varInfo.m_typeInfoExt.GetDimensions());
                    }
                }
                startAt = arrayOffset;
            }
            else if (IsProductType(varClass))
            {
                size = BuildUserDefinedMemberLayout(
                    membersContainer,
                    exportedType.m_typeId,
                    options,
                    layoutPacking,
                    startAt,
                    (std::string(namePrefix) + shortName + "."),
                    json);

                // Add packing into array
                size = Packing::PackIntoArray(layoutPacking, size, varInfo.m_typeInfoExt.GetDimensions());
            }
            else if (varInfo.m_typeInfoExt.IsArray())
            {
                // Get the size of one element from total size
                size = varInfo.m_typeInfoExt.GetSingleElementSize(layoutPacking, options.m_forceMatrixRowMajor);
            }
            else if (varInfo.GetTypeClass() == TypeClass::Enum)
            {
                auto* asClassInfo = m_ir->GetSymbolSubAs<ClassInfo>(varInfo.GetTypeId().GetName());
                size = asClassInfo->Get<EnumerationInfo>()->m_underlyingType.m_arithmeticInfo.m_baseSize;
            }

            offset = Packing::PackNextChunk(layoutPacking, size, startAt);

            // Alignment end
            if (exportedType.m_arithmeticInfo.IsMatrix() || exportedType.m_arithmeticInfo.IsVector())
            {
                const auto rows = exportedType.m_arithmeticInfo.m_rows;
                const auto cols = exportedType.m_arithmeticInfo.m_cols;
                auto packAlignment = Packing::Alignment::asVectorEnd;
                if (exportedType.m_arithmeticInfo.IsMatrix())
                {
                    packAlignment = Packing::Alignment::asMatrixEnd;
                }
                offset = Packing::AlignOffset(layoutPacking, offset, packAlignment, rows, cols);
            }

            if (varInfo.m_typeInfoExt.IsArray())
            {
                offset = Packing::AlignOffset(layoutPacking, offset, Packing::Alignment::asArrayEnd, 0, 0);
            }

            size = offset - startAt;

            // Array dimensions
            rapidjson::Value varArrayDimensions = json.MakeArray();
            for (const auto dim : varInfo.m_typeInfoExt.GetDimensions())
            {
                json.PushValue(varArrayDimensions, dim);
            }

            json.AddValue(memberLayout, "constantByteOffset", startAt);
            json.AddValue(memberLayout, "constantByteSize", size);
            json.AddValue(memberLayout, "typeDimensions", std::move(varArrayDimensions));

            json.PushValue(membersContainer, std::move(memberLayout));
        }

        return size;
    }

    void CodeReflection::DumpVariantList(const Options& options) const
    {
        AnalyzeOptionRanks();
        SetupOptionsSpecializationId(options);
        auto variantList = GetVariantList(options);
        JsonBuilder json(variantList);
        m_out << json.Serialize(variantList);
        m_out << "\n";
    }

    static void ReflectBinding(rapidjson::Value& output, const RootSigDesc::SrgParamDesc& bindInfo, JsonBuilder& json)
    {
        int count = bindInfo.m_registerRange;
        if (bindInfo.m_isUnboundedArray)
        {
            count = -1;
        }
        json.AddValue(output, "count", count);
        json.AddValue(output, "index", bindInfo.m_registerBinding.m_pair[BindingPair::Set::Untainted].m_registerIndex);
        json.AddValue(output, "space", bindInfo.m_registerBinding.m_pair[BindingPair::Set::Untainted].m_logicalSpace);
        json.AddValue(output, "index-merged", bindInfo.m_registerBinding.m_pair[BindingPair::Set::Merged].m_registerIndex);
        json.AddValue(output, "space-merged", bindInfo.m_registerBinding.m_pair[BindingPair::Set::Merged].m_logicalSpace);
    }

    void CodeReflection::DumpSRGLayout(const Options& options, PreprocessorLineDirectiveFinder* lineFinder) const
    {
        uint32_t numOf32bitConst = GetNumberOf32BitConstants(options, m_ir->m_rootConstantStructUID);
        RootSigDesc rootSig = BuildSignatureDescription(options, numOf32bitConst);

        rapidjson::Document srgRoot;
        srgRoot.SetObject();
        JsonBuilder json(srgRoot);
        json.AddString(srgRoot, "meta", "SRGs layout exported by AZSLC");
        json.AddString(srgRoot, "source", m_ir->OriginalSource());

        rapidjson::Value srgLayouts = json.MakeArray();

        // Reflect all SRGs
        for (auto& [srgUid, srgInfo] : m_ir->GetOrderedSymbolsOfSubType_2<SRGInfo>())
        {
            rapidjson::Value srgLayout = json.MakeObject();
            json.AddString(srgLayout, "id", srgInfo->m_declNode->Name->getText());

            // Try to locate the original filename where this SRG is declared
            size_t physical = srgInfo->m_declNode->getStart()->getLine();
            json.AddString(srgLayout, "originalFileName", std::filesystem::absolute(std::filesystem::path(lineFinder->GetVirtualFileName(physical).c_str())).lexically_normal().generic_string());
            json.AddValue(srgLayout, "originalLineNumber", static_cast<uint64_t>(lineFinder->GetVirtualLineNumber(physical)));

            auto semantic = m_ir->GetSymbolSubAs<ClassInfo>(srgInfo->m_semantic->GetName())->Get<SRGSemanticInfo>();

            json.AddValue(srgLayout, "bindingSlot", static_cast<int64_t>(*semantic->m_frequencyId)); // Semantic check has asserted that we have it, so we can dereference here.

            if (srgInfo->m_shaderVariantFallback)
            {
                json.AddValue(srgLayout, "fallbackSize", static_cast<int64_t>(*semantic->m_variantFallback));
                json.AddString(srgLayout, "fallbackName", srgInfo->m_shaderVariantFallback->GetNameLeaf());
            }

            rapidjson::Value buffersList = json.MakeArray();
            rapidjson::Value imagesList = json.MakeArray();

            // External CBVs
            for (const auto& cId : srgInfo->m_CBs)
            {
                const auto& bindInfo = rootSig.Get(cId);

                uint32_t strideSize = GetViewStride(cId, options.m_packConstantBuffers, options, json);

                auto& srgMemberInfo = *m_ir->GetSymbolSubAs<VarInfo>(cId.m_name);
                assert(srgMemberInfo.IsConstantBuffer());

                rapidjson::Value dataView = json.MakeObject();
                json.AddString(dataView, "id", ExtractLeaf(cId.m_name));
                json.AddString(dataView, "type", srgMemberInfo.m_typeInfoExt.GetDisplayShortName());
                json.AddString(dataView, "usage", "Read");
                json.AddValue(dataView, "stride", strideSize);
                ReflectBinding(dataView, bindInfo, json);

                json.PushValue(buffersList, std::move(dataView));
            }

            // SRVs and UAVs
            for (const auto& tId : srgInfo->m_srViews)
            {
                const auto& bindInfo = rootSig.Get(tId);

                uint32_t strideSize = GetViewStride(tId, options.m_packDataBuffers, options, json);

                auto& srgMemberInfo = *m_ir->GetSymbolSubAs<VarInfo>(tId.m_name);
                auto viewName = srgMemberInfo.m_typeInfoExt.GetDisplayShortName();
                bool isBufferView = IsViewTypeBuffer(srgMemberInfo.GetTypeClass());
                bool isReadWriteView = IsReadWriteView(viewName);

                // Relaxed vector std140 and std430 packing rules
                if (isBufferView && options.m_packConstantBuffers == AZ::ShaderCompiler::Packing::Layout::RelaxedStd140Packing)
                {
                    strideSize = Packing::AlignUp(strideSize, Packing::s_bytesPerRegister);
                }

                rapidjson::Value dataView = json.MakeObject();
                json.AddString(dataView, "id", ExtractLeaf(tId.m_name));
                json.AddString(dataView, "type", viewName);
                if (isReadWriteView)
                {
                    json.AddString(dataView, "usage", "ReadWrite");
                }
                else
                {
                    json.AddString(dataView, "usage", "Read");
                }
                ReflectBinding(dataView, bindInfo, json);
                json.AddValue(dataView, "stride", strideSize);

                if (isBufferView)
                {
                    json.PushValue(buffersList, std::move(dataView));
                }
                else
                {
                    json.PushValue(imagesList, std::move(dataView));
                }
            }

            json.AddValue(srgLayout, "inputsForBufferViews", std::move(buffersList));
            json.AddValue(srgLayout, "inputsForImageViews", std::move(imagesList));

            rapidjson::Value samplersList = json.MakeArray();
            for (const auto& sId : srgInfo->m_samplers)
            {
                const auto& bindInfo = rootSig.Get(sId);

                const auto* srgMemberInfo = m_ir->GetSymbolSubAs<VarInfo>(sId.m_name);
                const auto& samplerInfo = *srgMemberInfo->m_samplerState;

                rapidjson::Value samplerJson = json.MakeObject();
                json.AddString(samplerJson, "id", sId.GetNameLeaf());
                json.AddValue(samplerJson, "isDynamic", samplerInfo.m_isDynamic);
                ReflectBinding(samplerJson, bindInfo, json);

                if (!samplerInfo.m_isDynamic)
                {
                    // Emit predefined enums for static sampler
                    json.AddValue(samplerJson, "anisotropyMax", samplerInfo.m_anisotropyMax);
                    json.AddValue(samplerJson, "anisotropyEnable", samplerInfo.m_anisotropyEnable);
                    if (samplerInfo.m_filterMin == SamplerStateDesc::FilterMode::Linear)
                    {
                        json.AddString(samplerJson, "filterMin", "Linear");
                    }
                    else
                    {
                        json.AddString(samplerJson, "filterMin", "Point");
                    }

                    if (samplerInfo.m_filterMag == SamplerStateDesc::FilterMode::Linear)
                    {
                        json.AddString(samplerJson, "filterMag", "Linear");
                    }
                    else
                    {
                        json.AddString(samplerJson, "filterMag", "Point");
                    }

                    if (samplerInfo.m_filterMip == SamplerStateDesc::FilterMode::Linear)
                    {
                        json.AddString(samplerJson, "filterMip", "Linear");
                    }
                    else
                    {
                        json.AddString(samplerJson, "filterMip", "Point");
                    }
                    json.AddStreamableString(samplerJson, "reductionType", samplerInfo.m_reductionType);
                    json.AddStreamableString(samplerJson, "comparisonFunc", samplerInfo.m_comparisonFunc);
                    json.AddStreamableString(samplerJson, "addressU", samplerInfo.m_addressU);
                    json.AddStreamableString(samplerJson, "addressV", samplerInfo.m_addressV);
                    json.AddStreamableString(samplerJson, "addressW", samplerInfo.m_addressW);
                    json.AddValue(samplerJson, "mipLodMin", samplerInfo.m_mipLodMin);
                    json.AddValue(samplerJson, "mipLodMax", samplerInfo.m_mipLodMax);
                    json.AddValue(samplerJson, "mipLodBias", samplerInfo.m_mipLodBias);
                    json.AddStreamableString(samplerJson, "borderColor", samplerInfo.m_borderColor);
                    json.AddValue(samplerJson, "isComparison", samplerInfo.m_isComparison);
                }

                json.PushValue(samplersList, std::move(samplerJson));
            }
            json.AddValue(srgLayout, "inputsForSamplers", std::move(samplersList));

            // SRG Constants
            // Call extracted func for rgInfo->m_implicitStruct
            if (!srgInfo->m_implicitStruct.GetMemberFields().empty())
            {
                const auto& layoutPacking = options.m_packConstantBuffers;
                uint32_t offset = 0;
                rapidjson::Value structLayout = json.MakeArray();

                for (const auto& srgConst : srgInfo->m_implicitStruct.GetMemberFields())
                {
                    BuildMemberLayout(structLayout, "", srgConst, false, options, layoutPacking, offset, json);
                }

                json.AddValue(srgLayout, "inputsForSRGConstants", std::move(structLayout));

                // CBuffer for SRG Constants
                {
                    const auto& bindInfo = rootSig.Get(srgUid);

                    rapidjson::Value dataView = json.MakeObject();
                    json.AddString(dataView, "id", ExtractLeaf(srgUid.m_name));
                    json.AddString(dataView, "usage", "Read");
                    ReflectBinding(dataView, bindInfo, json);

                    json.AddValue(srgLayout, "bufferForSRGConstants", std::move(dataView));
                }
            }

            json.PushValue(srgLayouts, std::move(srgLayout));
        }
        json.AddValue(srgRoot, "ShaderResourceGroups", std::move(srgLayouts));

        rapidjson::Value rootConstantLayout = json.MakeObject();

        if (options.m_rootConstantsMaxSize > 0)
        {
            const auto& layoutPacking = options.m_packConstantBuffers;
            rapidjson::Value structLayout = json.MakeArray();
            uint32_t startAt = 0;
            uint32_t strideSize = BuildUserDefinedMemberLayout(
                structLayout,
                m_ir->m_rootConstantStructUID,
                options,
                options.m_packConstantBuffers,
                startAt,
                "",
                json);
            strideSize = GetPlatformEmitter().AlignRootConstants(strideSize);

            json.AddValue(rootConstantLayout, "inputsForRootConstants", std::move(structLayout));

            const auto& bindInfo = rootSig.Get(m_ir->m_rootConstantStructUID);
            rapidjson::Value dataView = json.MakeObject();
            json.AddString(dataView, "id", ExtractLeaf(m_ir->m_rootConstantStructUID.m_name));
            json.AddString(dataView, "usage", "Read");
            ReflectBinding(dataView, bindInfo, json);
            json.AddValue(dataView, "sizeInBytes", strideSize);

            json.AddValue(rootConstantLayout, "bufferForRootConstants", std::move(dataView));

            // Add the layout to Root node
            json.AddValue(srgRoot, "RootConstantBuffer", std::move(rootConstantLayout));
        }

        m_out << json.Serialize(srgRoot);
    }

    bool CodeReflection::IsNonOverloaded(const IdentifierUID& uid) const
    {
        // func() to func
        // because in AZIR mangling scheme, the parenthesized name is the concrete function name, and the core name is the overload-set name.
        std::string_view core = RemoveLastParenthesisGroup(uid.GetName());
        const auto* coreSymbol = m_ir->GetSymbolSubAs<OverloadSetInfo>(QualifiedNameView{core});
        return !coreSymbol || !coreSymbol->HasOverloads();
    }

    bool CodeReflection::IsPotentialEntryPoint(const IdentifierUID& uid) const
    {
        // only global and non overloaded, potentially qualifies. we can also add --ia detectors.
        return m_ir->GetKind(uid) == Kind::Function && IsGlobal(uid.GetName()) && IsNonOverloaded(uid);
    }

    void CodeReflection::DiscoverTopLevelFunctionDependencies(
        const IdentifierUID& uid,
        std::set<IdentifierUID>& output,
        const MapOfBeginToSpanAndUid& scopes,
        std::set<IdentifierUID>&& funcStack_ = {}) const
    {
        // discover references:
        auto* kindInfo = m_ir->GetKindInfo(uid);
        for (auto& seenat : kindInfo->GetSeenats())
        {
            assert(uid == seenat.m_referredDefinition);
            // careful of the invariant: distinct intervals. (can't support functions nested in functions nor imbricated block scopes)
            // ok for now because AZSL/HLSL don't have lambdas
            auto intervalIter = FindIntervalInDisjointSet(
                scopes,
                seenat.m_where.m_focusedTokenId,
                [](ssize_t key, auto& value)
                {
                    return value.first.properlyContains({key, key});
                });
            if (intervalIter != scopes.end())
            {
                const IdentifierUID& encloser = intervalIter->second.second;
                if (m_ir->GetKind(encloser) == Kind::Function)
                {
                    // detect cycles:
                    if (funcStack_.find(encloser) != funcStack_.end())
                    {
                        throw AzslcEmitterException(
                            EMITTER_RECURSION_NOT_PERMITTED,
                            seenat.m_where.m_line,
                            seenat.m_where.m_charPos,
                            ConcatString("Recursion not permitted in AZSL. ", uid.m_name, " in ", encloser.m_name));
                    }
                    if (IsPotentialEntryPoint(encloser)) // reduce a bit the amount of unused data by filtering by potential entry points
                    {
                        output.insert(encloser);
                    }
                    // recurse
                    const auto it = funcStack_.insert(encloser).first; // push
                    DiscoverTopLevelFunctionDependencies(encloser, output, scopes, CastToRValueReference(funcStack_));
                    funcStack_.erase(it); // pop
                }
            }
            else // no interval found -> assume global scope
            {
                // the only sort of reference that can appear outside of any scope are within global variable initializers.
                antlr4::ParserRuleContext* syntaxContext = m_ir->m_tokenMap.GetNode(seenat.m_where.m_focusedTokenId);
                const auto* varInitializerContext = ExtractInitializerParent(syntaxContext);
                if (varInitializerContext) // if null, it's highly suspicious though. what syntax construct allows that ?
                {
                    auto* declarator = polymorphic_downcast<AstUnnamedVarDecl*>(varInitializerContext->parent);
                    // reconstruct the name to get the UID
                    UnqualifiedName declaredIdentifier{ExtractVariableNameIdentifier(declarator)->getText()};
                    const QualifiedNameView globalScope{"/"};
                    QualifiedName variable = MakeFullyQualified(globalScope, declaredIdentifier);
                    auto& [identified, kind] = *m_ir->GetIdAndKindInfo(variable); // unchecked dereference here, because symbol presence is an invariant
                    assert(kind.GetKind() == Kind::Variable); // what else
                    DiscoverTopLevelFunctionDependencies(identified, output, scopes, CastToRValueReference(funcStack_)); // continue tracking
                }
            }
        }
    }

    void CodeReflection::DumpResourceBindingDependencies(const Options& options) const
    {
        uint32_t numOf32bitConst = GetNumberOf32BitConstants(options, m_ir->m_rootConstantStructUID);
        RootSigDesc rootSignature = BuildSignatureDescription(options, numOf32bitConst);

        // Prepare a lookup acceleration data structure for reverse mapping tokens to scopes.
        // (truth: we need a set of disjoint intervals as an invariant for the following algorithm)
        GenerateTokenScopeIntervalToUidReverseMap();

        rapidjson::Document srgRoot;
        srgRoot.SetObject();
        JsonBuilder json(srgRoot);
        // Order the reflection by SRG for convenience
        for (const RootSigDesc::SrgDesc& srgDesc : rootSignature.m_srGroups)
        {
            auto uid = srgDesc.m_uid;
            if (uid.m_name != m_ir->m_rootConstantStructUID.m_name) // Since root constant isn't part of SRG resource, and just an alias of SRG members, skip for now.
            {
                auto* srgInfo = m_ir->GetKindInfo(uid)->GetSubAs<SRGInfo>();
                rapidjson::Value srgMember = json.MakeObject();

                // functions to factorize node output
                auto dependencyListToJson = [&json](const std::set<IdentifierUID>& dependencyList) -> rapidjson::Value
                {
                    rapidjson::Value allEntriesJson = json.MakeArray();
                    for_each(
                        dependencyList.begin(),
                        dependencyList.end(),
                        [&allEntriesJson, &json](const IdentifierUID& id)
                        {
                            json.PushString(allEntriesJson, RemoveLastParenthesisGroup(id.GetNameLeaf()));
                        });
                    return allEntriesJson;
                };

                auto makeJsonNodeForOneResource = [&dependencyListToJson, &json](
                    const std::set<IdentifierUID>& dependencyList,
                    const RootSigDesc::SrgParamDesc& binding,
                    const rapidjson::Value* allConstants = nullptr)
                {
                    rapidjson::Value resourceJsonValue = json.MakeObject();
                    json.AddValue(resourceJsonValue, "dependentFunctions", dependencyListToJson(dependencyList));
                    if (allConstants && !allConstants->Empty())
                    {
                        rapidjson::Value allConstantsCopy;
                        allConstantsCopy.CopyFrom(*allConstants, json.GetAllocator());
                        json.AddValue(resourceJsonValue, "participantConstants", std::move(allConstantsCopy));
                    }
                    rapidjson::Value bindingJson = json.MakeObject();
                    json.AddString(bindingJson, "type", RootParamType::ToStr(binding.m_type));
                    ReflectBinding(bindingJson, binding, json);
                    json.AddValue(resourceJsonValue, "binding", std::move(bindingJson));
                    return resourceJsonValue;
                };

                std::optional<RootSigDesc::SrgParamDesc> srgConstants; // if we have SRG Constants we treat them later
                for (auto& srgParam : srgDesc.m_parameters)
                {
                    if (srgParam.m_type == RootParamType::SrgConstantCB)
                    {
                        assert(!srgConstants); // can't have multiple of those per SRG.
                        srgConstants = srgParam;
                    }
                    else
                    {
                        std::set<IdentifierUID> dependencyList;
                        DiscoverTopLevelFunctionDependencies(srgParam.m_uid, dependencyList, m_functionIntervals);
                        json.AddValue(srgMember, srgParam.m_uid.GetNameLeaf(), makeJsonNodeForOneResource(dependencyList, srgParam));
                    }
                }
                // SRG constants (and the variant-fallback) are in one special constant buffer
                if (srgConstants || srgInfo->m_shaderVariantFallback)
                {
                    rapidjson::Value allConstants = json.MakeArray();
                    // make a merged list of dependencies, shared by all individual constants.
                    std::set<IdentifierUID> dependencyList;
                    for (auto& srgConstant : srgInfo->m_implicitStruct.GetMemberFields())
                    {
                        json.PushString(allConstants, srgConstant.GetNameLeaf());
                        DiscoverTopLevelFunctionDependencies(srgConstant, dependencyList, m_functionIntervals);
                    }
                    // variant fallback support
                    if (srgInfo->m_shaderVariantFallback)
                    {
                        // no need to append m_shaderVariantFallback to allConstants because the fallback already exists in the memberFields
                        // all option variables converge to the fallback
                        for (auto& [varUid, varSub] : m_ir->GetOrderedSymbolsOfSubType_2<VarInfo>())
                        {
                            if (varSub->CheckHasStorageFlag(StorageFlag::Option))
                            {
                                DiscoverTopLevelFunctionDependencies(varUid, dependencyList, m_functionIntervals);
                            }
                        }
                    }
                    json.AddValue(
                        srgMember,
                        ExtractLeaf(MakeSrgConstantsCBName(uid)),
                        makeJsonNodeForOneResource(dependencyList, *srgConstants, &allConstants));
                }
                json.AddValue(srgRoot, uid.GetNameLeaf(), std::move(srgMember));
            }
        }

        m_out << json.Serialize(srgRoot);
    }

    // Helper routine for option rank analysis
    static int GuesstimateIntrinsicFunctionCost(const std::string_view funcName)
    {
        if (IsOneOf(funcName, "CallShader", "TraceRay"))
        {
            // non measurable but assumed high
            return 100;
        }
        if (IsOneOf(funcName, "Sample", "Load", "InterlockedCompareStore", "InterlockedCompareExchange", "InterlockedExchange", "Append"))
        {
            // memory access, locked or not, will have high latency
            return 10;
        }
        // unlisted intrinsics like lerp, log2, cos, distance.. will default to a cost of 1.
        return 1;
    }

    // Helper routine for option rank analysis. When picking AN overload is more useful than forfeiting.
    // The function GetConcreteFunctionThatMatchesArgumentList forfeits when the overloadset contains
    // strictly more than 1 concrete function with the queried arity. In our case, we prefer to just pick any.
    static IdentifierUID PickAnyOverloadThatMatchesArgCount(
        IntermediateRepresentation* ir,
        azslParser::FunctionCallExpressionContext* callNode,
        KindInfo& overload)
    {
        IdentifierUID concrete;
        const size_t numArgs = NumArgs(callNode);
        overload.GetSubAs<OverloadSetInfo>()->AnyOf(
            [&](const IdentifierUID& uid)
            {
                const auto* concreteFcInfo = ir->GetSymbolSubAs<FunctionInfo>(uid.GetName());
                const size_t numParams = concreteFcInfo->GetParameters(true).size();
                if (numParams == numArgs)
                {
                    concrete = uid; // we write the result through reference capture (not clean but convenient)
                    return true;
                }
                return false;
            });
        return concrete;
    }

    void CodeReflection::AnalyzeOptionRanks() const
    {
        // make sure we have the scope lookup cache ready
        GenerateTokenScopeIntervalToUidReverseMap();
        // loop over variables
        for (auto& [uid, varInfo, kindInfo] : m_ir->m_symbols.GetOrderedSymbolsOfSubType_3<VarInfo>())
        {
            // only options
            if (varInfo->CheckHasStorageFlag(StorageFlag::Option))
            {
                verboseCout << "Analyzing " << uid << "\n";
                int impactScore = 0;
                // loop over appearances over the program
                for (Seenat& ref : kindInfo->GetSeenats())
                {
                    verboseCout << "Seen-at line " << ref.m_where.m_line << "\n";
                    // determine an impact score
                    impactScore += AnalyzeImpact(ref.m_where) // dependent code that may be skipped depending on the value of that ref
                        + 1; // by virtue of being mentioned (seenat), we count the reference as an access of cost 1.
                }
                varInfo->m_estimatedCostImpact = impactScore;
                verboseCout << uid << " final cost " << impactScore << "\n";
            }
        }
    }

    template <typename CtxT>
    bool SetNextNodeIfChildOfCtxTCondViaNParents(
        ParserRuleContext*& node,
        const int maxDepth)
    {
        if (auto* searchNode = DeepParentAs<CtxT*>(node, maxDepth))
        {
            if (IsParentOf(searchNode->Condition, node))
            {
                node = searchNode->embeddedStatement();
                return true;
            }
        }
        return false;
    }

    int CodeReflection::AnalyzeImpact(const TokensLocation& location) const
    {
        // find the node at `location`:
        ParserRuleContext* node = m_ir->m_tokenMap.GetNode(location.m_focusedTokenId);
        // the "belonging" statements that we will consider, before recursing:
        using AstIf = azslParser::IfStatementContext;
        using AstFor = azslParser::ForStatementContext;
        using AstWhile = azslParser::WhileStatementContext;
        using AstDo = azslParser::DoStatementContext;
        using AstSwitch = azslParser::SwitchStatementContext;
        // go up the tree to meet one of them using arbitrary max depths of {5,6,7},
        // just enough to search up things like `for (a, b<(ref+1), c)` idExpr->IdentifierExpression->OtherExpression->BinaryExpr->ParenthesisExpr->BinaryExpr->Condition->For
        int complexityFactor = 1;
        const bool isNonLoop = SetNextNodeIfChildOfCtxTCondViaNParents<AstIf>(node, 6);
        if (!isNonLoop && (SetNextNodeIfChildOfCtxTCondViaNParents<AstFor>(node, 7)
            || SetNextNodeIfChildOfCtxTCondViaNParents<AstWhile>(node, 6)
            || SetNextNodeIfChildOfCtxTCondViaNParents<AstDo>(node, 6)))
        {
            complexityFactor = 2; // arbitrarily augment loop scores by virtue of assuming they repeat O(N=2)
        }
        else if (auto* switchNode = DeepParentAs<AstSwitch*>(node, 5))
        {
            if (IsParentOf(switchNode->Expr, node))
            {
                node = switchNode->switchBlock();
            }
        }
        int score = 0;
        AnalyzeImpact(node, score);
        return score * complexityFactor;
    }

    void CodeReflection::AnalyzeImpact(const ParserRuleContext* astNode, int& scoreAccumulator) const
    {
        for (const auto& c : astNode->children)
        {
            if (const auto* leaf = As<tree::TerminalNode*>(c))
            {
                // determine cost by number of full expressions separated by semicolon
                scoreAccumulator += leaf->getSymbol()->getType() == azslLexer::Semi; // bool as 0 or 1 trick
            }
            else if (auto* callNode = As<azslParser::FunctionCallExpressionContext*>(c))
            {
                // branch into an overload specialized for function lookup:
                AnalyzeImpact(callNode, scoreAccumulator);
            }
            else if (auto* node = As<ParserRuleContext*>(c))
            {
                AnalyzeImpact(node, scoreAccumulator); // recurse down to make sure to capture embedded calls, like e.g. "x ? f() : 0;"
            }
        }
    }

    void CodeReflection::AnalyzeImpact(azslParser::FunctionCallExpressionContext* callNode, int& scoreAccumulator) const
    {
        // to access the function symbol info we need the current scope, the function call name and perform a lookup.

        // figure out the scope at this token.
        // theoretically should be something in the like of the body of another function,
        // or an anonymous block within another function.
        const auto interval = m_intervals.GetClosestIntervalSurrounding(callNode->start->getTokenIndex());
        if (!interval.IsEmpty())
        {
            const IdentifierUID encloser = m_intervalToUid[interval];

            // Because we are past the end of the semantic analysis,
            // the scope tracker is registering the last seen scope (surely "/").
            // This is a stateful side-effect system unfortunately, and since we'll call
            // some feature of the semantic orchestrator (like TypeofExpr) we need to hack
            // the scope tracker:
            m_ir->m_sema.m_scope->m_currentScopePath = encloser.GetName();
            m_ir->m_sema.m_scope->UpdateCurScopeUID();

            QualifiedName startupLookupScope = encloser.GetName();
            UnqualifiedName funcName;
            if (auto* idExpr = As<azslParser::IdentifierExpressionContext*>(callNode->Expr))
            {
                funcName = ExtractNameFromIdExpression(idExpr->idExpression());
            }
            else if (const auto* maeExpr = As<AstMemberAccess*>(callNode->Expr))
            {
                startupLookupScope = m_ir->m_sema.TypeofExpr(maeExpr->LHSExpr);
                funcName = ExtractNameFromIdExpression(maeExpr->Member);
            }
            IdAndKind* overload = m_ir->m_symbols.LookupSymbol(startupLookupScope, funcName);
            if (!overload) // in case of function not found, we assume it's an intrinsic.
            {
                scoreAccumulator += GuesstimateIntrinsicFunctionCost(funcName);
            }
            else
            {
                azslParser::ArgumentListContext* args = GetArgumentListIfBelongsToFunctionCall(callNode);
                const IdAndKind* symbolMeantUnderCallNode = m_ir->m_sema.ResolveOverload(overload, args);
                IdentifierUID concrete;
                if (!symbolMeantUnderCallNode || m_ir->GetKind(symbolMeantUnderCallNode->first) == Kind::OverloadSet)
                {
                    // in case of strict selection failure, run a fuzzy select
                    concrete = PickAnyOverloadThatMatchesArgCount(m_ir, callNode, overload->second);
                    // if still not enough to get a fix (concrete=={}), it might be an ill-formed input. prefer to forfeit
                }
                else
                {
                    concrete = symbolMeantUnderCallNode->first;
                }

                if (auto* funcInfo = m_ir->GetSymbolSubAs<FunctionInfo>(concrete.GetName()))
                {
                    if (funcInfo->m_costScore == -1) // cost not yet discovered for this function
                    {
                        verboseCout << " " << concrete << " non-memoized. discovering cost\n";
                        funcInfo->m_costScore = 0;
                        if (funcInfo->m_defNode) // undefined functions can't be explored
                        {
                            using AstFDef = azslParser::HlslFunctionDefinitionContext;
                            AnalyzeImpact(
                                polymorphic_downcast<AstFDef*>(funcInfo->m_defNode->parent)->block(),
                                funcInfo->m_costScore); // recurse and cache
                        }
                    }
                    scoreAccumulator += funcInfo->m_costScore;
                    verboseCout << " " << concrete << " cost score " << funcInfo->m_costScore << " added\n";
                }
            }
            // other cases forfeited for now, but that would at least include things like eg braces (f)()
        }
        else // no interval found
        {
            // function calls outside of function bodies can appear in an initializer:
            //    int g_a = MakeA();  // global init
            //    class C { int m_a = CompA();  // constructor init (invalid AZSL/HLSL)
            //    class D { void Method(int a_a = DefaultA());  // default parameter value
            // in any case, extracting the scope is impossible with this system.
            // we forfeit evaluation of a score
        }
    }

    void CodeReflection::GenerateTokenScopeIntervalToUidReverseMap() const
    {
        if (m_functionIntervals.empty())
        {
            for (auto& [uid, interval] : m_ir->m_scope.m_scopeIntervals)
            {
                if (m_ir->GetKind(uid) == Kind::Function) // Filter out unnamed blocs and types.
                {
                    // the reason to choose .a as the key is so we can query using Infimum (sort of lower_bound)
                    m_functionIntervals[interval.a] = std::make_pair(interval, uid);
                }
                auto i = Interval<ssize_t>{interval.a, interval.b};
                m_intervals.Add(i);
                m_intervalToUid[i] = uid;
            }
            m_intervals.Seal();
        }
    }
}
