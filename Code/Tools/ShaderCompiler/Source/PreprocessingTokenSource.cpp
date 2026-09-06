/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "PreprocessingTokenSource.h"

#include <algorithm>

namespace AZ::ShaderCompiler
{
    SourceToken::SourceToken(
        antlr4::TokenSource* source, const SourceManager& manager, const PreprocessingToken& token, size_t type, size_t channel)
        : CommonToken({ source, nullptr }, type, channel, INVALID_INDEX, INVALID_INDEX)
        , m_location(manager.Location(token.location))
    {
        if (type == antlr4::Token::EOF)
        {
            setText("<EOF>");
        }
        else
        {
            setText(token.spelling);
        }
        ResolvedSourceLocation location = Location().Resolve();
        setLine(location.line);
        size_t column = 0;
        if (location.column)
        {
            column = location.column - 1;
        }
        setCharPositionInLine(column);
    }

    SourceLocation GetSourceLocation(const antlr4::Token* token)
    {
        if (const SourceToken* source = dynamic_cast<const SourceToken*>(token))
        {
            return source->Location();
        }
        return {};
    }

    PreprocessingTokenSource::PreprocessingTokenSource(CompilationUnit& unit)
        : m_unit(unit)
        , m_tokens(unit.Tokens())
    {
        if (m_tokens.empty() || m_tokens.back().kind != PreprocessingKind::End)
        {
            throw std::logic_error("preprocessing did not produce EOF");
        }
        for (size_t type = 1; type <= Vocabulary().getMaxTokenType(); ++type)
        {
            std::string literal(Vocabulary().getLiteralName(type));
            if (literal.size() >= 2)
                m_types.emplace(literal.substr(1, literal.size() - 2), type);
        }
    }

    size_t PreprocessingTokenSource::Classify(const PreprocessingToken& token)
    {
        const std::string& text = token.spelling;
        std::unordered_map<std::string, size_t>::iterator it = m_types.find(text);
        if (it != m_types.end())
        {
            return it->second;
        }
        antlr4::ANTLRInputStream input(text);
        azslLexer lexer(&input);
        lexer.removeErrorListeners();
        std::unique_ptr<antlr4::Token> classified = lexer.nextToken();
        std::unique_ptr<antlr4::Token> end = lexer.nextToken();
        if (lexer.getNumberOfSyntaxErrors() || classified->getType() == antlr4::Token::EOF || end->getType() != antlr4::Token::EOF)
            throw PreprocessingError(m_unit.sources.Location(token.location), "invalid AZSL token: " + std::string(text));
        size_t type = classified->getType();
        // Cache spellings by value so repeated identifiers do not need repeated lexer calls.
        m_types.emplace(text, type);
        return type;
    }

    std::unique_ptr<antlr4::Token> PreprocessingTokenSource::nextToken()
    {
        PreprocessingToken token = m_tokens[std::min(m_position, m_tokens.size() - 1)];
        if (token.kind == PreprocessingKind::Punctuation)
        {
            token.spelling = CanonicalPunctuator(token.spelling);
        }
        // AZSL has punctuation (for example '[[') spanning multiple C preprocessing tokens.
        // Discover these combinations from the generated vocabulary.
        if (token.kind == PreprocessingKind::Punctuation && m_position + 1 < m_tokens.size())
        {
            const PreprocessingToken& next = m_tokens[m_position + 1];
            if (next.kind == PreprocessingKind::Punctuation && !(next.flags & LeadingSpace))
            {
                std::string combined = token.spelling + std::string(CanonicalPunctuator(next.spelling));
                if (!IsPreprocessingPunctuator(combined) && m_types.find(combined) != m_types.end())
                {
                    token.spelling = std::move(combined);
                    ++m_position;
                }
            }
        }
        size_t type, channel = antlr4::Token::DEFAULT_CHANNEL;
        switch (token.kind)
        {
        case PreprocessingKind::End:
            type = antlr4::Token::EOF;
            break;
        case PreprocessingKind::Space:
            type = azslLexer::Whitespace;
            channel = antlr4::Token::HIDDEN_CHANNEL;
            break;
        case PreprocessingKind::Newline:
            type = azslLexer::Newline;
            channel = antlr4::Token::HIDDEN_CHANNEL;
            break;
        case PreprocessingKind::Comment:
            if (token.spelling.substr(0, 2) == "//")
            {
                type = azslLexer::LineComment;
            }
            else
            {
                type = azslLexer::BlockComment;
            }
            channel = azslLexer::COMMENTS;
            break;
        case PreprocessingKind::Directive:
            type = azslLexer::PragmaDirective;
            channel = azslLexer::PREPROCESSOR;
            break;
        default:
            type = Classify(token);
            break;
        }
        if (m_position < m_tokens.size())
        {
            ++m_position;
        }
        return std::make_unique<SourceToken>(this, m_unit.sources, token, type, channel);
    }

    size_t PreprocessingTokenSource::getLine() const
    {
        return m_unit.sources.Resolve(m_tokens[std::min(m_position, m_tokens.size() - 1)].location).line;
    }

    size_t PreprocessingTokenSource::getCharPositionInLine()
    {
        ResolvedSourceLocation location = m_unit.sources.Resolve(m_tokens[std::min(m_position, m_tokens.size() - 1)].location);
        if (location.column)
        {
            return location.column - 1;
        }
        return 0;
    }

    std::string PreprocessingTokenSource::getSourceName()
    {
        return std::string(m_unit.sources.Resolve(m_tokens[0].location).file);
    }
} // namespace AZ::ShaderCompiler
