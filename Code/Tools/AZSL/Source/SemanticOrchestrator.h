/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "KindInfo.h"
#include "ScopeTracker.h"
#include "UnboundedArraysValidator.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AZ::ShaderCompiler
{
    //! For argument of RegisterFunction
    enum class AsFunc
    {
        Declaration, Definition,
    };

    //! Deals with jobs that requires access to both Scope and SymbolTable
    struct SemanticOrchestrator
    {
        SemanticOrchestrator(SymbolAggregator* sema, ScopeTracker* scope, azslLexer* lexer);

        //! Helper shortcut: uses the current scope as a starting location to lookup a symbol.
        //! Returns whatever SymbolAggretator's eponymous returns.
        decltype(auto) LookupSymbol(const UnqualifiedNameView name) const
        {
            return m_symbols->LookupSymbol(m_scope->m_currentScopePath, name);
        }

        //! non-const version
        decltype(auto) LookupSymbol(const UnqualifiedNameView name)
        {
            return m_symbols->LookupSymbol(m_scope->m_currentScopePath, name);
        }

        //! Does this symbol resolves to an existing ID from the current scope ?
        //! Helper shortcut: will concatenate your symbol name to current scope before passing to SymbolAggregator
        //! Returns whatever SymbolAggretator's eponymous returns.
        decltype(auto) AddIdentifier(
            const UnqualifiedNameView usym,
            const Kind kind,
            const std::optional<size_t> lineNumber = std::nullopt)
        {
            return m_symbols->AddIdentifier(MakeFullyQualified(usym), kind, lineNumber);
        }

        IdAndKind& GetCurrentScopeIdAndKind();

        IdAndKind& GetCurrentParentScopeIdAndKind();

        //! Shorthand for get< AlternativeType > of the current-scope KindInfo's m_subInfo member.
        template <typename AlternativeType>
        AlternativeType&
        GetCurrentScopeSubInfoAs() noexcept(false)
        {
            static_assert(
                KindInfo::isSubAlternative_v<AlternativeType>,
                "please pass types that belongs to KindInfo::m_subInfo");
            auto& curScope = GetCurrentScopeIdAndKind();
            return curScope.second.GetSubRefAs<AlternativeType>();
        }

        //! Same as above but for the parent scope
        template <typename AlternativeType>
        AlternativeType&
        GetCurrentParentScopeSubInfoAs() noexcept(false)
        {
            static_assert(
                KindInfo::isSubAlternative_v<AlternativeType>,
                "please pass types that belongs to KindInfo::m_subInfo");
            auto& parentScopeIdKind = GetCurrentParentScopeIdAndKind();
            return parentScopeIdKind.second.GetSubRefAs<AlternativeType>();
        }

        //! Execute a class member function (method) definition
        //! @param name         the function name
        //! @param className    the C in C::Method(){}
        IdAndKind& RegisterDeportedMethod(
            UnqualifiedNameView name,
            azslParser::UserDefinedTypeContext* className,
            AstFuncSig* ctx);

        //! Execute a function definition or declaration registration into the intermediate representation.
        //! @param name is UNqualified in this version to allow to simplify the API for callers which don't care about symbols and scopes
        //!             this is notably used by the normal function declaration visitor where only the name is parseable through a simpler Identifier rule.
        IdAndKind& RegisterFunction(UnqualifiedNameView name, AstFuncSig* ctx, AsFunc statementGenre);

        //! Execute a function definition or declaration registration into the intermediate representation.
        //! @param name is qualified in this version, to allow callers to do the scope stitching or lookup and resolution themselves.
        //!             this is notably used by deported method definition registration, where classname C in C::M has to be resolved by the caller
        IdAndKind& RegisterFunction(QualifiedNameView name, AstFuncSig* ctx, AsFunc statementGenre);

        IdAndKind& RegisterFunctionDeclarationAndAddSeenat(UnqualifiedNameView name, AstFuncSig* signature);

        template <typename ContextType>
        IdAndKind&
        RegisterStructuredType(ContextType* ctx, const Kind kind)
        {
            auto idText = ctx->Name->getText();
            size_t line = ctx->Name->getLine();
            verboseCout << line << ": " << Kind::ToStr(kind) << " decl: " << idText << "\n";
            const auto uqNameView = UnqualifiedNameView{idText};
            if (auto* param = ExtractSpecificParent<azslParser::FunctionParamContext>(ctx))
            {
                // because arguments participate in the signature of the function,
                // and the scope of arguments is that very function, we have a catch-22.
                throw AzslcOrchestratorException(
                    ORCHESTRATOR_NO_INLINE_UDT_IN_PARAMETERS,
                    param->start,
                    ConcatString(
                        " introducing new type (",
                        idText,
                        ") within function arguments is ill-formed"));
            }
            // register the new identifier:
            IdAndKind& symbol = AddIdentifier(uqNameView, kind, line);
            auto& [uid, info] = symbol;
            // now fillup what we can about the kindinfo:
            auto& classInfo = info.GetSubRefAs<ClassInfo>();
            classInfo.m_kind = kind;
            classInfo.m_declNodeVt = ctx;

            if (kind == Kind::ShaderResourceGroupSemantic)
            {
                SRGSemanticInfo semanticInfo;
                classInfo.m_subInfo = semanticInfo;
            }

            if (kind == Kind::Enum)
            {
                EnumerationInfo enumInfo;
                auto* enumCtx = dynamic_cast<azslParser::EnumDefinitionContext*>(ctx);
                const auto* scopedCtx = dynamic_cast<azslParser::ScopedEnumContext*>(enumCtx->enumKey());
                enumInfo.m_isScoped = scopedCtx != nullptr;

                // If we decide to support the enum struct|class name : type syntax, the underlying type will come from there
                // The underlying type can be used in SemanticOrchestrator::GetTypeClass(IdentifierUID typeId) later if we need type casting
                // this int is provisional, because in reality it depends on the collective values of enumerators.
                const ExtractedTypeExt enumType = {UnqualifiedNameView("int"), nullptr};
                enumInfo.m_underlyingType = CreateTypeRefInfo(enumType);

                classInfo.m_subInfo = enumInfo;
            }

            // If this is a structure inside an SRG, add it to the map:
            const auto& [scopeId, scopeKind] = GetCurrentScopeIdAndKind();
            if (kind == Kind::Struct && scopeKind.GetKind() == Kind::ShaderResourceGroup)
            {
                // access the class SRGInfo and add a member:
                auto& srgInfo = GetCurrentScopeSubInfoAs<SRGInfo>();
                srgInfo.m_structs.push_back(uid);
            }
            // If nested, register it as a member in its parent
            if (scopeKind.IsKindOneOf(Kind::Struct, Kind::Class, Kind::Interface))
            {
                auto& parentInfo = GetCurrentScopeSubInfoAs<ClassInfo>();
                parentInfo.PushMember(uid, kind);
            }

            return symbol;
        }

        IdAndKind& RegisterTypeAlias(
            std::string_view newIdentifier,
            AstType* existingTypeCtx,
            azslParser::TypeAliasingDefinitionStatementContext* ctx);

        IdAndKind& RegisterSRGSemantic(AstSRGSemanticDeclNode* ctx);

        IdAndKind& RegisterInterface(AstInterfaceDeclNode* ctx);

        IdAndKind& RegisterClass(AstClassDeclNode* ctx);

        IdAndKind& RegisterStruct(AstStructDeclNode* ctx);

        IdAndKind& RegisterEnum(AstEnumDeclNode* ctx);

        IdAndKind* RegisterVar(Token* nameIdentifier, AstUnnamedVarDecl* ctx);

        void RegisterNamelessFunctionParameter(azslParser::FunctionParamContext* ctx);

        void FillOutSrgField(
            const AstNamedVarDecl* ctx,
            VarInfo& varInfo,
            IdentifierUID varUid,
            const ArrayDimensions& arrayDims);

        SamplerStateDesc ExtractSamplerState(AstVarInitializer* ctx);

        IdAndKind& RegisterSRG(AstSRGDeclNode* ctx);

        void RegisterSRGSemanticMember(const AstSRGSemanticMemberDeclNode* ctx);

        void RegisterEnumerator(azslParser::EnumeratorDeclaratorContext* ctx);

        void RegisterBases(azslParser::BaseListContext* ctx);

        void RegisterSeenat(IdAndKind& idPair, const TokensLocation& location);

        //! Will register one seenat for each found symbol in the nested name expression,
        //! `Base::Middle::Leaf` will register `Base`, `Base::Middle` and `Base::Middle::Leaf`
        //! It assumes starting scope is current scope.
        void RegisterSeenat(AstIdExpr* ctx);

        //! Same but for a context withing a member-access-expression where the scope is relative to the scope of the type of LHS.
        void RegisterSeenat(AstIdExpr* ctx, QualifiedNameView startupScope);

        //! Check that the type of LHS expression (in a member access expression context) satisfies well-formed semantics
        //! returns .first==true if is valid, and .first==false if the semantic fails to check.
        //!         .second is the typeof(LHS)
        std::pair<bool, QualifiedName> VerifyLHSExprOfMAExprIsValid(
            const azslParser::MemberAccessExpressionContext* ctx) const;

        //! Resolve the type from the expression, look it up, and verify that it is a kind that may hold sub-members.
        std::pair<bool, QualifiedName> VerifyTypeIsScopeComposable(azslParser::ExpressionContext* typeScope) const;

        //! look up the type, verify that it exists and is a kind that may hold sub-members.
        //! takes supplementary parameters for better verbose or warning diagnostics.
        std::pair<bool, QualifiedName> VerifyTypeIsScopeComposable(
            QualifiedNameView lhsTypeName,
            std::optional<std::string> lhsExpressionText = std::nullopt,
            std::optional<size_t> line = std::nullopt) const;

        //! assemble a type (left) and an idexpr (right) to see if it forms a symbol that exists, and extracts its type.
        QualifiedName ComposeMemberNameWithScopeAndGetType(QualifiedName scopingType, AstIdExpr* rhsMember) const;

        // This family of functions will attempt to evaluate the type of an expression.
        // returns: a typename. take care that unregistered types will probably not be rooted.
        QualifiedName TypeofExpr(azslParser::ExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::ExpressionExtContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::OtherExpressionContext* ctx) const;

        QualifiedName TypeofExpr(AstType* ctx) const;

        QualifiedName TypeofExpr(AstIdExpr* ctx) const;

        QualifiedName TypeofExpr(azslParser::IdentifierExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::MemberAccessExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::FunctionCallExpressionContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::ArrayAccessExpressionContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::ParenthesizedExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::CastExpressionContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::ConditionalExpressionContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::AssignmentExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::NumericConstructorExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::TypeofExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::LiteralExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::LiteralContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::CommaExpressionContext* ctx) const;

        QualifiedName TypeofExpr(const azslParser::PostfixUnaryExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::PrefixUnaryExpressionContext* ctx) const;

        QualifiedName TypeofExpr(azslParser::BinaryExpressionContext* ctx) const;

        //! Parse the AST from a variable declaration and attempt to extract array dimensions integer constants [dim1][dim2]...
        //! Return: <true> on success, <false> otherwise
        bool TryFoldArrayDimensions(const AstUnnamedVarDecl* ctx, ArrayDimensions& arrayDimensions);

        void ValidateClass(azslParser::ClassDefinitionContext* ctx) noexcept(false);

        void ValidateFunctionAndRegisterFamilySeenat(
            azslParser::LeadingTypeFunctionSignatureContext* ctx) noexcept(false);

        // Verify if a symbol overrides a symbol in a base (parent) of the current scope.
        // Currently Azsl can only have interface as parent of classes. And interfaces can only have function signatures.
        // Therefore for now, in practice this function can only be used to check if a method overrides a function in a base interface.
        // Returns: the symbol of the overridden function if found. nullopt if hidingCandidate doesn't hide.
        // Will diagnose-throw if multiple bases have the candidate symbol's name.
        IdAndKind* GetSymbolHiddenInBase(IdentifierUID hidingCandidate);

        void ValidateSrg(azslParser::SrgDefinitionContext* ctx) noexcept(false);

        void ValidateSrgSemantic(azslParser::SrgSemanticContext* ctx) noexcept(false);

        // That function is to get the interpreted numerical value of a literal token.
        // Careful this should be used only on IntegerLiteral tokens (floats will partially parse or throw).
        // Throws: out_of_range in case of overflow/underflow, or invalid_argument for unsupported letters.
        // Hint: The function has no knowledge of upper contexts, so we can hint if we prefer the const resolved as integer or float
        //   (by default it will try to resolve it as integer)
        // Return: either a int32, a uint32 or a float. but not a monostate.
        ConstNumericVal FoldEvalStaticConstExprNumericValue(
            tree::TerminalNode* numericLiteralToken,
            bool hintAsInt = true) const noexcept(false);

        // Extracted method from the AstIntLiteralOrId* overload for more flexibility.
        ConstNumericVal FoldEvalStaticConstExprNumericValue(AstIdExpr* idExp) const;

        // Primitive constant folding system. Evaluate integers in static const variables as much as possible.
        // The returned value can hold any of 3 types:
        //  - an empty monostate (no evaluable value detected, could be: unsupported expression, float, overflow, or non static-const..)
        //  - signed int64
        //  - unsigned int64
        ConstNumericVal FoldEvalStaticConstExprNumericValue(const VarInfo& varInfo) const;

        ConstNumericVal FoldEvalStaticConstExprNumericValue(AstExpr* expr) const;

        // Create an absolute path symbol for a new name, in the context of the current scope.
        QualifiedName MakeFullyQualified(UnqualifiedNameView unqualifiedName) const;

        // helper to query a type class from an ID
        TypeClass GetTypeClass(IdentifierUID typeId) const;

        enum class OnNotFoundOrWrongKind
        {
            Diagnose,
            Empty,
            CopeByCopy,
        };

        //! Shorthand for symbol lookup, but with supplementary checks, specifics to types.
        //! throws if: the found symbol is not a type, or no found symbol and policy is Diagnose.
        IdentifierUID LookupType(
            UnqualifiedNameView typeName,
            OnNotFoundOrWrongKind policy = OnNotFoundOrWrongKind::CopeByCopy,
            std::optional<size_t> sourceline = std::nullopt) const noexcept(false);

        //! Find and return a registered type from an AST node. Will also resolve typeof expressions.
        //! could work from any sort of context that has an ExtractTypeNameFromAstContext override
        template <typename TypeCtx>
        IdentifierUID LookupType(
            TypeCtx* ctx,
            const OnNotFoundOrWrongKind policy = OnNotFoundOrWrongKind::CopeByCopy) const
        {
            IdentifierUID typeRef;
            UnqualifiedName uqName;
            AstTypeofNode* typeofCtx = ExtractTypeofAstNode(ctx);
            if (typeofCtx)
            {
                uqName = UnqualifiedName{TypeofExpr(typeofCtx)};
            }
            else
            {
                auto core = ExtractComposedTypeNamesFromAstContext(ctx).m_core;
                // the concept here, is to activate recursive lookup in case we have nested generics and/or typeof, in any combination.
                // in practice for now, it will never recurse;
                // because only generic parameters may be further AstNodes. And since we ignore them for now (collapsing behavior), we're set.
                uqName = core.m_name;
                if (core.m_node)
                {
                    uqName = UnqualifiedName{LookupType(core.m_node, policy).m_name};
                }
            }
            const IdAndKind* idkind = LookupSymbol(uqName);
            if (idkind)
            {
                if (idkind->second.GetKind() == Kind::TypeAlias)
                {
                    // recurse until not an alias!
                    typeRef = LookupType(UnqualifiedNameView{GetTypeName(idkind)}, policy);
                }
                else
                {
                    // found core type (UDT or collapsed predefined)
                    typeRef = idkind->first;
                }
            }
            else
            {
                //! otherwise, we need to do damage control.
                // TODO: if it is a matrix<float,3,4> it needs to be canonicalized to float3x4
                if (policy == OnNotFoundOrWrongKind::Diagnose)
                {
                    throw std::runtime_error((DiagLine(ctx->start) + uqName + " is not a supported type expression ?").c_str());
                }
                PrintWarning(Warn::W3, ctx->start, "unidentified name ", uqName, " in type expression");
                if (policy == OnNotFoundOrWrongKind::CopeByCopy)
                {
                    typeRef.m_name = QualifiedName{uqName}; // try to shove it, if we defend enough in code downstream this could end up working.
                }
            }
            return typeRef;
        }

        // lookup the symbol database for a type of a given name (or discover the name through an Ast context) and compose a TypeRefInfo
        // ArgumentType maybe UnqualifiedNameView or a AstType
        template <typename ContextOrNameT>
        TypeRefInfo CreateTypeRefInfo(
            ContextOrNameT typeNameOrCtx,
            OnNotFoundOrWrongKind policy = OnNotFoundOrWrongKind::CopeByCopy) const
        {
            auto typeId = LookupType(typeNameOrCtx, policy);
            auto tClass = GetTypeClass(typeId);
            ArithmeticTraits arithmetic{};
            if (IsNonGenericArithmetic(tClass))
            {
                arithmetic = CreateArithmeticTraits(typeId.GetName());
            }
            return TypeRefInfo{typeId, arithmetic, tClass};
        }

        // shortcut helper, when we have a node ast, we use it in priority thanks to all the superior access it provides (for typeof)
        TypeRefInfo CreateTypeRefInfo(const ExtractedTypeExt& extractedType) const
        {
            if (extractedType.m_node)
            {
                return CreateTypeRefInfo(extractedType.m_node);
            }

            return CreateTypeRefInfo(extractedType.m_name);
        }

        // Just a helper function to compose the bigger version, that contains more data that can't be stored in the core type (TypeRefInfo).
        // Array dimensions are usually stored in VarInfo for example. If you have a custom way to discover them, use this helper to make your own ExtendedTypeInfo
        ExtendedTypeInfo CreateExtendedTypeInfo(AstType* ctx, ArrayDimensions dims) const;

        // Helper func which folds any possible generic dimensions into the extracted composed type
        bool TryFoldGenericArrayDimensions(
            ExtractedComposedType& extType,
            const std::vector<tree::TerminalNode*>& genericDims) const;

        // another helper to streamline what to do directly with the result from ExtractTypeNameFromAstContext function families.
        ExtendedTypeInfo CreateExtendedTypeInfo(
            const ExtractedComposedType&,
            const TypeQualifiers&,
            ArrayDimensions) const;

        //! check if current scope is a structured user defined type ("struct", "class" or "interface")
        bool IsScopeStructClassInterface() const;

        //! check if current scope is of type "struct".
        bool IsScopeStruct() const;

        //! Find a concrete function from an overload-set and an argument list.
        //! (adjust the shoot of a lookup to something more precise in case that it's possible)
        //! if maybeOverloadSet is not an overload-set the function returns identity
        IdAndKind* ResolveOverload(IdAndKind* maybeOverloadSet, azslParser::ArgumentListContext* argumentListCtx) const;

        //! Generate a unique name, create a corresponding namespace symbol, and enter its scope
        void MakeAndEnterAnonymousScope(std::string_view decorationPrefix, const Token* scopeFirstToken);

    private:
        //! for internal use when encountering unresolved symbols by lookup.
        void DiagnoseUndeclaredSub(
            Token* atToken,
            QualifiedNameView startupScope,
            std::string partialName) const;

        std::optional<int64_t> TryFoldSRGSemantic(
            const azslParser::SrgSemanticContext* ctx,
            size_t semanticTokenType,
            bool required = false);

        std::string CreateDecorationOfFunction(azslParser::FunctionParamsContext* parametersContext) const;

        QualifiedName CreateDecoratedIdentityOfFunction(
            QualifiedNameView name,
            azslParser::FunctionParamsContext* parametersContext) const;

        //! e.g. provided "int a; X GetX(int);" then from expression "(a, GetX(2), true)" MangleArgumentList returns "(?int,/X,?bool)"
        std::string MangleArgumentList(azslParser::ArgumentListContext* ctx) const;

        //! queries whether a function has default parameters
        bool HasAnyDefaultParameterValue(const IdentifierUID& functionUid) const;

    public:
        SymbolAggregator* m_symbols;
        ScopeTracker* m_scope;
        azslLexer* m_lexer;
        UnboundedArraysValidator m_unboundedArraysValidator;

    private:
        // remember frequencyId to srg association, for semantic validation
        std::unordered_map<int64_t, IdentifierUID> m_frequencyToSrg;
        int m_anonymousCounter;
    };
}
