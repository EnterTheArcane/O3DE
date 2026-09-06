/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "Grammar/azslLexer.h"
#include "Preprocessor.h"

#include <unordered_map>

namespace AZ::ShaderCompiler
{
    class SourceToken final : public antlr4::CommonToken
    {
    public:
        SourceToken(
            antlr4::TokenSource* source,
            const SourceManager& manager,
            const PreprocessingToken& token,
            size_t type,
            size_t channel);

        SourceLocation Location() const
        {
            return m_location;
        }

    private:
        SourceLocation m_location;
    };

    SourceLocation GetSourceLocation(const antlr4::Token* token);

    class PreprocessingTokenSource final : public antlr4::TokenSource
    {
    public:
        explicit PreprocessingTokenSource(CompilationUnit& unit);

        std::unique_ptr<antlr4::Token> nextToken() override;
        size_t getLine() const override;
        size_t getCharPositionInLine() override;

        antlr4::CharStream* getInputStream() override
        {
            return nullptr;
        }

        std::string getSourceName() override;

        antlr4::TokenFactory<antlr4::CommonToken>* getTokenFactory() override
        {
            return antlr4::CommonTokenFactory::DEFAULT.get();
        }

        const antlr4::dfa::Vocabulary& Vocabulary() const
        {
            return m_vocabulary.getVocabulary();
        }

    private:
        size_t Classify(const PreprocessingToken& token);

        CompilationUnit& m_unit;
        AZStd::span<const PreprocessingToken> m_tokens;
        size_t m_position = 0;
        azslLexer m_vocabulary{ nullptr };
        std::unordered_map<std::string, size_t> m_types;
    };
} // namespace AZ::ShaderCompiler
