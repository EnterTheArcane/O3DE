/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "PreprocessingTokenSource.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <unordered_set>

namespace AZ::ShaderCompiler
{
    namespace
    {
        bool CanJoinAzslPunctuation(
            const SourceManager& sources,
            const PreprocessingToken& left,
            const PreprocessingToken& right)
        {
            if (left.m_kind != PreprocessingKind::Punctuation
                || right.m_kind != PreprocessingKind::Punctuation
                || (right.m_flags & LeadingSpace))
            {
                return false;
            }

            static const std::unordered_set<std::string> compounds = []
            {
                azslLexer lexer(nullptr);
                const auto& vocabulary = lexer.getVocabulary();
                std::unordered_set<std::string> result;
                for (size_t type = 1; type <= vocabulary.getMaxTokenType(); ++type)
                {
                    const std::string_view literal = vocabulary.getLiteralName(type);
                    if (literal.size() > 3)
                    {
                        const std::string_view spelling = literal.substr(1, literal.size() - 2);
                        if (std::all_of(
                                spelling.begin(),
                                spelling.end(),
                                [](unsigned char c)
                                {
                                    return std::ispunct(c) != 0;
                                })
                            && !IsPreprocessingPunctuator(spelling))
                        {
                            result.emplace(spelling);
                        }
                    }
                }
                return result;
            }();

            std::string spelling(sources.Text(left.m_spelling));
            spelling += sources.Text(right.m_spelling);
            return compounds.find(spelling) != compounds.end();
        }

        size_t Decode(std::string_view text, size_t& offset)
        {
            unsigned char first = text[offset++];
            if (first < 128)
            {
                return first;
            }
            size_t count = 0;
            if (first >= 0xc2 && first <= 0xdf)
            {
                count = 1;
            }
            else if (first >= 0xe0 && first <= 0xef)
            {
                count = 2;
            }
            else if (first >= 0xf0 && first <= 0xf4)
            {
                count = 3;
            }
            if (!count || text.size() - offset < count)
            {
                throw std::runtime_error("invalid UTF-8 in shader source");
            }
            size_t value = first & (0x7f >> count);
            for (size_t i = 0; i < count; ++i)
            {
                unsigned char c = text[offset++];
                if ((c & 0xc0) != 0x80)
                {
                    throw std::runtime_error("invalid UTF-8 continuation byte in shader source");
                }
                value = (value << 6) | (c & 0x3f);
            }
            size_t minimumValue = 0x10000;
            if (count == 1)
            {
                minimumValue = 0x80;
            }
            else if (count == 2)
            {
                minimumValue = 0x800;
            }
            if (value < minimumValue || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            {
                throw std::runtime_error("invalid UTF-8 code point in shader source");
            }
            return value;
        }
    } // namespace

    BorrowedSpellingStream::BorrowedSpellingStream(std::string_view text)
        : m_text(text)
    {
        if (std::any_of(
                text.begin(),
                text.end(),
                [](unsigned char c)
                {
                    return c >= 128;
                }))
        {
            for (size_t p = 0; p < text.size();)
            {
                m_offsets.push_back(static_cast<uint32_t>(p));
                Decode(text, p);
            }
            m_offsets.push_back(static_cast<uint32_t>(text.size()));
        }
    }

    void BorrowedSpellingStream::consume()
    {
        if (m_position >= size())
        {
            throw antlr4::IllegalStateException("cannot consume EOF");
        }
        ++m_position;
    }

    size_t BorrowedSpellingStream::LA(ssize_t i)
    {
        if (!i)
        {
            return 0;
        }
        ssize_t lookaheadOffset = i;
        if (i > 0)
        {
            lookaheadOffset = i - 1;
        }
        ssize_t index = static_cast<ssize_t>(m_position) + lookaheadOffset;
        if (index < 0 || static_cast<size_t>(index) >= size())
        {
            return antlr4::IntStream::EOF;
        }
        size_t byte = Byte(static_cast<size_t>(index));
        return Decode(m_text, byte);
    }

    void BorrowedSpellingStream::seek(size_t position)
    {
        m_position = std::min(position, size());
    }

    std::string BorrowedSpellingStream::getText(const antlr4::misc::Interval& interval)
    {
        if (interval.a < 0 || interval.b < interval.a || static_cast<size_t>(interval.a) >= size())
        {
            return {};
        }
        size_t start = Byte(interval.a);
        size_t end = Byte(std::min(static_cast<size_t>(interval.b) + 1, size()));
        return std::string(m_text.substr(start, end - start));
    }

    SourceToken::SourceToken(
        antlr4::TokenSource* source,
        const SourceManager& manager,
        const PreprocessingToken& token,
        size_t type,
        size_t channel)
        : CommonToken(
              { source, nullptr },
              type,
              channel,
              token.m_spelling.m_offset,
              token.m_spelling.m_offset + token.m_spelling.m_length - (token.m_spelling.m_length != 0))
        , m_manager(manager)
        , m_token(token)
    {
        ResolvedSourceLocation location = Location().Resolve();
        setLine(location.m_line);
        size_t column = 0;
        if (location.m_column)
        {
            column = location.m_column - 1;
        }
        setCharPositionInLine(column);
    }

    std::string_view TokenTextView(const antlr4::Token* token)
    {
        if (const SourceToken* source = dynamic_cast<const SourceToken*>(token))
        {
            return source->TextView();
        }
        throw std::logic_error("TokenTextView requires a compilation-owned source token");
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
        if (m_tokens.empty() || m_tokens.back().m_kind != PreprocessingKind::End)
        {
            throw std::logic_error("preprocessing did not produce EOF");
        }
        for (size_t type = 1; type <= Vocabulary().getMaxTokenType(); ++type)
        {
            std::string_view literal = Vocabulary().getLiteralName(type);
            if (literal.size() >= 2)
            {
                m_types.emplace(literal.substr(1, literal.size() - 2), type);
            }
        }
    }

    size_t PreprocessingTokenSource::Classify(const PreprocessingToken& token)
    {
        std::string_view text = m_unit.m_sources.Text(token.m_spelling);
        std::unordered_map<std::string_view, size_t>::iterator it = m_types.find(text);
        if (it != m_types.end())
        {
            return it->second;
        }
        BorrowedSpellingStream input(text);
        azslLexer lexer(&input);
        lexer.removeErrorListeners();
        std::unique_ptr<antlr4::Token> classified = lexer.nextToken();
        std::unique_ptr<antlr4::Token> end = lexer.nextToken();
        if (lexer.getNumberOfSyntaxErrors() || classified->getType() == antlr4::Token::EOF || end->getType() != antlr4::Token::EOF)
        {
            throw PreprocessingError(m_unit.m_sources.Location(token.m_location), "invalid AZSL token: " + std::string(text));
        }
        size_t type = classified->getType();
        // Keys borrow immutable source/arena bytes. Repeated identifiers do not need repeated lexer calls.
        m_types.emplace(text, type);
        return type;
    }

    std::unique_ptr<antlr4::Token> PreprocessingTokenSource::nextToken()
    {
        PreprocessingToken token = m_tokens[std::min(m_position, m_tokens.size() - 1)];
        if (m_position + 1 < m_tokens.size()
            && CanJoinAzslPunctuation(m_unit.m_sources, token, m_tokens[m_position + 1]))
        {
            const PreprocessingToken& next = m_tokens[++m_position];
            if (token.m_spelling.m_buffer == next.m_spelling.m_buffer && token.m_spelling.m_offset + token.m_spelling.m_length == next.m_spelling.m_offset)
            {
                token.m_spelling.m_length += next.m_spelling.m_length;
            }
            else
            {
                const std::string spelling = std::string(m_unit.m_sources.Text(token.m_spelling)) + std::string(m_unit.m_sources.Text(next.m_spelling));
                token.m_spelling = m_unit.m_sources.StoreGenerated(spelling);
            }
        }
        size_t type;
        size_t channel = antlr4::Token::DEFAULT_CHANNEL;
        switch (token.m_kind)
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
            if (m_unit.m_sources.Text(token.m_spelling).substr(0, 2) == "//")
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
        return std::make_unique<SourceToken>(this, m_unit.m_sources, token, type, channel);
    }

    size_t PreprocessingTokenSource::getLine() const
    {
        return m_unit.m_sources.Resolve(m_tokens[std::min(m_position, m_tokens.size() - 1)].m_location).m_line;
    }

    size_t PreprocessingTokenSource::getCharPositionInLine()
    {
        ResolvedSourceLocation location = m_unit.m_sources.Resolve(m_tokens[std::min(m_position, m_tokens.size() - 1)].m_location);
        if (location.m_column)
        {
            return location.m_column - 1;
        }
        return 0;
    }

    std::string PreprocessingTokenSource::getSourceName()
    {
        return std::string(m_unit.m_sources.Resolve(m_tokens[0].m_location).m_file);
    }
    void CompilationUnit::WritePreprocessed(std::ostream& output) const
    {
        std::string_view file;
        size_t line = 0;
        bool separated = true;
        bool lineStart = true;
        const PreprocessingToken* previous = nullptr;
        for (const PreprocessingToken& token : m_tokens)
        {
            if (token.m_kind == PreprocessingKind::End)
            {
                break;
            }
            std::string_view text = m_sources.Text(token.m_spelling);
            if (token.m_kind != PreprocessingKind::Space
                && token.m_kind != PreprocessingKind::Comment
                && token.m_kind != PreprocessingKind::Newline)
            {
                ResolvedSourceLocation location = m_sources.Resolve(token.m_location);
                if (location.m_file != file || location.m_line != line)
                {
                    if (!lineStart)
                    {
                        output << '\n';
                    }
                    output << "#line " << location.m_line << ' ' << std::quoted(std::string(location.m_file)) << '\n';
                    file = location.m_file;
                    line = location.m_line;
                    separated = true;
                    lineStart = true;
                }
                // Conservative spacing also prevents three-token joins such as '.' '.' '.'.
                if (!separated && previous && !CanJoinAzslPunctuation(m_sources, *previous, token))
                {
                    output << ' ';
                }
            }
            // Normalize physical newlines before a Windows text stream translates LF to CRLF.
            for (size_t position = 0; position < text.size();)
            {
                const size_t newline = text.find_first_of("\r\n", position);
                if (newline == std::string_view::npos)
                {
                    output.write(text.data() + position, static_cast<std::streamsize>(text.size() - position));
                    lineStart = false;
                    break;
                }
                output.write(text.data() + position, static_cast<std::streamsize>(newline - position));
                output.put('\n');
                position = newline + 1;
                if (text[newline] == '\r' && position < text.size() && text[position] == '\n')
                {
                    ++position;
                }
                ++line;
                lineStart = true;
            }
            previous = &token;
            separated = token.m_kind == PreprocessingKind::Space
                || token.m_kind == PreprocessingKind::Comment
                || token.m_kind == PreprocessingKind::Newline;
        }
        if (!lineStart)
        {
            output << '\n';
        }
    }
} // namespace AZ::ShaderCompiler
