/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "antlr4-runtime.h"
#include "CompilerContext.h"
#include "GenericUtils.h"
#include "PreprocessorLineDirectiveFinder.h"

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
        AzslcException(
            const uint32_t errorCode,
            const std::string_view errorType,
            const std::optional<size_t> line,
            const std::optional<size_t> column,
            const std::string& message)
            : antlr4::RuntimeException(std::string(message.c_str(), message.size()))
            , m_errorCode(errorCode)
            , m_errorType(errorType)
            , m_errorMessage("")
            , m_line(line)
            , m_column(column)
        {
            BakeErrorMessage();
        }

        AzslcException(
            const uint32_t errorCode,
            const std::string_view errorType,
            const Token* token,
            const std::string& message)
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

        AzslcException(const uint32_t errorCode, const std::string_view errorType, const std::string& message)
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

        uint32_t GetErrorCode() const
        {
            return m_errorCode;
        }

        std::string_view GetErrorType() const
        {
            return m_errorType;
        }

        std::string_view GetErrorMessage() const
        {
            return RuntimeException::what();
        }

        const std::string& GetSourceFileName() const
        {
            return m_sourceFileName;
        }

        const std::optional<size_t>& GetSourceLine() const
        {
            return m_virtualLine;
        }

        const std::optional<size_t>& GetSourceColumn() const
        {
            return m_column;
        }

        static std::string MakeErrorMessage(
            std::string_view filename,
            std::string_view line,
            std::string_view column,
            std::string_view errorType,
            const bool error,
            std::string_view code,
            std::string_view message)
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

            const auto& lineFinder = GetLineDirectiveFinder();
            std::string virtualLineText;
            if (m_line)
            {
                m_virtualLine = lineFinder.GetVirtualLineNumber(*m_line);
                virtualLineText = ToString(*m_virtualLine);
            }

            std::string column;
            if (m_column)
            {
                column = ToString(*m_column);
            }

            m_sourceFileName = lineFinder.GetVirtualFileName(lineNumber);
            m_errorMessage = MakeErrorMessage(
                m_sourceFileName,
                virtualLineText,
                column,
                m_errorType,
                m_errorCode != WX_WARNINGS_AS_ERRORS,
                ToString(m_errorCode),
                RuntimeException::what());
        }

    protected:
        const uint32_t m_errorCode;
        const std::string_view m_errorType;
        const Token* m_token;
        std::string m_errorMessage;
        std::string m_sourceFileName;
        std::optional<size_t> m_line;
        std::optional<size_t> m_virtualLine;
        std::optional<size_t> m_column;
    };

    class AzslcOrchestratorException final : public AzslcException
    {
    private:
        inline static constexpr std::string_view ErrorType = "Semantic";

    public:
        AzslcOrchestratorException(
            const uint32_t errorCode,
            const std::optional<size_t> line,
            const std::optional<size_t> column,
            const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                line,
                column,
                message)
        {
        }

        AzslcOrchestratorException(const uint32_t errorCode, Token* token, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                token,
                message)
        {
        }

        AzslcOrchestratorException(const uint32_t errorCode, const std::string& message)
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
        AzslcIrException(
            const uint32_t errorCode,
            const std::string& message,
            const std::optional<size_t> line = std::nullopt)
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
        AzslcEmitterException(
            const uint32_t errorCode,
            const std::optional<size_t> line,
            const std::optional<size_t> column,
            const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                line,
                column,
                message)
        {
        }

        AzslcEmitterException(const uint32_t errorCode, Token* token, const std::string& message)
            : AzslcException(
                errorCode,
                ErrorType,
                token,
                message)
        {
        }

        AzslcEmitterException(const uint32_t errorCode, const std::string& message)
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
            const size_t line,
            const size_t charPositionInLine,
            const std::string& msg,
            const std::exception_ptr) override
        {
            const bool isKeyword = m_isKeywordPredicate(recognizer, offendingSymbol);
            const std::string offendingText = offendingSymbol ? offendingSymbol->getText() : "<unknown>";
            std::string errorMessage;
            if (isKeyword)
            {
                errorMessage = ConcatString(msg, " (", offendingText, " is a keyword)");
            }
            else
            {
                errorMessage = ConcatString(msg, " (", offendingText, " was unexpected)");
            }

            throw AzslcException{
                PARSER_SYNTAX_ERROR,
                "syntax",
                line,
                charPositionInLine + 1,
                errorMessage};
        }

        std::function<bool(antlr4::Recognizer*, antlr4::Token*)> m_isKeywordPredicate;

        void reportAmbiguity(antlr4::Parser*, const antlr4::dfa::DFA&, size_t, size_t, bool, const antlrcpp::BitSet&, antlr4::atn::ATNConfigSet*) override
        {
            // NO-OP
        }

        void reportAttemptingFullContext(antlr4::Parser*, const antlr4::dfa::DFA&, size_t, size_t, const antlrcpp::BitSet&, antlr4::atn::ATNConfigSet*) override
        {
            // NO-OP
        }

        void reportContextSensitivity(antlr4::Parser*, const antlr4::dfa::DFA&, size_t, size_t, size_t, antlr4::atn::ATNConfigSet*) override
        {
            // NO-OP
        }
    };

    inline void OutputNestedException(const std::exception& e, std::ostream& output)
    {
        output << e.what() << std::endl;
        try
        {
            std::rethrow_if_nested(e);
        }
        catch (const std::exception& ne)
        {
            OutputNestedException(ne, output);
        }
        catch (...)
        {
            output << "Unknown exception" << std::endl;
        }
    }
}
