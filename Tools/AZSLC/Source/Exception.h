/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "GenericUtils.h"
#include "PreprocessorLineDirectiveFinder.h"

#include "antlr4-runtime.h"

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

using namespace antlr4;
using namespace antlrcpp;

// NOTE: number the enum entries for quicker look-up
namespace AZ::ShaderCompiler
{
    enum AzslcErrorCode : uint16_t
    {
        PARSER_SYNTAX_ERROR = 1u,

        // orchestrator error codes
        ORCHESTRATOR_DEPORTED_METHOD_DEFINITION = 2u,
        ORCHESTRATOR_DEFINITION_FOREIGN_SCOPE = 3u,
        ORCHESTRATOR_NO_DECLERATION = 4u,
        ORCHESTRATOR_UNEXPECTED_KIND = 5u,
        ORCHESTRATOR_OVERLY_QUALIFIED = 6u,
        ORCHESTRATOR_DEPORTED_METHOD = 7u,
        ORCHESTRATOR_FUNCTION_ALREADY_DEFINED = 8u,
        ORCHESTRATOR_INVALID_INLINED_QUALIFIER = 9u,
        ORCHESTRATOR_INVALID_NONGLOBAL_OPTION_OR_ROOTCONSTANT = 11u,
        ORCHESTRATOR_ILLEGAL_GLOBAL_VARIABLE = 12u,
        ORCHESTRATOR_ILLEGAL_MEMBER_VARIABLE_IN_INTERFACE = 13u,
        ORCHESTRATOR_ILLEGAL_FOLDABLE_ARRAY_DIMENSIONS = 14u,
        ORCHESTRATOR_INVALID_QUALIFIER_MIX = 15u,
        ORCHESTRATOR_UNSPECIFIED_BASE_SYMBOL = 16u,
        ORCHESTRATOR_INVALID_INTERFACE = 17u,
        ORCHESTRATOR_CLASS_REDEFINE = 18u,
        ORCHESTRATOR_UNREGISTERED_METHOD = 19u,
        ORCHESTRATOR_HIDING_SYMBOL_BASE = 20u,
        ORCHESTRATOR_INVALID_OVERRIDE_SPECIFIER_CLASS = 21u,
        ORCHESTRATOR_INVALID_OVERRIDE_SPECIFIER_BASE = 22u,
        ORCHESTRATOR_INVALID_SEMANTIC_DECLARATION = 23u,
        ORCHESTRATOR_INVALID_SEMANTIC_DECLARATION_TYPE = 24u,
        ORCHESTRATOR_INVALID_EXTERNAL_BOUND_RESOURCE_VIEW = 25u,
        ORCHESTRATOR_INVALID_GENERIC_TYPE_CONSTANTBUFFER = 26u,
        ORCHESTRATOR_UNDECLARED_GENERIC_TYPE_CONSTANTBUFFER = 27u,
        ORCHESTRATOR_INVALID_GENERIC_TYPE_CONSTANTBUFFER_STRUCT = 28u,
        ORCHESTRATOR_LITERAL_REQUIRED_SRG_SEMANTIC = 29u,
        ORCHESTRATOR_INVALID_INTEGER_CONSTANT = 30u,
        ORCHESTRATOR_INVALID_RANGE_FREQUENCY_ID = 31u,
        // One Definition Rule
        ORCHESTRATOR_ODR_VIOLATION = 32u,
        ORCHESTRATOR_DISALLOWED_FUNCTION_MODIFIER = 33u,
        ORCHESTRATOR_MULTIPLE_HIDDEN_SYMBOLS = 34u,
        ORCHESTRATOR_SCOPE_NOT_FOUND = 35u,
        ORCHESTRATOR_INVALID_TYPEALIAS_TARGET = 36u,
        ORCHESTRATOR_NO_DOUBLE_DEFAULT_DECLARATION = 37u,
        ORCHESTRATOR_NO_DEFAULT_PARAM_WITH_OVERLOADS = 38u,
        ORCHESTRATOR_FUNCTION_INCONSISTENT_RETURN_TYPE = 39u,
        ORCHESTRATOR_NO_INLINE_UDT_IN_PARAMETERS = 40u,
        ORCHESTRATOR_OVERLOAD_RESOLUTION_HARD_FAILURE = 41u,
        ORCHESTRATOR_EXTERNAL_VARIABLE_WITH_INITIALIZER = 42u,
        ORCHESTRATOR_MEMBER_VARIABLE_WITH_INITIALIZER = 43u,
        ORCHESTRATOR_SRG_REUSES_A_FREQUENCY = 44u,
        ORCHESTRATOR_NON_PACKABLE_TYPE_IN_SRG_CONSTANT = 45u,
        ORCHESTRATOR_TRYING_TO_EXTEND_NOT_PARTIAL_SRG = 46u,
        ORCHESTRATOR_SRG_EXTENSION_HAS_DIFFERENT_SEMANTIC = 47u,
        ORCHESTRATOR_UNBOUNDED_RESOURCE_ISSUE = 48u,
        ORCHESTRATOR_UNKNOWN_OPTION_TYPE = 49u,
        ORCHESTRATOR_CONSTANT_FOLDING_FAULT = 50u,
        ORCHESTRATOR_TYPE_LOOKUP_FAULT = 51u,

        // Treat all compiler warnings as errors
        WX_WARNINGS_AS_ERRORS = 127u,

        // intermediate representation error codes
        IR_MULTIPLE_SRG_FALLBACK = 128u,
        IR_NO_FALLBACK_ASSIGNED = 129u,
        IR_SRG_WITHOUT_SEMANTIC = 130u,
        IR_POTENTIAL_DX12_VS_VULKAN_ALIGNMENT_ERROR = 131u,
        IR_INVALID_PAD_TO_ARGUMENTS = 132u,
        IR_INVALID_PAD_TO_LOCATION = 133u,
        IR_PAD_TO_CASE_REQUIRES_POWER_OF_TWO = 134u,

        // emitter error codes
        EMITTER_INVALID_ARRAY_DIMENSIONS = 256u,
        EMITTER_RECURSION_NOT_PERMITTED = 257u,
        EMITTER_OVERFLOW_BIT_BOUNDARY = 258u,
        EMITTER_OPTION_EXCEEDING_BITS_COUNT = 259u,
        EMITTER_INTEGER_RANGE_NEEDS_ATTRIBUTE = 260u,
        EMITTER_INTEGER_RANGE_MIN_IS_NOT_CONST = 261u,
        EMITTER_INTEGER_RANGE_MAX_IS_NOT_CONST = 262u,
        EMITTER_INTEGER_RANGE_MIN_IS_BIGGER_THAN_MAX = 263u,
        EMITTER_INTEGER_HAS_NO_RANGE = 264u,
        EMITTER_OPTION_HAS_UNSUPPORTED_TYPE = 265u,
        EMITTER_UNEXPECTED_EXPRESSION = 266u,
        EMITTER_UNDEFINED_SRG_MEMBER = 267u,

        // others
        ADVANCED_SYNTAX_DOUBLE_SCOPE_RESOLUTION = 514u,
        ADVANCED_RESERVED_NAME_USED = 515u,
        ADVANCED_SYNTAX_FUNCTION_IN_STRUCT = 516u,
    };

    class AzslcException : public antlr4::RuntimeException
    {
    public:
        AzslcException(uint32_t errorCode, std::string_view errorType, std::optional<size_t> line, std::optional<size_t> column, const std::string& message)
            : antlr4::RuntimeException(std::string(message.c_str(), message.size()))
            , m_errorCode(errorCode)
            , m_errorType(errorType)
            , m_errorMessage("")
            , m_line(line)
            , m_column(column)
        {
            BakeErrorMessage();
        }

        AzslcException(uint32_t errorCode, std::string_view errorType, Token* token, const std::string& message)
            : RuntimeException(std::string(message.c_str(), message.size()))
            , m_errorCode(errorCode)
            , m_errorType(errorType)
            , m_token(token)
            , m_errorMessage("")
        {
            if (m_token)
            {
                m_line = m_token->getLine();
                m_column = m_token->getCharPositionInLine();
            }
            else
            {
                m_line = std::nullopt;
                m_column = std::nullopt;
            }

            BakeErrorMessage();
        }

        AzslcException(uint32_t errorCode, std::string_view errorType, const std::string& message)
            : RuntimeException(std::string(message.c_str(), message.size()))
            , m_errorCode(errorCode)
            , m_errorType(errorType)
            , m_token(nullptr)
            , m_errorMessage("")
            , m_line(std::nullopt)
            , m_column(std::nullopt)
        {
            BakeErrorMessage();
        }

        const char* what() const noexcept override
        {
            return m_errorMessage.c_str();
        }

        inline uint16_t GetErrorCode() const
        {
            return m_errorCode;
        }

        static std::string MakeErrorMessage(std::string_view filename, std::string_view line, std::string_view column, std::string_view errorType, bool error, std::string_view code, std::string_view message)
        {
            // global filename for error messages. visual studio standard build-tool error format is:
            // {filename(line# [, column#]) | toolname} : [ any text ] {error | warning} code+number:localizable string [ any text ]
            // so to respect this, we're going to simplify the API by setting the report file here.
            const char* errorTypeText = " warning";
            if (error)
            {
                errorTypeText = " error";
            }

            const char* codePrefix = "";
            if (!code.empty())
            {
                codePrefix = " #";
            }

            return ConcatString(
                filename,
                "(",
                line,
                ",",
                column,
                ") : ",
                errorType,
                errorTypeText,
                codePrefix,
                code,
                ": ",
                message);
        }

    protected:
        void BakeErrorMessage()
        {
            size_t lineNumber = 0;
            if (m_line)
            {
                lineNumber = *m_line;
            }

            std::string virtualLine;
            if (m_line)
            {
                virtualLine = ToString(s_lineFinder->GetVirtualLineNumber(*m_line));
            }

            std::string column;
            if (m_column)
            {
                column = ToString(*m_column);
            }

            m_errorMessage = MakeErrorMessage(
                s_lineFinder->GetVirtualFileName(lineNumber),
                virtualLine,
                column,
                m_errorType,
                m_errorCode != WX_WARNINGS_AS_ERRORS,
                ToString(m_errorCode),
                RuntimeException::what());
        }

    public:
        static inline PreprocessorLineDirectiveFinder* s_lineFinder;

    protected:
        const uint16_t m_errorCode;
        const std::string_view m_errorType;
        const Token* m_token;
        std::string m_errorMessage;
        std::optional<size_t> m_line;
        std::optional<size_t> m_column;
    };

    class AzslcOrchestratorException final : public AzslcException
    {
    private:
        inline static constexpr std::string_view ErrorType = "Semantic";

    public:
        AzslcOrchestratorException(uint32_t errorCode, std::optional<size_t> line, std::optional<size_t> column, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                line,
                column,
                message)
        {
        }

        AzslcOrchestratorException(uint32_t errorCode, Token* token, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                token,
                message)
        {
        }

        AzslcOrchestratorException(uint32_t errorCode, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                message)
        {
        }
    };

    class AzslcIrException final : public AzslcException
    {
    private:
        inline static constexpr std::string_view ErrorType = "IR";

    public:
        AzslcIrException(uint32_t errorCode, const std::string& message, std::optional<size_t> line = std::nullopt)
            : AzslcException(
                errorCode,
                ErrorType,
                line,
                std::nullopt,
                message)
        {
        }
    };

    class AzslcEmitterException final : public AzslcException
    {
    private:
        inline static constexpr std::string_view ErrorType = "Emitter";

    public:
        AzslcEmitterException(uint32_t errorCode, std::optional<size_t> line, std::optional<size_t> column, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                line,
                column,
                message)
        {
        }

        AzslcEmitterException(uint32_t errorCode, Token* token, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                token,
                message)
        {
        }

        AzslcEmitterException(uint32_t errorCode, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                message)
        {
        }
    };

    class AzslParserEventListener final : public antlr4::BaseErrorListener
    {
    public:
        void syntaxError(
            antlr4::Recognizer* recognizer,
            antlr4::Token* offendingSymbol,
            size_t line,
            size_t charPositionInLine,
            const std::string& msg,
            std::exception_ptr e) override
        {
            bool isKeyword = m_isKeywordPredicate(recognizer, offendingSymbol);
            using Ex = AzslcException;
            std::string errorMessage;
            if (isKeyword)
            {
                errorMessage = Ex::MakeErrorMessage(
                    Ex::s_lineFinder->GetVirtualFileName(line),
                    ToString(Ex::s_lineFinder->GetVirtualLineNumber(line)),
                    ToString(charPositionInLine + 1),
                    "syntax",
                    true,
                    ToString(PARSER_SYNTAX_ERROR),
                    ConcatString(msg, " (", offendingSymbol->getText(), " is a keyword)"));
            }
            else
            {
                errorMessage = Ex::MakeErrorMessage(
                    Ex::s_lineFinder->GetVirtualFileName(line),
                    ToString(Ex::s_lineFinder->GetVirtualLineNumber(line)),
                    ToString(charPositionInLine + 1),
                    "syntax",
                    true,
                    ToString(PARSER_SYNTAX_ERROR),
                    ConcatString(msg, " (", offendingSymbol->getText(), " was unexpected)"));
            }

            antlr4::ParseCancellationException parseException(std::string(errorMessage.c_str(), errorMessage.size()));
            if (e)
            {
                std::throw_with_nested(parseException);
            }
            throw parseException;
        }

        std::function<bool(antlr4::Recognizer*, antlr4::Token*)> m_isKeywordPredicate;

        void reportAmbiguity(
            antlr4::Parser* recognizer,
            const antlr4::dfa::DFA& dfa,
            size_t startIndex,
            size_t stopIndex,
            bool exact,
            const antlrcpp::BitSet& ambigAlts,
            antlr4::atn::ATNConfigSet* configs) override
        {
        }

        void reportAttemptingFullContext(
            antlr4::Parser* recognizer,
            const antlr4::dfa::DFA& dfa,
            size_t startIndex,
            size_t stopIndex,
            const antlrcpp::BitSet& conflictingAlts,
            antlr4::atn::ATNConfigSet* configs) override
        {
        }

        void reportContextSensitivity(
            antlr4::Parser* recognizer,
            const antlr4::dfa::DFA& dfa,
            size_t startIndex,
            size_t stopIndex,
            size_t prediction,
            antlr4::atn::ATNConfigSet* configs) override
        {
        }
    };

    inline void OutputNestedAndException(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        try
        {
            std::rethrow_if_nested(e);
        }
        catch (const std::exception& ne)
        {
            OutputNestedAndException(ne);
        }
        catch (...)
        {
            std::cerr << "Unknown exception" << std::endl;
        }
    }
};
