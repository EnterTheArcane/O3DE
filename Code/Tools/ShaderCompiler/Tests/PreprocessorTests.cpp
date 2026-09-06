/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PreprocessingTokenSource.h>
#include <json/json.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

using namespace AZ::ShaderCompiler;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            throw SourceError(message);
        }
    }

    std::vector<std::string> Meaning(CompilationUnit& unit, const std::string& source)
    {
        PreprocessRequest request;
        request.m_source = unit.m_sources.AddSource("test.azsl", source);
        request.m_sourcePath = "test.azsl";
        PreprocessResult result = unit.Preprocess(request);
        std::vector<std::string> text;
        size_t endCount = 0;
        for (const PreprocessingToken& token : result.m_tokens)
        {
            if (token.m_kind == PreprocessingKind::End)
            {
                ++endCount;
            }
            if (token.m_kind <= PreprocessingKind::Punctuation)
            {
                text.emplace_back(unit.m_sources.Text(token.m_spelling));
            }
        }
        Check(endCount == 1 && result.m_tokens.back().m_kind == PreprocessingKind::End, "expected one terminal EOF");
        return text;
    }

    void Expect(const std::string& source, std::vector<std::string> expected)
    {
        CompilationUnit unit;
        std::vector<std::string> actual = Meaning(unit, source);
        if (actual != expected)
        {
            std::ostringstream message;
            message << "preprocessing mismatch:\n" << source << "\nactual: ";
            for (const std::string& text : actual)
            {
                message << text << ' ';
            }
            throw SourceError(message.str());
        }
    }
} // namespace

int main()
{
    try
    {
#if defined(_MSC_VER)
        static_assert(_HAS_EXCEPTIONS == 1, "AZSL requires standard STL exception semantics");
#endif
        bool caughtAntlrException = false;
        try
        {
            antlr4::ANTLRInputStream input("");
            input.consume();
        } catch (const std::exception& error)
        {
            caughtAntlrException = std::string_view(error.what()).find("cannot consume EOF") != std::string_view::npos;
        }
        Check(caughtAntlrException, "ANTLR exception types must match the compiler");

        const std::string jsonMessage(120, 'j');
        bool caughtJsonException = false;
        try
        {
            Json::throwLogicError(jsonMessage);
        } catch (const std::exception& error)
        {
            caughtJsonException = error.what() == jsonMessage;
        }
        Check(caughtJsonException, "JsonCpp exception types and messages must match the compiler");

        Expect("", {});
        Expect("// empty translation unit", {});
        Expect("#define A /* first\nsecond */ 1\nA\n", { "1" });
        Expect("#if true && !false\nyes\n#else\n#error boolean constants\n#endif", { "yes" });
        Expect("#if true and not false and ((7 bitand 3) not_eq 0)\nyes\n#endif", { "yes" });
        Expect("#define A F\n#define F(x) x+x\nA(3)", { "3", "+", "3" });
        Expect("#define A B\n#define B A\nA B", { "A", "B" });
        Expect("#define f(x) x\nf(f(1))", { "1" });
        Expect("#define S(x) #x\n#define W(x) S(x)\nW(a __LINE__)\n", { "\"a 3\"" });
        Expect("#define S(x) #x\n#define W(x) S(x)\n#define F(x) a #x\nW(F(b))\n", { "\"a \\\"b\\\"\"" });
        Expect(
            "#define str(x) #x\n#define cat(a,b) a##b\nstr(a  + /* x */ b) cat(fl,oat) cat(,ok) cat(ok,) cat(,)\n",
            { "\"a + b\"", "float", "ok", "ok" });
        Expect("#define v(a,...) a __VA_ARGS__\nv(1) v(2,3,4)", { "1", "2", "3", ",", "4" });
        Expect(
            "#define A 1\n#if defined(A) && 2+3*4 == 14 && (1 || 1/0) && (-1 < 1) && !( -1 < 1u)\nyes\n#else\n#error failed\n#endif",
            { "yes" });
        Expect("#if 0\n#include \"absent\"\n#error not executed\n@ invalid AZSL\n#if 1/0\n#endif\n#elif !missing\nok\n#endif", { "ok" });
        Expect("\xef\xbb\xbf#define A na\\\r\nme\r\nA\r\n", { "name" });
        Expect("#line 77 \"override.azsl\"\n__LINE__ __FILE__", { "77", "\"override.azsl\"" });
        Expect("#define f(x) x x\nf(2) f(3)", { "2", "2", "3", "3" });
        {
            CompilationUnit unit;
            Meaning(unit, "#define TYPE float4\nTYPE value;\n");
            PreprocessingTokenSource source(unit);
            antlr4::CommonTokenStream stream(&source);
            stream.fill();
            Check(stream.LT(1)->getType() == azslLexer::Float4, "macro keyword classification");
            Check(GetSourceLocation(stream.LT(1)).Resolve().m_line == 2, "macro expansion location");
            Check(TokenTextView(stream.LT(1)) == "float4", "source-backed token text");
        }
        {
            CompilationUnit original;
            Meaning(
                original,
                "#define E\n#define N 0x1e\n#define ATTR [[azsl::int_range(0,3)]]\n"
                "/* caf\xc3\xa9 */\nATTR option int choice = 0;\nfloat f() { return N+2; }\n"
                "x+E+1; . . . / * + + < < [ [\n#pragma custom value\n");
            std::ostringstream exported;
            original.WritePreprocessed(exported);
            CompilationUnit imported;
            PreprocessRequest request;
            request.m_source = imported.m_sources.AddSource("exported", exported.str());
            request.m_preprocessed = true;
            imported.Preprocess(request);
            auto adapted = [](CompilationUnit& unit)
            {
                std::vector<std::pair<size_t, std::string>> result;
                PreprocessingTokenSource source(unit);
                for (;;)
                {
                    auto token = source.nextToken();
                    if (token->getChannel() != antlr4::Token::HIDDEN_CHANNEL)
                    {
                        result.emplace_back(token->getType(), token->getText());
                    }
                    if (token->getType() == antlr4::Token::EOF)
                    {
                        break;
                    }
                }
                return result;
            };
            Check(adapted(original) == adapted(imported), "export/import changed AZSL tokens or comments");
        }
        {
            SourceManager sources;
            BufferId tiny = sources.AddSource("tiny", "x");
            std::string_view view = sources.BufferText(tiny);
            TextSpan generated = sources.StoreGenerated("paste");
            std::string_view paste = sources.Text(generated);
            for (size_t i = 0; i < 10000; ++i)
            {
                sources.AddSource("tiny", "y");
                sources.StoreGenerated(std::string(99, 'a'));
            }
            Check(view == "x" && view.data() == sources.BufferText(tiny).data(), "short source buffer moved");
            Check(paste == "paste" && paste.data() == sources.Text(generated).data(), "arena spelling moved");
        }
        {
            CompilationUnit unit;
            std::string input = "#define BODY long_identifier + another_identifier\n";
            for (size_t i = 0; i < 1000; ++i)
            {
                input += "BODY\n";
            }
            Meaning(unit, input);
            const char* spelling = nullptr;
            for (const PreprocessingToken& token : unit.Tokens())
            {
                if (unit.m_sources.Text(token.m_spelling) == "long_identifier")
                {
                    const char* tokenSpelling = unit.m_sources.Text(token.m_spelling).data();
                    if (!spelling)
                    {
                        spelling = tokenSpelling;
                    }
                    Check(spelling == tokenSpelling, "macro spelling copied during expansion");
                }
            }
        }
        {
            std::filesystem::path directory = std::filesystem::temp_directory_path() /
                ("azsl-source-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directory(directory);
            struct Cleanup
            {
                std::filesystem::path m_path;
                ~Cleanup()
                {
                    std::error_code e;
                    std::filesystem::remove_all(m_path, e);
                }
            } cleanup{ directory };
            std::filesystem::path header = directory / "tiny.azsli";
            {
                std::ofstream out(header);
                out << "float NAME;\n";
            }
            CompilationUnit unit;
            PreprocessRequest request;
            request.m_sourcePath = (directory / "root.azsl").string();
            request.m_source = unit.m_sources.AddSource(
                request.m_sourcePath,
                "#define NAME first\n#include \"tiny.azsli\"\n#undef NAME\n#define NAME second\n#include \"./tiny.azsli\"\n");
            PreprocessResult result = unit.Preprocess(request);
            Check(unit.m_sources.FileReads() == 1, "aliased/repeated include read more than once");
            Check(result.m_dependencies.size() == 3, "root or include instances were collapsed");
            PreprocessingTokenSource source(unit);
            antlr4::CommonTokenStream tokens(&source);
            tokens.fill();
            Check(tokens.LT(1)->getType() == azslLexer::Float, "generated spellings lost before parsing");
            Check(GetSourceLocation(tokens.LT(1)).Resolve().m_file == header.generic_string(), "include source identity");
        }
        {
            CompilationUnit unit;
            std::string source = "#define ORIGINAL stable\n";
            for (size_t i = 0; i < 2000; ++i)
            {
                source += "#define MACRO" + std::to_string(i) + " ORIGINAL\n";
            }
            source += "MACRO0 MACRO1999 ORIGINAL\n";
            Check(
                Meaning(unit, source) == std::vector<std::string>({ "stable", "stable", "stable" }),
                "macro-table growth invalidated spellings");
        }
        {
            BorrowedSpellingStream stream("a\xc3\xa9\xf0\x9f\x98\x80");
            Check(stream.size() == 3 && stream.LA(2) == 0xe9 && stream.LA(3) == 0x1f600, "UTF-8 character positions");
            Check(stream.getText(antlr4::misc::Interval(size_t{ 1 }, size_t{ 2 })) == "\xc3\xa9\xf0\x9f\x98\x80", "UTF-8 source interval");
        }
        // Retained audit cases, with checked Clang-compatible stringification results.
        Expect(
            "#define E\n#define F(x) a x\n#define G(x) x a\n#define P(a,b) a##b\n#define I(x) x\n"
            "#define S(x) #x\n#define W(x) S(x)\n"
            "W(F()+b) W(F(E)+b) W(a E+b) W(a+E b) W(a E E+b) W(a E+E b) W(P(,)+b)\n"
            "W(F(P(,))+b) W(a I(E)+b) W(G()+b) W(F()E+b) W(F()+E b) W(I(a E)+b) W(P(a,)+b)\n",
            { "\"a +b\"",
              "\"a +b\"",
              "\"a +b\"",
              "\"a+ b\"",
              "\"a +b\"",
              "\"a + b\"",
              "\"+b\"",
              "\"a +b\"",
              "\"a +b\"",
              "\"a+b\"",
              "\"a +b\"",
              "\"a + b\"",
              "\"a+b\"",
              "\"a+b\"" });
        Expect(
            "#define E\n#define S(x) #x\n#define W(x) S(x)\n#define F(a,b) a##b+x\n#define G(a,b) a##b x\n"
            "#define H(a,b) x a##b\n#define J(a,b) x+a##b\n#define K(a,b) x a##b+y\n"
            "W(a F(,)) W(a G(,)) W(H(,) +b) W(J(,) +b) W(a F(,)+b) W(a E F(,))\n"
            "W(K(,q)) W(K(q,)) W(K(,)) W(K(,)+b) W(H(,)+b) W(J(,)+b) W(a F(,q)) W(a F(q,))\n",
            { "\"a +x\"",
              "\"a x\"",
              "\"x +b\"",
              "\"x+ +b\"",
              "\"a +x+b\"",
              "\"a +x\"",
              "\"x q+y\"",
              "\"x q+y\"",
              "\"x +y\"",
              "\"x +y+b\"",
              "\"x +b\"",
              "\"x++b\"",
              "\"a q+x\"",
              "\"a q+x\"" });
        Expect(
            "#define E\n#define S(x) #x\n#define W(x) S(x)\n#define P(a,b) a##b\n#define F(a,b) a##b+x\n"
            "#define Q(a,b) a##b +x\nW(a+P(,)b) W(a P(,)b) W(a+Q(,)) W(a Q(,)) W(a+F(,)) W(a+P(,) E b)\n",
            { "\"a+b\"", "\"a b\"", "\"a++x\"", "\"a +x\"", "\"a++x\"", "\"a+ b\"" });
        Expect("#if L'a' == 97 && L'\\u00e9' == 233 && L'\\U0001f600' == 128512\nyes\n#endif", { "yes" });
        Expect("#if L'\xc3\xa9' == 233\nyes\n#endif", { "yes" });
        Expect("#if L'\xf0\x9f\x98\x80' == 128512\nyes\n#endif", { "yes" });
        Expect("L'a'", { "L'a'" });
        Expect(
            "#if L'\\n' == 10 && L'\\777' == 511 && L'\\x1f600' == 128512 && 'ab' == 0x6162 && '\xc3\xa9' == 0xc3a9 && '\\u00e9' == "
            "0x7530306539\nyes\n#endif",
            { "yes" });
        for (const char* constant : { "L'ab'", "L'\\q'", "L'\\uD800'", "L'\\U00110000'" })
        {
            try
            {
                CompilationUnit unit;
                Meaning(unit, std::string("#if ") + constant + "\n#endif\n");
                Check(false, "expected invalid wide character constant");
            } catch (const PreprocessingError& error)
            {
                Check(std::string(error.what()).find("error #600:") != std::string::npos, "invalid wide constant diagnostic");
            }
        }
        try
        {
            CompilationUnit unit;
            Meaning(unit, "#line 40 \"presumed.azsl\"\n#error deliberately_long_owned_error_message\n");
            Check(false, "expected preprocessing error");
        } catch (const PreprocessingError& error)
        {
            Check(
                std::string(error.what()).find("presumed.azsl(40,1) : error #600: #error deliberately_long_owned_error_message") !=
                    std::string::npos,
                "diagnostic did not survive compilation-unit destruction");
        }
        {
            SourceManager sources;
            const std::string text = "\xef\xbb\xbf"
                                     "ab\xc3\xa9\xf0\x9f\x98\x80z\r\nx";
            const BufferId buffer = sources.AddSource("columns", text);
            const SourceInstanceId instance = sources.EnterSource(buffer, "columns");
            Check(sources.Resolve(sources.Locate(instance, 3)).m_column == 1, "BOM column");
            Check(sources.Resolve(sources.Locate(instance, 11)).m_column == 5, "Unicode column");
            Check(sources.Resolve(sources.Locate(instance, 14)).m_column == 1, "CRLF column");
        }
        Expect("#define S(x) #x\n#define W(x) S(x)\n#define F(x) a x+b\nW(F())\n", { "\"a +b\"" });
        try
        {
            SourceManager sources;
            sources.AddSource("source_whose_name_requires_an_owned_diagnostic_string.azsl", "\xff");
            Check(false, "expected UTF-8 source error");
        } catch (const std::exception& error)
        {
            Check(
                std::string(error.what()).find("invalid UTF-8 at byte 0 in source_whose_name_requires_an_owned_diagnostic_string.azsl") !=
                    std::string::npos,
                "source diagnostic did not survive source-manager destruction");
        }
        {
            SourceManager sources;
            const TextSpan generated = sources.StoreGenerated("spelling");
            try
            {
                sources.EnterSource(generated.m_buffer, "invalid source");
                Check(false, "generated storage accepted as a physical source");
            } catch (const std::invalid_argument&)
            {
            }
        }
        {
            SourceManager sources;
            const auto buffer = sources.AddSource("notes", "left\nright\ncall\n");
            const auto instance = sources.EnterSource(buffer, "notes");
            const auto left = sources.Locate(instance, 0);
            const auto right = sources.Locate(instance, 5);
            const auto invocation = sources.Locate(instance, 11);
            const auto paste = sources.Expand(left, invocation, right);
            const auto wrapped = sources.Expand(paste, invocation);
            const auto notes = sources.Notes(wrapped);
            Check(notes.find("notes(1,1)") != std::string::npos, "paste lost its left origin");
            Check(notes.find("notes(2,1)") != std::string::npos, "wrapped paste lost its right origin");
            Check(std::count(notes.begin(), notes.end(), '\n') == 2, "paste duplicated source notes");

            auto nested = invocation;
            for (size_t i = 0; i < 1000; ++i)
            {
                nested = sources.Expand(paste, nested, wrapped);
            }
            const auto bounded = sources.Notes(nested);
            Check(std::count(bounded.begin(), bounded.end(), '\n') <= 32, "diagnostic traversal exceeded its note limit");

            const auto many = sources.AddSource("many", std::string(96, 'x'));
            const auto manyInstance = sources.EnterSource(many, "many");
            for (uint32_t i = 0; i < 96; ++i)
            {
                nested = sources.Expand(sources.Locate(manyInstance, i), nested);
            }
            const auto limited = sources.Notes(nested);
            Check(std::count(limited.begin(), limited.end(), '\n') <= 32, "diagnostics emitted too many distinct notes");
        }
        // Also runs under ASan/UBSan.
        // Each case has bounded text, expansion and token budgets. Expected preprocessing errors must unwind all owners.
        std::mt19937 random(122);
        constexpr std::string_view alphabet = "abc012#()/*\\\n'\"+- \t";
        for (size_t i = 0; i < 1000; ++i)
        {
            CompilationUnit unit;
            std::string input = "#define F(x) x x\n#define C(a,b) a##b\n";
            for (size_t j = 0; j < 128; ++j)
            {
                input += alphabet[random() % alphabet.size()];
            }
            PreprocessRequest request;
            request.m_source = unit.m_sources.AddSource("fuzz", std::move(input));
            request.m_tokenLimit = 1024;
            request.m_expansionLimit = 1024;
            try
            {
                unit.Preprocess(request);
                PreprocessingTokenSource source(unit);
                antlr4::CommonTokenStream tokens(&source);
                tokens.fill();
            } catch (const PreprocessingError& error)
            {
                Check(std::string(error.what()).find("error #600:") != std::string::npos, "fuzz diagnostic lost its message");
            }
        }
        std::cout << "Native preprocessing and ownership tests passed\n";
        return 0;
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
