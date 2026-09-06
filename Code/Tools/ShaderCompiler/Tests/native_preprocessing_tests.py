#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import json
import random
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

COMPILER = str(Path(sys.argv.pop(1)).resolve())


class NativePreprocessing(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="azsl preprocessing ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def file(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode("utf-8") if isinstance(text, str) else text)
        return path

    def run_compiler(self, *args, source=None, expected_exit=0):
        result = subprocess.run(
            [COMPILER, *map(str, args)], input=source, text=True, encoding="utf-8",
            capture_output=True, cwd=self.root, timeout=20)
        self.assertEqual(result.returncode, expected_exit, result.stderr)
        return result

    def test_symbol_dump_json_preserves_types_and_empty_collections(self):
        source = self.file("symbols.azsl", "struct Empty {};\n"
                           "typedef Empty Alias;\n"
                           "static const int Count = 2147483647;\n"
                           "static const int Expression = 1 + 2;\n"
                           "float declared(float value);\n"
                           "float prototype(float);\n"
                           "float empty() { return 1.0; }\n")
        result = self.run_compiler(source, "--dumpsym")
        symbols = json.loads(result.stdout)
        self.assertEqual(result.stdout, self.run_compiler(source, "--dumpsym").stdout)
        self.assertEqual(symbols["Symbol '/Empty'"]["members"], [])
        self.assertEqual(symbols["Symbol '/Alias'"]["references"], [])
        self.assertEqual(symbols["Symbol '/Alias'"]["canonical type"]["core"]["name"], "/Empty")
        self.assertEqual(symbols["Symbol '/Count'"]["val"], 2147483647)
        self.assertNotIn("val", symbols["Symbol '/Expression'"])
        self.assertIs(symbols["Symbol '/Count'"]["has sampler state"], False)
        function = symbols["Symbol '/declared(?float)'"]
        self.assertEqual(function["def line"], "undef")
        self.assertEqual(function["parameters"][0]["name"], "value")
        self.assertEqual(function["parameters"][0]["type"]["core"]["name"], "?float")
        self.assertEqual(function["return type"]["generic"], "<NA>")
        prototype = symbols["Symbol '/prototype(?float)'"]["parameters"][0]
        self.assertEqual(prototype["non-var identifier (type?)"]["core"]["name"], "?float")
        self.assertIs(function["is method"], False)
        self.assertEqual(symbols["Symbol '/empty()'"]["parameters"], [])
        self.assertEqual(symbols["Symbol '/empty()'"]["has overriding children"], [])
        self.assertIsInstance(symbols["Symbol '/empty()'"]["def line"], int)
        self.assertEqual(json.loads(self.run_compiler("-", "--dumpsym", source="").stdout), {})

    def test_predefined_types_json(self):
        result = self.run_compiler("--listpredefined")
        categories = json.loads(result.stdout)
        self.assertIn("float", categories["Scalar"])
        self.assertIn("float4", categories["Vector"])
        self.assertIn("float4x4", categories["Matrix"])
        for tokens in categories.values():
            self.assertIsInstance(tokens, list)
            self.assertEqual(tokens, sorted(set(tokens)))
            self.assertTrue(all(isinstance(token, str) for token in tokens))
        self.assertEqual(result.stdout, self.run_compiler("--listpredefined").stdout)

    def test_expected_exit_status_rejects_crashes(self):
        with mock.patch.object(subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 1, "", "source error")
            self.run_compiler("-", expected_exit=1)
            for status in (0xC0000005, -11, 15):
                with self.subTest(status=status):
                    run.return_value = subprocess.CompletedProcess([], status, "", "")
                    with self.assertRaises(AssertionError):
                        self.run_compiler("-", expected_exit=1)

    def test_skipped_directive_operands(self):
        for directive in ("#include <unfinished", "#ifdef 123 garbage\n#endif", "#ifndef 123 garbage\n#endif"):
            with self.subTest(directive=directive):
                source = self.file("skipped.azsl", "#if 0\n" + directive + "\n#endif\nfloat okay;\n")
                self.run_compiler(source, "--syntax")
                report = json.loads(self.run_compiler(source, "--dependencies").stdout)
                self.assertTrue(report["complete"])
                self.assertEqual(report["dependencies"], [source.as_posix()])
                self.run_compiler("-", "-E", source=directive + "\n", expected_exit=1)

    def test_alternative_operators_require_operands(self):
        operators = ("and", "or", "not", "not_eq", "bitand", "bitor", "xor", "compl", "and_eq", "or_eq", "xor_eq")
        for operator in operators:
            for expression in (operator + " || 1", "1 || " + operator):
                with self.subTest(expression=expression):
                    result = self.run_compiler("-", "-E", source="#if " + expression + "\n#endif\n", expected_exit=1)
                    self.assertIn("error #600:", result.stderr)
        self.run_compiler("-", "--syntax", source="#define and 1\n#if and && not false && (compl 0 != 0)\nfloat okay;\n#endif\n")

    def test_invalid_utf8_stdin_has_partial_dependencies(self):
        result = subprocess.run([COMPILER, "-", "--dependencies"], input=b"\xff\n",
                                capture_output=True, cwd=self.root, timeout=20)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(json.loads(result.stdout), {"version": 1, "complete": False, "dependencies": []})
        self.assertIn(b"invalid UTF-8", result.stderr)

    def test_paste_diagnostics_include_both_wrapped_origins(self):
        self.file("left.azsli", "#define LEFT 0\n")
        self.file("right.azsli", "#define RIGHT x\n")
        source = self.file("paste.azsl", '#include "left.azsli"\n#include "right.azsli"\n'
                           '#define CAT(a,b) a##b\n#define WRAP(a,b) CAT(a,b)\nWRAP(LEFT,RIGHT) value;\n')
        result = self.run_compiler(source, "--syntax", expected_exit=1)
        self.assertIn("invalid AZSL token: 0x", result.stderr)
        self.assertIn("left.azsli(1,", result.stderr)
        self.assertIn("right.azsli(1,", result.stderr)
        notes = [line for line in result.stderr.splitlines() if ") : note:" in line]
        self.assertEqual(len(notes), len(set(notes)))
        self.assertLessEqual(len(notes), 32)

    def test_dependency_report_and_missing_header_recovery(self):
        self.file("later/value.azsli", "#define VALUE 1\n")
        source = self.file("root.azsl", '#define HEADER <value.azsli>\n#include HEADER\n#if 0\n#include "inactive.azsli"\n#endif\n')
        args = [source, "--dependencies", "-I", self.root / "earlier", "-I", self.root / "later"]
        report = json.loads(self.run_compiler(*args).stdout)
        self.assertEqual(report["version"], 1)
        self.assertTrue(report["complete"])
        self.assertEqual({Path(path) for path in report["dependencies"]},
                         {source, self.root / "earlier/value.azsli", self.root / "later/value.azsli"})
        self.file("earlier/value.azsli", "#define VALUE 2\n")
        paths = {Path(path) for path in json.loads(self.run_compiler(*args).stdout)["dependencies"]}
        self.assertEqual(paths, {source, self.root / "earlier/value.azsli"})
        missing = self.file("missing.azsl", '#include "future.azsli"\n')
        report = json.loads(self.run_compiler(missing, "--dependencies", expected_exit=1).stdout)
        self.assertFalse(report["complete"])
        self.assertIn(self.root.joinpath("future.azsli").as_posix(), report["dependencies"])
        self.file("future.azsli", "static const float ready = 1;\n")
        self.assertTrue(json.loads(self.run_compiler(missing, "--dependencies").stdout)["complete"])
        destination = self.root / "deps.json"
        self.run_compiler(missing, "--dependencies", "-o", destination)
        self.assertTrue(json.loads(destination.read_text(encoding="utf-8"))["complete"])
        self.assertTrue(json.loads(self.run_compiler(missing, "--dependencies", "-o", "-").stdout)["complete"])
        self.assertFalse((self.root / "-").exists())
        self.run_compiler(source, "--dependencies", "-E", expected_exit=108)

    def test_command_line_diagnostics_own_messages(self):
        source = self.file("root.azsl", "")
        cases = [
            (["--dependencies", "-E"], ["--dependencies", "-E"], 108),
            (["--dependencies", "--unknown-preprocessor-option"], ["--unknown-preprocessor-option"], 109),
            (["--dependencies", "-I"], ["-I"], 114),
            (["--dependencies", "-D"], ["-D"], 114),
            (["--dependencies", "--include"], ["--include"], 114),
        ]
        for arguments, expected, status in cases:
            with self.subTest(arguments=arguments):
                result = self.run_compiler(source, *arguments, expected_exit=status)
                for text in expected:
                    self.assertIn(text, result.stderr)
                if "-E" in arguments:
                    self.assertEqual(result.returncode, 108)

    def test_shared_scene_srg_does_not_require_terrain_gem_assets(self):
        engine = Path(__file__).resolve().parents[4]
        common = engine / "Gems/Atom/Feature/Common/Assets/ShaderResourceGroups"
        shared = common / "TerrainSceneSrg.azsli"
        includes = ["-I", engine / "Gems", "-I", engine / "Gems/Atom/RPI/Assets/ShaderLib"]
        source = self.file("scene.azsl", '#define AZ_COLLECTING_PARTIAL_SRGS\n'
                           '#include <Atom/Feature/Common/Assets/ShaderResourceGroups/SceneSrgAll.azsli>\n')
        report = json.loads(self.run_compiler(source, "--dependencies", *includes).stdout)
        paths = {Path(name) for name in report["dependencies"]}
        self.assertIn(shared, paths)
        self.assertFalse(any(path.is_relative_to(engine / "Gems/Terrain") for path in paths))

        source = self.file("legacy_scene.azsl", '#define AZ_COLLECTING_PARTIAL_SRGS\n'
                           '#include <Terrain/Assets/Shaders/Terrain/SceneSrg.azsli>\n')
        report = json.loads(self.run_compiler(source, "--dependencies", *includes).stdout)
        self.assertIn(shared, {Path(name) for name in report["dependencies"]})

    def test_unicode_dependency_paths(self):
        header = self.file("café 😀.azsli", "static const float included = 1;\n")
        source = self.file("shader é😀.azsl", '#include "café 😀.azsli"\n')
        self.run_compiler(source, "--syntax")
        report = json.loads(self.run_compiler(source, "--dependencies").stdout)
        self.assertTrue(report["complete"])
        self.assertEqual({Path(path) for path in report["dependencies"]}, {source, header})
        self.run_compiler("-", "--include", header, "--syntax", source="")
        exported = self.root / "exported é😀.preprocessed"
        self.run_compiler(source, "-E", "-o", exported)
        self.run_compiler(exported, "--preprocessed", "--syntax")

    def test_dependency_environments_and_nested_forced_headers(self):
        forced = self.file("forced/header.azsli", '#include "nested.azsli"\n')
        nested = self.file("forced/nested.azsli", '#define HEADER "choice.azsli"\n')
        quoted = self.file("choice.azsli", "// quoted winner\n")
        angled = self.file("includes/choice.azsli", "// angled winner\n")
        red = self.file("red.azsli", "// red\n")
        blue = self.file("blue.azsli", "// blue\n")
        repeated = self.file("repeated.azsli", '#if BLUE\n#include "blue.azsli"\n#else\n#include "red.azsli"\n#endif\n')
        source = self.file("root.azsl", '#include /* comment */ \\\nHEADER\n#include <choice.azsli>\n#include "repeated.azsli"\n')
        args = [source, "--dependencies", "--include=" + str(forced), "-I", self.root / "includes"]
        common = {source, forced, nested, quoted, angled, repeated}
        for definition, selected in (("-DBLUE=0", red), ("-DBLUE=1", blue)):
            report = json.loads(self.run_compiler(*args, definition).stdout)
            self.assertEqual({Path(path) for path in report["dependencies"]}, common | {selected})
            self.assertEqual(report["dependencies"], sorted(set(report["dependencies"])))
        source.write_text('#define BLUE 0\n#include "repeated.azsli"\n#undef BLUE\n#define BLUE 1\n#include "repeated.azsli"\n', encoding="utf-8")
        report = json.loads(self.run_compiler(*args).stdout)
        self.assertEqual({Path(path) for path in report["dependencies"]},
                         {source, forced, nested, repeated, red, blue})

    def test_empty_expansion_wide_constants_and_eof(self):
        source = self.file("once.azsl", "#pragma once\n#if L'a' == 97 && L'\\u00e9' == 233\nstatic const float value = 1;\n#endif\n")
        self.run_compiler(source, "--include=" + str(source), "--syntax")
        source = self.file("once_error.azsl", '#pragma once\n#line 40 "forced_presumed"\nfloat f() {\n')
        result = self.run_compiler(source, "--include", source, "--syntax", expected_exit=1)
        self.assertIn("forced_presumed(41,", result.stderr)
        for value in ("ab", "\\U00110000", "\\uD800", "\\x", "\\u12", "\\q", "\\xFFFFFFFFFFFFFFFFF"):
            result = self.run_compiler("-", "-E", source="#if L'" + value + "'\n#endif\n", expected_exit=1)
            self.assertIn("error #600:", result.stderr)
        self.file("a +b", "static const float included = 1;\n")
        source = self.file("empty.azsl", '#define E\n#define S(x) #x\n#define W(x) S(x)\n#include W(a E+b)\n')
        self.run_compiler(source, "--syntax")

    def test_export_number_boundaries_and_warning_policy(self):
        invalid_namespace = self.run_compiler("-", "--namespace=mt,vk", source="float value;", expected_exit=1)
        self.assertIn("Invalid namespace 'mt,vk'", invalid_namespace.stderr)
        source = self.file("number.azsl", "#define N 0x1e\nfloat f() { return N+2; }\n")
        exported = self.root / "number.preprocessed"
        self.run_compiler(source, "--syntax")
        self.run_compiler(source, "-E", "-o", exported)
        self.run_compiler(exported, "--preprocessed", "--syntax")
        source = self.file("comment_lines.azsl", b'#line 20 "presumed.azsl"\r\nfloat a; /* first\r\nsecond\r\n*/ float b;\r\nfloat ;\r\n')
        direct = self.run_compiler(source, "--syntax", expected_exit=1)
        self.run_compiler(source, "-E", "-o", exported)
        roundtrip = self.run_compiler(exported, "--preprocessed", "--syntax", expected_exit=1)
        self.assertIn("presumed.azsl(23,", direct.stderr)
        self.assertIn("presumed.azsl(23,", roundtrip.stderr)
        raw = subprocess.run([COMPILER, str(source), "-E"], capture_output=True, check=True, timeout=20)
        self.assertNotIn(b"\r\r\n", raw.stdout)
        warning = self.file("warning.azsl", "#warning deliberate\nstatic const float value = 1;\n")
        for mode in ([], ["--syntax"], ["-E"], ["--dependencies"]):
            self.assertEqual(self.run_compiler(warning, *mode, "--W0").stderr, "")
            result = self.run_compiler(warning, *mode, "--Wx", expected_exit=1)
            self.assertIn("warning #601: deliberate", result.stderr)
            if mode == ["--dependencies"]:
                self.assertFalse(json.loads(result.stdout)["complete"])
        warning.write_text('#warning first\n#include "never_reached.azsli"\n', encoding="utf-8")
        result = self.run_compiler(warning, "--dependencies", "--Wx", expected_exit=1)
        self.assertIn("warning #601: first", result.stderr)
        self.assertEqual({Path(path) for path in json.loads(result.stdout)["dependencies"]}, {warning})
        result = self.run_compiler(warning, "--dependencies", expected_exit=1)
        self.assertIn("warning #601: first", result.stderr)
        self.assertIn("include file not found", result.stderr)

    @staticmethod
    def body(result):
        return re.sub(r"^#(?:line| )[^\n]*\n", "", result.stdout, flags=re.M).strip()

    def test_ordered_command_line_and_quoted_values(self):
        result = self.run_compiler("-", "-E", "-DA=first", "-U", "A", "-D", "A=last", source="A\n")
        self.assertIn("last", result.stdout)
        self.assertNotIn("first", result.stdout)
        result = self.run_compiler("-", "-E", "-DA=1", "-DA=2", source="A")
        self.assertEqual(self.body(result), "2")
        # Real literal quotes survive argument forwarding without shell interpretation.
        result = self.run_compiler("-", "-E", '-DTEXT="hello world"', source="TEXT")
        self.assertEqual(self.body(result), '"hello world"')

    def test_includes_search_order_forced_header_and_repeated_include(self):
        self.file("first/value.azsli", "#define VALUE 7\n")
        self.file("second/value.azsli", "#define VALUE 9\n")
        header = self.file("forced header.azsli", "#define TYPE float\n")
        self.file("body.azsli", "TYPE NAME = VALUE;\n")
        source = self.file("shader.unusual", '#include <value.azsli>\n#define NAME first\n#include "body.azsli"\n#undef NAME\n#define NAME second\n#include "body.azsli"\n')
        result = self.run_compiler(source, "--include", header, "-I", self.root / "first", "-I" + str(self.root / "second"), "-E")
        self.assertRegex(result.stdout, r"float\s+first\s*=\s*7")
        self.assertRegex(result.stdout, r"float\s+second\s*=\s*7")
        self.run_compiler(source, "--include", header, "-I", self.root / "first", "--syntax")

    def test_include_guards_once_macro_headers_and_direct_header_names(self):
        self.file("a.azsli", '#ifndef A_GUARD\n#define A_GUARD\n#include "b.azsli"\nfloat a;\n#endif\n')
        self.file("b.azsli", '#pragma once\n#include "a.azsli"\nfloat b;\n')
        self.file("unchanged", "float direct;\n")
        source = self.file("root", '#define HEADER "a.azsli"\n#include HEADER\n#include "b.azsli"\n#define unchanged bogus\n#include <unchanged>\n')
        result = self.run_compiler(source, "-I", self.root, "-E")
        self.assertEqual(result.stdout.count("float a"), 1)
        self.assertEqual(result.stdout.count("float b"), 1)
        self.assertIn("float direct", result.stdout)

    def test_header_names_preserve_literal_characters(self):
        self.file("folder/header.azsli", "float from_angle;\n")
        self.file(r"two\\slashes.azsli", "float from_quoted;\n")
        source = self.file(
            "root", '#include <folder//header.azsli>\n'
                    '#include "two\\\\slashes.azsli"\n'
                    '#define HEADER "two\\\\slashes.azsli"\n'
                    '#include HEADER\n')
        result = self.run_compiler(source, "-I", self.root, "-E")
        self.assertEqual(result.stdout.count("float from_angle"), 1)
        self.assertEqual(result.stdout.count("float from_quoted"), 2)
        self.run_compiler(source, "-I", self.root, "--syntax")
        self.run_compiler("-", "-E", source="#include <unterminated\n", expected_exit=1)

    def test_inactive_branches_and_conditional_arithmetic(self):
        source = """#if 0
#include "missing"
#error inactive
@This is invalid AZSL
#elif defined(YES) && (2 + 3 * 4 == 14) && (1 || 1/0) && (0 ? 1/0 : 1)
#if (-1 < 1u) || (-1 > 1) || (1u << 2 != 4) || ('\\n' != 10)
#error arithmetic
#endif
float good;
#else
#error configuration
#endif
"""
        self.run_compiler("-", "--syntax", "-DYES", source=source)
        self.run_compiler("-", "-E", source="#if 1/0\n#endif\n", expected_exit=1)

    def test_rescanning_paste_variadics_and_suppression(self):
        source = """#define str(x) #x
#define cat(a,b) a##b
#define expandcat(a,b) cat(a,b)
#define L fl
#define F(x) x
#define ALIAS F
#define V(first,...) first __VA_ARGS__
#define SELF SELF
#define MUTUAL OTHER
#define OTHER MUTUAL
expandcat(L,oat) ALIAS(value);
str(a /*comment*/ + b)
cat(,empty) cat(empty,) cat(,) V(one,two,three) V(one)
F(F(nested)) SELF MUTUAL
"""
        result = self.run_compiler("-", "-E", source=source)
        text = self.body(result)
        self.assertRegex(text, r"float\s+value\s*;")
        self.assertIn('"a + b"', text)
        self.assertRegex(text, r"empty\s+empty\s+one\s+two\s*,\s*three\s+one")
        self.assertIn("nested SELF MUTUAL", text)
        self.run_compiler("-", "-E", source="#define C(a,b) a##b\nC(+,*)\n", expected_exit=1)

    def test_bom_crlf_splices_comments_and_unicode(self):
        source = self.file("utf8", b'\xef\xbb\xbf#define TY fl\\\r\noat\r\n/* caf\xc3\xa9 */\r\nTY value; // \xf0\x9f\x98\x80\r\n')
        result = self.run_compiler(source, "-E", "-C", "-+")
        self.assertIn("café", result.stdout)
        self.assertIn("😀", result.stdout)
        self.run_compiler(source, "--syntax")
        diagnostic = self.run_compiler("-", source="/*é*/ @", expected_exit=1)
        self.assertIn("(1,7)", diagnostic.stderr)

    def test_preprocessed_export_roundtrip_and_legacy_inline_line(self):
        emitted = self.run_compiler("-", source="#define E\nvoid f() { int x; x+E+1; }\n")
        self.assertRegex(emitted.stdout, r"x\s*\+\s+\+\s*1")
        source = self.file("original", '#define ATTR [[azsl::int_range(0, 3)]]\nATTR option int choice = 0;\n')
        exported = self.root / "exported"
        self.run_compiler(source, "-E", "-o", exported)
        self.run_compiler(exported, "--preprocessed", "--syntax")
        self.run_compiler("-", "--preprocessed", "--syntax", source='float a;#line 20 "legacy"\nfloat b;\n')
        self.run_compiler("-", "--syntax", source='float a;#line 20 "legacy"\nfloat b;\n', expected_exit=1)

    def test_diagnostics_have_use_definition_and_include_locations(self):
        self.file("header", "#define BAD @\n")
        source = self.file("root", '#include "header"\n#line 40 "presumed.azsl"\nBAD value;\n')
        result = self.run_compiler(source, expected_exit=1)
        self.assertIn("presumed.azsl(40,", result.stderr)
        self.assertIn("header(1,", result.stderr)
        result = self.run_compiler("-", "-E", source='#include "absent"\n', expected_exit=1)
        self.assertIn("include file not found", result.stderr)

    def test_reflection_and_hlsl_file_switches(self):
        source = self.file("mapped", '''#line 10 "first.azsli"
ShaderResourceGroupSemantic FirstSemantic { FrequencyId = 0; };
ShaderResourceGroup First : FirstSemantic { float x; }
#line 10 "second.azsli"
ShaderResourceGroupSemantic SecondSemantic { FrequencyId = 1; };
ShaderResourceGroup Second : SecondSemantic { float y; }
float main() { return First::x + Second::y; }
''')
        result = self.run_compiler(source, "--srg")
        self.assertIn("first.azsli", result.stdout)
        self.assertIn("second.azsli", result.stdout)
        result = self.run_compiler(source)
        self.assertIn("first.azsli", result.stdout)
        self.assertIn("second.azsli", result.stdout)

    def test_warnings_pragmas_and_eof(self):
        result = self.run_compiler("-", "-E", source="#warning diagnostic text\n#pragma custom value\nfloat value;")
        self.assertIn("warning #601: diagnostic text", result.stderr)
        self.assertIn("#pragma custom value", result.stdout)
        self.assertRegex(result.stdout, r"float\s+value\s*;")
        self.run_compiler("-", "--syntax", source="// empty translation unit")
        invalid = self.file("invalid-utf8", b"// \xff\n")
        result = self.run_compiler(invalid, "--syntax", expected_exit=1)
        self.assertIn("invalid UTF-8", result.stderr)

    def test_generated_spellings_survive_reflection_and_emission(self):
        # Pasted identifiers exceed one spelling-arena block.
        # The earliest and latest names must both remain valid in parser, reflection and emission.
        prefix = "generated_shader_resource_group_field_with_a_long_name_"
        source = self.file("generated", "#define CAT(a,b) a##b\n"
                           "#define TYPE CAT(float,4)\n"
                           f"#define FIELD(n) TYPE CAT({prefix},n);\n"
                           "ShaderResourceGroupSemantic S { FrequencyId = 0; };\n"
                           "ShaderResourceGroup CAT(Generated,Srg) : S {\n" +
                           "".join(f"FIELD({i})\n" for i in range(1400)) +
                           "};\nfloat4 Main() { return GeneratedSrg::" + prefix + "1399; }\n")
        reflection = self.run_compiler(source, "--srg")
        json.loads(reflection.stdout)
        emitted = self.run_compiler(source)
        for result in (reflection, emitted):
            self.assertIn(prefix + "0", result.stdout)
            self.assertIn(prefix + "1399", result.stdout)

    def test_bounded_malformed_inputs(self):
        for source in ("#if 1\n", "#else\n", "#define F(x,x) x\n", "#define F(x) # nope\n", "/* unterminated",
                       "#define F(x) x\nF(", "#if " + "!" * 600 + "1\n#endif", "/*\x00*/"):
            self.run_compiler("-", "-E", source=source, expected_exit=1)
        self.run_compiler("-", "--unsupported-preprocessor-switch", source="", expected_exit=109)
        rng = random.Random(122)
        alphabet = "abc012#()/*\\\n'\"+- \t"
        for _ in range(40):
            source = "".join(rng.choice(alphabet) for _ in range(100))
            result = subprocess.run([COMPILER, "-", "-E"], input=source, text=True, encoding="utf-8",
                                    capture_output=True, cwd=self.root, timeout=10)
            self.assertIn(result.returncode, (0, 1), result.stderr)


if __name__ == "__main__":
    unittest.main()
