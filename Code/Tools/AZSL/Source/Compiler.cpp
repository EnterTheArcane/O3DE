/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AZSL/Compiler.h>

#include "CompilerContext.h"
#include "Emitter.h"
#include "Exception.h"
#include "HomonymVisitor.h"
#include "PlatformEmitter.h"
#include "Reflection.h"
#include "SubpassInputToTexture2DCodeMutator.h"
#include "Texture2DMSto2DCodeMutator.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <format>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace AZ::ShaderCompiler
{
    namespace
    {
        thread_local CompilationContext* s_activeCompilationContext = nullptr;

        using MapOfStringViewToSetOfString = std::map<std::string_view, std::set<std::string>>;

        template <typename SetType>
        void SetMerge(SetType& destination, SetType& source)
        {
            for (auto iterator = source.begin(); iterator != source.end();)
            {
                if (destination.find(*iterator) == destination.end())
                {
                    destination.insert(std::move(*iterator));
                    iterator = source.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }
        }

        template <typename TypeClassFilterPredicate = std::nullptr_t>
        void VisitTokens(
            const antlr4::Recognizer* recognizer,
            MapOfStringViewToSetOfString& acceptedTokens,
            std::set<std::string>& nonTypes,
            TypeClassFilterPredicate filter = nullptr)
        {
            const auto& vocabulary = recognizer->getVocabulary();
            const size_t maxToken = vocabulary.getMaxTokenType();
            for (size_t index = 0; index < maxToken; ++index)
            {
                auto tokenName = vocabulary.getLiteralName(index);
                std::string_view token(tokenName.data(), tokenName.size());
                token = Trim(token, "\"'");
                if (token.empty())
                {
                    continue;
                }

                TypeClass typeClass = AnalyzeTypeClass(TentativeName{std::string{token}});
                bool accept = true;
                if constexpr (!std::is_same_v<std::nullptr_t, std::remove_reference_t<decltype(filter)>>)
                {
                    accept = filter(typeClass);
                }

                if (typeClass == TypeClass::IsNotType)
                {
                    nonTypes.insert(std::string{token});
                }

                if (accept)
                {
                    acceptedTokens[TypeClass::ToStr(typeClass)].emplace(token);
                }
            }
        }

        template <typename FilterFunction>
        void ClassifyAllTokens(
            const azslLexer* lexer,
            MapOfStringViewToSetOfString& classifiedTokens,
            FilterFunction filter)
        {
            std::set<std::string> firstPassNonTypes;
            VisitTokens(lexer, classifiedTokens, firstPassNonTypes, filter);

            const std::string someScalar = *classifiedTokens[TypeClass::ToStr(TypeClass::Scalar)].begin();
            enum class RetryState
            {
                OneTypeGenericParameter,
                OneTypeAndDimensionGenericParameters,
                OneTypeAndTwoDimensionsGenericParameters,
                End,
            };

            constexpr auto isNotTypeKey = TypeClass::ToStr(TypeClass::IsNotType);
            std::set<std::string> nonTypes = std::move(classifiedTokens[isNotTypeKey]);
            classifiedTokens.erase(isNotTypeKey);
            SetMerge(nonTypes, firstPassNonTypes);

            for (const std::string& token : nonTypes)
            {
                RetryState state = RetryState::OneTypeGenericParameter;
                TypeClass typeClass;
                do
                {
                    std::string attemptedTypeName = token;
                    switch (state)
                    {
                    case RetryState::OneTypeGenericParameter:
                        attemptedTypeName += "<" + someScalar + ">";
                        state = RetryState::OneTypeAndDimensionGenericParameters;
                        break;
                    case RetryState::OneTypeAndDimensionGenericParameters:
                        attemptedTypeName += "<" + someScalar + ",1>";
                        state = RetryState::OneTypeAndTwoDimensionsGenericParameters;
                        break;
                    case RetryState::OneTypeAndTwoDimensionsGenericParameters:
                        attemptedTypeName += "<" + someScalar + "1,1>";
                        state = RetryState::End;
                        break;
                    case RetryState::End:
                        break;
                    }
                    typeClass = AnalyzeTypeClass(TentativeName{attemptedTypeName});
                } while (typeClass == TypeClass::IsNotType && state != RetryState::End);

                bool accept = true;
                if constexpr (!std::is_same_v<std::nullptr_t, std::remove_reference_t<decltype(filter)>>)
                {
                    accept = filter(typeClass);
                }
                if (accept)
                {
                    classifiedTokens[TypeClass::ToStr(typeClass)].insert(token);
                }
            }
        }

        bool IsKeyword(const antlr4::Recognizer* recognizer, const antlr4::Token* token)
        {
            if (!recognizer || !token || token->getText().empty())
            {
                return false;
            }

            const std::string tokenText = token->getText();
            const std::string_view literalName = recognizer->getVocabulary().getLiteralName(token->getType());
            const std::string_view literalText = Trim(literalName, "\"'");
            const bool isLiteral = literalText == tokenText;
            const bool isNotIdentifier = recognizer->getVocabulary().getSymbolicName(token->getType()) != "Identifier";
            const bool startsWithLetter = std::isalpha(static_cast<unsigned char>(tokenText[0])) != 0;
            return isLiteral && isNotIdentifier && startsWithLetter;
        }

        void ConstructLineMap(
            std::vector<std::unique_ptr<antlr4::Token>>& allTokens,
            PreprocessorLineDirectiveFinder& lineFinder)
        {
            std::string lastNonEmptyFileName = lineFinder.m_physicalSourceFileName;
            for (auto& token : allTokens)
            {
                if (token->getType() != azslLexer::LineDirective)
                {
                    continue;
                }

                LineDirectiveInfo directiveInfo{0, 0};
                const auto lineText = token->getText();
                const std::regex lineRegex(R"__(#\s*(line\s+)?\s*(\d+)\s*("(.*)")?)__");
                const auto matchBegin = std::sregex_iterator(lineText.begin(), lineText.end(), lineRegex);
                const auto& groups = *matchBegin;
                directiveInfo.m_physicalTokenLine = token->getLine();
                directiveInfo.m_forcedLineNumber = std::atoi(groups[2].str().c_str());
                directiveInfo.m_containingFilename = groups[4].str();
                if (directiveInfo.m_containingFilename.empty())
                {
                    directiveInfo.m_containingFilename = lastNonEmptyFileName;
                }
                else
                {
                    lastNonEmptyFileName = directiveInfo.m_containingFilename;
                }
                lineFinder.PushLineDirective(std::move(directiveInfo));
            }

            if (lineFinder.m_lineMap.find(1) == lineFinder.m_lineMap.end())
            {
                lineFinder.PushLineDirective({0, 1, lineFinder.m_physicalSourceFileName});
            }
        }

        std::string PrintAst(antlr4::tree::ParseTree* tree, azslParser& parser)
        {
            std::ostringstream output;
            const std::string treeText = tree->toStringTree(&parser);
            std::string indentation;
            for (const char character : treeText)
            {
                if (character == '(')
                {
                    output << '\n';
                    indentation += "  ";
                    output << indentation << character;
                }
                else if (character == ')')
                {
                    output << character << '\n';
                    indentation = indentation.substr(0, std::max(2_sz, indentation.size()) - 2);
                    output << indentation;
                }
                else
                {
                    output << character;
                }
            }
            output << std::endl;
            return output.str();
        }

        std::string PrintVisitSymbol(
            IntermediateRepresentation& ir,
            std::string_view symbolName,
            const RelationshipExtentFlag visitOptions)
        {
            std::ostringstream output;
            const IdAndKind* symbol = ir.GetIdAndKindInfo(QualifiedNameView{symbolName});
            if (!symbol)
            {
                GetCompilationContext().m_errorOutput
                    << "Error: symbol " << symbolName
                    << " not found. To list all symbols use --dumpsym option.\n";
                return {};
            }

            output << "Symbol found. kind: " << Kind::ToStr(ir.GetKind(symbol->first)) << ". Homonyms list:\n";
            const HomonymVisitor visitor{
                [&ir](QualifiedNameView name)
                {
                    return ir.GetKindInfo({{name}});
                }};
            visitor(
                symbol->first,
                [&output](const Seenat& location, const RelationshipExtent category)
                {
                    output << "- {categ: " << RelationshipExtent::ToStr(category)
                           << ", id: " << Decorate("'", location.m_referredDefinition.GetName())
                           << ", at: ':" << location.m_where.m_line << ":" << location.m_where.m_charPos + 1 << "'"
                           << ", token#: " << location.m_where.m_focusedTokenId << "}\n";
                },
                visitOptions);
            return output.str();
        }

        bool HasArtifact(const CompileOptions& options, ArtifactType type)
        {
            return std::find(options.m_artifacts.begin(), options.m_artifacts.end(), type) != options.m_artifacts.end();
        }

        Warn ToInternalWarningLevel(WarningLevel level)
        {
            switch (level)
            {
            case WarningLevel::Suppress:
                return Warn::W0;
            case WarningLevel::Level2:
                return Warn::W2;
            case WarningLevel::Level3:
                return Warn::W3;
            case WarningLevel::Level1:
            default:
                return Warn::W1;
            }
        }

        Warn ToInternalWarningsAsErrors(WarningsAsErrors level)
        {
            switch (level)
            {
            case WarningsAsErrors::EnabledWarnings:
                return Warn::Wx;
            case WarningsAsErrors::Level1:
                return Warn::Wx1;
            case WarningsAsErrors::Level2:
                return Warn::Wx2;
            case WarningsAsErrors::Level3:
                return Warn::Wx3;
            case WarningsAsErrors::Disabled:
            default:
                return Warn::EndEnumeratorSentinel_;
            }
        }

        Options MakeEmissionOptions(const CompileOptions& options)
        {
            Options result;
            result.m_useUniqueIndices = options.m_useUniqueIndices;
            result.m_emitConstantBufferBody = options.m_emitConstantBufferBody;
            result.m_emitRootSig = options.m_emitRootSignature;
            result.m_padRootConstantCB = options.m_padRootConstantBuffer;
            result.m_skipAlignmentValidation = options.m_skipAlignmentValidation;
            result.m_useSpecializationConstantsForOptions = options.m_useSpecializationConstants;
            result.m_subpassInputsOffset = options.m_subpassInputOffset;

            if (options.m_rootConstantsMaxSize)
            {
                result.m_rootConstantsMaxSize = *options.m_rootConstantsMaxSize;
            }
            if (options.m_maxSpaces)
            {
                result.m_maxSpaces = *options.m_maxSpaces;
            }
            if (options.m_minDescriptorLimits)
            {
                result.m_minAvailableDescriptors.m_descriptorsTotal = options.m_minDescriptorLimits->m_descriptorsTotal;
                result.m_minAvailableDescriptors.m_spaces = options.m_minDescriptorLimits->m_spaces;
                result.m_minAvailableDescriptors.m_samplers = options.m_minDescriptorLimits->m_samplers;
                result.m_minAvailableDescriptors.m_textures = options.m_minDescriptorLimits->m_textures;
                result.m_minAvailableDescriptors.m_buffers = options.m_minDescriptorLimits->m_buffers;
            }

            if (options.m_matrixOrder != MatrixOrder::Default)
            {
                result.m_forceEmitMajor = true;
                result.m_forceMatrixRowMajor = options.m_matrixOrder == MatrixOrder::RowMajor;
            }

            switch (options.m_packing)
            {
            case PackingMode::DirectX:
                result.m_packConstantBuffers = Packing::Layout::DirectXPacking;
                result.m_packDataBuffers = Packing::Layout::DirectXStoragePacking;
                break;
            case PackingMode::Vulkan:
                result.m_packConstantBuffers = Packing::Layout::RelaxedStd140Packing;
                result.m_packDataBuffers = Packing::Layout::RelaxedStd430Packing;
                break;
            case PackingMode::OpenGL:
                result.m_packConstantBuffers = Packing::Layout::StrictStd140Packing;
                result.m_packDataBuffers = Packing::Layout::StrictStd430Packing;
                break;
            case PackingMode::Default:
                break;
            }

            return result;
        }

        void AppendWarningDiagnostics(std::string_view renderedWarnings, CompileResult& result)
        {
            if (renderedWarnings.empty())
            {
                return;
            }

            const std::regex warningPattern(R"(^(.*)\(([^,]*),([^)]*)\) : (.*) warning(?: #([0-9]+))?: (.*)$)");
            std::istringstream lines{std::string{renderedWarnings}};
            std::string line;
            while (std::getline(lines, line))
            {
                if (line.empty())
                {
                    continue;
                }

                Diagnostic diagnostic;
                diagnostic.m_severity = DiagnosticSeverity::Warning;
                diagnostic.m_renderedText = line;
                std::smatch match;
                if (std::regex_match(line, match, warningPattern))
                {
                    diagnostic.m_sourcePath = match[1].str();
                    if (!match[2].str().empty())
                    {
                        diagnostic.m_line = static_cast<size_t>(std::stoull(match[2].str()));
                    }
                    if (!match[3].str().empty())
                    {
                        diagnostic.m_column = static_cast<size_t>(std::stoull(match[3].str()));
                    }
                    diagnostic.m_category = match[4].str();
                    if (!match[5].str().empty())
                    {
                        diagnostic.m_code = static_cast<uint32_t>(std::stoul(match[5].str()));
                    }
                    diagnostic.m_message = match[6].str();
                }
                else
                {
                    diagnostic.m_message = line;
                }
                result.m_diagnostics.push_back(std::move(diagnostic));
            }
        }

        void AppendExceptionDiagnostic(const AzslcException& exception, CompileResult& result)
        {
            Diagnostic diagnostic;
            diagnostic.m_severity = DiagnosticSeverity::Error;
            diagnostic.m_code = exception.GetErrorCode();
            diagnostic.m_category = exception.GetErrorType();
            diagnostic.m_message = exception.GetErrorMessage();
            diagnostic.m_sourcePath = exception.GetSourceFileName();
            diagnostic.m_line = exception.GetSourceLine();
            diagnostic.m_column = exception.GetSourceColumn();
            diagnostic.m_renderedText = exception.what();
            result.m_diagnostics.push_back(std::move(diagnostic));
        }
    } // namespace

    ScopedCompilationContext::ScopedCompilationContext(CompilationContext& context)
        : m_previousContext(s_activeCompilationContext)
    {
        s_activeCompilationContext = &context;
    }

    ScopedCompilationContext::~ScopedCompilationContext()
    {
        s_activeCompilationContext = m_previousContext;
    }

    CompilationContext& GetCompilationContext()
    {
        assert(s_activeCompilationContext);
        return *s_activeCompilationContext;
    }

    DiagnosticStream& GetVerboseStream()
    {
        return GetCompilationContext().m_verboseStream;
    }

    DiagnosticStream& GetWarningStream()
    {
        return GetCompilationContext().m_warningStream;
    }

    std::ostream& GetInformationalStream()
    {
        return GetCompilationContext().m_informationalOutput;
    }

    PreprocessorLineDirectiveFinder& GetLineDirectiveFinder()
    {
        return GetCompilationContext().m_lineFinder;
    }

    std::string_view Compiler::GetVersion()
    {
        return "1.9.0";
    }

    std::string Compiler::GetVersionString()
    {
        const auto osName = GetCurrentOsName();
        return std::format(
            "AZSL Compiler {} {}",
            GetVersion(),
            std::string_view(osName.data(), osName.size()));
    }

    std::string Compiler::GetPredefinedVocabulary()
    {
        CompilationContext context{"<vocabulary>"};
        ScopedCompilationContext scopedContext{context};
        antlr4::ANTLRInputStream input;
        azslLexer lexer{&input};
        MapOfStringViewToSetOfString classifiedTokens;
        ClassifyAllTokens(
            &lexer,
            classifiedTokens,
            [](const TypeClass typeClass)
            {
                return IsPredefinedType(typeClass);
            });

        std::ostringstream output;
        for (const auto& [category, tokens] : classifiedTokens)
        {
            output << category << ":\n";
            for (const std::string& token : tokens)
            {
                output << "  - \"" << token << "\"\n";
            }
        }
        return output.str();
    }

    std::vector<std::string> Compiler::GetRegisteredPlatformEmitters()
    {
        RegisterPlatformEmitters();
        std::vector<std::string> result;
        for (const char* name : {"dx", "vk", "mt"})
        {
            if (PlatformEmitter::GetEmitter(name))
            {
                result.emplace_back(name);
            }
        }
        return result;
    }

    CompileResult Compiler::Compile(const CompileRequest& request) const
    {
        CompileResult result;
        CompilationContext context{request.m_sourcePath};
        ScopedCompilationContext scopedContext{context};
        context.m_verboseStream.m_on = request.m_options.m_verbose;
        context.m_warningStream.SetRevealedWarningLevel(ToInternalWarningLevel(request.m_options.m_warningLevel));
        const Warn warningsAsErrors = ToInternalWarningsAsErrors(request.m_options.m_warningsAsErrors);
        if (warningsAsErrors != Warn::EndEnumeratorSentinel_)
        {
            context.m_warningStream.SetAsErrorLevel(warningsAsErrors);
        }
        context.m_warningStream.m_onErrorCallback = [](const std::string_view message)
        {
            throw AzslcException{WX_WARNINGS_AS_ERRORS, "as-error", std::string{message}};
        };

        try
        {
            if (request.m_options.m_emitRootSignature && request.m_options.m_attributeNamespaces.empty())
            {
                throw std::runtime_error("--root-sig requested but no API was selected. Use a --namespace option as well.");
            }

            RegisterPlatformEmitters();

            auto& lineFinder = context.m_lineFinder;
            lineFinder.PushLineDirective({0, 1, lineFinder.m_physicalSourceFileName});

            antlr4::ANTLRInputStream input{request.m_source};
            azslLexer lexer{&input};
            AzslParserEventListener parserEventListener;
            parserEventListener.m_isKeywordPredicate = IsKeyword;
            lexer.removeErrorListeners();
            lexer.addErrorListener(&parserEventListener);
            antlr4::CommonTokenStream tokens{&lexer};
            IntermediateRepresentation ir{&lexer};
            auto allTokens = lexer.getAllTokens();
            ConstructLineMap(allTokens, lineFinder);
            lexer.reset();

            azslParser parser{&tokens};
            parser.removeErrorListeners();
            parser.addErrorListener(&parserEventListener);
            antlr4::tree::ParseTree* syntaxTree = parser.compilationUnit();

            if (HasArtifact(request.m_options, ArtifactType::SyntaxTree))
            {
                result.m_artifacts.push_back({ArtifactType::SyntaxTree, PrintAst(syntaxTree, parser)});
            }

            if (request.m_options.m_phase == CompilationPhase::Syntax)
            {
                result.m_succeeded = true;
            }
            else
            {
                ir.m_metaData.m_insource = request.m_sourcePath.empty() ? "<memory>" : std::string{request.m_sourcePath};
                for (const std::string& attributeNamespace : request.m_options.m_attributeNamespaces)
                {
                    ir.AddAttributeNamespaceFilter(attributeNamespace);
                }

                SubpassInputSupportFlag subpassInputSupport = Backend::GetPlatformEmitter(&ir).GetSubpassInputSupport();
                if (request.m_options.m_removeSubpassInputs)
                {
                    subpassInputSupport = SubpassInputSupportFlag::None;
                }

                antlr4::tree::ParseTreeWalker walker;
                Texture2DMSto2DCodeMutator textureMutator{&ir, &tokens};
                SubpassInputToTexture2DCodeMutator subpassMutator{&ir, &tokens, subpassInputSupport};
                SemaCheckListener semanticListener{&ir};

                const bool hasDeveloperArtifact = std::any_of(
                    request.m_options.m_artifacts.begin(),
                    request.m_options.m_artifacts.end(),
                    [](ArtifactType type)
                    {
                        return type != ArtifactType::Shader;
                    });
                context.m_warningStream.m_on = !hasDeveloperArtifact && !request.m_options.m_stripUnusedShaderResourceGroups;
                semanticListener.m_silentPrintExtensions =
                    request.m_options.m_phase != CompilationPhase::Semantic || request.m_options.m_verbose;
                if (request.m_options.m_removeMultisampling)
                {
                    semanticListener.m_functionCallMutators.push_back(&textureMutator);
                }
                semanticListener.m_functionCallMutators.push_back(&subpassMutator);

                UnboundedArraysValidator::Options validatorOptions;
                validatorOptions.m_useUniqueIndicesEnabled = request.m_options.m_useUniqueIndices;
                if (request.m_options.m_maxSpaces)
                {
                    validatorOptions.m_maxSpaces = *request.m_options.m_maxSpaces;
                }
                ir.m_sema.m_unboundedArraysValidator.SetOptions(validatorOptions);
                walker.walk(&semanticListener, syntaxTree);

                Options emissionOptions = MakeEmissionOptions(request.m_options);
                MiddleEndConfiguration middleEndConfiguration{
                    emissionOptions.m_rootConstantsMaxSize,
                    emissionOptions.m_packConstantBuffers,
                    emissionOptions.m_packDataBuffers,
                    emissionOptions.m_forceMatrixRowMajor,
                    emissionOptions.m_padRootConstantCB,
                    emissionOptions.m_skipAlignmentValidation,
                };
                ir.MiddleEnd(middleEndConfiguration, &lineFinder);
                if (request.m_options.m_removeMultisampling)
                {
                    textureMutator.RunMiddleEndMutations();
                }
                const bool hasSubpassInputMutations = subpassMutator.RunMiddleEndMutations();
                if (ir.m_rootConstantStructUID.m_name.empty())
                {
                    emissionOptions.m_rootConstantsMaxSize = 0;
                }
                ir.Validate();

                if (request.m_options.m_stripUnusedShaderResourceGroups)
                {
                    ir.RemoveUnusedSrgs();
                }

                if (HasArtifact(request.m_options, ArtifactType::Symbols))
                {
                    std::ostringstream output;
                    DumpSymbols(ir, output);
                    result.m_artifacts.push_back({ArtifactType::Symbols, output.str()});
                }

                if (HasArtifact(request.m_options, ArtifactType::SymbolRelationships))
                {
                    using Relationship = RelationshipExtent;
                    RelationshipExtentFlag visitOptions{Relationship::Self};
                    if (request.m_options.m_visitDirectReferences)
                    {
                        visitOptions |= Relationship::Reference;
                    }
                    if (request.m_options.m_visitFamily)
                    {
                        visitOptions |= Relationship::Family;
                    }
                    if (request.m_options.m_visitOverloadSet)
                    {
                        visitOptions |= Relationship::OverloadSet;
                    }
                    if (request.m_options.m_visitRecursively)
                    {
                        visitOptions |= Relationship::Recursive;
                    }
                    result.m_artifacts.push_back(
                        {ArtifactType::SymbolRelationships,
                         PrintVisitSymbol(ir, request.m_options.m_symbolToVisit, visitOptions)});
                }

                if (request.m_options.m_phase == CompilationPhase::Emit)
                {
                    GetVerboseStream() << "--Emission/Reflection--\n";
                    for (ArtifactType artifactType : request.m_options.m_artifacts)
                    {
                        if (artifactType == ArtifactType::SyntaxTree ||
                            artifactType == ArtifactType::Symbols ||
                            artifactType == ArtifactType::SymbolRelationships)
                        {
                            continue;
                        }

                        std::ostringstream output;
                        if (artifactType == ArtifactType::Shader)
                        {
                            CodeEmitter emitter{&ir, &tokens, output, &lineFinder};
                            if (request.m_options.m_removeMultisampling)
                            {
                                emitter.AddCodeMutator(&textureMutator);
                            }
                            if (hasSubpassInputMutations)
                            {
                                emitter.AddCodeMutator(&subpassMutator);
                            }
                            emitter << "// HLSL emission by " << GetVersionString() << "\n";
                            emitter.Run(emissionOptions);
                        }
                        else
                        {
                            CodeReflection reflector{&ir, &tokens, output};
                            switch (artifactType)
                            {
                            case ArtifactType::InputAssembler:
                                reflector.DumpShaderEntries();
                                break;
                            case ArtifactType::OutputMerger:
                                reflector.DumpOutputMergerLayout();
                                break;
                            case ArtifactType::ShaderResourceGroup:
                                reflector.DumpSRGLayout(emissionOptions, &lineFinder);
                                break;
                            case ArtifactType::Options:
                                reflector.DumpVariantList(emissionOptions);
                                break;
                            case ArtifactType::BindingDependencies:
                                reflector.DumpResourceBindingDependencies(emissionOptions);
                                break;
                            default:
                                break;
                            }
                        }
                        result.m_artifacts.push_back({artifactType, output.str()});
                    }
                }

                result.m_succeeded = true;
            }
        }
        catch (const AzslcException& exception)
        {
            AppendExceptionDiagnostic(exception, result);
            std::ostringstream output;
            OutputNestedException(exception, output);
            context.m_errorOutput << output.str();
        }
        catch (const std::exception& exception)
        {
            Diagnostic diagnostic;
            diagnostic.m_severity = DiagnosticSeverity::Error;
            diagnostic.m_message = exception.what();
            diagnostic.m_sourcePath = context.m_lineFinder.m_physicalSourceFileName;
            diagnostic.m_renderedText = exception.what();
            result.m_diagnostics.push_back(std::move(diagnostic));
            OutputNestedException(exception, context.m_errorOutput);
        }
        catch (...)
        {
            Diagnostic diagnostic;
            diagnostic.m_severity = DiagnosticSeverity::Error;
            diagnostic.m_message = "Unknown exception";
            diagnostic.m_sourcePath = context.m_lineFinder.m_physicalSourceFileName;
            diagnostic.m_renderedText = "Unknown exception";
            result.m_diagnostics.push_back(std::move(diagnostic));
            context.m_errorOutput << "Unknown exception\n";
        }

        result.m_informationalOutput = context.m_informationalOutput.str();
        const std::string warningOutput = context.m_warningOutput.str();
        result.m_legacyDiagnostics = warningOutput + context.m_errorOutput.str();
        AppendWarningDiagnostics(warningOutput, result);
        return result;
    }
} // namespace AZ::ShaderCompiler
