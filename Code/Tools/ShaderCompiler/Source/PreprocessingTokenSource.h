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
    // Bounded UTF-8 view used only for one preprocessing token at a time.
    // Character positions belong to this stream. Source byte offsets belong to SourceManager.
    class BorrowedSpellingStream final : public antlr4::CharStream
    {
    public:
        explicit BorrowedSpellingStream(std::string_view text);

        void consume() override;

        size_t LA(ssize_t i) override;

        ssize_t mark() override
        {
            return -1;
        }

        void release(ssize_t) override
        {
        }

        size_t index() override
        {
            return m_position;
        }

        void seek(size_t position) override;

        size_t size() override
        {
            if (m_offsets.empty())
            {
                return m_text.size();
            }
            return m_offsets.size() - 1;
        }

        std::string getSourceName() const override
        {
            return "<preprocessing token>";
        }

        std::string getText(const antlr4::misc::Interval& interval) override;

        std::string toString() const override
        {
            return std::string(m_text);
        }

    private:
        size_t Byte(size_t index) const
        {
            if (m_offsets.empty())
            {
                return index;
            }
            return m_offsets.at(index);
        }

        std::string_view m_text;
        std::vector<uint32_t> m_offsets; // Allocated only for non-ASCII tokens; never stores UTF-32 text.
        size_t m_position = 0;
    };

    class SourceToken final : public antlr4::CommonToken
    {
    public:
        SourceToken(
            antlr4::TokenSource* source,
            const SourceManager& manager,
            const PreprocessingToken& token,
            size_t type,
            size_t channel);

        std::string getText() const override
        {
            return std::string(TextView());
        }

        std::string_view TextView() const
        {
            if (getType() == EOF)
            {
                return std::string_view("<EOF>");
            }
            return m_manager.Text(m_token.m_spelling);
        }

        SourceLocation Location() const
        {
            return m_manager.Location(m_token.m_location);
        }

    private:
        const SourceManager& m_manager;
        PreprocessingToken m_token;
    };

    std::string_view TokenTextView(const antlr4::Token* token);

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
        std::unordered_map<std::string_view, size_t> m_types;
    };
} // namespace AZ::ShaderCompiler
