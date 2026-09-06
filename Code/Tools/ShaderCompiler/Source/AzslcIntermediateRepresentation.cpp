/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AzslcIntermediateRepresentation.h"

#include <json/json.h>

namespace AZ::ShaderCompiler
{
    using std::cout;
    using std::endl;

    void IntermediateRepresentation::RegisterAttributeSpecifier(AttributeScope scope,
                                                                AttributeCategory category,
                                                                SourceLocation sourceLocation,
                                                                string_view space,
                                                                string_view name,
                                                                azslParser::AttributeArgumentListContext* argList)
    {
        AttributeInfo attrInfo;
        attrInfo.m_scope      = scope;
        attrInfo.m_category   = category;
        attrInfo.m_sourceLocation = sourceLocation;
        attrInfo.m_namespace  = space;
        attrInfo.m_attribute  = name;

        if (argList)
        {
            for (auto arg : argList->attributeArguments()->literal())
            {
                if (!arg->StringLiteral().empty())
                {
                    for (int i = 0; i < arg->StringLiteral().size(); ++i)
                    {
                        attrInfo.m_argList.push_back(arg->StringLiteral(i)->getText());
                    }
                }
                else if (arg->IntegerLiteral())
                {
                    attrInfo.m_argList.push_back(m_sema.FoldEvalStaticConstExprNumericValue(arg->IntegerLiteral()));
                }
                else if (arg->FloatLiteral())
                {
                    attrInfo.m_argList.push_back(m_sema.FoldEvalStaticConstExprNumericValue(arg->FloatLiteral(), false));
                }
                else if (arg->True())
                {
                    attrInfo.m_argList.push_back(true);
                }
                else if (arg->False())
                {
                    attrInfo.m_argList.push_back(false);
                }
                else
                {
                    // A new case has been added - it should be resolved properly
                    assert(false);
                }
            }
        }

        if (ProcessAttributeSpecifier(attrInfo))
        {
            m_symbols.PushPendingAttribute(attrInfo, scope);
        }
    }

    bool IntermediateRepresentation::ProcessAttributeSpecifier(const AttributeInfo& attrInfo)
    {
        // Hint for the pixel output format for all or one of the render targets
        if (attrInfo.m_attribute == "output_format")
        {
            bool isDefault = (attrInfo.m_argList.size() == 1 && holds_alternative<string>(attrInfo.m_argList[0]));
            bool isIndexed = (attrInfo.m_argList.size() == 2 && holds_alternative<string>(attrInfo.m_argList[1]) && holds_alternative<ConstNumericVal>(attrInfo.m_argList[0]));
            if (!isDefault && !isIndexed)
            {
                return true;
            }

            OutputFormat hint = OutputFormat::FromStr(Trim(get<string>(attrInfo.m_argList[isDefault ? 0 : 1]), "\""));

            if (isDefault)
            {
                std::for_each(m_metaData.m_outputFormatHint, m_metaData.m_outputFormatHint + 8, [&](OutputFormat& fmt) { fmt = hint; });
            }
            else
            {
                auto rtIndex = ExtractValueAsInt64(get<ConstNumericVal>(attrInfo.m_argList[0]), std::numeric_limits<int64_t>::min());
                if (rtIndex >= 0 && rtIndex <= 7)
                {
                    m_metaData.m_outputFormatHint[rtIndex] = hint;
                }
            }
        }
        else if (attrInfo.m_attribute == "pad_to")
        {
            m_padToAttributeMutator.ProcessPadToAttribute(attrInfo);
            return false; //Do not store this attribute
        }
        // The attribute has no special meaning to AZSLc, just pass it
        return true;
    }

    void IntermediateRepresentation::RegisterTokenToNodeAssociation(ssize_t tokenId, antlr4::ParserRuleContext* node)
    {
        m_tokenMap.SetAssociation(tokenId, node);
    }

    //! execute any logic that relates to intermediate treatment that would need to be done between front end and back end
    void IntermediateRepresentation::MiddleEnd(const MiddleEndConfiguration& middleEndconfigration)
    {
        // At this point we have an order apparition vector that stores symbols in the immediate naive
        // order in which they are first seen in the source code.
        //
        // However for code re-emission, it's not really the ordering we need. The ordering we need 
        // corresponds exactly to the "head recursion visit of AST nodes".
        // We're not going to transform our flat vector to tree and to vector again,
        // instead we'll shift around the elements to respect that order.
        // And to do so, we'll use a dependency DAG and a topological solver.
        verboseCout << "--Middle End--\n";

        m_symbols.ReorderBySymbolDependency();

        RegisterRootConstantStruct(middleEndconfigration);

        m_padToAttributeMutator.RunMutationsForPadToAttributes(middleEndconfigration);

        if (!middleEndconfigration.m_skipAlignmentValidation)
        {
            ValidateAlignmentIssueWhenScalarOrFloat2PrecededByMatrix(middleEndconfigration);
        }
    }

    bool IntermediateRepresentation::Validate()
    {
        auto hasOptions = false;
        for (auto& [uid, varInfo] : m_symbols.GetOrderedSymbolsOfSubType_2<VarInfo>())
        {
            if (varInfo->CheckHasStorageFlag(StorageFlag::Option))
            {
                hasOptions = true;
                break;
            }
        }

        auto variantFallbackGroups = 0;
        for (auto srgInfo : GetOrderedSubInfosOfSubType<SRGInfo>())
        {
            if (!srgInfo->m_semantic)
            {
                auto errorMsg = FormatString("Missing ShaderResourceGroupSemantic for ShaderResourceGroup [%s]", srgInfo->m_declNode->getText().c_str());
                throw AzslcIrException(IR_SRG_WITHOUT_SEMANTIC, errorMsg);
            }

            auto* semanticAsClassInfo = GetSymbolSubAs<ClassInfo>(srgInfo->m_semantic->GetName());
            optional<int64_t>& variantFallback = semanticAsClassInfo->Get<SRGSemanticInfo>()->m_variantFallback;
            if (variantFallback)
            {
                variantFallbackGroups++;

                if (!hasOptions)
                {
                    PrintWarning(Warn::W1, semanticAsClassInfo->GetDeclNode()->start,
                                 "If you have no options, SRG do not need a ShaderVariantFallback");
                }
            }
        }

        if (variantFallbackGroups >= 2)
        {
            throw AzslcIrException(IR_MULTIPLE_SRG_FALLBACK, "Two or more SRGs cannot all be designated as the default ShaderVariantFallback");
        }

        if (hasOptions && variantFallbackGroups == 0)
        {
            throw AzslcIrException(IR_NO_FALLBACK_ASSIGNED, "If you have non-static options, one SRG must be designated as the default ShaderVariantFallback");
        }

        return true;
    }

    vector<IdentifierUID> IntermediateRepresentation::GetChildren(const IdentifierUID& parentUid) const
    {
        vector<IdentifierUID> results;
        for (const auto& uid : m_symbols.GetOrderedSymbols())
        {
            if (uid.IsParent(parentUid))
            {
                results.push_back(uid);
            }
        }
        return results;
    }

    void IntermediateRepresentation::RemoveUnusedSrgs()
    {
        // We use this function pointer to find SRGs that have no references.
        auto unreferencedSymbolFilterFunc = +[](KindInfo* kindInfo) {
            const SRGInfo* srgInfo = kindInfo->GetSubAs<SRGInfo>();
            const bool isFallbackSrg = !!srgInfo->m_shaderVariantFallback;
            return !isFallbackSrg && (kindInfo->GetSeenats().size() == 0);
        };

        vector<IdentifierUID> unusedSrgs = GetFilteredSymbolsOfSubType<SRGInfo>(unreferencedSymbolFilterFunc);
        while (!unusedSrgs.empty())
        {
            // For any given symbol, collects all the scope intervals owned by the symbol or any of its children.
            auto getSymbolScopesFunc = [&](const IdentifierUID& srgId) -> vector<misc::Interval>
            {
                vector<misc::Interval> intervals;
                for (const auto & [symId, interval] : m_scope.m_scopeIntervals)
                {
                    if ((symId.m_name == srgId.m_name) || symId.IsParent(srgId))
                    {
                        intervals.push_back(interval);
                    }
                }
                return intervals;
            };

            // With this filter we can extract symbols that are being referenced.
            auto referencedSymbolFilterFunc = +[](KindInfo* kindInfo) {
                return !kindInfo->GetSeenats().empty();
            };

            decltype(unusedSrgs) usedSrgs = GetFilteredSymbolsOfSubType<SRGInfo>(referencedSymbolFilterFunc);

            for (const auto& srgUid : unusedSrgs)
            {
                // We will place here the scopes owned by the unused SRG.
                // it must be a vector of intervals because thanks to "partial" SRGs
                // the intervals may split. The key idea is that before an unused SRG is removed from
                // the symbols table, we will loop through the used SRGs and check if their Seenats
                // fall within an unused SRG scope interval. The matching Seenats will be removed from the used SRGs.
                // In turn if all Seenats for any given used SRG are removed then the SRG becomes unused.
                vector<misc::Interval> unusedSrgScopes = getSymbolScopesFunc(srgUid);

                vector<IdentifierUID> symbolsToDelete = GetChildren(srgUid);
                for (const auto& uid : symbolsToDelete)
                {
                    // Before removing the symbol let's remove it from the Seenat of the Srgs
                    // that may have a reference to it.
                    m_symbols.DeleteIdentifier(uid);
                }

                // In this loop used SRGs may become unused SRGs.
                // The loop removes Seenats from used SRGs when they fall within the scope intervals
                // of an unused SRG.
                for (const auto& usedSrgId : usedSrgs)
                {
                    KindInfo* usedSrgKindInfo = GetKindInfo(usedSrgId);
                    vector<Seenat>& seenAts = usedSrgKindInfo->GetSeenats();
                    seenAts.erase(std::remove_if(seenAts.begin(), seenAts.end(), [&](const Seenat& seenAt) -> bool {
                        return std::any_of(unusedSrgScopes.begin(), unusedSrgScopes.end(), [&](const misc::Interval& interval) -> bool {
                            return (seenAt.m_where.m_focusedTokenId >= interval.a) && (seenAt.m_where.m_focusedTokenId <= interval.b);
                            });
                    }), seenAts.end());
                }

                //Finally, remove the SRG.
                m_symbols.DeleteIdentifier(srgUid);
            }

            //Refresh, there could be new unused SRGs.
            unusedSrgs = GetFilteredSymbolsOfSubType<SRGInfo>(unreferencedSymbolFilterFunc);
        }

    }

    static Json::Value TypeToJson(const TypeRefInfo& type, const IntermediateRepresentation& ir)
    {
        if (type.IsEmpty())
        {
            return "<NA>";
        }
        Json::Value result(Json::objectValue);
        result["name"] = type.m_typeId.m_name;
        if (ir.m_symbols.HasIdentifier(type.m_typeId.m_name))
        {
            result["validity"] = "found";
        }
        else
        {
            result["validity"] = "undeclared";
        }
        result["tclass"] = string{TypeClass::ToStr(type.m_typeClass)};
        result["underlying_scalar"] = type.m_arithmeticInfo.UnderlyingScalarToStr();
        return result;
    }

    static Json::Value TypeToJson(const ExtendedTypeInfo& type, const IntermediateRepresentation& ir)
    {
        Json::Value result(Json::objectValue);
        result["core"] = TypeToJson(type.m_coreType, ir);
        result["generic"] = TypeToJson(type.m_genericParameter, ir);
        return result;
    }

    void DumpSymbols(IntermediateRepresentation& ir)
    {
        Json::Value symbols(Json::objectValue);
        std::unordered_set<IdentifierUID> seen;
        for (auto& uid : ir.m_symbols.GetOrderedSymbols())
        {
            // A function can appear at both its declaration and definition.
            if (!seen.insert(uid).second)
            {
                continue;
            }
            auto& [id, sym] = *ir.m_symbols.GetIdAndKindInfo(uid.m_name);
            assert(uid == id);
            auto& symbol = symbols["Symbol " + Decorate("'", uid.m_name)];
            symbol["kind"] = string{Kind::ToStr(sym.GetKind())};
            auto& references = symbol["references"] = Json::Value(Json::arrayValue);
            for (const auto& seenAt : sym.GetSeenats())
            {
                Json::Value reference(Json::objectValue);
                reference["line"] = Json::UInt64(seenAt.m_where.m_sourceLocation.Resolve().m_line);
                reference["col"] = Json::UInt64(seenAt.m_where.m_charPos + 1);
                references.append(std::move(reference));
            }
            switch (sym.GetKind())
            {
            case Kind::Variable:
            {
                auto& sub = sym.GetSubRefAs<VarInfo>();
                // Synthetic attribute variables have no declaration.
                if (sub.m_declNode)
                {
                    symbol["line"] = Json::UInt64(sub.m_declNode->start->getLine());
                }
                else
                {
                    symbol["line"] = "NA";
                }
                symbol["type"] = TypeToJson(sub.m_typeInfoExt, ir);
                symbol["storage"] = sub.m_typeInfoExt.m_qualifiers.GetDisplayName();
                symbol["array dim"] = sub.m_typeInfoExt.m_arrayDims.ToString();
                symbol["has sampler state"] = static_cast<bool>(sub.m_samplerState);
                if (!holds_alternative<monostate>(sub.m_constVal))
                {
                    symbol["val"] = Json::Int64(ExtractValueAsInt64(sub.m_constVal));
                }
            }
            break;
            case Kind::Interface:
            case Kind::Struct:
            case Kind::Class:
            case Kind::Enum:
            {
                auto& sub = sym.GetSubRefAs<ClassInfo>();
                symbol["line"] = Json::UInt64(sub.GetOriginalSourceLocation().Resolve().m_line);
                auto& members = symbol["members"] = Json::Value(Json::arrayValue);
                for (auto&& member : sub.GetOrderedMembers())
                {
                    auto* subFieldSym = ir.m_symbols.GetIdAndKindInfo(member.m_name);
                    Json::Value field(Json::objectValue);
                    field["kind"] = string{Kind::ToStr(subFieldSym->second.GetKind())};
                    field["name"] = member.m_name;
                    members.append(std::move(field));
                }
            }
            break;
            case Kind::Function:
            {
                auto& sub = sym.GetSubRefAs<FunctionInfo>();
                symbol["line"] = Json::UInt64(sym.VisitSub(GetOriginalSourceLocation_Visitor{}).Resolve().m_line);
                if (sub.IsUndefinedFunction())
                {
                    symbol["def line"] = "undef";
                }
                else
                {
                    symbol["def line"] = Json::UInt64(sub.m_defNode->start->getLine());
                }
                symbol["must override"] = sub.m_mustOverride;
                symbol["is method"] = sub.m_isMethod;
                symbol["is virtual"] = sub.m_isVirtual;
                symbol["return type"] = TypeToJson(sub.m_returnType, ir);
                symbol["storage"] = sub.m_returnType.m_qualifiers.GetDisplayName();
                auto& overrides = symbol["has overriding children"] = Json::Value(Json::arrayValue);
                for (const auto& child : sub.m_overrides)
                {
                    Json::Value override(Json::objectValue);
                    override["name"] = child.m_name;
                    overrides.append(std::move(override));
                }
                symbol["is hiding base symbol"] = "";
                if (sub.m_base)
                {
                    symbol["is hiding base symbol"] = sub.m_base->m_name;
                }
                auto& parameters = symbol["parameters"] = Json::Value(Json::arrayValue);
                for (auto& param : sub.GetParameters(1))
                {
                    Json::Value parameter(Json::objectValue);
                    auto* varInfo = ir.GetSymbolSubAs<VarInfo>(param.m_varId.GetName());
                    if (varInfo)
                    {
                        parameter["name"] = "<unnamed>";
                        if (!varInfo->m_identifier.empty())
                        {
                            parameter["name"] = static_cast<string&>(varInfo->m_identifier);
                        }
                        parameter["type"] = TypeToJson(varInfo->m_typeInfoExt, ir);
                    }
                    else
                    {
                        parameter["non-var identifier (type?)"] = TypeToJson(param.m_typeInfo, ir);
                    }
                    parameters.append(std::move(parameter));
                }
            }
            break;
            case Kind::OverloadSet:
            {
                auto& sub = sym.GetSubRefAs<OverloadSetInfo>();
                auto& functions = symbol["functions"] = Json::Value(Json::arrayValue);
                sub.ForEach([&functions](auto& function) { functions.append(string{function.GetName()}); });
            }
            break;
            case Kind::ShaderResourceGroup:
            {
                auto& sub = sym.GetSubRefAs<SRGInfo>();
                symbol["line"] = Json::UInt64(sub.m_declNode->start->getLine());
                auto names = [](const auto& identifiers)
                {
                    Json::Value result(Json::arrayValue);
                    for (const auto& identifier : identifiers)
                    {
                        result.append(string{identifier.GetNameLeaf()});
                    }
                    return result;
                };
                symbol["structs"] = names(sub.m_structs);
                symbol["srViews"] = names(sub.m_srViews);
                symbol["samplers"] = names(sub.m_samplers);
                symbol["CBs"] = names(sub.m_CBs);
            }
            break;
            case Kind::Type:
            {
                symbol["type"] = TypeToJson(sym.GetSubRefAs<TypeRefInfo>(), ir);
            }
            break;
            case Kind::TypeAlias:
            {
                auto& sub = sym.GetSubRefAs<TypeAliasInfo>();
                symbol["line"] = Json::UInt64(sym.VisitSub(GetOriginalSourceLocation_Visitor{}).Resolve().m_line);
                symbol["canonical type"] = TypeToJson(sub.m_canonicalType, ir);
            }
            break;
            default:
                break;
            }
        }
        cout << symbols << '\n';
    }

    int IntermediateRepresentation::ValidateRootConstantStruct(const MiddleEndConfiguration& middleEndconfigration)
    {
        int rootConstantsSize = 0;
        for (auto& [uid, varInfo] : m_symbols.GetOrderedSymbolsOfSubType_2<VarInfo>())
        {
            if (varInfo->CheckHasAnyStorageFlags({ StorageFlag::Rootconstant }) && IsGlobal(uid.GetName()))
            {
                assert(!IsChameleon(varInfo->GetTypeClass()));

                auto exportedType = varInfo->m_typeInfoExt.m_coreType;
                if (!exportedType.IsPackable())
                {
                    throw SourceError{ "Internal Error: The type '"
                        + exportedType.m_typeId.m_name
                        + "' in layout member '"
                        + uid.m_name
                        + "' cannot be packed." };
                }

                // GetTotalSize of each member of the structure
                rootConstantsSize += varInfo->m_typeInfoExt.GetTotalSize(middleEndconfigration.m_packDataBuffers, middleEndconfigration.m_isRowMajor);
            }
        }

        // We have a limited space for rootconstant (specified by --root-const=N) check that we're not going over
        if (rootConstantsSize > middleEndconfigration.m_rootConstantsMaxSize)
        {
            throw SourceError{ "The rootconstants size of "
                + ToString(rootConstantsSize)
                + " bytes exceeds the limit of "
                + ToString(middleEndconfigration.m_rootConstantsMaxSize)
                + "bytes supported on the target platform." };
        }

        // Spec requires at least 128 bytes of block, bigger values (>128 bytes) need to be checked against support by implementation
        constexpr int MinByteSizeRequired = 128;
        if (rootConstantsSize > MinByteSizeRequired)
        {
            PrintWarning(Warn::W2, none, "The root constant size ", rootConstantsSize, " is greater than 128 bytes. ",
                "Large root constant size may not be supported by the implementation.");
        }

        return rootConstantsSize;
    }

    uint32_t IntermediateRepresentation::CalculateLayoutForRootConstantMember(
        const IdentifierUID& memberId,
        const bool isRowMajor,
        const AZ::ShaderCompiler::Packing::Layout layoutPacking,
        const uint32_t startingOffset,
        uint32_t& nextMemberStartingOffset) const
    {
        const auto* varInfoPtr = GetSymbolSubAs<VarInfo>(memberId.m_name);
        uint32_t size = 0;

        if (varInfoPtr)
        {
            const auto& varInfo = *varInfoPtr;

            // View types should only be called from GetViewStride until we decide to support them as struct constants
            assert(!IsChameleon(varInfo.GetTypeClass()));

            auto exportedType = varInfo.m_typeInfoExt.m_coreType;

            if (!exportedType.IsPackable())
            {
                throw SourceError{ "error: unpackable type ("
                    + exportedType.m_typeId.m_name
                    + ") in layout member "
                    + memberId.m_name };
            }
            size = varInfo.m_typeInfoExt.GetTotalSize(layoutPacking, isRowMajor);
            auto startAt = startingOffset;
            nextMemberStartingOffset = startingOffset;

            // Alignment start
            if (exportedType.m_arithmeticInfo.IsMatrix() || exportedType.m_arithmeticInfo.IsVector())
            {
                const auto rows = exportedType.m_arithmeticInfo.m_rows;
                const auto cols = exportedType.m_arithmeticInfo.m_cols;
                const auto packAlignment = exportedType.m_arithmeticInfo.IsMatrix() ? Packing::Alignment::asMatrixStart : Packing::Alignment::asVectorStart;
                startAt = nextMemberStartingOffset = Packing::AlignOffset(layoutPacking, nextMemberStartingOffset, packAlignment, rows, cols);
            }

            // Compound types are not supported in root constants.
            assert(!IsProductType(exportedType.m_typeClass));

            if (varInfo.GetTypeClass() == TypeClass::Enum)
            {
                auto* asClassInfo = GetSymbolSubAs<ClassInfo>(varInfo.GetTypeId().GetName());
                size = asClassInfo->Get<EnumerationInfo>()->m_underlyingType.m_arithmeticInfo.m_baseSize;
            }

            nextMemberStartingOffset = Packing::PackNextChunk(layoutPacking, size, startAt);

            // Alignment end
            if (exportedType.m_arithmeticInfo.IsMatrix() || exportedType.m_arithmeticInfo.IsVector())
            {
                const auto rows = exportedType.m_arithmeticInfo.m_rows;
                const auto cols = exportedType.m_arithmeticInfo.m_cols;
                const auto packAlignment = exportedType.m_arithmeticInfo.IsMatrix() ? Packing::Alignment::asMatrixEnd : Packing::Alignment::asVectorEnd;
                nextMemberStartingOffset = Packing::AlignOffset(layoutPacking, nextMemberStartingOffset, packAlignment, rows, cols);
            }

            size = nextMemberStartingOffset - startAt;
        }

        return size;
    }

    uint32_t IntermediateRepresentation::CalculateSizeOfRootConstantsCB(
        const IdentifierUID& exportedTypeId,
        const bool isRowMajor,
        const AZ::ShaderCompiler::Packing::Layout layoutPacking) const
    {
        uint32_t memberOffset = 0;
        uint32_t largestMemberSize = 0;

        const auto* classInfo = GetSymbolSubAs<ClassInfo>(exportedTypeId.m_name);
        for (const auto& memberField : classInfo->GetMemberFields())
        {
            const auto currentStride = memberOffset;
            CalculateLayoutForRootConstantMember(memberField, isRowMajor, GetExtendedLayout(layoutPacking), currentStride, memberOffset);
            largestMemberSize = std::max(largestMemberSize, memberOffset - currentStride);
        }

        memberOffset = Packing::AlignStructToLargestMember(layoutPacking, memberOffset, largestMemberSize);
        return memberOffset;
    }

    void IntermediateRepresentation::RegisterRootConstantStruct(const MiddleEndConfiguration& middleEndconfigration)
    {
        int rootConstantByteSize = ValidateRootConstantStruct(middleEndconfigration);

        QualifiedName name{ "/Root_Constants" };

        assert(!m_symbols.HasIdentifier(name));  // reserved name semantic error must have prevented this state to happen.

        if (rootConstantByteSize == 0)
        {
            return;
        }

        // Register a struct Root_Constants and its members to ir
        auto& [rootConstantStructUid, rootConstantStructKindInfo] = m_symbols.AddIdentifier(name, Kind::Struct, none, AddIdentifierChecks::None);
        rootConstantStructKindInfo.GetSubRefAs<ClassInfo>().m_kind = Kind::Struct;
        
        for (auto& [uid, varInfo] : m_symbols.GetOrderedSymbolsOfSubType_2<VarInfo>())
        {
            if (varInfo->CheckHasAnyStorageFlags({ StorageFlag::Rootconstant })
                && IsGlobal(uid.GetName()))
            {
                assert(!IsChameleon(varInfo->GetTypeClass()));
                auto exportedType = varInfo->m_typeInfoExt.m_coreType;

                // copy the variable into a new symbol. so that its path can make sense
                // as a child of the rootconstants struct.
                QualifiedName copiedFieldName{ JoinPath(rootConstantStructUid.GetName(), ReplaceSeparators(uid.m_name, Underscore)) };
                auto& [newVarUid, newVarKind] = m_symbols.AddIdentifier(copiedFieldName, Kind::Variable);
                newVarKind.GetSubRefAs<VarInfo>() = *varInfo;
                newVarKind.GetSubRefAs<VarInfo>().m_declNode = nullptr;  // the original declaration in the AST isn't inherited by a copied symbol.
                // Add root constants to the registered struct as member
                rootConstantStructKindInfo.GetSubRefAs<ClassInfo>().PushMember(newVarUid, Kind::Variable);
            }
        }

        if (middleEndconfigration.m_padRootConstantCB)
        {
            const auto layoutPacking = middleEndconfigration.m_packConstantBuffers;
            uint32_t strideSize = CalculateSizeOfRootConstantsCB(rootConstantStructUid, middleEndconfigration.m_isRowMajor, layoutPacking);
            uint32_t endOffset = 0;
            switch (layoutPacking)
            {
                case Packing::Layout::DirectXPacking:          
                case Packing::Layout::RelaxedDirectXPacking:
                    // asStructEnd does nothing in DirectX, but in case of padding the root constant We force
                    // asStructStart in order to get 16byte alignment.
                    endOffset = Packing::AlignOffset(layoutPacking, strideSize, Packing::Alignment::asStructStart, 0, 0);
                    break;
                default:
                    endOffset = Packing::AlignOffset(layoutPacking, strideSize, Packing::Alignment::asStructEnd, 0, 0);
                    break;
            }

            // If the structure is NOT aligned, we inject a 'pad' member to enforce that padding.
            // There are 4 cases: uint1, uint2, uint3, or uint4.
            if (endOffset > strideSize)
            {
                uint32_t padSize = endOffset - strideSize;

                QualifiedName padFieldName{ JoinPath(rootConstantStructUid.GetName(), "__pad__") };
                auto& [newVarUid, newVarKind] = m_symbols.AddIdentifier(padFieldName, Kind::Variable);

                VarInfo varInfo;
                varInfo.m_declNode = nullptr;
                varInfo.m_isPublic = false;

                assert(padSize == 16 || padSize == 12 || padSize == 8 || padSize == 4);
                string typeName = FormatString("uint%d", padSize / 4);

                ExtractedTypeExt padType = { UnqualifiedNameView(typeName), nullptr };
                varInfo.m_typeInfoExt = ExtendedTypeInfo{m_sema.CreateTypeRefInfo(padType), {},
                                                         {}, {}, {}, Packing::MatrixMajor::Default };

                newVarKind.GetSubRefAs<VarInfo>() = varInfo;
                rootConstantStructKindInfo.GetSubRefAs<ClassInfo>().PushMember(newVarUid, Kind::Variable);
            }
        }

        m_rootConstantStructUID = rootConstantStructUid;
    }

    void IntermediateRepresentation::ValidateAlignmentIssueWhenScalarOrFloat2PrecededByMatrix(const MiddleEndConfiguration& middleEndconfigration)
    {
        //! Helper lambda to check if a symbol is a matrix, it also returns
        //! the number of columns in @numColumns
        auto isMatrix = [&](const IdentifierUID& symbolUid, int& numColumns)
        {
            KindInfo* kindInfo = GetKindInfo(symbolUid);
            const VarInfo* varInfo = kindInfo->GetSubAs<VarInfo>();
            if (!varInfo)
            {
                // Not a variable.
                return false;
            }
            if (varInfo->GetTypeClass() != TypeClass::Matrix)
            {
                // Not a matrix.
                return false;
            }
            const auto* arithmeticInfo = &varInfo->m_typeInfoExt.m_coreType.m_arithmeticInfo;
            if (arithmeticInfo->m_rows < 2)
            {
                return false;
            }
            numColumns = arithmeticInfo->m_cols;
            if (symbolUid.GetName().find("(") != string::npos)
            {
                // It's the return value of a function or a function argument. Skip.
                return false;
            }
            return true;
        };

        //! Helper lambda. Checks if @varInfo size is 8 or smaller.
        //! The actual size in bytes is returned in sizeInBytes.
        auto isVariableOf8BytesOrLess = [](const MiddleEndConfiguration& middleEndconfigration, const VarInfo* varInfo, size_t& sizeInBytes)
        {
            if (!IsFundamental(varInfo->GetTypeClass()))
            {
                return false;
            }
            if (varInfo->m_typeInfoExt.IsArray())
            {
                return false;
            }
            const auto byteCount = varInfo->m_typeInfoExt.GetTotalSize(middleEndconfigration.m_packDataBuffers, middleEndconfigration.m_isRowMajor);
            if (byteCount > 8)
            {
                return false;
            }
            sizeInBytes = byteCount;
            return true;
        };

        // We'll loop through all symbols.
        const auto& orderedSymbols = m_symbols.m_elastic.m_order;
        // Keep record of the structs that end in float2x2, float3x2, float4x2
        unordered_set<IdentifierUID> structsWithMatrixRx2AsLastField;
        // and also the structs that end in float2x3, float3x3, float4x3
        unordered_set<IdentifierUID> structsWithMatrixRx3AsLastField;

        enum class PrepadType
        {
            Float2,
            Float3,
            Unknown,
        };
        struct UidWithPrepadType
        {
            PrepadType m_prepadType;
            IdentifierUID m_uid;
        };
        vector<UidWithPrepadType> symbolsToPrepad;
        for (size_t symbolIndex = 0; symbolIndex < orderedSymbols.size(); ++symbolIndex)
        {
            const IdentifierUID uid = orderedSymbols[symbolIndex];
            KindInfo* kindInfo = GetKindInfo(uid);
            if (IsGlobal(uid.GetName()))
            {
                continue; // no reason to align things in the global scope,
                          // options and rootconstant will be align-checked through their implicit structs.
            }

            // We need to keep track of the structs for which the last member is
            // a float3x3.
            const bool isStructOrClass = kindInfo->IsKindOneOf(Kind::Class, Kind::Struct);
            if (isStructOrClass)
            {
                auto& classInfo = kindInfo->GetSubRefAs<ClassInfo>();
                const auto& memberFieldList = classInfo.GetMemberFields();
                if (memberFieldList.empty())
                {
                    continue;
                }
                const auto memberCount = memberFieldList.size();
                auto& lastMemberUid = memberFieldList[memberCount - 1];
                int numColumns = 0;
                if (isMatrix(lastMemberUid, numColumns))
                {
                    if (numColumns == 2)
                    {
                        structsWithMatrixRx2AsLastField.insert(uid);
                    }
                    else if (numColumns == 3)
                    {
                        structsWithMatrixRx3AsLastField.insert(uid);
                    }
                }
                continue;
            }

            const VarInfo* varInfo = kindInfo->GetSubAs<VarInfo>();
            if (!varInfo)
            {
                continue;
            }

            size_t variableSizeInBytes;
            if (!isVariableOf8BytesOrLess(middleEndconfigration, varInfo, variableSizeInBytes))
            {
                continue;
            }

            if (uid.GetName().find("(") != string::npos)
            {
                // It's a function argument. Skip.
                continue;
            }

            // We only care if the previous VarInfo is one of:
            // 1- A struct or class whose last member is a float2x2, float3x2, float4x2,
            //    float2x3, float3x3, float4x3.
            // OR
            // 2- a float2x2, float3x2, float4x2,
            //    float2x3, float3x3, float4x3.
            //! Helper lambda to get the previously declared variable.
            auto getPreviousVarInfo = [&](ssize_t& startSearchSymbolIndex /*in out*/, string_view scope) -> VarInfo*
            {
                while (startSearchSymbolIndex >= 0)
                {
                    const auto& prevSymbolUid = orderedSymbols[startSearchSymbolIndex];
                    if (GetParentName(prevSymbolUid.GetName()) != scope)
                    {
                        return nullptr;  // we can't relate variables that don't even belong to same struct or srg
                    }
                    KindInfo* prevKindInfo = GetKindInfo(prevSymbolUid);
                    VarInfo* prevVarInfo = prevKindInfo->GetSubAs<VarInfo>();
                    if (prevVarInfo)
                    {
                        return prevVarInfo;
                    }
                    startSearchSymbolIndex--;
                }
                return nullptr;
            };

            ssize_t prevSymbolIndex = symbolIndex - 1;
            const VarInfo* prevVarInfo = getPreviousVarInfo(prevSymbolIndex, GetParentName(uid.GetName()));
            if (prevVarInfo == nullptr)
            {
                // Nothing to do, keep going.
                continue;
            }

            // Is it a Matrix?
            const auto& prevSymbolUid = orderedSymbols[prevSymbolIndex];
            int numColumns = 0;
            if (isMatrix(prevSymbolUid, numColumns))
            {
                if (numColumns == 2)
                {
                    symbolsToPrepad.push_back(UidWithPrepadType{PrepadType::Float3, uid});
                }
                else if ((numColumns == 3) && (variableSizeInBytes <= 4))
                {
                    symbolsToPrepad.push_back(UidWithPrepadType{PrepadType::Float2, uid});
                }
                continue;
            }
            
            // Is the type a struct or class whose last member is a Matrix of interest?
            const auto prevVarTypeUid = prevVarInfo->m_typeInfoExt.m_coreType.m_typeId;
            if (structsWithMatrixRx2AsLastField.count(prevVarTypeUid))
            {
                symbolsToPrepad.push_back(UidWithPrepadType{PrepadType::Float3, uid});
            }
            else if (structsWithMatrixRx3AsLastField.count(prevVarTypeUid) && (variableSizeInBytes <= 4))
            {
                symbolsToPrepad.push_back(UidWithPrepadType{PrepadType::Float2, uid});
            }

        } // for loop end.

        //! Helper function that returns a string suggesting padding solution
        auto getPaddingSolutionMessage = [&](PrepadType prepadType, IdentifierUID insertBeforeThisUid) -> string
        {
            // We can deduce the name of parent struct, class or SRG from the name of the field that should come AFTER
            // the dummy float2/float3.
            auto parentName = GetParentName(insertBeforeThisUid.GetName());

            // The padding is not needed if the variable @insertBeforeThisUid is the first member
            // of a struct/class/SRG
            KindInfo* parentKindInfo = GetKindInfo({ parentName });
            assert(parentKindInfo);
            const bool isClass = parentKindInfo->IsKindOneOf(Kind::Class);
            const bool isStruct = parentKindInfo->IsKindOneOf(Kind::Struct);
            const bool isSrg = parentKindInfo->IsKindOneOf(Kind::ShaderResourceGroup);
            if (isStruct || isClass)
            {
                auto firstMemberUid = parentKindInfo->GetSubRefAs<ClassInfo>().GetMemberFields()[0];
                if (firstMemberUid == insertBeforeThisUid)
                {
                    return {};
                }
            }
            else if (isSrg)
            {
                auto& srgInfo = parentKindInfo->GetSubRefAs<SRGInfo>();
                auto firstMemberUid = srgInfo.m_implicitStruct.GetMemberFields()[0];
                if (firstMemberUid == insertBeforeThisUid)
                {
                    return {};
                }
            }
            else
            {
                throw SourceError{ "error: Was expecting " + string(parentName) +
                    + " to be struct, class or ShaderResourceGroup" };
            }

            string typeName;
            if (isClass)
            {
                typeName = "class";
            }
            else if (isStruct)
            {
                typeName = "struct";
            }
            else
            {
                typeName = "ShaderResourceGroup";
            }

            // Let's get the line number where @insertBeforeThisUid was found in the flat AZSL file.
            const auto* constThis = this; // To disambiguate which cv-version of GetSymbolSubAs<> to call.
            const auto* varInfo = constThis->GetSymbolSubAs<VarInfo>(insertBeforeThisUid.GetName());
            const SourceLocation source = varInfo->GetOriginalSourceLocation();
            if (!source)
            {
                return {};
            }
            const ResolvedSourceLocation location = source.Resolve();
            string solution = FormatString("- A 'float%d' variable should be added before the variable '%s' in '%s %s' at Line number %zu of '%s'\n",
                                           prepadType == PrepadType::Float2 ? 2 : 3, insertBeforeThisUid.GetNameLeaf().c_str(), typeName.c_str(), parentName.data(),
                                           location.m_line, std::string(location.m_file).c_str());
            return solution;
        };

        string solutionsReport;
        for (const auto& uidAndPrepadType : symbolsToPrepad)
        {
            solutionsReport += getPaddingSolutionMessage(uidAndPrepadType.m_prepadType, uidAndPrepadType.m_uid);
        }

        if (solutionsReport.empty())
        {
            return;
        }

        string errorMessage(
            "Detected potential alignment issues related with DXC flag '-fvk-use-dx-layout'.\n"
            "Alternatively you can use the option '--no-alignment-validation' to void this error and compile as is.:\n");
        errorMessage += solutionsReport;
        throw AzslcIrException(IR_POTENTIAL_DX12_VS_VULKAN_ALIGNMENT_ERROR, errorMessage);
    }

    IdAndKind& IntermediateRepresentation::GetCurrentScopeIdAndKind()
    {
        auto nameOfScope = m_scope.GetNameOfCurScope();
        auto* idAndKindPtr = m_symbols.GetIdAndKindInfo(nameOfScope);
        if (!idAndKindPtr)
        {
            throw std::logic_error("Internal error: current scope not registered");
        }
        return *idAndKindPtr;
    }

    IdentifierUID IntermediateRepresentation::GetLastMemberVariable(const IdentifierUID& uid)
    {
        auto kind = GetKind(uid);
        ClassInfo* classInfo = nullptr; 
        if (kind.IsOneOf(Kind::Struct, Kind::Class))
        {
            classInfo = GetSymbolSubAs<ClassInfo>(uid.GetName());
        }
        else if (kind.IsOneOf(Kind::ShaderResourceGroup))
        {
            auto srgInfo = GetSymbolSubAs<SRGInfo>(uid.GetName());
            classInfo = &srgInfo->m_implicitStruct;
        }

        if (!classInfo)
        {
            return {};
        }

        const auto& memberList = classInfo->GetMemberFields();
        if (memberList.empty())
        {
            return {};
        }
        return memberList[memberList.size() - 1];
    }
}  // end of namespace AZ::ShaderCompiler
