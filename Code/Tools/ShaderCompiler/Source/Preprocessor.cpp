/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "Preprocessor.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <ctime>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <unordered_set>

namespace AZ::ShaderCompiler
{
    namespace
    {
        using Token = PreprocessingToken;
        using Kind = PreprocessingKind;
        using Tokens = std::vector<Token>;

        constexpr uint8_t ReplacementPaste = 4; // Only operators from replacement lists execute token pasting.

        static constexpr std::string_view Punctuators[] = {
            "%:%:", "<<=", ">>=", "->*", "<=>", "...", "##", "++", "--", "->", "::", "<<", ">>", "<=", ">=",
            "==", "!=", "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", ".*", "<:", ":>", "<%", "%>", "%:"
        };

        bool Trivia(const Token& t)
        {
            return t.kind == Kind::Space || t.kind == Kind::Comment || t.kind == Kind::Newline;
        }

        bool Digit(char c)
        {
            return c >= '0' && c <= '9';
        }

        bool Identifier(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || static_cast<unsigned char>(c) >= 128;
        }

        int HexDigit(char c)
        {
            if (Digit(c))
            {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f')
            {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F')
            {
                return c - 'A' + 10;
            }
            return -1;
        }

        void AppendUtf8(std::string& text, uint32_t value)
        {
            if (value < 0x80)
            {
                text += static_cast<char>(value);
            }
            else
            {
                if (value < 0x800)
                {
                    text += static_cast<char>(0xc0 | (value >> 6));
                }
                else
                {
                    if (value < 0x10000)
                    {
                        text += static_cast<char>(0xe0 | (value >> 12));
                    }
                    else
                    {
                        text += static_cast<char>(0xf0 | (value >> 18));
                        text += static_cast<char>(0x80 | ((value >> 12) & 0x3f));
                    }
                    text += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
                }
                text += static_cast<char>(0x80 | (value & 0x3f));
            }
        }

        // Preserve original token spelling for stringification, but compare UCN and UTF-8 names by code point.
        std::string IdentifierName(const Token& token)
        {
            if (token.kind != Kind::Identifier || token.spelling.find('\\') == std::string::npos)
            {
                return token.spelling;
            }
            std::string name;
            for (size_t i = 0; i < token.spelling.size(); ++i)
            {
                char c = token.spelling[i];
                if (c == '\\' && i + 1 < token.spelling.size() && (token.spelling[i + 1] == 'u' || token.spelling[i + 1] == 'U'))
                {
                    size_t digits = token.spelling[++i] == 'u' ? 4 : 8;
                    uint32_t value = 0;
                    while (digits--)
                    {
                        value = value * 16 + HexDigit(token.spelling[++i]);
                    }
                    AppendUtf8(name, value);
                }
                else
                {
                    name += c;
                }
            }
            return name;
        }

        std::string Quote(std::string_view text)
        {
            std::string result = "\"";
            for (char c : text)
            {
                static constexpr std::string_view controls = "\a\b\f\n\r\t\v";
                static constexpr std::string_view escapes = "abfnrtv";
                size_t control = controls.find(c);
                if (control != std::string_view::npos)
                {
                    result += '\\';
                    result += escapes[control];
                    continue;
                }
                if (static_cast<unsigned char>(c) < 32)
                {
                    result += "\\0";
                    result += static_cast<char>('0' + ((static_cast<unsigned char>(c) >> 3) & 7));
                    result += static_cast<char>('0' + (c & 7));
                    continue;
                }
                if (c == '\\' || c == '"')
                {
                    result += '\\';
                }
                result += c;
            }
            return result + '"';
        }

        std::string Unquote(std::string_view text)
        {
            std::string result;
            for (size_t i = 1; i + 1 < text.size(); ++i)
            {
                if (text[i] == '\\' && i + 2 < text.size() && (text[i + 1] == '\\' || text[i + 1] == '"'))
                {
                    ++i;
                }
                result += text[i];
            }
            return result;
        }

        Tokens Significant(AZStd::span<const Token> tokens)
        {
            Tokens result;
            bool space = false;
            for (Token t : tokens)
            {
                if (Trivia(t))
                {
                    space = true;
                    continue;
                }
                if (t.kind == Kind::End)
                {
                    continue;
                }
                if (space)
                {
                    t.flags |= LeadingSpace;
                }
                result.push_back(t);
                space = false;
            }
            return result;
        }

        // Translation phase 2 is performed through cursor lookahead.
        // Each scanner owns its input and each token owns its spelling after line splicing.
        class Scanner
        {
        public:
            Scanner(SourceManager& sources, BufferId buffer, SourceInstanceId instance)
                : m_sources(sources)
                , m_instance(instance)
                , m_text(sources.BufferText(buffer))
            {
                if (m_text.substr(0, 3) == "\xef\xbb\xbf")
                {
                    m_pos = 3;
                }
            }

            Scanner(SourceManager& sources, std::string text, SourceLocationId location)
                : m_sources(sources)
                , m_instance(0)
                , m_text(std::move(text))
                , m_fixedLocation(location)
            {
            }

            uint32_t Offset() const
            {
                return static_cast<uint32_t>(m_pos);
            }

            Token Next(bool expectHeaderName = false)
            {
                SkipSplices(m_pos);
                size_t start = m_pos;
                Kind kind = Kind::Punctuation;
                size_t rawBodyStart = 0, rawBodyEnd = 0;
                char c = Peek();
                if (!c && m_pos < m_text.size())
                {
                    Fail("NUL byte in shader source", start);
                }
                if (!c && m_pos == m_text.size())
                {
                    if (m_blockComment)
                    {
                        Fail("unterminated block comment", start);
                    }
                    return Make(Kind::End, start);
                }
                if (c == '\r' || c == '\n')
                {
                    Take();
                    if (c == '\r' && Peek() == '\n')
                    {
                        Take();
                    }
                    Token token = Make(Kind::Newline, start);
                    m_startOfLine = true;
                    m_space = true;
                    return token;
                }
                if (m_blockComment || (c == '/' && Peek(1) == '*'))
                {
                    if (!m_blockComment)
                    {
                        Take();
                        Take();
                        m_blockComment = true;
                    }
                    while (Peek())
                    {
                        if (Peek() == '*' && Peek(1) == '/')
                        {
                            Take();
                            Take();
                            m_blockComment = false;
                            break;
                        }
                        Take();
                    }
                    if (m_pos == m_text.size() && m_blockComment)
                    {
                        Fail("unterminated block comment", start);
                    }
                    kind = Kind::Comment;
                }
                else if (c == '/' && Peek(1) == '/')
                {
                    while (Peek() && Peek() != '\n' && Peek() != '\r')
                    {
                        Take();
                    }
                    kind = Kind::Comment;
                }
                else if (c == ' ' || c == '\t' || c == '\v' || c == '\f')
                {
                    do
                    {
                        Take();
                        c = Peek();
                    } while (c == ' ' || c == '\t' || c == '\v' || c == '\f');
                    kind = Kind::Space;
                }
                else if (expectHeaderName && (c == '"' || c == '<'))
                {
                    // Header names have their own lexical context: neither comments nor string escape sequences apply inside them.
                    char closing = '"';
                    if (c == '<')
                    {
                        closing = '>';
                    }
                    Take();
                    while (Peek() != closing)
                    {
                        if (!Peek() || Peek() == '\n' || Peek() == '\r')
                        {
                            Fail("unterminated include header name", start);
                        }
                        Take();
                    }
                    Take();
                    kind = Kind::HeaderName;
                }
                else if (LiteralPrefix() != std::string::npos)
                {
                    size_t prefix = LiteralPrefix();
                    bool raw = prefix && Peek(prefix - 1) == 'R';
                    while (prefix--)
                    {
                        Take();
                    }
                    char quote = Take();
                    if (raw)
                    {
                        // Translation phases 1 and 2 are reverted inside the quotes of a raw string.
                        rawBodyStart = m_pos;
                        size_t delimiterStart = m_pos;
                        while (m_pos < m_text.size() && m_text[m_pos] != '(')
                        {
                            unsigned char delimiter = m_text[m_pos];
                            if (m_pos - delimiterStart == 16 || delimiter <= ' ' || delimiter >= 127 || delimiter == ')' || delimiter == '\\')
                            {
                                Fail("invalid raw string delimiter", start);
                            }
                            ++m_pos;
                        }
                        if (m_pos == m_text.size())
                        {
                            Fail("unterminated raw string delimiter", start);
                        }
                        std::string closing = ")" + m_text.substr(delimiterStart, m_pos - delimiterStart) + '"';
                        size_t end = m_text.find(closing, ++m_pos);
                        if (end == std::string::npos)
                        {
                            Fail("unterminated raw string literal", start);
                        }
                        m_pos = end + closing.size();
                        rawBodyEnd = m_pos;
                    }
                    else
                    {
                        while (Peek() != quote)
                        {
                            if (!Peek() || Peek() == '\n' || Peek() == '\r')
                            {
                                Fail("unterminated string or character constant", start);
                            }
                            if (Take() == '\\')
                            {
                                if (!Peek() || Peek() == '\n' || Peek() == '\r')
                                {
                                    Fail("unterminated escape", start);
                                }
                                Take();
                            }
                        }
                        Take();
                    }
                    if (Identifier(Peek()) || UniversalCharacter())
                    {
                        ScanIdentifier(); // A user-defined literal suffix is part of the same preprocessing token.
                    }
                    kind = quote == '"' ? Kind::String : Kind::Character;
                }
                else if (Identifier(c) || UniversalCharacter())
                {
                    ScanIdentifier();
                    kind = Kind::Identifier;
                }
                else if (Digit(c) || (c == '.' && Digit(Peek(1))))
                {
                    char previous = Take();
                    while (Identifier(Peek()) || UniversalCharacter() || Digit(Peek()) || Peek() == '.' ||
                           (Peek() == '\'' && (Identifier(Peek(1)) || Digit(Peek(1)) ||
                                               (Peek(1) == '\\' && (Peek(2) == 'u' || Peek(2) == 'U')))) ||
                           ((Peek() == '+' || Peek() == '-') && (previous == 'e' || previous == 'E' || previous == 'p' || previous == 'P')))
                    {
                        if (UniversalCharacter() || static_cast<unsigned char>(Peek()) >= 128)
                        {
                            ScanIdentifierCharacter(false);
                            previous = 0;
                        }
                        else
                        {
                            previous = Take();
                        }
                    }
                    kind = Kind::Number;
                }
                else
                {
                    size_t length = 1;
                    for (std::string_view punctuator : Punctuators)
                    {
                        // C++'s <:: exception keeps a template opener separate from a qualified name.
                        if (punctuator == "<:" && Peek(2) == ':' && Peek(3) != ':' && Peek(3) != '>')
                        {
                            continue;
                        }
                        bool match = true;
                        for (size_t i = 0; i < punctuator.size(); ++i)
                        {
                            if (Peek(i) != punctuator[i])
                            {
                                match = false;
                            }
                        }
                        if (match)
                        {
                            length = punctuator.size();
                            break;
                        }
                    }
                    if (!c)
                    {
                        Fail("NUL byte in shader source", start);
                    }
                    while (length--)
                    {
                        Take();
                    }
                }
                Token token = Make(kind, start, rawBodyStart, rawBodyEnd);
                if (token.kind == Kind::Identifier && CanonicalPunctuator(token.spelling) != token.spelling)
                {
                    token.kind = Kind::Punctuation;
                }
                if (Trivia(token))
                {
                    m_space = true;
                }
                else
                {
                    m_space = false;
                    m_startOfLine = false;
                }
                return token;
            }

        private:
            size_t LiteralPrefix() const
            {
                size_t prefix = 0;
                if (Peek() == 'u' && Peek(1) == '8')
                {
                    prefix = 2;
                }
                else if (Peek() == 'u' || Peek() == 'U' || Peek() == 'L')
                {
                    prefix = 1;
                }
                if (Peek(prefix) == 'R' && Peek(prefix + 1) == '"')
                {
                    return prefix + 1;
                }
                if (Peek(prefix) == '"' || Peek(prefix) == '\'')
                {
                    return prefix;
                }
                return std::string::npos;
            }

            bool UniversalCharacter() const
            {
                return Peek() == '\\' && (Peek(1) == 'u' || Peek(1) == 'U');
            }

            void ScanIdentifierCharacter(bool first)
            {
                size_t start = m_pos;
                uint32_t value = static_cast<unsigned char>(Take());
                if (value == '\\')
                {
                    size_t digits = Take() == 'u' ? 4 : 8;
                    value = 0;
                    while (digits--)
                    {
                        int digit = HexDigit(Take());
                        if (digit < 0)
                        {
                            Fail("invalid universal character name", start);
                        }
                        value = value * 16 + digit;
                    }
                }
                else if (value >= 128)
                {
                    size_t remaining = value >= 0xf0 ? 3 : (value >= 0xe0 ? 2 : 1);
                    uint32_t minimum = remaining == 3 ? 0x10000 : (remaining == 2 ? 0x800 : 0x80);
                    if (value < 0xc2 || value > 0xf4)
                    {
                        Fail("invalid UTF-8 sequence", start);
                    }
                    value &= (1u << (6 - remaining)) - 1;
                    while (remaining--)
                    {
                        unsigned char next = Take();
                        if ((next & 0xc0) != 0x80)
                        {
                            Fail("invalid UTF-8 continuation", start);
                        }
                        value = (value << 6) | (next & 0x3f);
                    }
                    if (value < minimum)
                    {
                        Fail("overlong UTF-8 sequence", start);
                    }
                }
                else
                {
                    return;
                }
                // C++20 [lex.name], tables 2 and 3. These are source-language ranges,
                // independent of the host locale and its Unicode library version.
                static constexpr uint32_t ranges[][2] = {
                    {0x00a8, 0x00a8}, {0x00aa, 0x00aa}, {0x00ad, 0x00ad}, {0x00af, 0x00af},
                    {0x00b2, 0x00b5}, {0x00b7, 0x00ba}, {0x00bc, 0x00be}, {0x00c0, 0x00d6},
                    {0x00d8, 0x00f6}, {0x00f8, 0x167f}, {0x1681, 0x180d}, {0x180f, 0x1fff},
                    {0x200b, 0x200d}, {0x202a, 0x202e}, {0x203f, 0x2040}, {0x2054, 0x2054},
                    {0x2060, 0x206f}, {0x2070, 0x218f}, {0x2460, 0x24ff}, {0x2776, 0x2793},
                    {0x2c00, 0x2dff}, {0x2e80, 0x2fff}, {0x3004, 0x3007}, {0x3021, 0x302f},
                    {0x3031, 0xd7ff}, {0xf900, 0xfd3d}, {0xfd40, 0xfdcf}, {0xfdf0, 0xfe44},
                    {0xfe47, 0xfffd}
                };
                bool allowed = value >= 0x10000 && value <= 0xefffd && (value & 0xffff) <= 0xfffd;
                for (const auto& range : ranges)
                {
                    allowed = allowed || (value >= range[0] && value <= range[1]);
                }
                if (!allowed || (first && ((value >= 0x0300 && value <= 0x036f) || (value >= 0x1dc0 && value <= 0x1dff) ||
                                          (value >= 0x20d0 && value <= 0x20ff) || (value >= 0xfe20 && value <= 0xfe2f))))
                {
                    Fail("character is not permitted in a C++20 identifier", start);
                }
            }

            void ScanIdentifier()
            {
                ScanIdentifierCharacter(true);
                while (Identifier(Peek()) || UniversalCharacter() || Digit(Peek()))
                {
                    ScanIdentifierCharacter(false);
                }
            }

            void SkipSplices(size_t& pos) const
            {
                while (pos + 1 < m_text.size() && m_text[pos] == '\\')
                {
                    if (m_text[pos + 1] == '\n')
                    {
                        pos += 2;
                    }
                    else if (m_text[pos + 1] == '\r' && pos + 2 < m_text.size() && m_text[pos + 2] == '\n')
                    {
                        pos += 3;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            char Peek(size_t ahead = 0) const
            {
                size_t pos = m_pos;
                for (;;)
                {
                    SkipSplices(pos);
                    if (pos == m_text.size())
                    {
                        return 0;
                    }
                    if (!ahead--)
                    {
                        return m_text[pos];
                    }
                    ++pos;
                }
            }

            char Take()
            {
                SkipSplices(m_pos);
                if (m_pos < m_text.size())
                {
                    return m_text[m_pos++];
                }
                return 0;
            }

            [[noreturn]] void Fail(const char* message, size_t at)
            {
                SourceLocationId location = m_fixedLocation;
                if (!location)
                {
                    location = m_sources.Locate(m_instance, static_cast<uint32_t>(at));
                }
                throw PreprocessingError(m_sources.Location(location), message);
            }

            Token Make(Kind kind, size_t start, size_t rawBodyStart = 0, size_t rawBodyEnd = 0)
            {
                std::string text = m_text.substr(start, m_pos - start);
                if (text.find("\\\n") != std::string_view::npos || text.find("\\\r\n") != std::string_view::npos)
                {
                    std::string joined;
                    joined.reserve(text.size());
                    for (size_t p = start; p < m_pos;)
                    {
                        if (p == rawBodyStart && rawBodyEnd)
                        {
                            joined.append(m_text, p, rawBodyEnd - p);
                            p = rawBodyEnd;
                            continue;
                        }
                        SkipSplices(p);
                        if (p < m_pos)
                        {
                            joined += m_text[p++];
                        }
                    }
                    text = std::move(joined);
                }
                SourceLocationId location = m_fixedLocation;
                if (!location)
                {
                    location = m_sources.Locate(m_instance, static_cast<uint32_t>(start));
                }
                uint8_t flags = 0;
                if (m_space)
                {
                    flags |= LeadingSpace;
                }
                if (m_startOfLine)
                {
                    flags |= StartOfLine;
                }
                return { std::move(text), location, 0, kind, flags };
            }

            SourceManager& m_sources;
            SourceInstanceId m_instance;
            std::string m_text;
            SourceLocationId m_fixedLocation = 0;
            size_t m_pos = 0;
            bool m_blockComment = false, m_startOfLine = true, m_space = false;
        };
    } // namespace

    bool IsPreprocessingPunctuator(std::string_view spelling)
    {
        return spelling.size() == 1 || std::find(std::begin(Punctuators), std::end(Punctuators), spelling) != std::end(Punctuators);
    }

    std::string_view CanonicalPunctuator(std::string_view spelling)
    {
        static constexpr std::string_view alternate[] = {
            "<:", ":>", "<%", "%>", "%:", "%:%:", "and", "or", "not", "bitand", "bitor", "xor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq"
        };
        static constexpr std::string_view primary[] = {
            "[", "]", "{", "}", "#", "##", "&&", "||", "!", "&", "|", "^", "~", "&=", "|=", "^=", "!="
        };
        for (size_t i = 0; i < std::size(alternate); ++i)
        {
            if (spelling == alternate[i])
            {
                return primary[i];
            }
        }
        return spelling;
    }

    PreprocessingError::PreprocessingError(SourceLocation location, const std::string& message)
        : std::runtime_error(
              [&]
              {
                  const ResolvedSourceLocation resolved = location.Resolve();
                  return std::string(resolved.file) + "(" + std::to_string(resolved.line) + "," + std::to_string(resolved.column) +
                      ") : error #600: " + message + location.Notes();
              }())
    {
    }

    class Preprocessor
    {
        struct Macro
        {
            uint32_t id;
            bool function = false, variadic = false;
            std::vector<std::string> parameters;
            Tokens body;
        };

        struct Suppression
        {
            uint32_t macro = 0, parent = 0, depth = 0;
        };

        struct Conditional
        {
            bool parent, active, taken, hadElse;
            SourceLocationId location;
        };

    public:
        Preprocessor(CompilationUnit& unit, const PreprocessRequest& request)
            : m_unit(unit)
            , m_sources(unit.sources)
            , m_request(request)
        {
            m_suppression.push_back({});
        }

        void Run()
        {
            DefineBuiltin("__cplusplus", "202002L");
            DefineBuiltin("__STDC_HOSTED__", "0");
            DefineBuiltin("__AZSL__", "1");
            std::time_t now = std::time(nullptr);
            std::tm date{};
#ifdef _WIN32
            localtime_s(&date, &now);
#else
            localtime_r(&now, &date);
#endif
            char timeString[40];
            std::strftime(timeString, sizeof(timeString), "\"%b %d %Y\"", &date);
            if (timeString[5] == '0')
            {
                timeString[5] = ' ';
            }
            DefineBuiltin("__DATE__", timeString);
            std::strftime(timeString, sizeof(timeString), "\"%H:%M:%S\"", &date);
            DefineBuiltin("__TIME__", timeString);

            for (const MacroOperation& op : m_request.macros)
            {
                if (op.undefine)
                {
                    Tokens name = Significant(LexGenerated(op.definition, "<command line>"));
                    if (name.size() != 1 || name[0].kind != Kind::Identifier)
                    {
                        Fail(0, "-U requires a single macro name");
                    }
                    if (Reserved(IdentifierName(name[0])))
                    {
                        Fail(name[0].location, "cannot undefine reserved preprocessing name");
                    }
                    m_macros.erase(IdentifierName(name[0]));
                    continue;
                }
                size_t equal = op.definition.find('=');
                std::string value = "1";
                if (equal != std::string::npos)
                {
                    value = op.definition.substr(equal + 1);
                }
                std::string text = op.definition.substr(0, equal) + " " + value;
                Tokens tokens = LexGenerated(std::move(text), "<command line>");
                Tokens definition = Significant(tokens);
                // Ordered command-line definitions override earlier configuration layers.
                if (!definition.empty() && definition[0].kind == Kind::Identifier)
                {
                    m_macros.erase(IdentifierName(definition[0]));
                }
                Define(definition);
            }

            BufferId root = m_request.source;
            if (!root)
            {
                root = LoadFile(m_request.sourcePath, 0);
            }
            std::string path = "<stdin>";
            if (!m_request.sourcePath.empty())
            {
                path = m_request.sourcePath;
            }
            std::filesystem::path directory = std::filesystem::current_path();
            if (!m_request.sourcePath.empty())
            {
                directory = std::filesystem::path(path).parent_path();
            }
            if (!m_request.preprocessed)
            {
                for (const std::string& include : m_request.forcedIncludes)
                {
                    Include(include, false, directory, 0, 0);
                }
            }

            Process(root, path, 0, 0);
        }

    private:
        BufferId LoadFile(const std::string& path, SourceLocationId includedAt)
        {
            try
            {
                return m_sources.LoadFile(path);
            }
            catch (const std::runtime_error& error)
            {
                Fail(includedAt, error.what());
            }
        }

        [[noreturn]] void Fail(SourceLocationId loc, const std::string& text)
        {
            throw PreprocessingError(m_sources.Location(loc), text);
        }

        const std::string& Text(const Token& t) const
        {
            return t.spelling;
        }

        bool Is(const Token& t, std::string_view s) const
        {
            return CanonicalPunctuator(Text(t)) == s;
        }

        Token Generated(std::string text, Kind kind, SourceLocationId location)
        {
            return { std::move(text), location, 0, kind, 0 };
        }

        Tokens LexGenerated(std::string text, std::string name)
        {
            BufferId buffer = m_sources.AddSource(name, std::move(text));
            Scanner scanner(m_sources, buffer, m_sources.EnterSource(buffer, std::move(name)));
            Tokens tokens;
            for (;;)
            {
                Token token = scanner.Next();
                if (token.kind == Kind::End)
                {
                    break;
                }
                tokens.push_back(token);
            }
            return tokens;
        }

        void DefineBuiltin(std::string name, std::string value)
        {
            if (!m_builtinLocation)
            {
                BufferId buffer = m_sources.AddSource("<built-in>", "");
                m_builtinLocation = m_sources.Locate(m_sources.EnterSource(buffer, "<built-in>"), 0);
            }
            Scanner scanner(m_sources, name + " " + value, m_builtinLocation);
            Tokens tokens;
            for (;;)
            {
                Token token = scanner.Next();
                if (token.kind == Kind::End)
                {
                    break;
                }
                tokens.push_back(token);
            }
            Define(Significant(tokens), true);
        }

        bool Reserved(const std::string& name) const
        {
            return name == "defined" || name == "__FILE__" || name == "__LINE__" || name == "__DATE__" || name == "__TIME__" ||
                name == "__cplusplus" || name == "__STDC_HOSTED__" || name == "__has_include" || name == "__has_cpp_attribute" ||
                name == "_Pragma" || name == "__VA_ARGS__" || name == "__VA_OPT__";
        }

        void Define(const Tokens& tokens, bool builtin = false)
        {
            if (tokens.empty() || tokens[0].kind != Kind::Identifier)
            {
                SourceLocationId location = 0;
                if (!tokens.empty())
                {
                    location = tokens[0].location;
                }
                Fail(location, "expected macro name");
            }
            const std::string name = IdentifierName(tokens[0]);
            if (!builtin && Reserved(name))
            {
                Fail(tokens[0].location, "cannot redefine reserved macro " + std::string(name));
            }
            Macro macro{ ++m_nextMacro };
            size_t p = 1;
            if (p < tokens.size() && Is(tokens[p], "(") && !(tokens[p].flags & LeadingSpace))
            {
                macro.function = true;
                ++p;
                if (p < tokens.size() && !Is(tokens[p], ")"))
                {
                    for (;;)
                    {
                        if (p == tokens.size())
                        {
                            Fail(tokens[0].location, "unterminated macro parameter list");
                        }
                        if (Is(tokens[p], "..."))
                        {
                            macro.variadic = true;
                            macro.parameters.push_back("__VA_ARGS__");
                            ++p;
                            break;
                        }
                        if (tokens[p].kind != Kind::Identifier)
                        {
                            Fail(tokens[p].location, "expected macro parameter");
                        }
                        std::string parameter = IdentifierName(tokens[p++]);
                        if (parameter == "__VA_ARGS__" || parameter == "__VA_OPT__")
                        {
                            Fail(tokens[p - 1].location, "reserved variadic macro parameter");
                        }
                        if (std::find(macro.parameters.begin(), macro.parameters.end(), parameter) != macro.parameters.end())
                        {
                            Fail(tokens[p - 1].location, "duplicate macro parameter");
                        }
                        macro.parameters.push_back(parameter);
                        if (p == tokens.size() || !Is(tokens[p], ","))
                        {
                            break;
                        }
                        ++p;
                    }
                }
                if (p == tokens.size() || !Is(tokens[p++], ")"))
                {
                    Fail(tokens[0].location, "expected ')' after macro parameters");
                }
            }
            else if (p < tokens.size() && !(tokens[p].flags & LeadingSpace))
            {
                Fail(tokens[p].location, "expected whitespace before object-like macro replacement");
            }

            macro.body.assign(tokens.begin() + p, tokens.end());
            ValidateReplacement(macro, macro.body);

            std::unordered_map<std::string, Macro>::iterator existing = m_macros.find(name);
            if (existing != m_macros.end())
            {
                Macro& old = existing->second;
                bool same = old.function == macro.function && old.variadic == macro.variadic && old.parameters == macro.parameters &&
                    old.body.size() == macro.body.size();
                for (size_t i = 0; same && i < macro.body.size(); ++i)
                {
                    same = Text(old.body[i]) == Text(macro.body[i]) && (!i || !((old.body[i].flags ^ macro.body[i].flags) & LeadingSpace));
                }
                if (!same)
                {
                    Fail(tokens[0].location, "incompatible redefinition of macro " + std::string(name));
                }
                return;
            }
            m_macros.emplace(name, std::move(macro));
        }

        size_t VaOptEnd(AZStd::span<const Token> body, size_t begin)
        {
            if (begin + 1 == body.size() || !Is(body[begin + 1], "("))
            {
                Fail(body[begin].location, "__VA_OPT__ requires parenthesized replacement tokens");
            }
            size_t nesting = 1;
            for (size_t i = begin + 2; i < body.size(); ++i)
            {
                if (Is(body[i], "("))
                {
                    ++nesting;
                }
                else if (Is(body[i], ")") && !--nesting)
                {
                    return i;
                }
            }
            Fail(body[begin].location, "unterminated __VA_OPT__ replacement");
        }

        void ValidateReplacement(const Macro& macro, AZStd::span<const Token> body, bool inVaOpt = false)
        {
            if (!body.empty() && (Is(body.front(), "##") || Is(body.back(), "##")))
            {
                Fail(body.front().location, "'##' cannot appear at either end of a replacement list");
            }
            for (size_t i = 0; i < body.size(); ++i)
            {
                if ((Is(body[i], "__VA_ARGS__") || Is(body[i], "__VA_OPT__")) && !macro.variadic)
                {
                    Fail(body[i].location, "variadic replacement requires a variadic macro");
                }
                if (Is(body[i], "__VA_OPT__"))
                {
                    if (inVaOpt)
                    {
                        Fail(body[i].location, "__VA_OPT__ cannot be nested");
                    }
                    size_t end = VaOptEnd(body, i);
                    ValidateReplacement(macro, body.subspan(i + 2, end - i - 2), true);
                    i = end;
                }
                else if (macro.function && Is(body[i], "#") &&
                         (i + 1 == body.size() || (!Is(body[i + 1], "__VA_OPT__") &&
                          std::find(macro.parameters.begin(), macro.parameters.end(), IdentifierName(body[i + 1])) == macro.parameters.end())))
                {
                    Fail(body[i].location, "'#' must be followed by a macro parameter or __VA_OPT__");
                }
            }
        }

        bool Suppressed(uint32_t set, uint32_t macro) const
        {
            for (; set; set = m_suppression[set].parent)
            {
                if (m_suppression[set].macro == macro)
                {
                    return true;
                }
            }
            return false;
        }

        uint32_t Add(uint32_t set, uint32_t macro)
        {
            if (Suppressed(set, macro))
            {
                return set;
            }
            if (m_suppression.size() >= std::numeric_limits<uint32_t>::max())
            {
                Fail(0, "macro suppression storage limit exceeded");
            }
            if (m_suppression[set].depth >= 256)
            {
                Fail(m_expandingAt, "macro expansion nesting exceeds 256");
            }
            uint32_t id = static_cast<uint32_t>(m_suppression.size());
            m_suppression.push_back({ macro, set, m_suppression[set].depth + 1 });
            return id;
        }

        uint32_t Union(uint32_t a, uint32_t b)
        {
            for (; b; b = m_suppression[b].parent)
            {
                a = Add(a, m_suppression[b].macro);
            }
            return a;
        }

        uint32_t Intersection(uint32_t a, uint32_t b)
        {
            uint32_t result = 0;
            for (; a; a = m_suppression[a].parent)
            {
                if (Suppressed(b, m_suppression[a].macro))
                {
                    result = Add(result, m_suppression[a].macro);
                }
            }
            return result;
        }

        void CheckSize(size_t size, SourceLocationId location)
        {
            if (size > m_request.tokenLimit)
            {
                Fail(location, "preprocessing token limit exceeded; check recursive or exponential macro expansion");
            }
        }

        Token Stringify(AZStd::span<const Token> tokens, const Token& parameter)
        {
            std::string spelling = "\"";
            bool first = true;
            for (const Token& token : tokens)
            {
                if (token.kind == Kind::Placemark)
                {
                    continue;
                }
                if (!first && (token.flags & LeadingSpace))
                {
                    spelling += ' ';
                }
                for (char c : token.spelling)
                {
                    if (token.kind == Kind::String || token.kind == Kind::Character)
                    {
                        if (c == '\\' || c == '"')
                        {
                            spelling += '\\';
                        }
                        else if (c == '\n' || c == '\r')
                        {
                            spelling += c == '\n' ? "\\n" : "\\r";
                            continue;
                        }
                    }
                    spelling += c;
                }
                first = false;
            }
            spelling += '"';
            Token result = Generated(std::move(spelling), Kind::String, parameter.location);
            result.flags = parameter.flags;
            return result;
        }

        Tokens Paste(AZStd::span<const Token> replacement, const Token& invocation)
        {
            Tokens result;
            for (size_t i = 0; i < replacement.size(); ++i)
            {
                Token token = replacement[i];
                if (token.kind != Kind::Placemark && (token.flags & ReplacementPaste))
                {
                    if (result.empty() || i + 1 == replacement.size())
                    {
                        Fail(invocation.location, "invalid token paste");
                    }
                    Token left = result.back();
                    Token right = replacement[++i];
                    result.pop_back();
                    if (left.kind == Kind::Placemark)
                    {
                        token = right;
                    }
                    else if (right.kind == Kind::Placemark)
                    {
                        token = left;
                    }
                    else
                    {
                        std::string spelling = left.spelling + right.spelling;
                        Scanner scanner(m_sources, spelling, invocation.location);
                        token = scanner.Next();
                        if (scanner.Next().kind != Kind::End || Trivia(token))
                        {
                            Fail(invocation.location, "pasting produces an invalid preprocessing token: " + spelling);
                        }
                        token.location = m_sources.Expand(left.location, invocation.location, right.location);
                        token.suppression = Intersection(left.suppression, right.suppression);
                    }
                    token.flags = left.flags & ~ReplacementPaste;
                }
                result.push_back(std::move(token));
            }
            return result;
        }

        Tokens Substitute(const Macro& macro, AZStd::span<const Token> body, const std::vector<Tokens>& arguments,
                          std::vector<Tokens>& expanded, std::vector<bool>& prescanned, const Token& invocation, size_t depth)
        {
            Tokens result;
            bool leadingSpace = false;
            auto prescan = [&](size_t index) -> const Tokens&
            {
                if (!prescanned[index])
                {
                    expanded[index] = Expand(arguments[index], depth + 1);
                    prescanned[index] = true;
                }
                return expanded[index];
            };
            for (size_t i = 0; i < body.size(); ++i)
            {
                Token token = body[i];
                leadingSpace = leadingSpace || (token.flags & LeadingSpace);
                uint8_t flags = token.flags;
                bool stringify = macro.function && Is(token, "#");
                if (stringify)
                {
                    token = body[++i];
                }
                Tokens values;
                if (macro.variadic && Is(token, "__VA_OPT__"))
                {
                    size_t end = VaOptEnd(body, i);
                    if (!prescan(arguments.size() - 1).empty())
                    {
                        values = Paste(Substitute(macro, body.subspan(i + 2, end - i - 2), arguments,
                                                  expanded, prescanned, invocation, depth), invocation);
                    }
                    if (stringify)
                    {
                        values = { Stringify(values, token) };
                    }
                    i = end;
                }
                else
                {
                    auto parameter = std::find(macro.parameters.begin(), macro.parameters.end(), IdentifierName(token));
                    if (macro.function && parameter != macro.parameters.end())
                    {
                        size_t index = parameter - macro.parameters.begin();
                        bool paste = (i && Is(body[i - 1], "##")) || (i + 1 < body.size() && Is(body[i + 1], "##"));
                        if (stringify)
                        {
                            values.push_back(Stringify(arguments[index], token));
                        }
                        else
                        {
                            values = paste ? arguments[index] : prescan(index);
                        }
                    }
                    else
                    {
                        if (Is(token, "##"))
                        {
                            token.flags |= ReplacementPaste;
                        }
                        values.push_back(token);
                    }
                }
                if (values.empty())
                {
                    // Keep boundary placemarkers until the enclosing replacement list has been pasted.
                    // In particular, an empty parameter at a VA_OPT boundary must not expose the next token to ##.
                    values.push_back({ {}, token.location, 0, Kind::Placemark, flags });
                }
                values.front().flags = (values.front().flags & ReplacementPaste) | flags;
                if (leadingSpace)
                {
                    values.front().flags |= LeadingSpace;
                }
                if (values.front().kind != Kind::Placemark)
                {
                    leadingSpace = false;
                }
                CheckSize(result.size() + values.size(), invocation.location);
                result.insert(result.end(), values.begin(), values.end());
            }
            return result;
        }

        Tokens OperatorArguments(std::deque<Token>& pending, const Token& token, Token* opening = nullptr, Token* closing = nullptr)
        {
            while (!pending.empty() && Trivia(pending.front()))
            {
                pending.pop_front();
            }
            if (pending.empty() || !Is(pending.front(), "("))
            {
                Fail(token.location, token.spelling + " requires parenthesized arguments");
            }
            if (opening)
            {
                *opening = pending.front();
            }
            pending.pop_front();
            Tokens arguments;
            size_t nesting = 0;
            while (!pending.empty())
            {
                Token next = pending.front();
                pending.pop_front();
                if (Is(next, ")") && !nesting)
                {
                    if (closing)
                    {
                        *closing = next;
                    }
                    return Significant(arguments);
                }
                if (Is(next, "("))
                {
                    if (++nesting > 256)
                    {
                        Fail(token.location, "preprocessing operator nesting exceeds 256");
                    }
                }
                else if (Is(next, ")"))
                {
                    --nesting;
                }
                arguments.push_back(next);
            }
            Fail(token.location, "unterminated " + token.spelling + " argument list");
        }

        std::pair<std::string, bool> HeaderName(const Tokens& tokens, SourceLocationId location)
        {
            if (tokens.size() == 1 && (tokens[0].kind == Kind::HeaderName || tokens[0].kind == Kind::String))
            {
                const std::string& text = tokens[0].spelling;
                bool angled = text.front() == '<';
                if ((angled || text.front() == '"') && text.size() > 2 && text.back() == (angled ? '>' : '"'))
                {
                    return { text.substr(1, text.size() - 2), angled };
                }
            }
            else if (tokens.size() >= 3 && Is(tokens.front(), "<") && Is(tokens.back(), ">"))
            {
                std::string name;
                for (size_t i = 1; i + 1 < tokens.size(); ++i)
                {
                    if (Is(tokens[i], ">"))
                    {
                        Fail(location, "extra tokens after include header name");
                    }
                    if (i > 1 && (tokens[i].flags & LeadingSpace))
                    {
                        name += ' ';
                    }
                    name += tokens[i].spelling;
                }
                return { std::move(name), true };
            }
            Fail(location, "expected a nonempty quoted or angled include name");
        }

        std::string LineFilename(const Token& token)
        {
            std::string_view text = token.spelling;
            if (token.kind != Kind::String || text.back() != '"')
            {
                Fail(token.location, "expected an ordinary string literal for #line filename");
            }
            if (text.substr(0, 2) == "R\"")
            {
                size_t body = text.find('(') + 1;
                size_t delimiter = body - 3;
                return std::string(text.substr(body, text.size() - body - delimiter - 2));
            }
            if (text.front() != '"')
            {
                Fail(token.location, "encoding prefixes are not permitted on #line filenames");
            }
            std::string result;
            for (size_t i = 1; i + 1 < text.size(); ++i)
            {
                if (text[i] != '\\')
                {
                    result += text[i];
                    continue;
                }
                char escape = text[++i];
                static constexpr std::string_view escapes = "abfnrtv\\'\"?";
                static constexpr std::string_view values = "\a\b\f\n\r\t\v\\'\"?";
                size_t simple = escapes.find(escape);
                if (simple != std::string_view::npos)
                {
                    result += values[simple];
                    continue;
                }
                uint32_t value = 0;
                if (escape == 'u' || escape == 'U')
                {
                    size_t digits = escape == 'u' ? 4 : 8;
                    while (digits--)
                    {
                        if (i + 2 >= text.size() || HexDigit(text[++i]) < 0)
                        {
                            Fail(token.location, "invalid universal character escape in #line filename");
                        }
                        value = value * 16 + HexDigit(text[i]);
                    }
                    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                    {
                        Fail(token.location, "invalid Unicode scalar in #line filename");
                    }
                    AppendUtf8(result, value);
                }
                else if (escape == 'x' || (escape >= '0' && escape <= '7'))
                {
                    int base = escape == 'x' ? 16 : 8;
                    size_t begin = escape == 'x' ? i + 1 : i;
                    size_t end = begin;
                    while (end + 1 < text.size() && HexDigit(text[end]) >= 0 && HexDigit(text[end]) < base &&
                           (base == 16 || end - begin < 3))
                    {
                        ++end;
                    }
                    auto parsed = std::from_chars(text.data() + begin, text.data() + end, value, base);
                    if (parsed.ec != std::errc{} || value > 255)
                    {
                        Fail(token.location, "invalid numeric escape in #line filename");
                    }
                    result += static_cast<char>(value);
                    i = end - 1;
                }
                else
                {
                    Fail(token.location, "unsupported escape in #line filename");
                }
            }
            return result;
        }

        Tokens Expand(AZStd::span<const Token> input, size_t depth = 0)
        {
            if (depth > 256)
            {
                SourceLocationId location = 0;
                if (!input.empty())
                {
                    location = input[0].location;
                }
                Fail(location, "macro argument nesting exceeds 256");
            }
            std::deque<Token> pending(input.begin(), input.end());
            Tokens result;
            while (!pending.empty())
            {
                Token token = pending.front();
                pending.pop_front();
                if (token.kind != Kind::Identifier)
                {
                    result.push_back(token);
                    continue;
                }
                std::string name = IdentifierName(token);
                if (name == "__VA_OPT__" || name == "__VA_ARGS__")
                {
                    Fail(token.location, name + " is only permitted in a variadic macro replacement");
                }
                if (name == "_Pragma" || name == "__has_include" || name == "__has_cpp_attribute")
                {
                    if (name != "_Pragma" && !m_inCondition)
                    {
                        Fail(token.location, name + " is only permitted in a preprocessing conditional");
                    }
                    Token opening, closing;
                    Tokens arguments = OperatorArguments(pending, token, &opening, &closing);
                    if (arguments.empty() || arguments[0].kind != Kind::HeaderName)
                    {
                        arguments = Significant(Expand(arguments, depth + 1));
                    }
                    if (name == "__has_include")
                    {
                        auto header = HeaderName(arguments, token.location);
                        std::string path = FindInclude(header.first, header.second, m_directory);
                        m_unit.m_dependencies.push_back({ header.first, path, token.location });
                        token.kind = Kind::Number;
                        token.spelling = path.empty() ? "0" : "1";
                        result.push_back(token);
                    }
                    else if (name == "__has_cpp_attribute")
                    {
                        if (!((arguments.size() == 1 && arguments[0].kind == Kind::Identifier) ||
                              (arguments.size() == 3 && arguments[0].kind == Kind::Identifier && Is(arguments[1], "::") &&
                               arguments[2].kind == Kind::Identifier)))
                        {
                            Fail(token.location, "expected an attribute name in __has_cpp_attribute");
                        }
                        // C++ language attributes are not AZSL attributes. Never advertise unsupported AZSL semantics.
                        token.kind = Kind::Number;
                        token.spelling = "0";
                        result.push_back(token);
                    }
                    else
                    {
                        if (arguments.size() != 1 || arguments[0].kind != Kind::String)
                        {
                            Fail(token.location, "_Pragma requires a single string literal");
                        }
                        std::string_view literal = arguments[0].spelling;
                        if (literal.substr(0, 2) == "u8")
                        {
                            literal.remove_prefix(2);
                        }
                        else if (literal.front() == 'L' || literal.front() == 'u' || literal.front() == 'U')
                        {
                            literal.remove_prefix(1);
                        }
                        if (literal.back() != '"')
                        {
                            Fail(token.location, "user-defined literal suffixes are not permitted on _Pragma operands");
                        }
                        if (depth)
                        {
                            // Validate the complete operator during prescan, but defer its side effects until rescan.
                            // The surrounding macro may stringify or discard this argument.
                            result.push_back(token);
                            result.push_back(opening);
                            result.insert(result.end(), arguments.begin(), arguments.end());
                            result.push_back(closing);
                            continue;
                        }
                        std::string text;
                        if (literal.substr(0, 2) == "R\"")
                        {
                            // Match Clang's treatment of raw operands: remove delimiters without unescaping the body.
                            size_t body = literal.find('(') + 1;
                            size_t delimiter = body - 3;
                            text = literal.substr(body, literal.size() - body - delimiter - 2);
                        }
                        else
                        {
                            text = Unquote(literal);
                        }
                        Scanner scanner(m_sources, text, token.location);
                        Tokens pragma;
                        for (Token next = scanner.Next(); next.kind != Kind::End; next = scanner.Next())
                        {
                            if (next.kind == Kind::Newline)
                            {
                                Fail(token.location, "newline in _Pragma directive");
                            }
                            pragma.push_back(next);
                        }
                        pragma = Significant(pragma);
                        if (pragma.size() == 1 && Is(pragma[0], "once"))
                        {
                            m_once.insert(m_currentBuffer);
                        }
                        else
                        {
                            result.push_back(Generated("#pragma " + text, Kind::Directive, token.location));
                        }
                        if (!pending.empty())
                        {
                            pending.front().flags |= token.flags & LeadingSpace;
                        }
                    }
                    continue;
                }
                if (name == "__FILE__" || name == "__LINE__")
                {
                    ResolvedSourceLocation location = m_sources.Resolve(token.location);
                    std::string value;
                    if (name == "__FILE__")
                    {
                        value = Quote(location.file);
                    }
                    else
                    {
                        value = std::to_string(location.line);
                    }
                    token.spelling = value;
                    if (name == "__FILE__")
                    {
                        token.kind = Kind::String;
                    }
                    else
                    {
                        token.kind = Kind::Number;
                    }
                    result.push_back(token);
                    continue;
                }

                std::unordered_map<std::string, Macro>::iterator it = m_macros.find(name);
                if (it == m_macros.end() || Suppressed(token.suppression, it->second.id))
                {
                    result.push_back(token);
                    continue;
                }

                const Macro& macro = it->second;
                std::vector<Tokens> arguments;
                uint32_t hide = token.suppression;
                if (macro.function)
                {
                    size_t p = 0;
                    while (p < pending.size() && Trivia(pending[p]))
                    {
                        ++p;
                    }
                    if (p == pending.size() || !Is(pending[p], "("))
                    {
                        result.push_back(token);
                        continue;
                    }
                    pending.erase(pending.begin(), pending.begin() + p + 1);
                    arguments.emplace_back();
                    size_t nesting = 0;
                    for (;;)
                    {
                        if (pending.empty())
                        {
                            Fail(token.location, "unterminated invocation of " + std::string(name));
                        }
                        Token argumentToken = pending.front();
                        pending.pop_front();
                        if (Is(argumentToken, ")") && !nesting)
                        {
                            hide = Intersection(hide, argumentToken.suppression);
                            break;
                        }
                        if (Is(argumentToken, "("))
                        {
                            ++nesting;
                        }
                        else if (Is(argumentToken, ")"))
                        {
                            --nesting;
                        }
                        if (Is(argumentToken, ",") && !nesting && (!macro.variadic || arguments.size() < macro.parameters.size()))
                        {
                            arguments.emplace_back();
                        }
                        else
                        {
                            arguments.back().push_back(argumentToken);
                        }
                    }
                    for (Tokens& argument : arguments)
                    {
                        argument = Significant(argument);
                    }
                    if (macro.parameters.empty() && arguments.size() == 1 && arguments[0].empty())
                    {
                        arguments.clear();
                    }
                    if (macro.variadic && arguments.size() + 1 == macro.parameters.size())
                    {
                        arguments.emplace_back();
                    }
                    if (arguments.size() != macro.parameters.size())
                    {
                        Fail(token.location, "wrong argument count for macro " + std::string(name));
                    }
                }
                if (++m_expansions > m_request.expansionLimit)
                {
                    Fail(token.location, "macro expansion limit exceeded");
                }
                m_expandingAt = token.location;
                hide = Add(hide, macro.id);

                std::vector<Tokens> expanded(arguments.size());
                std::vector<bool> prescanned(arguments.size(), false);
                Tokens pasted = Paste(Substitute(macro, macro.body, arguments, expanded, prescanned, token, depth), token);

                for (Token& t : pasted)
                {
                    t.flags &= ~ReplacementPaste;
                    t.location = m_sources.Expand(t.location, token.location);
                    t.suppression = Union(t.suppression, hide);
                }
                if (!pasted.empty())
                {
                    pasted.front().flags = token.flags;
                }
                if (std::all_of(pasted.begin(), pasted.end(), [](const Token& t) { return t.kind == Kind::Placemark; }) && !pending.empty())
                {
                    pending.front().flags |= token.flags & (LeadingSpace | StartOfLine);
                }
                for (Tokens::reverse_iterator tokenIterator = pasted.rbegin(); tokenIterator != pasted.rend(); ++tokenIterator)
                {
                    if (tokenIterator->kind != Kind::Placemark)
                    {
                        pending.push_front(*tokenIterator);
                    }
                }
                CheckSize(pending.size() + result.size(), token.location);
            }
            return result;
        }

        struct Integer
        {
            uint64_t bits = 0;
            bool unsignedValue = false;
        };

        class Expression
        {
        public:
            Expression(Preprocessor& pp, Tokens tokens, SourceLocationId location)
                : m_pp(pp)
                , m_tokens(std::move(tokens))
                , m_location(location)
            {
            }

            bool Evaluate()
            {
                Integer value = Parse(0, true);
                if (m_pos != m_tokens.size())
                {
                    Error("unexpected token in #if expression");
                }
                return value.bits != 0;
            }

        private:
            [[noreturn]] void Error(const char* message)
            {
                SourceLocationId location = m_location;
                if (m_pos < m_tokens.size())
                {
                    location = m_tokens[m_pos].location;
                }
                m_pp.Fail(location, message);
            }

            std::string_view Peek()
            {
                if (m_pos < m_tokens.size())
                {
                    return m_pp.Text(m_tokens[m_pos]);
                }
                return {};
            }

            static std::string_view Operator(std::string_view text)
            {
                // C++ alternative operator tokens also apply to preprocessing expressions.
                if (text == "and")
                {
                    return "&&";
                }
                if (text == "or")
                {
                    return "||";
                }
                if (text == "not")
                {
                    return "!";
                }
                if (text == "not_eq")
                {
                    return "!=";
                }
                if (text == "bitand")
                {
                    return "&";
                }
                if (text == "bitor")
                {
                    return "|";
                }
                if (text == "xor")
                {
                    return "^";
                }
                if (text == "compl")
                {
                    return "~";
                }
                return text;
            }

            bool Consume(std::string_view text)
            {
                if (Operator(Peek()) != text)
                {
                    return false;
                }
                ++m_pos;
                return true;
            }

            static int Precedence(std::string_view op)
            {
                if (op == "||")
                {
                    return 1;
                }
                if (op == "&&")
                {
                    return 2;
                }
                if (op == "|")
                {
                    return 3;
                }
                if (op == "^")
                {
                    return 4;
                }
                if (op == "&")
                {
                    return 5;
                }
                if (op == "==" || op == "!=")
                {
                    return 6;
                }
                if (op == "<" || op == ">" || op == "<=" || op == ">=")
                {
                    return 7;
                }
                if (op == "<<" || op == ">>")
                {
                    return 8;
                }
                if (op == "+" || op == "-")
                {
                    return 9;
                }
                if (op == "*" || op == "/" || op == "%")
                {
                    return 10;
                }
                return -1;
            }

            Integer Number(std::string_view spelling)
            {
                std::string digits;
                for (size_t i = 0; i < spelling.size(); ++i)
                {
                    if (spelling[i] == '\'')
                    {
                        if (!i || i + 1 == spelling.size() || HexDigit(spelling[i - 1]) < 0 || HexDigit(spelling[i + 1]) < 0)
                        {
                            Error("invalid digit separator in #if");
                        }
                    }
                    else
                    {
                        digits += spelling[i];
                    }
                }
                int base = 10;
                size_t start = 0;
                if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
                {
                    base = 16;
                    start = 2;
                }
                else if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B'))
                {
                    base = 2;
                    start = 2;
                }
                else if (digits.size() > 1 && digits[0] == '0')
                {
                    base = 8;
                }
                Integer value;
                const char* end = digits.data() + digits.size();
                auto parsed = std::from_chars(digits.data() + start, end, value.bits, base);
                if (parsed.ec != std::errc{} || parsed.ptr == digits.data() + start)
                {
                    Error("invalid or overflowing integer in #if");
                }
                const char* suffix = parsed.ptr;
                bool hasUnsigned = false, hasLong = false;
                while (suffix != end)
                {
                    char c = *suffix++;
                    if ((c == 'u' || c == 'U') && !hasUnsigned)
                    {
                        hasUnsigned = true;
                    }
                    else if ((c == 'l' || c == 'L') && !hasLong)
                    {
                        hasLong = true;
                        if (suffix != end && *suffix == c)
                        {
                            ++suffix;
                        }
                    }
                    else
                    {
                        Error("invalid integer suffix in #if");
                    }
                }
                if (base == 10 && !hasUnsigned && value.bits > uint64_t(INT64_MAX))
                {
                    Error("decimal integer exceeds the signed preprocessing range; use an unsigned suffix");
                }
                value.unsignedValue = hasUnsigned || value.bits > uint64_t(INT64_MAX);
                return value;
            }

            Integer Character(std::string_view text)
            {
                size_t quote = text.find('\'');
                if (text.back() != '\'')
                {
                    Error("user-defined character literals are not permitted in #if");
                }
                std::string_view prefix = text.substr(0, quote);
                uint64_t value = 0;
                size_t count = 0;
                for (size_t i = quote + 1; i + 1 < text.size(); ++i)
                {
                    uint64_t c = static_cast<unsigned char>(text[i]);
                    bool numericEscape = false;
                    if (c == '\\')
                    {
                        c = static_cast<unsigned char>(text[++i]);
                        static constexpr std::string_view escapes = "abfnrtv\\'\"?";
                        static constexpr std::string_view values = "\a\b\f\n\r\t\v\\'\"?";
                        size_t escape = escapes.find(static_cast<char>(c));
                        if (escape != std::string_view::npos)
                        {
                            c = values[escape];
                        }
                        else if (c == 'u' || c == 'U')
                        {
                            size_t length = c == 'u' ? 4 : 8;
                            c = 0;
                            while (length--)
                            {
                                if (i + 2 >= text.size() || HexDigit(text[++i]) < 0)
                                {
                                    Error("invalid universal character escape in #if");
                                }
                                c = c * 16 + HexDigit(text[i]);
                            }
                            if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
                            {
                                Error("invalid Unicode scalar in character literal");
                            }
                        }
                        else if (c == 'x')
                        {
                            const char* begin = text.data() + i + 1;
                            auto parsed = std::from_chars(begin, text.data() + text.size() - 1, c, 16);
                            if (parsed.ec != std::errc{} || parsed.ptr == begin)
                            {
                                Error("invalid hexadecimal character escape");
                            }
                            i = parsed.ptr - text.data() - 1;
                            numericEscape = true;
                        }
                        else if (c >= '0' && c <= '7')
                        {
                            c -= '0';
                            size_t digits = 1;
                            while (digits++ < 3 && i + 2 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '7')
                            {
                                c = c * 8 + text[++i] - '0';
                            }
                            numericEscape = true;
                        }
                        else
                        {
                            Error("unsupported character escape in #if");
                        }
                    }
                    else if (c >= 128)
                    {
                        size_t remaining = c >= 0xf0 ? 3 : (c >= 0xe0 ? 2 : 1);
                        uint32_t minimum = remaining == 3 ? 0x10000 : (remaining == 2 ? 0x800 : 0x80);
                        if (c < 0xc2 || c > 0xf4)
                        {
                            Error("invalid UTF-8 character literal");
                        }
                        c &= (1u << (6 - remaining)) - 1;
                        while (remaining--)
                        {
                            if (i + 2 >= text.size() || (static_cast<unsigned char>(text[++i]) & 0xc0) != 0x80)
                            {
                                Error("invalid UTF-8 character literal");
                            }
                            c = (c << 6) | (text[i] & 0x3f);
                        }
                        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
                        {
                            Error("invalid Unicode scalar in character literal");
                        }
                    }
                    // AZSL uses UTF-8, signed 8-bit char and 32-bit wchar_t on every host.
                    // Multi-character ordinary constants pack up to four bytes, most significant byte first.
                    uint64_t maximum = prefix.empty() || prefix == "u8" ? 255 : (prefix == "u" ? 65535 : UINT32_MAX);
                    if (c > maximum || (prefix.empty() && !numericEscape && c > 127) ||
                        (prefix == "u8" && !numericEscape && c > 127) || (prefix == "u" && c >= 0xd800 && c <= 0xdfff))
                    {
                        Error("character is not representable in the literal's encoding");
                    }
                    if (++count > (prefix.empty() ? 4u : 1u))
                    {
                        Error("too many characters in character literal");
                    }
                    value = prefix.empty() ? (value << 8) | c : c;
                }
                if (!count)
                {
                    Error("empty character constant");
                }
                if (prefix.empty() && count == 1)
                {
                    value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(value)));
                }
                else if (prefix.empty() || prefix == "L")
                {
                    value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));
                }
                return { value, prefix == "u" || prefix == "U" || prefix == "u8" };
            }

            Integer Atom(bool evaluate)
            {
                if (++m_depth > 256)
                {
                    Error("#if expression nesting exceeds 256");
                }
                struct DepthGuard
                {
                    size_t& depth;
                    ~DepthGuard()
                    {
                        --depth;
                    }
                } guard{ m_depth };
                if (Consume("("))
                {
                    Integer value = Parse(0, evaluate);
                    if (!Consume(")"))
                    {
                        Error("expected ')' in #if");
                    }
                    return value;
                }
                if (Consume("!"))
                {
                    return { !Atom(evaluate).bits, false };
                }
                if (Consume("~"))
                {
                    Integer value = Atom(evaluate);
                    value.bits = ~value.bits;
                    return value;
                }
                if (Consume("+"))
                {
                    return Atom(evaluate);
                }
                if (Consume("-"))
                {
                    Integer value = Atom(evaluate);
                    if (evaluate && !value.unsignedValue && value.bits == uint64_t(INT64_MIN))
                    {
                        Error("signed overflow in #if");
                    }
                    value.bits = 0 - value.bits;
                    return value;
                }
                if (m_pos == m_tokens.size())
                {
                    Error("expected operand in #if");
                }
                Token token = m_tokens[m_pos++];
                std::string_view text = m_pp.Text(token);
                if (token.kind == Kind::Identifier)
                {
                    return { text == "true", false };
                }
                if (token.kind == Kind::Character)
                {
                    return Character(text);
                }
                if (token.kind != Kind::Number)
                {
                    Error("expected integer constant in #if");
                }
                return Number(text);
            }

            Integer Parse(int minimum, bool evaluate)
            {
                if (++m_depth > 256)
                {
                    Error("#if expression nesting exceeds 256");
                }
                Integer left = Atom(evaluate);
                for (;;)
                {
                    std::string_view op = Operator(Peek());
                    int precedence = Precedence(op);
                    if (precedence < minimum)
                    {
                        break;
                    }
                    ++m_pos;
                    bool rhsEvaluate = evaluate && !(op == "&&" && !left.bits) && !(op == "||" && left.bits);
                    Integer right = Parse(precedence + 1, rhsEvaluate);
                    bool leftUnsigned = left.unsignedValue;
                    bool unsignedValue = leftUnsigned || right.unsignedValue;
                    uint64_t a = left.bits, b = right.bits;
                    int64_t sa = static_cast<int64_t>(a), sb = static_cast<int64_t>(b);
                    if (op == "||")
                    {
                        left = { bool(a) || bool(b), false };
                    }
                    else if (op == "&&")
                    {
                        left = { bool(a) && bool(b), false };
                    }
                    else if (op == "==")
                    {
                        left = { a == b, false };
                    }
                    else if (op == "!=")
                    {
                        left = { a != b, false };
                    }
                    else if (op == "<")
                    {
                        if (unsignedValue)
                        {
                            left = { a < b, false };
                        }
                        else
                        {
                            left = { sa < sb, false };
                        }
                    }
                    else if (op == ">")
                    {
                        if (unsignedValue)
                        {
                            left = { a > b, false };
                        }
                        else
                        {
                            left = { sa > sb, false };
                        }
                    }
                    else if (op == "<=")
                    {
                        if (unsignedValue)
                        {
                            left = { a <= b, false };
                        }
                        else
                        {
                            left = { sa <= sb, false };
                        }
                    }
                    else if (op == ">=")
                    {
                        if (unsignedValue)
                        {
                            left = { a >= b, false };
                        }
                        else
                        {
                            left = { sa >= sb, false };
                        }
                    }
                    else
                    {
                        left.unsignedValue = unsignedValue;
                        if (op == "+")
                        {
                            left.bits = a + b;
                            if (evaluate && !unsignedValue && ((sa < 0) == (sb < 0)) &&
                                ((static_cast<int64_t>(left.bits) < 0) != (sa < 0)))
                            {
                                Error("signed addition overflow in #if");
                            }
                        }
                        else if (op == "-")
                        {
                            left.bits = a - b;
                            if (evaluate && !unsignedValue && ((sa < 0) != (sb < 0)) &&
                                ((static_cast<int64_t>(left.bits) < 0) != (sa < 0)))
                            {
                                Error("signed subtraction overflow in #if");
                            }
                        }
                        else if (op == "*")
                        {
                            uint64_t magnitudeA = sa < 0 ? 0 - a : a;
                            uint64_t magnitudeB = sb < 0 ? 0 - b : b;
                            uint64_t maximum = uint64_t(INT64_MAX) + ((sa < 0) != (sb < 0));
                            if (evaluate && !unsignedValue && magnitudeB && magnitudeA > maximum / magnitudeB)
                            {
                                Error("signed multiplication overflow in #if");
                            }
                            left.bits = a * b;
                        }
                        else if (op == "&")
                        {
                            left.bits = a & b;
                        }
                        else if (op == "|")
                        {
                            left.bits = a | b;
                        }
                        else if (op == "^")
                        {
                            left.bits = a ^ b;
                        }
                        else if (op == "<<" || op == ">>")
                        {
                            // Shift type is the promoted left operand, independent of the right operand.
                            left.unsignedValue = leftUnsigned;
                            if (evaluate && b >= 64)
                            {
                                Error("shift count is outside [0, 63] in #if");
                            }
                            if (!evaluate || b >= 64)
                            {
                                left.bits = 0;
                            }
                            else if (op == "<<")
                            {
                                left.bits = a << b;
                            }
                            else if (left.unsignedValue)
                            {
                                left.bits = a >> b;
                            }
                            else
                            {
                                left.bits = static_cast<uint64_t>(sa >> b);
                            }
                        }
                        else if (op == "/" || op == "%")
                        {
                            if (evaluate && !b)
                            {
                                Error("division by zero in #if");
                            }
                            if (!evaluate || !b)
                            {
                                left.bits = 0;
                            }
                            else if (unsignedValue)
                            {
                                if (op == "/")
                                {
                                    left.bits = a / b;
                                }
                                else
                                {
                                    left.bits = a % b;
                                }
                            }
                            else if (sa == INT64_MIN && sb == -1)
                            {
                                Error("signed division overflow in #if");
                            }
                            else
                            {
                                if (op == "/")
                                {
                                    left.bits = static_cast<uint64_t>(sa / sb);
                                }
                                else
                                {
                                    left.bits = static_cast<uint64_t>(sa % sb);
                                }
                            }
                        }
                    }
                }
                if (!minimum && Consume("?"))
                {
                    Integer yes = Parse(0, evaluate && left.bits);
                    if (!Consume(":"))
                    {
                        Error("expected ':' in conditional expression");
                    }
                    Integer no = Parse(0, evaluate && !left.bits);
                    uint64_t selectedValue = no.bits;
                    if (left.bits)
                    {
                        selectedValue = yes.bits;
                    }
                    left = { selectedValue, yes.unsignedValue || no.unsignedValue };
                }
                --m_depth;
                return left;
            }

            Preprocessor& m_pp;
            Tokens m_tokens;
            SourceLocationId m_location;
            size_t m_pos = 0, m_depth = 0;
        };

        bool Defined(const std::string& name) const
        {
            return name == "__LINE__" || name == "__FILE__" || name == "__has_include" || name == "__has_cpp_attribute" ||
                name == "_Pragma" || m_macros.find(name) != m_macros.end();
        }

        bool Condition(const Tokens& tokens, SourceLocationId loc)
        {
            Tokens protectedTokens;
            for (size_t i = 0; i < tokens.size(); ++i)
            {
                Token token = tokens[i];
                if (!Is(token, "defined"))
                {
                    protectedTokens.push_back(token);
                    continue;
                }
                bool paren = i + 1 < tokens.size() && Is(tokens[i + 1], "(");
                if (paren)
                {
                    ++i;
                }
                if (++i == tokens.size() || tokens[i].kind != Kind::Identifier)
                {
                    Fail(token.location, "defined requires an identifier");
                }
                bool value = Defined(IdentifierName(tokens[i]));
                if (paren && (++i == tokens.size() || !Is(tokens[i], ")")))
                {
                    Fail(token.location, "expected ')' after defined");
                }
                std::string valueText = "0";
                if (value)
                {
                    valueText = "1";
                }
                protectedTokens.push_back(Generated(valueText, Kind::Number, token.location));
            }
            m_inCondition = true;
            Tokens expanded = Significant(Expand(protectedTokens));
            m_inCondition = false;
            return Expression(*this, std::move(expanded), loc).Evaluate();
        }

        std::string FindInclude(const std::string& spelling, bool angled, const std::filesystem::path& directory) const
        {
            std::vector<std::filesystem::path> candidates;
            if (std::filesystem::path(spelling).is_absolute())
            {
                candidates.emplace_back(spelling);
            }
            else
            {
                if (!angled)
                {
                    candidates.push_back(directory / spelling);
                }
                for (const std::string& include : m_request.includeDirectories)
                {
                    candidates.push_back(std::filesystem::path(include) / spelling);
                }
            }
            for (std::filesystem::path& candidate : candidates)
            {
                std::error_code error;
                if (!std::filesystem::is_regular_file(candidate, error))
                {
                    continue;
                }
                // Do not canonicalize the resolution path: aliases can change relative includes.
                return candidate.generic_string();
            }
            return {};
        }

        void Include(const std::string& spelling, bool angled, const std::filesystem::path& directory, SourceLocationId loc, size_t depth)
        {
            std::string path = FindInclude(spelling, angled, directory);
            if (path.empty())
            {
                Fail(loc, "include file not found: " + spelling);
            }
            m_unit.m_dependencies.push_back({ spelling, path, loc });
            Process(LoadFile(path, loc), path, loc, depth + 1);
        }

        void Append(AZStd::span<const Token> tokens)
        {
            SourceLocationId location = 0;
            if (!tokens.empty())
            {
                location = tokens[0].location;
            }
            CheckSize(m_unit.m_tokens.size() + tokens.size(), location);
            for (Token token : tokens)
            {
                // Expansion is complete.
                // Do not publish IDs into the temporary suppression-set table. Source and expansion location IDs stay valid.
                token.suppression = 0;
                if (m_request.preserveComments || token.kind != Kind::Comment)
                {
                    m_unit.m_tokens.push_back(token);
                }
                else
                {
                    m_unit.m_tokens.push_back(Generated(" ", Kind::Space, token.location));
                }
            }
        }

        void Process(BufferId buffer, const std::string& path, SourceLocationId includedAt, size_t depth)
        {
            if (m_once.count(buffer))
            {
                return;
            }
            if (depth > m_request.includeLimit)
            {
                Fail(includedAt, "include nesting limit exceeded while opening " + path);
            }
            struct SourceFrame
            {
                Preprocessor& preprocessor;
                BufferId buffer;
                std::filesystem::path directory;
                ~SourceFrame()
                {
                    preprocessor.m_currentBuffer = buffer;
                    preprocessor.m_directory = std::move(directory);
                }
            } frame{ *this, m_currentBuffer, m_directory };
            m_currentBuffer = buffer;
            m_directory = path == "<stdin>" ? std::filesystem::current_path() : std::filesystem::path(path).parent_path();
            SourceInstanceId instance = m_sources.EnterSource(buffer, path, includedAt);
            Scanner scanner(m_sources, buffer, instance);
            std::vector<Conditional> conditionals;
            Tokens pending;
            std::function<bool()> active = [&]
            {
                return conditionals.empty() || conditionals.back().active;
            };
            std::function<void()> flush = [&]
            {
                if (!pending.empty())
                {
                    Tokens expanded;
                    if (m_request.preprocessed)
                    {
                        expanded = std::move(pending);
                    }
                    else
                    {
                        expanded = Expand(pending);
                    }
                    Append(expanded);
                    pending.clear();
                }
            };
            for (;;)
            {
                Tokens line;
                Token ending;
                size_t directiveTokens = 0;
                bool includeLine = false;
                size_t hasInclude = 0;
                for (;;)
                {
                    ending = scanner.Next((includeLine && directiveTokens == 2) || hasInclude == 2);
                    // Count discarded directives and inactive text too.
                    // An exponentially branching include graph can otherwise exhaust memory without output.
                    CheckSize(++m_scannedTokens, ending.location);
                    if (ending.kind == Kind::End || ending.kind == Kind::Newline)
                    {
                        break;
                    }
                    if (!Trivia(ending))
                    {
                        if (hasInclude == 1 && Is(ending, "("))
                        {
                            hasInclude = 2;
                        }
                        else
                        {
                            hasInclude = Is(ending, "__has_include") ? 1 : 0;
                        }
                        ++directiveTokens;
                        if (directiveTokens == 1)
                        {
                            includeLine = Is(ending, "#");
                        }
                        else if (directiveTokens == 2)
                        {
                            includeLine = includeLine && Is(ending, "include");
                        }
                    }
                    line.push_back(ending);
                }
                if (m_request.preprocessed)
                {
                    // Legacy input permits line markers immediately after a language token.
                    Tokens::iterator marker = std::find_if(
                        line.begin(),
                        line.end(),
                        [&](const Token& t)
                        {
                            return t.kind == Kind::Punctuation && Is(t, "#");
                        });
                    if (marker != line.end() && marker != line.begin())
                    {
                        pending.insert(pending.end(), line.begin(), marker);
                        line.erase(line.begin(), marker);
                    }
                }
                Tokens tokens = Significant(line);
                bool directive = !tokens.empty() && Is(tokens[0], "#");
                if (!directive)
                {
                    if (active())
                    {
                        pending.insert(pending.end(), line.begin(), line.end());
                        if (ending.kind != Kind::End)
                        {
                            pending.push_back(ending);
                        }
                    }
                }
                else
                {
                    flush();
                    SourceLocationId location = tokens[0].location;
                    if (tokens.size() > 1)
                    {
                        std::string name = Text(tokens[1]);
                        Tokens args(tokens.begin() + 2, tokens.end());
                        if (m_request.preprocessed && name != "line" && tokens[1].kind != Kind::Number && name != "pragma")
                        {
                            Fail(location, "directive requires native preprocessing: " + std::string(name));
                        }
                        if (name == "if" || name == "ifdef" || name == "ifndef")
                        {
                            bool parent = active(), value = false;
                            if (name == "if")
                            {
                                if (parent)
                                {
                                    value = Condition(args, location);
                                }
                            }
                            else
                            {
                                if (parent && (args.size() != 1 || args[0].kind != Kind::Identifier))
                                {
                                    Fail(location, "expected a single identifier after #" + std::string(name));
                                }
                                value = parent && (Defined(IdentifierName(args[0])) != (name == "ifndef"));
                            }
                            if (conditionals.size() >= 256)
                            {
                                Fail(location, "conditional nesting exceeds 256");
                            }
                            conditionals.push_back({ parent, value, value, false, location });
                        }
                        else if (name == "elif" || name == "else" || name == "endif")
                        {
                            if (conditionals.empty())
                            {
                                Fail(location, "unmatched #" + std::string(name));
                            }
                            Conditional& conditional = conditionals.back();
                            if (name == "endif")
                            {
                                if (!args.empty())
                                {
                                    Fail(location, "extra tokens after #endif");
                                }
                                conditionals.pop_back();
                            }
                            else
                            {
                                if (conditional.hadElse)
                                {
                                    Fail(location, "#" + std::string(name) + " after #else");
                                }
                                bool eligible = conditional.parent && !conditional.taken;
                                if (name == "else")
                                {
                                    if (!args.empty())
                                    {
                                        Fail(location, "extra tokens after #else");
                                    }
                                    conditional.hadElse = true;
                                    conditional.active = eligible;
                                }
                                else
                                {
                                    conditional.active = eligible && Condition(args, location);
                                }
                                conditional.taken = conditional.taken || conditional.active;
                            }
                        }
                        else if (active())
                        {
                            if (name == "define")
                            {
                                Define(args);
                            }
                            else if (name == "undef")
                            {
                                if (args.size() != 1 || args[0].kind != Kind::Identifier)
                                {
                                    Fail(location, "expected a single macro name after #undef");
                                }
                                if (Reserved(IdentifierName(args[0])))
                                {
                                    Fail(location, "cannot undefine reserved preprocessing name");
                                }
                                m_macros.erase(IdentifierName(args[0]));
                            }
                            else if (name == "include")
                            {
                                Tokens expanded;
                                if (!args.empty() && args[0].kind == Kind::HeaderName)
                                {
                                    expanded = args;
                                }
                                else
                                {
                                    expanded = Significant(Expand(args));
                                }
                                auto header = HeaderName(expanded, location);
                                Include(header.first, header.second, m_directory, location, depth);
                            }
                            else if (name == "line" || tokens[1].kind == Kind::Number)
                            {
                                if (tokens[1].kind == Kind::Number)
                                {
                                    args.insert(args.begin(), tokens[1]);
                                }
                                Tokens expanded;
                                if (m_request.preprocessed)
                                {
                                    expanded = args;
                                }
                                else
                                {
                                    expanded = Significant(Expand(args));
                                }
                                size_t value = 0;
                                if (expanded.empty())
                                {
                                    Fail(location, "expected line number");
                                }
                                std::string_view number = Text(expanded[0]);
                                std::from_chars_result parsed = std::from_chars(number.data(), number.data() + number.size(), value);
                                if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || !value || value > INT32_MAX)
                                {
                                    Fail(location, "invalid #line number");
                                }
                                std::optional<std::string> filename;
                                if (expanded.size() > 1)
                                {
                                    if (expanded[1].kind != Kind::String)
                                    {
                                        Fail(location, "expected #line filename");
                                    }
                                    filename = LineFilename(expanded[1]);
                                }
                                if (name == "line" && expanded.size() > 2)
                                {
                                    Fail(location, "extra tokens after #line");
                                }
                                m_sources.Remap(instance, scanner.Offset(), value, std::move(filename));
                            }
                            else if (name == "pragma")
                            {
                                if (!m_request.preprocessed && args.size() == 1 && Is(args[0], "once"))
                                {
                                    m_once.insert(buffer);
                                }
                                else
                                {
                                    // One directive token retains exact ordinary pragma spelling for ANTLR and emission.
                                    std::string spelling;
                                    for (const Token& token : line)
                                    {
                                        spelling += Text(token);
                                    }
                                    Token pragma = Generated(spelling, Kind::Directive, location);
                                    Append(AZStd::span<const Token>(&pragma, 1));
                                    if (ending.kind != Kind::End)
                                    {
                                        Append(AZStd::span<const Token>(&ending, 1));
                                    }
                                }
                            }
                            else if (name == "error" || name == "warning")
                            {
                                std::string message;
                                for (const Token& token : args)
                                {
                                    if (!message.empty() && (token.flags & LeadingSpace))
                                    {
                                        message += ' ';
                                    }
                                    message += Text(token);
                                }
                                if (name == "error")
                                {
                                    Fail(location, "#error " + message);
                                }
                                const ResolvedSourceLocation resolved = m_sources.Resolve(location);
                                m_unit.m_diagnostics.push_back(
                                    std::string(resolved.file) + "(" + std::to_string(resolved.line) + "," +
                                    std::to_string(resolved.column) + ") : warning #601: " + message);
                            }
                            else
                            {
                                Fail(location, "unsupported preprocessing directive #" + std::string(name));
                            }
                        }
                    }
                }
                if (ending.kind == Kind::End)
                {
                    flush();
                    if (!conditionals.empty())
                    {
                        Fail(conditionals.back().location, "unterminated conditional directive");
                    }
                    if (!depth)
                    {
                        Append(AZStd::span<const Token>(&ending, 1));
                    }
                    break;
                }
                CheckSize(pending.size(), ending.location);
            }
        }

        CompilationUnit& m_unit;
        SourceManager& m_sources;
        const PreprocessRequest& m_request;
        std::unordered_map<std::string, Macro> m_macros;
        std::vector<Suppression> m_suppression;
        std::unordered_set<BufferId> m_once;
        uint32_t m_nextMacro = 0;
        size_t m_expansions = 0;
        size_t m_scannedTokens = 0;
        SourceLocationId m_expandingAt = 0;
        SourceLocationId m_builtinLocation = 0;
        BufferId m_currentBuffer = 0;
        std::filesystem::path m_directory;
        bool m_inCondition = false;
    };

    PreprocessResult CompilationUnit::Preprocess(const PreprocessRequest& request)
    {
        if (m_started)
        {
            throw std::logic_error("a compilation unit can only be preprocessed once");
        }
        m_started = true;
        Preprocessor(*this, request).Run();
        return { m_tokens, m_dependencies, m_diagnostics };
    }
    void CompilationUnit::WritePreprocessed(std::ostream& output) const
    {
        std::string file;
        size_t line = 0;
        bool separated = true, lineStart = true;
        const Token* previous = nullptr;
        SourceManager spellingSource;
        BufferId buffer = spellingSource.AddSource("<export>", "");
        SourceLocationId spellingLocation = spellingSource.Locate(spellingSource.EnterSource(buffer, "<export>"), 0);
        for (const Token& token : m_tokens)
        {
            if (token.kind == Kind::End)
            {
                break;
            }
            const std::string& text = token.spelling;
            if (!Trivia(token))
            {
                ResolvedSourceLocation location = sources.Resolve(token.location);
                if (location.file != file || (lineStart && location.line != line))
                {
                    if (!lineStart)
                    {
                        output << '\n';
                    }
                    output << "#line " << location.line << ' ' << Quote(location.file) << '\n';
                    file = location.file;
                    line = location.line;
                    separated = true;
                    lineStart = true;
                }
                if (!separated)
                {
                    bool needsSpace = token.flags & LeadingSpace;
                    if (!needsSpace && previous)
                    {
                        // Re-lex just this boundary, including prefixes, suffixes and pp-numbers. Keep AZSL's [[ adjacency.
                        // Export is the only path that serializes tokens; normal compilation never performs this scan.
                        try
                        {
                            Scanner scanner(spellingSource, previous->spelling + text, spellingLocation);
                            needsSpace = scanner.Next().spelling != previous->spelling || scanner.Next().spelling != text ||
                                scanner.Next().kind != Kind::End;
                        }
                        catch (const PreprocessingError&)
                        {
                            needsSpace = true; // For example, '/' followed by '*' would start an unterminated comment.
                        }
                    }
                    if (needsSpace)
                    {
                        output << ' ';
                    }
                }
            }
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            previous = &token;
            separated = Trivia(token);
            lineStart = token.kind == Kind::Newline;
            if (token.kind == Kind::Directive)
            {
                output << '\n';
                lineStart = true;
                separated = true;
            }
            if (lineStart)
            {
                ++line;
            }
        }
        if (!lineStart)
        {
            output << '\n';
        }
    }
} // namespace AZ::ShaderCompiler
