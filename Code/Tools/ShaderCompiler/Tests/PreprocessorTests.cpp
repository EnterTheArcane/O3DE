/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "PreprocessingTokenSource.h"

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
            throw std::runtime_error(message);
        }
    }

    std::vector<std::string> Meaning(CompilationUnit& unit, const std::string& source)
    {
        PreprocessRequest request;
        request.source = unit.sources.AddSource("test.azsl", source);
        request.sourcePath = "test.azsl";
        PreprocessResult result = unit.Preprocess(request);
        std::vector<std::string> text;
        for (const PreprocessingToken& token : result.tokens)
        {
            if (token.kind <= PreprocessingKind::Punctuation)
            {
                text.emplace_back(token.spelling);
            }
        }
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
            throw std::runtime_error(message.str());
        }
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc >= 3 && (std::string_view(argv[1]) == "--tokens" || std::string_view(argv[1]) == "--preprocessed-tokens"))
        {
            CompilationUnit unit;
            PreprocessRequest request;
            request.preprocessed = std::string_view(argv[1]) == "--preprocessed-tokens";
            request.sourcePath = argv[2];
            if (request.sourcePath == "-")
            {
                request.sourcePath = "<stdin>";
                request.source = unit.sources.AddSource("<stdin>", std::string(std::istreambuf_iterator<char>(std::cin), {}));
            }
            for (int i = 3; i < argc; ++i)
            {
                request.includeDirectories.emplace_back(argv[i]);
            }
            for (const PreprocessingToken& token : unit.Preprocess(request).tokens)
            {
                if (token.kind <= PreprocessingKind::Punctuation)
                {
                    // Length framing preserves arbitrary UTF-8, quotes and raw-string newlines without a test-side lexer.
                    std::cout << token.spelling.size() << ':' << token.spelling << ',';
                }
            }
            return 0;
        }
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
            Check(GetSourceLocation(stream.LT(1)).Resolve().line == 2, "macro expansion location");
            Check(stream.LT(1)->getText() == "float4", "owned token text");
        }
        {
            std::unique_ptr<antlr4::Token> token;
            {
                CompilationUnit unit;
                Meaning(unit, "float value;");
                PreprocessingTokenSource source(unit);
                token = source.nextToken();
            }
            // Text owns its bytes; resolving source locations still requires the compilation unit.
            Check(token->getText() == "float", "token text depends on destroyed source storage");
        }
        {
            SourceManager sources;
            BufferId tiny = sources.AddSource("tiny", "x");
            std::string text = sources.BufferText(tiny);
            SourceLocation location = sources.Location(sources.Locate(sources.EnterSource(tiny, "tiny"), 0));
            ResolvedSourceLocation resolved = location.Resolve();
            for (size_t i = 0; i < 10000; ++i)
            {
                BufferId added = sources.AddSource("tiny", "y");
                sources.EnterSource(added, "another source");
            }
            Check(text == "x" && sources.BufferText(tiny) == "x", "source copies changed after container growth");
            text[0] = 'z';
            Check(sources.BufferText(tiny) == "x", "source bytes shared with a mutable copy");
            Check(resolved.file == "tiny" && location.Resolve().file == "tiny", "source location lost after container growth");
        }
        {
            CompilationUnit unit;
            std::string input = "#define BODY long_identifier + another_identifier\n";
            for (size_t i = 0; i < 1000; ++i)
            {
                input += "BODY\n";
            }
            Meaning(unit, input);
            size_t expanded = 0;
            std::vector<PreprocessingToken> copies(unit.Tokens().begin(), unit.Tokens().end());
            for (size_t i = 0; i < copies.size(); ++i)
            {
                if (copies[i].spelling == "long_identifier")
                {
                    ++expanded;
                    copies[i].spelling[0] = 'X';
                    Check(unit.Tokens()[i].spelling == "long_identifier", "copied macro spelling aliases the original");
                }
            }
            Check(expanded == 1000, "repeated macro expansion lost tokens");
        }
        {
            std::filesystem::path directory = std::filesystem::temp_directory_path() /
                ("azsl-source-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directory(directory);
            struct Cleanup
            {
                std::filesystem::path path;
                ~Cleanup()
                {
                    std::error_code e;
                    std::filesystem::remove_all(path, e);
                }
            } cleanup{ directory };
            std::filesystem::path header = directory / "tiny.azsli";
            {
                std::ofstream out(header);
                out << "float NAME;\n";
            }
            CompilationUnit unit;
            PreprocessRequest request;
            request.sourcePath = (directory / "root.azsl").string();
            request.source = unit.sources.AddSource(
                request.sourcePath,
                "#define NAME first\n#include \"tiny.azsli\"\n#undef NAME\n#define NAME second\n#include \"./tiny.azsli\"\n");
            PreprocessResult result = unit.Preprocess(request);
            Check(unit.sources.FileReads() == 1, "aliased/repeated include read more than once");
            Check(result.dependencies.size() == 2, "include instances were collapsed");
            PreprocessingTokenSource source(unit);
            antlr4::CommonTokenStream tokens(&source);
            tokens.fill();
            Check(tokens.LT(1)->getType() == azslLexer::Float, "generated spellings lost before parsing");
            Check(GetSourceLocation(tokens.LT(1)).Resolve().file == header.generic_string(), "include source identity");
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
            CompilationUnit unit;
            Meaning(unit, "\"a\xc3\xa9\xf0\x9f\x98\x80\" value");
            PreprocessingTokenSource source(unit);
            antlr4::CommonTokenStream tokens(&source);
            tokens.fill();
            Check(tokens.LT(1)->getText() == "\"a\xc3\xa9\xf0\x9f\x98\x80\"", "UTF-8 token text");
            Check(tokens.LT(2)->getCharPositionInLine() == 6, "UTF-8 character positions");
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
            request.source = unit.sources.AddSource("fuzz", std::move(input));
            request.tokenLimit = 1024;
            request.expansionLimit = 1024;
            try
            {
                unit.Preprocess(request);
                PreprocessingTokenSource source(unit);
                antlr4::CommonTokenStream tokens(&source);
                tokens.fill();
            } catch (const PreprocessingError&)
            {
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
