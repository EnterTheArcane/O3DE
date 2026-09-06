"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""
import json
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import unittest

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

    def run_compiler(self, *args, source=None, success=True):
        result = subprocess.run(
            [COMPILER, *map(str, args)], input=source, text=True,
            capture_output=True, cwd=self.root, timeout=20)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertGreater(result.returncode, 0, result.stderr)
        return result

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
        self.run_compiler("-", "-E", source="#include <unterminated\n", success=False)

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
        self.run_compiler("-", "-E", source="#if 1/0\n#endif\n", success=False)

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
        self.assertIn("float value;", text)
        self.assertIn('"a + b"', text)
        self.assertRegex(text, r"empty\s+empty\s+one\s+two,three\s+one")
        self.assertIn("nested SELF MUTUAL", text)
        self.run_compiler("-", "-E", source="#define C(a,b) a##b\nC(+,*)\n", success=False)

    def test_bom_crlf_splices_comments_and_unicode(self):
        source = self.file("utf8", b'\xef\xbb\xbf#define TY fl\\\r\noat\r\n/* caf\xc3\xa9 */\r\nTY value; // \xf0\x9f\x98\x80\r\n')
        result = self.run_compiler(source, "-E", "-C", "-+")
        self.assertIn("café", result.stdout)
        self.assertIn("😀", result.stdout)
        self.run_compiler(source, "--syntax")
        diagnostic = self.run_compiler("-", source="/*é*/ @", success=False)
        self.assertIn("(1,7)", diagnostic.stderr)

    def test_preprocessed_export_roundtrip_and_legacy_inline_line(self):
        emitted = self.run_compiler("-", source="#define E\nvoid f() { int x; x+E+1; }\n")
        self.assertRegex(emitted.stdout, r"x\s*\+\s+\+\s*1")
        source = self.file("original", '#define ATTR [[azsl::int_range(0, 3)]]\nATTR option int choice = 0;\n')
        exported = self.root / "exported"
        self.run_compiler(source, "-E", "-o", exported)
        self.run_compiler(exported, "--preprocessed", "--syntax")
        self.run_compiler("-", "--preprocessed", "--syntax", source='float a;#line 20 "legacy"\nfloat b;\n')
        self.run_compiler("-", "--syntax", source='float a;#line 20 "legacy"\nfloat b;\n', success=False)

    def test_diagnostics_have_use_definition_and_include_locations(self):
        self.file("header", "#define BAD @\n")
        source = self.file("root", '#include "header"\n#line 40 "presumed.azsl"\nBAD value;\n')
        result = self.run_compiler(source, success=False)
        self.assertIn("presumed.azsl(40,", result.stderr)
        self.assertIn("header(1,", result.stderr)
        result = self.run_compiler("-", "-E", source='#include "absent"\n', success=False)
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
        self.assertIn("float value;", result.stdout)
        self.run_compiler("-", "--syntax", source="// empty translation unit")
        invalid = self.file("invalid-utf8", b"// \xff\n")
        result = self.run_compiler(invalid, "--syntax", success=False)
        self.assertIn("invalid UTF-8", result.stderr)

    def test_generated_spellings_survive_reflection_and_emission(self):
        # Exercise large generated spellings through reflection and emission.
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
            self.run_compiler("-", "-E", source=source, success=False)
        self.run_compiler("-", "--unsupported-preprocessor-switch", source="", success=False)
        rng = random.Random(122)
        alphabet = "abc012#()/*\\\n'\"+- \t"
        for _ in range(40):
            source = "".join(rng.choice(alphabet) for _ in range(100))
            result = subprocess.run([COMPILER, "-", "-E"], input=source, text=True,
                                    capture_output=True, cwd=self.root, timeout=10)
            self.assertIn(result.returncode, (0, 1), result.stderr)


if __name__ == "__main__":
    unittest.main()
