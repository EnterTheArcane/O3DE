"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

C++20 preprocessing regression tests. Normal CTest runs need no reference compiler.
Pass --reference /path/to/clang for an additional comparison against its C++20 mode.
Fixed expected token sequences test lexing independently; reference output is then
lexed in preprocessed mode to compare macro/directive results without whitespace noise.
"""

import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess
import tempfile
import unittest


parser = argparse.ArgumentParser()
parser.add_argument("probe", type=Path)
parser.add_argument("compiler", type=Path)
parser.add_argument("--reference", type=Path)
options = parser.parse_args()


def decode_tokens(data):
    tokens = []
    position = 0
    while position < len(data):
        colon = data.index(b":", position)
        end = colon + 1 + int(data[position:colon])
        assert data[end:end + 1] == b",", "invalid token framing"
        tokens.append(data[colon + 1:end].decode("utf-8"))
        position = end + 1
    return tokens


class Conformance(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="azsl conformance ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def file(self, name, source):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(source.encode("utf-8") if isinstance(source, str) else source)
        return path

    def invoke(self, source, preprocessed=False):
        path = self.file("input.azsl", source)
        command = [str(options.probe.resolve()), "--preprocessed-tokens" if preprocessed else "--tokens", str(path), str(self.root)]
        return subprocess.run(command, capture_output=True, timeout=20)

    def expect(self, source, expected, reference=True):
        result = self.invoke(source)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(decode_tokens(result.stdout), expected, source)
        if options.reference and reference:
            command = [str(options.reference.resolve()), "-E", "-P", "-x", "c++", "-std=c++20", "-pedantic-errors",
                       "-I", str(self.root), str(self.root / "input.azsl")]
            oracle = subprocess.run(command, capture_output=True, timeout=20)
            self.assertEqual(oracle.returncode, 0, oracle.stderr.decode("utf-8", errors="replace"))
            tokens = self.invoke(oracle.stdout, preprocessed=True)
            self.assertEqual(tokens.returncode, 0, tokens.stderr.decode("utf-8", errors="replace"))
            self.assertEqual(decode_tokens(tokens.stdout), expected, source)

    def reject(self, source, diagnostic=None):
        result = self.invoke(source)
        self.assertGreater(result.returncode, 0, source)
        self.assertIn(b"error #600:", result.stderr)
        if diagnostic:
            self.assertIn(diagnostic.encode(), result.stderr)

    def test_literal_boundaries_and_pasting(self):
        self.expect('#define L WRONG\n#define u8 WRONG\n#define _suffix WRONG\nL"hello" u8"hello" "hello"_suffix\n',
                    ['L"hello"', 'u8"hello"', '"hello"_suffix'])
        self.expect('#define JOIN(a,b) a ## b\nJOIN(L,"text") JOIN(u8,\'x\') JOIN(1e,+2)\n',
                    ['L"text"', "u8'x'", '1e+', '2'])
        self.expect('#define JOIN(a,b) a ## b\nJOIN(->,*) JOIN(.,*) JOIN(<=,>)\n', ['->*', '.*', '<=>'])
        self.expect('#define x WRONG\n#define TEXT(x) #x\nTEXT(1e+x)\n', ['"1e+x"'])

    def test_raw_strings_and_splicing(self):
        for prefix in ['', 'u8', 'u', 'U', 'L']:
            raw = prefix + 'R"mark(first " second\n#define INSIDE 1\n)mark"'
            self.expect(raw + '\nINSIDE\n', [raw, 'INSIDE'])
        raw = 'R"(one\\\ntwo)"'
        self.expect(raw + '\n', [raw])
        self.expect('u\\\n8"text"\n', ['u8"text"'])
        self.expect('R\\\n"(one\\\ntwo)"\n', ['R"(one\\\ntwo)"'])
        self.expect('"a\\\r\nb"\n', ['"ab"'])
        self.reject('R"0123456789abcdefg(text)0123456789abcdefg"\n', 'delimiter')
        self.reject('R"(unterminated\n', 'unterminated raw')

    def test_digraphs_and_alternative_tokens(self):
        self.expect('%:define VALUE 9\nVALUE\n', ['9'])
        self.expect('#define TEXT(x) %:x\n#define JOIN(a,b) a %:%: b\nTEXT(hello) JOIN(first,second)\n',
                    ['"hello"', 'firstsecond'])
        self.expect('a<::b> a<::> a<:::b>\n', ['a', '<', '::', 'b', '>', 'a', '<:', ':>', 'a', '<:', '::', 'b', '>'])
        self.expect('#if true and not false and (6 bitand 2) and (1 not_eq 2)\nyes\n#endif\n', ['yes'])
        self.reject('#define and replacement\n')

    def test_unicode_names(self):
        self.expect('#define \\u03b1 7\nα \\u03B1\n', ['7', '7'])
        self.expect('#define ID(α) α\nID(42)\n', ['42'])
        self.expect('#define α 1\n#if defined(\\u03b1)\nyes\n#endif\n#undef \\u03b1\nα\n', ['yes', 'α'])
        for source in ['#define \\uD800 1\n', '#define \\U00110000 1\n', '#define \\u0301 1\n', '#define \\u0041 1\n',
                       b'#define \xc0\xaf 1\n', b'#define \xf4\x90\x80\x80 1\n']:
            self.reject(source)

    def test_numbers_and_character_expressions(self):
        expressions = ["0b1010 == 10", "0B11u == 3", "1'000 == 1000", "0xA'B == 171", "0b1'010 == 10",
                       "L'A' == 65", "u'\\u03b1' == 945", "U'😀' == 0x1f600", "u8'A' == 65",
                       "'\\101' == 65", "'\\x41' == 65", "'ab' == 0x6162", "'\\xff' == -1",
                       "u'\\xffff' == 65535", "(u'a' - 100) > 0", "(-1 < 1) && !(-1 < 1u)",
                       "((1 ? -1 : 0u) > 0)", "((-8 >> 1u) == -4)", "0xffffffffffffffffu + 1u == 0"]
        for expression in expressions:
            with self.subTest(expression=expression):
                self.expect('#if ' + expression + '\nyes\n#else\nno\n#endif\n', ['yes'])
        for suffix in ['', 'u', 'U', 'l', 'L', 'll', 'LL', 'ul', 'uL', 'Ull', 'ULL', 'lu', 'LU', 'llU', 'LLu']:
            self.expect('#if 1' + suffix + '\nyes\n#endif\n', ['yes'])
        for number in ['1lL', '1Ll', '1lul', '1uu', '0b102', "1'", "0x'1", "1'u", '09', '1.0', '1e+2', '0x',
                       '18446744073709551616u', '9223372036854775808']:
            self.reject('#if ' + number + '\nyes\n#endif\n')
        for character in ["''", "u'ab'", "u8'é'", "u'😀'", "U'\\U00110000'", "'\\x'", "'\\q'", "'a'_suffix"]:
            self.reject('#if ' + character + '\nyes\n#endif\n')

    def test_short_circuit_and_overflow(self):
        for expression in ['1 || 1/0', '!(0 && 1/0)', '(1 ? 4 : 1/0) == 4', '(0 ? 1/0 : 7) == 7',
                           '1 || (1 << 64)', '1 || (9223372036854775807 + 1)', '1 || (-(-9223372036854775807 - 1))']:
            self.expect('#if ' + expression + '\nyes\n#endif\n', ['yes'])
        for expression in ['1/0', '1 << 64', '1 << -1', '9223372036854775807 + 1', '(-9223372036854775807 - 1) - 1',
                           '9223372036854775807 * 2', '(-9223372036854775807 - 1) / -1', '-(-9223372036854775807 - 1)']:
            self.reject('#if ' + expression + '\nyes\n#endif\n')
        self.reject('#if 1 || (2 + )\nyes\n#endif\n')

    def test_inactive_structure(self):
        self.expect('#if 0\n#ifdef 123 + 456\n#include "absent"\n#error ignored\n#endif\n#ifndef\n#endif\n#endif\nyes\n', ['yes'])
        self.expect('#if 1\nyes\n#elif 1/0\n#error ignored\n#else\n#error ignored\n#endif\n', ['yes'])
        self.reject('#ifdef 123 + 456\n#endif\n')
        self.reject('#if 0\n#else\n#else\n#endif\n')
        self.reject('#if 0\n/* unterminated\n')

    def test_empty_macro_whitespace(self):
        self.expect('#define EMPTY\n#define TEXT(x) #x\n#define EXPAND_TEXT(x) TEXT(x)\nEXPAND_TEXT(a EMPTY+b)\n', ['"a +b"'])
        self.expect('#define TEXT(x) #x\n#define F(x) TEXT(a x+b)\nF()\n', ['"a +b"'])
        self.expect('#define TEXT(x) #x\nTEXT(a\t/**/ b) TEXT( )\n', ['"a b"', '""'])

    def test_rescanning_and_suppression(self):
        self.expect('#define A B\n#define B A\nA B\n', ['A', 'B'])
        self.expect('#define ID(x) x\n#define ALIAS ID\nID(ALIAS)(2)\n', ['ID', '(', '2', ')'])
        self.expect('#define ID(x) x\nID(ID(ID(7)))\n', ['7'])
        self.expect('#define FN(x) x+x\n#define NAME FN\nNAME(3)\n', ['3', '+', '3'])
        self.expect('#define G(x) x\n#define F(x) G(x)\nF(F)(2)\n', ['F', '(', '2', ')'])
        self.expect('#define JOIN(a,b) a ## b\n#define XY 9\nJOIN(X,Y) JOIN(,XY) JOIN(XY,) JOIN(,)\n', ['9', '9', '9'])
        self.expect('#define T(a,b,c) a ## b ## c\nT(x,,z) T(,,) T(,y,)\n', ['xz', 'y'])
        self.reject('#define JOIN(a,b) a ## b\nJOIN(+,*)\n', 'invalid preprocessing token')

    def test_va_opt(self):
        self.expect('#define V(...) begin __VA_OPT__(, __VA_ARGS__) end\nV() V(a,b)\n',
                    ['begin', 'end', 'begin', ',', 'a', ',', 'b', 'end'])
        self.expect('#define EMPTY\n#define V(...) __VA_OPT__(present)\nV(EMPTY) V(,) V()\n', ['present'])
        self.expect('#define V(x,...) x __VA_OPT__(+ __VA_ARGS__)\nV(1) V(1,) V(1,2,3)\n', ['1', '1', '1', '+', '2', ',', '3'])
        self.expect('#define V(x,...) #__VA_OPT__(x ## x x ## x)\nV(,0) V(a,0) V(a)\n', ['""', '"aa aa"', '""'])
        self.expect('#define V(x,...) __VA_OPT__(a x ## x) ## b\nV(,1) V(x,1) V(x)\n', ['a', 'b', 'a', 'xxb', 'b'])
        self.expect('#define V(x,...) b ## __VA_OPT__(x 1) ## 1\nV(,1) V(a,1) V(a)\n', ['b', '11', 'ba', '11', 'b1'])
        self.expect('#define V(...) #__VA_OPT__() X ## __VA_OPT__()\nV() V(1)\n', ['""', 'X', '""', 'X'])
        self.expect('#define EMPTY\n#define V(...) #__VA_OPT__(EMPTY)\nV(1)\n', ['"EMPTY"'])
        self.expect('#define VALUE 7\n#define V(x,...) #__VA_OPT__(VALUE x)\nV(VALUE,1)\n', ['"VALUE 7"'])
        self.expect('#define OPEN() (\n#define G(x) 42\n#define F(a,b,...) __VA_OPT__(G a b) )\nF(OPEN(),0,yes)\n', ['42'])
        for body in ['__VA_OPT__', '__VA_OPT__(', '__VA_OPT__(__VA_OPT__())', '__VA_OPT__(## x)', '__VA_OPT__(x ##)', '__VA_OPT__(#)']:
            self.reject('#define V(...) ' + body + '\n')
        self.reject('#define F() __VA_OPT__(x)\n')
        self.reject('#define F __VA_ARGS__\n')

    def test_macro_definition_diagnostics(self):
        self.expect('#define X a+b\n#define X a+b\nX\n', ['a', '+', 'b'])
        for source in ['#define X a+b\n#define X a +b\n', '#define X+1\n', '#define X(a,a) a\n',
                       '#define X(a,) a\n', '#define X(a) #wrong\n', '#define X ## a\n', '#define X a ##\n',
                       '#define X(a,b) a\nX(1)\n', '#define X() 1\nX(1)\n']:
            self.reject(source)
        for name in ['__LINE__', '__FILE__', '__DATE__', '__TIME__', '__cplusplus', '__has_include', '__has_cpp_attribute', '_Pragma']:
            self.reject('#define ' + name + ' 1\n')
            self.reject('#undef ' + name + '\n')

    def test_header_queries(self):
        self.file('folder/header.azsli', '#error query must not open this file\n')
        self.expect('#if defined(__has_include) && __has_include("folder/header.azsli") && !__has_include("absent")\nyes\n#endif\n', ['yes'])
        self.expect('#define HEADER "folder/header.azsli"\n#define QUERY __has_include\n#if QUERY(HEADER)\nyes\n#endif\n', ['yes'])
        self.expect('#define folder absent\n#if __has_include(<folder/header.azsli>)\nyes\n#endif\n', ['yes'])
        self.expect('#if __has_include(<folder//header.azsli>)\nyes\n#endif\n', ['yes'])
        self.expect('#if defined(__has_cpp_attribute) && !__has_cpp_attribute(azsl_test::absent)\nyes\n#endif\n', ['yes'])
        self.expect('#define ATTR azsl_test::absent\n#if !__has_cpp_attribute(ATTR)\nyes\n#endif\n', ['yes'])
        self.reject('#if __has_include()\n#endif\n')
        self.reject('#if __has_cpp_attribute(a+b)\n#endif\n')

    def test_pragma_operator(self):
        self.file('once.azsli', '#define DO(x) _Pragma(#x)\nDO(once)\nvalue\n')
        self.expect('#include "once.azsli"\n#include "once.azsli"\n', ['value'])
        self.expect('#define DO(x) _Pragma(#x)\n#define TEXT(x) #x\n#define WRAP(x) TEXT(x)\nWRAP(DO(azsl_unknown))\n', ['"_Pragma(\\"azsl_unknown\\")"'])
        self.expect('_Pragma(L"azsl_unknown") value\n', ['value'])
        for prefix in ['', 'L', 'u8', 'u', 'U']:
            self.file('raw-once.azsli', '_Pragma(' + prefix + 'R"tag(once)tag")\nvalue\n')
            self.expect('#include "raw-once.azsli"\n#include "raw-once.azsli"\n', ['value'])
            self.expect('_Pragma(' + prefix + '"azsl_unknown") value\n', ['value'])
        self.reject('_Pragma(1)\n')
        self.reject('_Pragma("unknown"_suffix)\n')
        self.reject('#define ID(x) x\nID(_Pragma)("unknown")\n')
        self.reject('#define TEXT(x) #x\n#define WRAP(x) TEXT(x)\nWRAP(_Pragma(1))\n')
        self.expect('#define TEXT(x) #x\n#define WRAP(x) TEXT(x)\nWRAP(_Pragma ( "unknown" ))\n',
                    ['"_Pragma ( \\"unknown\\" )"'])

    def test_locations_and_export(self):
        self.expect('#line 77 "logical.azsl"\n#define HERE __LINE__\nHERE __FILE__\n', ['78', '"logical.azsl"'])
        self.expect('#line 17 ""\n__FILE__ __LINE__\n#line 29\n__FILE__ __LINE__\n', ['""', '17', '""', '29'])
        self.expect('#line 3 "a\\u03b1b"\n__FILE__\n', ['"aαb"'])
        self.expect('#line 3 "a\\nb"\n__FILE__\n', ['"a\\nb"'])
        self.expect('#line 3 R"(a\\nb)"\n__FILE__\n', ['"a\\\\nb"'])
        # C++20 permits numeric escapes here; newer Clang applies the later
        # unevaluated-string-literal restriction even in its C++20 mode.
        self.expect('#line 3 "a\\141b"\n__FILE__\n', ['"aab"'], reference=False)
        for filename in ['L"name"', 'u8"name"', '"name"_suffix', '"\\q"', '"\\x"', '"\\uD800"']:
            self.reject('#line 3 ' + filename + '\n')
        for filename, expected in [('""', '""'), ('"a\\nb"', '"' + Path.cwd().as_posix() + '/a\\nb"')]:
            source = self.file('mapped.azsl', '#line 3 ' + filename + '\nstatic float value;\n')
            output = self.root / 'mapped.hlsl'
            result = subprocess.run([str(options.compiler.resolve()), str(source), '-o', str(output)], capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('#line 3 ' + expected + '\n', output.read_text())
        source = self.file('export.azsl', '#define PLUS +\n#define PREFIX L\nPLUS+ PREFIX"text"\n')
        result = subprocess.run([str(options.compiler.resolve()), '-E', str(source)], capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        tokens = self.invoke(result.stdout, preprocessed=True)
        self.assertEqual(tokens.returncode, 0, tokens.stderr)
        self.assertEqual(decode_tokens(tokens.stdout), ['+', '+', 'L', '"text"'])

    def test_azsl_parser_integration(self):
        source = self.file('shader.azsl', '''
#define JOIN(a,b) a ## b
#define TYPE JOIN(flo,at)
#define DECLARE(name,...) TYPE name __VA_OPT__(= __VA_ARGS__);
%:if __cplusplus >= 202002L and not false
struct S <% TYPE values<:2:>; %>;
DECLARE(uninitialized)
DECLARE(initialized, 1.0)
%:else
This is deliberately invalid AZSL.
%:endif
''')
        result = subprocess.run([str(options.compiler.resolve()), '--syntax', str(source)], capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr.decode('utf-8', errors='replace'))

    def test_structured_expression_corpus(self):
        rng = random.Random(938723)
        source = []
        expected = []
        for index in range(500):
            a, b, c = [rng.randrange(-1000, 1001) for _ in range(3)]
            expression = f'(({a}) + ({b}) * ({c}))'
            value = a + b * c
            source.append(f'#if {expression} == ({value}) && (1 || 1/0)\npassed_{index}\n#else\n#error arithmetic mismatch\n#endif\n')
            expected.append(f'passed_{index}')
        self.expect(''.join(source), expected)

    def test_structured_macro_corpus(self):
        source = ['#define JOIN(a,b) a ## b\n#define TEXT(a) #a\n#define EXPAND_TEXT(a) TEXT(a)\n',
                  '#define ID(a) a\n#define EMPTY\n#define OPTIONAL(...) __VA_OPT__(__VA_ARGS__)\n']
        expected = []
        for index in range(300):
            source.append(f'#define VALUE_{index} {index}\nID(ID(JOIN(VALUE_,{index}))) OPTIONAL(EMPTY) OPTIONAL({index}) EXPAND_TEXT(JOIN(VALUE_,{index}))\n')
            expected.extend([str(index), str(index), f'"{index}"'])
        self.expect(''.join(source), expected)

    def test_pinned_clang_regressions(self):
        directory = Path(__file__).parent / 'Preprocessor' / 'Clang'
        manifest = json.loads((directory / 'cases.json').read_text())
        for case in manifest['cases']:
            with self.subTest(upstream=case['file']):
                source = (directory / case['file']).read_bytes()
                self.assertEqual(hashlib.sha256(source).hexdigest(), case['sha256'])
                if case['reject']:
                    self.reject(source)
                    if options.reference:
                        oracle = subprocess.run([str(options.reference.resolve()), '-E', '-P', '-x', 'c++', '-std=c++20',
                                                 '-pedantic-errors', str(self.root / 'input.azsl')], capture_output=True, timeout=20)
                        self.assertNotEqual(oracle.returncode, 0, case['file'])
                else:
                    self.expect(source, case['tokens'])


if __name__ == '__main__':
    unittest.main(argv=[__file__], verbosity=2)
