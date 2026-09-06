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
#include <limits>
#include <unordered_set>

namespace AZ::ShaderCompiler
{
    namespace
    {
        constexpr uint8_t ReplacementPaste = 4; // Only operators from replacement lists execute token pasting.

        static constexpr std::string_view Punctuators[] = {
            "<<=", ">>=", "...", "##", "++", "--", "->", "::", "<<", ">>", "<=", ">=",
            "==",  "!=",  "&&",  "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",
        };

        bool Trivia(const PreprocessingToken& t)
        {
            return t.m_kind == PreprocessingKind::Space
                || t.m_kind == PreprocessingKind::Comment
                || t.m_kind == PreprocessingKind::Newline;
        }

        bool Digit(char c)
        {
            return c >= '0'
                && c <= '9';
        }

        bool IsIdentifierStart(char c)
        {
            return (c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || c == '_'
                || static_cast<unsigned char>(c) >= 128;
        }

        std::string Quote(std::string_view text)
        {
            std::string result = "\"";
            for (char c : text)
            {
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

        std::vector<PreprocessingToken> Significant(AZStd::span<const PreprocessingToken> tokens)
        {
            std::vector<PreprocessingToken> result;
            bool space = false;
            for (PreprocessingToken t : tokens)
            {
                if (Trivia(t))
                {
                    space = true;
                    continue;
                }
                if (t.m_kind == PreprocessingKind::End)
                {
                    continue;
                }
                if (space)
                {
                    t.m_flags |= LeadingSpace;
                }
                result.push_back(t);
                space = false;
            }
            return result;
        }

        // Translation phase 2 is performed through cursor lookahead.
        // Unspliced tokens borrow their original bytes.
        // Only a token straddling a splice needs generated spelling.
        class Scanner
        {
        public:
            Scanner(SourceManager& sources, BufferId buffer, SourceInstanceId instance)
                : m_sources(sources)
                , m_buffer(buffer)
                , m_instance(instance)
                , m_text(sources.BufferText(buffer))
            {
                if (m_text.substr(0, 3) == "\xef\xbb\xbf")
                {
                    m_pos = 3;
                }
            }

            Scanner(SourceManager& sources, TextSpan span, SourceLocationId location)
                : m_sources(sources)
                , m_buffer(span.m_buffer)
                , m_instance(0)
                , m_text(sources.Text(span))
                , m_baseOffset(span.m_offset)
                , m_fixedLocation(location)
            {
            }

            uint32_t Offset() const
            {
                return static_cast<uint32_t>(m_pos);
            }

            PreprocessingToken Next(bool expectHeaderName = false)
            {
                SkipSplices(m_pos);
                size_t start = m_pos;
                PreprocessingKind kind = PreprocessingKind::Punctuation;
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
                    return Make(PreprocessingKind::End, start);
                }
                if (c == '\r' || c == '\n')
                {
                    Take();
                    if (c == '\r' && Peek() == '\n')
                    {
                        Take();
                    }
                    PreprocessingToken token = Make(PreprocessingKind::Newline, start);
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
                    kind = PreprocessingKind::Comment;
                }
                else if (c == '/' && Peek(1) == '/')
                {
                    while (Peek() && Peek() != '\n' && Peek() != '\r')
                    {
                        Take();
                    }
                    kind = PreprocessingKind::Comment;
                }
                else if (c == ' ' || c == '\t' || c == '\v' || c == '\f')
                {
                    do
                    {
                        Take();
                        c = Peek();
                    } while (c == ' ' || c == '\t' || c == '\v' || c == '\f');
                    kind = PreprocessingKind::Space;
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
                    kind = PreprocessingKind::HeaderName;
                }
                else if (IsIdentifierStart(c) && !(c == 'L' && Peek(1) == '\''))
                {
                    do
                    {
                        Take();
                    } while (IsIdentifierStart(Peek()) || Digit(Peek()));
                    kind = PreprocessingKind::Identifier;
                }
                else if (Digit(c) || (c == '.' && Digit(Peek(1))))
                {
                    char previous = Take();
                    while (IsIdentifierStart(Peek())
                            || Digit(Peek())
                            || Peek() == '.'
                            || ((Peek() == '+' || Peek() == '-') && (previous == 'e' || previous == 'E' || previous == 'p' || previous == 'P')))
                    {
                        previous = Take();
                    }
                    kind = PreprocessingKind::Number;
                }
                else if (c == '"' || c == '\'' || (c == 'L' && Peek(1) == '\''))
                {
                    if (c == 'L')
                    {
                        Take();
                        c = '\'';
                    }
                    Take();
                    while (Peek() != c)
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
                    if (c == '"')
                    {
                        kind = PreprocessingKind::String;
                    }
                    else
                    {
                        kind = PreprocessingKind::Character;
                    }
                }
                else
                {
                    size_t length = 1;
                    for (std::string_view punctuator : Punctuators)
                    {
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
                PreprocessingToken token = Make(kind, start);
                if (Trivia(token))
                {
                    m_space = true;
                }
                else
                {
                    m_space = false;
                }
                return token;
            }

        private:
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

            PreprocessingToken Make(PreprocessingKind kind, size_t start)
            {
                TextSpan span{ m_buffer, m_baseOffset + static_cast<uint32_t>(start), static_cast<uint32_t>(m_pos - start) };
                std::string_view text = m_sources.Text(span);
                if (text.find("\\\n") != std::string_view::npos || text.find("\\\r\n") != std::string_view::npos)
                {
                    std::string joined;
                    joined.reserve(text.size());
                    for (size_t p = start; p < m_pos;)
                    {
                        SkipSplices(p);
                        if (p < m_pos)
                        {
                            joined += m_text[p++];
                        }
                    }
                    span = m_sources.StoreGenerated(joined);
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
                return { span, location, 0, kind, flags };
            }

            SourceManager& m_sources;
            BufferId m_buffer;
            SourceInstanceId m_instance;
            std::string_view m_text;
            uint32_t m_baseOffset = 0;
            SourceLocationId m_fixedLocation = 0;
            size_t m_pos = 0;
            bool m_blockComment = false;
            bool m_space = false;
        };
    } // namespace

    bool IsPreprocessingPunctuator(std::string_view spelling)
    {
        return spelling.size() == 1
            || std::find(std::begin(Punctuators), std::end(Punctuators), spelling) != std::end(Punctuators);
    }

    PreprocessingError::PreprocessingError(SourceLocation location, const std::string& message)
        : SourceError(
              [&]
              {
                  const ResolvedSourceLocation resolved = location.Resolve();
                  return std::string(resolved.m_file)
                    + "(" + std::to_string(resolved.m_line)
                    + "," + std::to_string(resolved.m_column)
                    + ") : error #600: "
                    + message
                    + location.Notes();
              }())
    {
    }

    class Preprocessor
    {
        struct Macro
        {
            uint32_t m_id;
        bool m_function = false;
        bool m_variadic = false;
            std::vector<std::string_view> m_parameters;
            std::vector<PreprocessingToken> m_body;
        };

        struct Suppression
        {
        uint32_t m_macro = 0;
        uint32_t m_parent = 0;
        uint32_t m_depth = 0;
        };

        struct Conditional
        {
        bool m_parent;
        bool m_active;
        bool m_taken;
        bool m_hadElse;
            SourceLocationId m_location;
        };

    public:
        Preprocessor(CompilationUnit& unit, const PreprocessRequest& request)
            : m_unit(unit)
            , m_sources(unit.m_sources)
            , m_request(request)
        {
            m_suppression.push_back({});
        }

        void Run()
        {
            DefineBuiltin("__cplusplus", "199711L");
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

            for (const MacroOperation& op : m_request.m_macros)
            {
                if (op.m_undefine)
                {
                    std::vector<PreprocessingToken> name = Significant(LexGenerated(op.m_definition, "<command line>"));
                    if (name.size() != 1 || name[0].m_kind != PreprocessingKind::Identifier)
                    {
                        Fail(0, "-U requires a single macro name");
                    }
                    if (Is(name[0], "__LINE__") || Is(name[0], "__FILE__"))
                    {
                        Fail(name[0].m_location, "cannot undefine location built-in");
                    }
                    m_macros.erase(Text(name[0]));
                    continue;
                }
                size_t equal = op.m_definition.find('=');
                std::string value = "1";
                if (equal != std::string::npos)
                {
                    value = op.m_definition.substr(equal + 1);
                }
                std::string text = op.m_definition.substr(0, equal) + " " + value;
                std::vector<PreprocessingToken> tokens = LexGenerated(std::move(text), "<command line>");
                std::vector<PreprocessingToken> definition = Significant(tokens);
                // Ordered command-line definitions override earlier configuration layers.
                if (!definition.empty() && definition[0].m_kind == PreprocessingKind::Identifier)
                {
                    m_macros.erase(Text(definition[0]));
                }
                Define(definition);
            }

            BufferId root = m_request.m_source;
            if (!root)
            {
                root = m_sources.LoadFile(m_request.m_sourcePath);
            }
            m_rootBuffer = root;
            std::string path = "<stdin>";
            if (!m_request.m_sourcePath.empty())
            {
                path = m_request.m_sourcePath;
            }
            std::filesystem::path directory = std::filesystem::current_path();
            if (!m_request.m_sourcePath.empty())
            {
                directory = std::filesystem::path(path).parent_path();
            }
            if (!m_request.m_preprocessed)
            {
                for (const std::string& include : m_request.m_forcedIncludes)
                {
                    Include(include, false, directory, 0, 0);
                }
            }

            Process(root, path, 0, 0);
            Append(AZStd::span<const PreprocessingToken>(&m_end, 1));
        }

    private:
        [[noreturn]] void Fail(SourceLocationId loc, const std::string& text)
        {
            throw PreprocessingError(m_sources.Location(loc), text);
        }

        std::string_view Text(const PreprocessingToken& t) const
        {
            return m_sources.Text(t.m_spelling);
        }

        bool Is(const PreprocessingToken& t, std::string_view s) const
        {
            return Text(t) == s;
        }

        PreprocessingToken Generated(std::string_view text, PreprocessingKind kind, SourceLocationId location)
        {
            return { m_sources.StoreGenerated(text), location, 0, kind, 0 };
        }

        std::vector<PreprocessingToken> LexGenerated(std::string text, std::string name)
        {
            BufferId buffer = m_sources.AddSource(name, std::move(text));
            Scanner scanner(m_sources, buffer, m_sources.EnterSource(buffer, std::move(name)));
            std::vector<PreprocessingToken> tokens;
            for (;;)
            {
                PreprocessingToken token = scanner.Next();
                if (token.m_kind == PreprocessingKind::End)
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
            Scanner scanner(m_sources, m_sources.StoreGenerated(name + " " + value), m_builtinLocation);
            std::vector<PreprocessingToken> tokens;
            for (;;)
            {
                PreprocessingToken token = scanner.Next();
                if (token.m_kind == PreprocessingKind::End)
                {
                    break;
                }
                tokens.push_back(token);
            }
            Define(Significant(tokens));
        }

        void Define(const std::vector<PreprocessingToken>& tokens)
        {
            if (tokens.empty() || tokens[0].m_kind != PreprocessingKind::Identifier)
            {
                SourceLocationId location = 0;
                if (!tokens.empty())
                {
                    location = tokens[0].m_location;
                }
                Fail(location, "expected macro name");
            }
            const std::string_view name = Text(tokens[0]);
            if (name == "defined" || name == "__FILE__" || name == "__LINE__")
            {
                Fail(tokens[0].m_location, "cannot redefine reserved macro " + std::string(name));
            }
            Macro macro{ ++m_nextMacro };
            size_t p = 1;
            if (p < tokens.size() && Is(tokens[p], "(") && !(tokens[p].m_flags & LeadingSpace))
            {
                macro.m_function = true;
                ++p;
                if (p < tokens.size() && !Is(tokens[p], ")"))
                {
                    for (;;)
                    {
                        if (p == tokens.size())
                        {
                            Fail(tokens[0].m_location, "unterminated macro parameter list");
                        }
                        if (Is(tokens[p], "..."))
                        {
                            macro.m_variadic = true;
                            macro.m_parameters.push_back("__VA_ARGS__");
                            ++p;
                            break;
                        }
                        if (tokens[p].m_kind != PreprocessingKind::Identifier)
                        {
                            Fail(tokens[p].m_location, "expected macro parameter");
                        }
                        std::string_view parameter = Text(tokens[p++]);
                        if (std::find(macro.m_parameters.begin(), macro.m_parameters.end(), parameter) != macro.m_parameters.end())
                        {
                            Fail(tokens[p - 1].m_location, "duplicate macro parameter");
                        }
                        macro.m_parameters.push_back(parameter);
                        if (p == tokens.size() || !Is(tokens[p], ","))
                        {
                            break;
                        }
                        ++p;
                    }
                }
                if (p == tokens.size() || !Is(tokens[p++], ")"))
                {
                    Fail(tokens[0].m_location, "expected ')' after macro parameters");
                }
            }

            macro.m_body.assign(tokens.begin() + p, tokens.end());
            if (!macro.m_body.empty() && (Is(macro.m_body.front(), "##") || Is(macro.m_body.back(), "##")))
            {
                Fail(tokens[0].m_location, "'##' cannot appear at either end of a replacement list");
            }
            for (size_t i = 0; i < macro.m_body.size(); ++i)
            {
                if (macro.m_function && Is(macro.m_body[i], "#") &&
                    (i + 1 == macro.m_body.size() ||
                     std::find(macro.m_parameters.begin(), macro.m_parameters.end(), Text(macro.m_body[i + 1])) == macro.m_parameters.end()))
                {
                    Fail(macro.m_body[i].m_location, "'#' must be followed by a macro parameter");
                }
            }

            std::unordered_map<std::string_view, Macro>::iterator existing = m_macros.find(name);
            if (existing != m_macros.end())
            {
                Macro& old = existing->second;
                bool same = old.m_function == macro.m_function && old.m_variadic == macro.m_variadic && old.m_parameters == macro.m_parameters &&
                    old.m_body.size() == macro.m_body.size();
                for (size_t i = 0; same && i < macro.m_body.size(); ++i)
                {
                    same = Text(old.m_body[i]) == Text(macro.m_body[i]) && (!i || !((old.m_body[i].m_flags ^ macro.m_body[i].m_flags) & LeadingSpace));
                }
                if (!same)
                {
                    Fail(tokens[0].m_location, "incompatible redefinition of macro " + std::string(name));
                }
                return;
            }
            m_macros.emplace(name, std::move(macro));
        }

        bool Suppressed(uint32_t set, uint32_t macro) const
        {
            for (; set; set = m_suppression[set].m_parent)
            {
                if (m_suppression[set].m_macro == macro)
                {
                    return true;
                }
            }
            return false;
        }

        uint32_t AddSuppression(uint32_t set, uint32_t macro)
        {
            if (Suppressed(set, macro))
            {
                return set;
            }
            if (m_suppression.size() >= std::numeric_limits<uint32_t>::max())
            {
                Fail(0, "macro suppression storage limit exceeded");
            }
            if (m_suppression[set].m_depth >= 256)
            {
                Fail(m_expandingAt, "macro expansion nesting exceeds 256");
            }
            uint32_t id = static_cast<uint32_t>(m_suppression.size());
            m_suppression.push_back({ macro, set, m_suppression[set].m_depth + 1 });
            return id;
        }

        uint32_t Union(uint32_t a, uint32_t b)
        {
            for (; b; b = m_suppression[b].m_parent)
            {
                a = AddSuppression(a, m_suppression[b].m_macro);
            }
            return a;
        }

        uint32_t Intersection(uint32_t a, uint32_t b)
        {
            uint32_t result = 0;
            for (; a; a = m_suppression[a].m_parent)
            {
                if (Suppressed(b, m_suppression[a].m_macro))
                {
                    result = AddSuppression(result, m_suppression[a].m_macro);
                }
            }
            return result;
        }

        void CheckSize(size_t size, SourceLocationId location)
        {
            if (size > m_request.m_tokenLimit)
            {
                Fail(location, "preprocessing token limit exceeded; check recursive or exponential macro expansion");
            }
        }

        std::vector<PreprocessingToken> Expand(AZStd::span<const PreprocessingToken> input, size_t depth = 0)
        {
            if (depth > 256)
            {
                SourceLocationId location = 0;
                if (!input.empty())
                {
                    location = input[0].m_location;
                }
                Fail(location, "macro argument nesting exceeds 256");
            }
            std::deque<PreprocessingToken> pending(input.begin(), input.end());
            std::vector<PreprocessingToken> result;
            while (!pending.empty())
            {
                PreprocessingToken token = pending.front();
                pending.pop_front();
                if (token.m_kind != PreprocessingKind::Identifier)
                {
                    result.push_back(token);
                    continue;
                }
                std::string_view name = Text(token);
                if (name == "__FILE__" || name == "__LINE__")
                {
                    ResolvedSourceLocation location = m_sources.Resolve(token.m_location);
                    std::string value;
                    if (name == "__FILE__")
                    {
                        value = Quote(location.m_file);
                    }
                    else
                    {
                        value = std::to_string(location.m_line);
                    }
                    token.m_spelling = m_sources.StoreGenerated(value);
                    if (name == "__FILE__")
                    {
                        token.m_kind = PreprocessingKind::String;
                    }
                    else
                    {
                        token.m_kind = PreprocessingKind::Number;
                    }
                    result.push_back(token);
                    continue;
                }

                std::unordered_map<std::string_view, Macro>::iterator it = m_macros.find(name);
                if (it == m_macros.end() || Suppressed(token.m_suppression, it->second.m_id))
                {
                    result.push_back(token);
                    continue;
                }

                const Macro& macro = it->second;
                std::vector<std::vector<PreprocessingToken>> arguments;
                uint32_t hide = token.m_suppression;
                if (macro.m_function)
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
                            Fail(token.m_location, "unterminated invocation of " + std::string(name));
                        }
                        PreprocessingToken argumentToken = pending.front();
                        pending.pop_front();
                        if (Is(argumentToken, ")") && !nesting)
                        {
                            hide = Intersection(hide, argumentToken.m_suppression);
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
                        if (Is(argumentToken, ",") && !nesting && (!macro.m_variadic || arguments.size() < macro.m_parameters.size()))
                        {
                            arguments.emplace_back();
                        }
                        else
                        {
                            arguments.back().push_back(argumentToken);
                        }
                    }
                    for (std::vector<PreprocessingToken>& argument : arguments)
                    {
                        argument = Significant(argument);
                    }
                    if (macro.m_parameters.empty() && arguments.size() == 1 && arguments[0].empty())
                    {
                        arguments.clear();
                    }
                    if (macro.m_variadic && arguments.size() + 1 == macro.m_parameters.size())
                    {
                        arguments.emplace_back();
                    }
                    if (arguments.size() != macro.m_parameters.size())
                    {
                        Fail(token.m_location, "wrong argument count for macro " + std::string(name));
                    }
                }
                if (++m_expansions > m_request.m_expansionLimit)
                {
                    Fail(token.m_location, "macro expansion limit exceeded");
                }
                m_expandingAt = token.m_location;
                hide = AddSuppression(hide, macro.m_id);

                std::vector<PreprocessingToken> replacement;
                std::vector<std::vector<PreprocessingToken>> expanded(arguments.size());
                std::vector<bool> prescanned(arguments.size(), false);
                bool pendingSpace = false;
                for (size_t i = 0; i < macro.m_body.size(); ++i)
                {
                    const size_t replacementBegin = replacement.size();
                    PreprocessingToken t = macro.m_body[i];
                    if (pendingSpace)
                    {
                        t.m_flags |= LeadingSpace;
                    }
                    const uint8_t replacementFlags = t.m_flags;
                    bool stringify = macro.m_function && Is(t, "#");
                    if (stringify)
                    {
                        t = macro.m_body[++i];
                    }
                    std::vector<std::string_view>::const_iterator parameter =
                        std::find(macro.m_parameters.begin(), macro.m_parameters.end(), Text(t));
                    if (macro.m_function && parameter != macro.m_parameters.end())
                    {
                        size_t index = parameter - macro.m_parameters.begin();
                        if (stringify)
                        {
                            std::string value;
                            for (const PreprocessingToken& argumentToken : arguments[index])
                            {
                                if (!value.empty() && (argumentToken.m_flags & LeadingSpace))
                                {
                                    value += ' ';
                                }
                                value += Text(argumentToken);
                            }
                            PreprocessingToken stringToken = Generated(Quote(value), PreprocessingKind::String, t.m_location);
                            stringToken.m_flags = replacementFlags;
                            replacement.push_back(stringToken);
                        }
                        else
                        {
                            bool paste = (i && Is(macro.m_body[i - 1], "##")) || (i + 1 < macro.m_body.size() && Is(macro.m_body[i + 1], "##"));
                            if (!paste && !prescanned[index])
                            {
                                expanded[index] = Expand(arguments[index], depth + 1);
                                prescanned[index] = true;
                            }
                            std::vector<PreprocessingToken>* values = &expanded[index];
                            if (paste)
                            {
                                values = &arguments[index];
                            }
                            if (values->empty() && paste)
                            {
                                replacement.push_back({ {}, t.m_location, 0, PreprocessingKind::Placemark, t.m_flags });
                            }
                            else
                            {
                                size_t begin = replacement.size();
                                replacement.insert(replacement.end(), values->begin(), values->end());
                                if (replacement.size() > begin)
                                {
                                    replacement[begin].m_flags = t.m_flags;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (Is(t, "##"))
                        {
                            t.m_flags |= ReplacementPaste;
                        }
                        replacement.push_back(t);
                    }
                    pendingSpace = replacement.size() == replacementBegin && (replacementFlags & LeadingSpace);
                    CheckSize(replacement.size(), token.m_location);
                }

                std::vector<PreprocessingToken> pasted;
                for (size_t i = 0; i < replacement.size(); ++i)
                {
                    PreprocessingToken t = replacement[i];
                    if (t.m_kind != PreprocessingKind::Placemark && (t.m_flags & ReplacementPaste))
                    {
                        if (pasted.empty() || i + 1 == replacement.size())
                        {
                            Fail(token.m_location, "invalid token paste");
                        }
                        PreprocessingToken right = replacement[++i];
                        PreprocessingToken left = pasted.back();
                        pasted.pop_back();
                        if (left.m_kind == PreprocessingKind::Placemark)
                        {
                            t = right;
                            t.m_flags = left.m_flags;
                        }
                        else if (right.m_kind == PreprocessingKind::Placemark)
                        {
                            t = left;
                        }
                        else
                        {
                            std::string spelling = std::string(Text(left)) + std::string(Text(right));
                            // Reuse the preprocessing scanner to enforce the single-token paste rule.
                            TextSpan span = m_sources.StoreGenerated(spelling);
                            Scanner scanner(m_sources, span, token.m_location);
                            PreprocessingToken scanned = scanner.Next();
                            if (scanner.Next().m_kind != PreprocessingKind::End || Trivia(scanned))
                            {
                                Fail(token.m_location, "pasting produces an invalid preprocessing token: " + spelling);
                            }
                            t = scanned;
                            t.m_location = m_sources.Expand(left.m_location, token.m_location, right.m_location);
                            t.m_suppression = Intersection(left.m_suppression, right.m_suppression);
                            t.m_flags = left.m_flags;
                        }
                        pasted.push_back(t);
                    }
                    else
                    {
                        pasted.push_back(t);
                    }
                }

                // Invocation spacing applies both before and after removing leading placemarkers.
                if (!pasted.empty())
                {
                    pasted.front().m_flags = token.m_flags;
                }
                size_t significantCount = 0;
                bool placemarkSpace = false;
                for (PreprocessingToken t : pasted)
                {
                    if (t.m_kind == PreprocessingKind::Placemark)
                    {
                        placemarkSpace |= (t.m_flags & LeadingSpace) != 0;
                        continue;
                    }
                    if (placemarkSpace)
                    {
                        t.m_flags |= LeadingSpace;
                        placemarkSpace = false;
                    }
                    t.m_flags &= ~ReplacementPaste;
                    t.m_location = m_sources.Expand(t.m_location, token.m_location);
                    t.m_suppression = Union(t.m_suppression, hide);
                    pasted[significantCount++] = t;
                }
                pasted.resize(significantCount);
                if (!pasted.empty())
                {
                    pasted.front().m_flags = token.m_flags;
                }
                if (pasted.empty() && !pending.empty())
                {
                    pending.front().m_flags |= token.m_flags & LeadingSpace;
                }
                if ((pendingSpace || placemarkSpace) && !pending.empty())
                {
                    pending.front().m_flags |= LeadingSpace;
                }
                for (std::vector<PreprocessingToken>::reverse_iterator tokenIterator = pasted.rbegin(); tokenIterator != pasted.rend(); ++tokenIterator)
                {
                    pending.push_front(*tokenIterator);
                }
                CheckSize(pending.size() + result.size(), token.m_location);
            }
            return result;
        }

        struct Integer
        {
            uint64_t m_bits = 0;
            bool m_unsignedValue = false;
        };

        class Expression
        {
        public:
            Expression(Preprocessor& pp, std::vector<PreprocessingToken> tokens, SourceLocationId location)
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
                return value.m_bits != 0;
            }

        private:
            [[noreturn]] void Error(const char* message)
            {
                SourceLocationId location = m_location;
                if (m_pos < m_tokens.size())
                {
                    location = m_tokens[m_pos].m_location;
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
                if (text == "and_eq")
                {
                    return "&=";
                }
                if (text == "or_eq")
                {
                    return "|=";
                }
                if (text == "xor_eq")
                {
                    return "^=";
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

            Integer Atom(bool evaluate)
            {
                if (++m_depth > 256)
                {
                    Error("#if expression nesting exceeds 256");
                }
                struct DepthGuard
                {
                    size_t& m_depth;
                    ~DepthGuard()
                    {
                        --m_depth;
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
                    return { !Atom(evaluate).m_bits, false };
                }
                if (Consume("~"))
                {
                    Integer value = Atom(evaluate);
                    value.m_bits = ~value.m_bits;
                    return value;
                }
                if (Consume("+"))
                {
                    return Atom(evaluate);
                }
                if (Consume("-"))
                {
                    Integer value = Atom(evaluate);
                    value.m_bits = 0 - value.m_bits;
                    return value;
                }
                if (m_pos == m_tokens.size())
                {
                    Error("expected operand in #if");
                }
                PreprocessingToken token = m_tokens[m_pos++];
                std::string_view text = m_pp.Text(token);
                if (token.m_kind == PreprocessingKind::Identifier)
                {
                    if (Operator(text) != text)
                    {
                        Error("expected operand before alternative operator in #if");
                    }
                    return { text == "true", false };
                }
                if (token.m_kind == PreprocessingKind::Character)
                {
                    uint64_t value = 0;
                    size_t count = 0;
                    const bool wide = text.front() == 'L';
                    size_t contentBegin = 1;
                    if (wide)
                    {
                        contentBegin = 2;
                    }
                    for (size_t i = contentBegin; i + 1 < text.size(); ++i)
                    {
                        uint64_t c = static_cast<unsigned char>(text[i]);
                        if (c == '\\')
                        {
                            c = static_cast<unsigned char>(text[++i]);
                            if (c == 'n')
                            {
                                c = '\n';
                            }
                            else if (c == 'r')
                            {
                                c = '\r';
                            }
                            else if (c == 't')
                            {
                                c = '\t';
                            }
                            else if (c == 'a')
                            {
                                c = '\a';
                            }
                            else if (c == 'b')
                            {
                                c = '\b';
                            }
                            else if (c == 'f')
                            {
                                c = '\f';
                            }
                            else if (c == 'v')
                            {
                                c = '\v';
                            }
                            else if (c == 'x')
                            {
                                const char* begin = text.data() + i + 1;
                                std::from_chars_result parsed = std::from_chars(begin, text.data() + text.size() - 1, c, 16);
                                if (parsed.ec != std::errc{} || parsed.ptr == begin)
                                {
                                    Error("invalid hexadecimal character escape");
                                }
                                i = parsed.ptr - text.data() - 1;
                            }
                            else if (wide && (c == 'u' || c == 'U'))
                            {
                                size_t digits = 4;
                                if (c == 'U')
                                {
                                    digits = 8;
                                }
                                if (text.size() - i - 2 < digits)
                                {
                                    Error("incomplete universal character escape");
                                }
                                const char* first = text.data() + i + 1;
                                const auto parsed = std::from_chars(first, first + digits, c, 16);
                                if (parsed.ec != std::errc{} || parsed.ptr != first + digits)
                                {
                                    Error("invalid universal character escape");
                                }
                                i += digits;
                            }
                            else if (c >= '0' && c <= '7')
                            {
                                c -= '0';
                                size_t digits = 1;
                                while (digits++ < 3 && i + 2 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '7')
                                {
                                    c = c * 8 + text[++i] - '0';
                                }
                            }
                            else if (wide && c != '\\' && c != '\'' && c != '"' && c != '?')
                            {
                                Error("unsupported wide character escape");
                            }
                        }
                        else if (wide && c >= 128)
                        {
                            size_t trailing = 1;
                            if (c >= 0xe0)
                            {
                                trailing = 2;
                            }
                            if (c >= 0xf0)
                            {
                                trailing = 3;
                            }
                            c &= 0x7f >> trailing;
                            for (size_t j = 0; j < trailing; ++j)
                            {
                                if (i + 2 >= text.size())
                                {
                                    Error("invalid UTF-8 character constant");
                                }
                                c = (c << 6) | (static_cast<unsigned char>(text[++i]) & 0x3f);
                            }
                        }
                        ++count;
                        if (wide)
                        {
                            if (count != 1 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
                            {
                                Error("wide character constant requires one Unicode scalar value");
                            }
                            value = c;
                        }
                        else
                        {
                            if (count > 8 || c > 255)
                            {
                                Error("character constant exceeds the AZSL preprocessing integer range");
                            }
                            value = (value << 8) | c;
                        }
                    }
                    if (!count)
                    {
                        Error("empty character constant");
                    }
                    return { value, false };
                }
                if (token.m_kind != PreprocessingKind::Number)
                {
                    Error("expected integer constant in #if");
                }
                int base = 10;
                size_t start = 0;
                if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
                {
                    base = 16;
                    start = 2;
                }
                else if (text.size() > 1 && text[0] == '0')
                {
                    base = 8;
                }
                Integer value;
                std::from_chars_result parsed = std::from_chars(text.data() + start, text.data() + text.size(), value.m_bits, base);
                if (parsed.ec != std::errc{} || parsed.ptr == text.data() + start)
                {
                    Error("invalid or overflowing integer in #if");
                }
                std::string suffix(parsed.ptr, text.data() + text.size());
                for (char& c : suffix)
                {
                    if (c >= 'A' && c <= 'Z')
                    {
                        c += 'a' - 'A';
                    }
                }
                if (suffix != "" && suffix != "u" && suffix != "l" && suffix != "ll" && suffix != "ul" && suffix != "lu" &&
                    suffix != "ull" && suffix != "llu")
                {
                    Error("invalid integer suffix in #if");
                }
                value.m_unsignedValue = suffix.find('u') != std::string::npos || value.m_bits > uint64_t(INT64_MAX);
                return value;
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
                    bool rhsEvaluate = evaluate && !(op == "&&" && !left.m_bits) && !(op == "||" && left.m_bits);
                    Integer right = Parse(precedence + 1, rhsEvaluate);
                    bool leftUnsigned = left.m_unsignedValue;
                    bool unsignedValue = leftUnsigned || right.m_unsignedValue;
            uint64_t a = left.m_bits;
            uint64_t b = right.m_bits;
            int64_t sa = static_cast<int64_t>(a);
            int64_t sb = static_cast<int64_t>(b);
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
                        left.m_unsignedValue = unsignedValue;
                        if (op == "+")
                        {
                            left.m_bits = a + b;
                        }
                        else if (op == "-")
                        {
                            left.m_bits = a - b;
                        }
                        else if (op == "*")
                        {
                            left.m_bits = a * b;
                        }
                        else if (op == "&")
                        {
                            left.m_bits = a & b;
                        }
                        else if (op == "|")
                        {
                            left.m_bits = a | b;
                        }
                        else if (op == "^")
                        {
                            left.m_bits = a ^ b;
                        }
                        else if (op == "<<" || op == ">>")
                        {
                            // Shift type is the promoted left operand, independent of the right operand.
                            left.m_unsignedValue = leftUnsigned;
                            if (evaluate && b >= 64)
                            {
                                Error("shift count is outside [0, 63] in #if");
                            }
                            if (!evaluate || b >= 64)
                            {
                                left.m_bits = 0;
                            }
                            else if (op == "<<")
                            {
                                left.m_bits = a << b;
                            }
                            else if (left.m_unsignedValue)
                            {
                                left.m_bits = a >> b;
                            }
                            else
                            {
                                left.m_bits = static_cast<uint64_t>(sa >> b);
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
                                left.m_bits = 0;
                            }
                            else if (unsignedValue)
                            {
                                if (op == "/")
                                {
                                    left.m_bits = a / b;
                                }
                                else
                                {
                                    left.m_bits = a % b;
                                }
                            }
                            else if (sa == INT64_MIN && sb == -1)
                            {
                                if (op == "/")
                                {
                                    left.m_bits = a;
                                }
                                else
                                {
                                    left.m_bits = 0;
                                }
                            }
                            else
                            {
                                if (op == "/")
                                {
                                    left.m_bits = static_cast<uint64_t>(sa / sb);
                                }
                                else
                                {
                                    left.m_bits = static_cast<uint64_t>(sa % sb);
                                }
                            }
                        }
                    }
                }
                if (!minimum && Consume("?"))
                {
                    Integer yes = Parse(0, evaluate && left.m_bits);
                    if (!Consume(":"))
                    {
                        Error("expected ':' in conditional expression");
                    }
                    Integer no = Parse(0, evaluate && !left.m_bits);
                    uint64_t selectedValue = no.m_bits;
                    if (left.m_bits)
                    {
                        selectedValue = yes.m_bits;
                    }
                    left = { selectedValue, yes.m_unsignedValue || no.m_unsignedValue };
                }
                --m_depth;
                return left;
            }

            Preprocessor& m_pp;
            std::vector<PreprocessingToken> m_tokens;
            SourceLocationId m_location;
        size_t m_pos = 0;
        size_t m_depth = 0;
        };

        bool Defined(std::string_view name) const
        {
            return name == "__LINE__" || name == "__FILE__" || m_macros.find(name) != m_macros.end();
        }

        bool Condition(const std::vector<PreprocessingToken>& tokens, SourceLocationId loc)
        {
            std::vector<PreprocessingToken> protectedTokens;
            for (size_t i = 0; i < tokens.size(); ++i)
            {
                PreprocessingToken token = tokens[i];
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
                if (++i == tokens.size() || tokens[i].m_kind != PreprocessingKind::Identifier)
                {
                    Fail(token.m_location, "defined requires an identifier");
                }
                bool value = Defined(Text(tokens[i]));
                if (paren && (++i == tokens.size() || !Is(tokens[i], ")")))
                {
                    Fail(token.m_location, "expected ')' after defined");
                }
                std::string_view valueText = "0";
                if (value)
                {
                    valueText = "1";
                }
                protectedTokens.push_back(Generated(valueText, PreprocessingKind::Number, token.m_location));
            }
            return Expression(*this, Significant(Expand(protectedTokens)), loc).Evaluate();
        }

        void Include(const std::string& spelling, bool angled, const std::filesystem::path& directory, SourceLocationId loc, size_t depth)
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
                for (const std::string& include : m_request.m_includeDirectories)
                {
                    candidates.push_back(std::filesystem::path(include) / spelling);
                }
            }
            for (std::filesystem::path& candidate : candidates)
            {
                const std::string path = std::filesystem::absolute(candidate).generic_string();
                m_unit.m_dependencies.push_back({ path });
                std::error_code error;
                if (!std::filesystem::is_regular_file(candidate, error))
                {
                    continue;
                }
                // Do not canonicalize the resolution path: aliases can change relative includes.
                Process(m_sources.LoadFile(path), path, loc, depth + 1);
                return;
            }
            Fail(loc, "include file not found: " + spelling);
        }

        void Append(AZStd::span<const PreprocessingToken> tokens)
        {
            SourceLocationId location = 0;
            if (!tokens.empty())
            {
                location = tokens[0].m_location;
            }
            CheckSize(m_unit.m_tokens.size() + tokens.size(), location);
            for (PreprocessingToken token : tokens)
            {
                // Expansion is complete.
                // Do not publish IDs into the temporary suppression-set table. Source and expansion location IDs stay valid.
                token.m_suppression = 0;
                if (m_request.m_preserveComments || token.m_kind != PreprocessingKind::Comment)
                {
                    m_unit.m_tokens.push_back(token);
                }
                else
                {
                    m_unit.m_tokens.push_back(Generated(" ", PreprocessingKind::Space, token.m_location));
                }
            }
        }

        void Process(BufferId buffer, const std::string& path, SourceLocationId includedAt, size_t depth)
        {
            if (m_once.count(buffer))
            {
                return;
            }
            if (depth > m_request.m_includeLimit)
            {
                Fail(includedAt, "include nesting limit exceeded while opening " + path);
            }
            SourceInstanceId instance = m_sources.EnterSource(buffer, path, includedAt);
            Scanner scanner(m_sources, buffer, instance);
            std::vector<Conditional> conditionals;
            std::vector<PreprocessingToken> pending;
            const auto active = [&]
            {
                return conditionals.empty() || conditionals.back().m_active;
            };
            const auto flush = [&]
            {
                if (!pending.empty())
                {
                    std::vector<PreprocessingToken> expanded;
                    if (m_request.m_preprocessed)
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
                std::vector<PreprocessingToken> line;
                PreprocessingToken ending;
                size_t directiveTokens = 0;
                bool includeLine = false;
                for (;;)
                {
                    ending = scanner.Next(active() && includeLine && directiveTokens == 2);
                    // Count discarded directives and inactive text too.
                    // An exponentially branching include graph can otherwise exhaust memory without output.
                    CheckSize(++m_scannedTokens, ending.m_location);
                    if (ending.m_kind == PreprocessingKind::End || ending.m_kind == PreprocessingKind::Newline)
                    {
                        break;
                    }
                    if (!Trivia(ending))
                    {
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
                if (m_request.m_preprocessed)
                {
                    // Legacy input permits line markers immediately after a language token.
                    std::vector<PreprocessingToken>::iterator marker = std::find_if(
                        line.begin(),
                        line.end(),
                        [&](const PreprocessingToken& t)
                        {
                            return t.m_kind == PreprocessingKind::Punctuation && Is(t, "#");
                        });
                    if (marker != line.end() && marker != line.begin())
                    {
                        pending.insert(pending.end(), line.begin(), marker);
                        line.erase(line.begin(), marker);
                    }
                }
                std::vector<PreprocessingToken> tokens = Significant(line);
                bool directive = !tokens.empty() && Is(tokens[0], "#");
                if (!directive)
                {
                    if (active())
                    {
                        pending.insert(pending.end(), line.begin(), line.end());
                        if (ending.m_kind != PreprocessingKind::End)
                        {
                            pending.push_back(ending);
                        }
                    }
                }
                else
                {
                    flush();
                    SourceLocationId location = tokens[0].m_location;
                    if (tokens.size() > 1)
                    {
                        std::string_view name = Text(tokens[1]);
                        std::vector<PreprocessingToken> args(tokens.begin() + 2, tokens.end());
                        if (m_request.m_preprocessed && name != "line" && tokens[1].m_kind != PreprocessingKind::Number && name != "pragma")
                        {
                            Fail(location, "directive requires native preprocessing: " + std::string(name));
                        }
                        if (name == "if" || name == "ifdef" || name == "ifndef")
                        {
                bool parent = active();
                bool value = false;
                            if (name == "if")
                            {
                                if (parent)
                                {
                                    value = Condition(args, location);
                                }
                            }
                            else if (parent)
                            {
                                if (args.size() != 1 || args[0].m_kind != PreprocessingKind::Identifier)
                                {
                                    Fail(location, "expected a single identifier after #" + std::string(name));
                                }
                                value = parent && (Defined(Text(args[0])) != (name == "ifndef"));
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
                                if (conditional.m_hadElse)
                                {
                                    Fail(location, "#" + std::string(name) + " after #else");
                                }
                                bool eligible = conditional.m_parent && !conditional.m_taken;
                                if (name == "else")
                                {
                                    if (!args.empty())
                                    {
                                        Fail(location, "extra tokens after #else");
                                    }
                                    conditional.m_hadElse = true;
                                    conditional.m_active = eligible;
                                }
                                else
                                {
                                    conditional.m_active = eligible && Condition(args, location);
                                }
                                conditional.m_taken = conditional.m_taken || conditional.m_active;
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
                                if (args.size() != 1 || args[0].m_kind != PreprocessingKind::Identifier)
                                {
                                    Fail(location, "expected a single macro name after #undef");
                                }
                                if (Is(args[0], "__LINE__") || Is(args[0], "__FILE__"))
                                {
                                    Fail(location, "cannot undefine location built-in");
                                }
                                m_macros.erase(Text(args[0]));
                            }
                            else if (name == "include")
                            {
                                std::vector<PreprocessingToken> expanded;
                                if (!args.empty() && args[0].m_kind == PreprocessingKind::HeaderName)
                                {
                                    expanded = args;
                                }
                                else
                                {
                                    expanded = Significant(Expand(args));
                                }
                                bool angled = false;
                                std::string spelling;
                                if (expanded.size() == 1 && (expanded[0].m_kind == PreprocessingKind::HeaderName || expanded[0].m_kind == PreprocessingKind::String))
                                {
                                    const std::string_view header = Text(expanded[0]);
                                    angled = header.front() == '<';
                                    spelling = header.substr(1, header.size() - 2);
                                }
                                else if (expanded.size() >= 3 && Is(expanded.front(), "<") && Is(expanded.back(), ">"))
                                {
                                    angled = true;
                                    for (size_t i = 1; i + 1 < expanded.size(); ++i)
                                    {
                                        if (i > 1 && (expanded[i].m_flags & LeadingSpace))
                                        {
                                            spelling += ' ';
                                        }
                                        spelling += Text(expanded[i]);
                                    }
                                }
                                else
                                {
                                    Fail(location, "expected quoted or angled include name");
                                }
                                std::filesystem::path directory = std::filesystem::current_path();
                                if (path != "<stdin>")
                                {
                                    directory = std::filesystem::path(path).parent_path();
                                }
                                Include(spelling, angled, directory, location, depth);
                            }
                            else if (name == "line" || tokens[1].m_kind == PreprocessingKind::Number)
                            {
                                if (tokens[1].m_kind == PreprocessingKind::Number)
                                {
                                    args.insert(args.begin(), tokens[1]);
                                }
                                std::vector<PreprocessingToken> expanded;
                                if (m_request.m_preprocessed)
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
                                std::string filename;
                                if (expanded.size() > 1)
                                {
                                    if (expanded[1].m_kind != PreprocessingKind::String)
                                    {
                                        Fail(location, "expected #line filename");
                                    }
                                    filename = Unquote(Text(expanded[1]));
                                }
                                if (name == "line" && expanded.size() > 2)
                                {
                                    Fail(location, "extra tokens after #line");
                                }
                                m_sources.Remap(instance, scanner.Offset(), value, std::move(filename));
                            }
                            else if (name == "pragma")
                            {
                                if (!m_request.m_preprocessed && args.size() == 1 && Is(args[0], "once"))
                                {
                                    m_once.insert(buffer);
                                }
                                else
                                {
                                    // One directive token retains exact ordinary pragma spelling for ANTLR and emission.
                                    std::string spelling;
                                    for (const PreprocessingToken& token : line)
                                    {
                                        spelling += Text(token);
                                    }
                                    PreprocessingToken pragma = Generated(spelling, PreprocessingKind::Directive, location);
                                    Append(AZStd::span<const PreprocessingToken>(&pragma, 1));
                                    if (ending.m_kind != PreprocessingKind::End)
                                    {
                                        Append(AZStd::span<const PreprocessingToken>(&ending, 1));
                                    }
                                }
                            }
                            else if (name == "error" || name == "warning")
                            {
                                std::string message;
                                for (const PreprocessingToken& token : args)
                                {
                                    if (!message.empty() && (token.m_flags & LeadingSpace))
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
                                    std::string(resolved.m_file)
                                    + "("
                                    + std::to_string(resolved.m_line)
                                    + ","
                                    + std::to_string(resolved.m_column)
                                    + ") : warning #601: "
                                    + message + m_sources.Notes(location));
                                if (m_request.m_warning)
                                {
                                    m_request.m_warning(m_unit.m_diagnostics.back());
                                }
                            }
                            else
                            {
                                Fail(location, "unsupported preprocessing directive #" + std::string(name));
                            }
                        }
                    }
                }
                if (ending.m_kind == PreprocessingKind::End)
                {
                    flush();
                    if (buffer == m_rootBuffer)
                    {
                        m_end = ending;
                    }
                    if (!conditionals.empty())
                    {
                        Fail(conditionals.back().m_location, "unterminated conditional directive");
                    }
                    break;
                }
                CheckSize(pending.size(), ending.m_location);
            }
        }

        CompilationUnit& m_unit;
        SourceManager& m_sources;
        const PreprocessRequest& m_request;
        std::unordered_map<std::string_view, Macro> m_macros;
        std::vector<Suppression> m_suppression;
        std::unordered_set<BufferId> m_once;
        uint32_t m_nextMacro = 0;
        size_t m_expansions = 0;
        size_t m_scannedTokens = 0;
        SourceLocationId m_expandingAt = 0;
        SourceLocationId m_builtinLocation = 0;
        BufferId m_rootBuffer = 0;
        PreprocessingToken m_end{ {}, 0, 0, PreprocessingKind::End, 0 };
    };

    PreprocessResult CompilationUnit::Preprocess(const PreprocessRequest& request)
    {
        if (m_started)
        {
            throw std::logic_error("a compilation unit can only be preprocessed once");
        }
        m_started = true;
        if (!request.m_sourcePath.empty())
        {
            m_dependencies.push_back({ std::filesystem::absolute(request.m_sourcePath).generic_string() });
        }
        Preprocessor(*this, request).Run();
        return { m_tokens, m_dependencies, m_diagnostics };
    }
} // namespace AZ::ShaderCompiler
