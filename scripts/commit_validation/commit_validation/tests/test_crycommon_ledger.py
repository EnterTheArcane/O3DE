#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from commit_validation.crycommon_ledger import (
    _parse_header,
    _source_reference_tokens,
    build_ledger,
    csv_text,
    validation_errors,
)


def test_declaration_parser_distinguishes_constants_functions_and_local_aliases(tmp_path):
    header = tmp_path / 'Cry_Math.h'
    header.write_text(
        '''
#define FEATURE_SVO_GI 1
const float gf_PI = float(3.14159);
const double g_PI = 3.14159;
template <typename T, unsigned N>
char (*RtlpNumberOf(T (&)[N]))[N];
template <class T>
const T& min(const T& a, const T& b) { typedef T LocalAlias; return a < b ? a : b; }
template <class T>
void ZeroStruct(T& value)
{ consume(static_cast<void*>(&value)); }
template <class T>
T& non_const(const T& value)
{ return const_cast<T&>(value); }
namespace AZ
{
    class Name;
}
''',
        encoding='utf8',
    )

    declarations, unresolved, _ = _parse_header(header, 'Cry_Math.h')
    sites = {(site.qualified_name, site.kind) for site in declarations}

    assert ('gf_PI', 'constant') in sites
    assert ('g_PI', 'constant') in sites
    assert ('RtlpNumberOf', 'template_function') in sites
    assert ('min', 'template_function') in sites
    assert ('ZeroStruct', 'template_function') in sites
    assert ('non_const', 'template_function') in sites
    assert ('AZ::Name', 'class') in sites
    assert not any(site.qualified_name == 'float' and site.kind.endswith('function') for site in declarations)
    assert not any(site.qualified_name == 'char' and site.kind.endswith('function') for site in declarations)
    assert not any(site.qualified_name in ('static_cast', 'const_cast') for site in declarations)
    assert not any(site.qualified_name == 'LocalAlias' for site in declarations)
    assert not unresolved


def test_preprocessor_symbol_references_are_retained():
    assert list(_source_reference_tokens('#if FEATURE_SVO_GI && defined(LOBYTE)')) == [
        'FEATURE_SVO_GI', 'defined', 'LOBYTE'
    ]
    assert list(_source_reference_tokens('#define LOCAL_BYTE(value) LOBYTE(value)')) == [
        'LOBYTE', 'value'
    ]
    assert not list(_source_reference_tokens('#include <CryCommon/platform.h>'))


def test_full_ledger_is_deterministic_and_self_validating(tmp_path):
    crycommon_root = tmp_path / 'Code' / 'Legacy' / 'CryCommon'
    crycommon_root.mkdir(parents=True)
    (tmp_path / 'Gems' / 'Example').mkdir(parents=True)
    (crycommon_root / 'Cry_Math.h').write_text(
        '#define FEATURE_SVO_GI 1\n'
        'using f32 = float;\n'
        'const f32 gf_PI = f32(3.14159);\n'
        'const double g_PI = 3.14159;\n',
        encoding='utf8',
    )
    (tmp_path / 'Code' / 'Use.cpp').write_text(
        '#include <CryCommon/Cry_Math.h>\n#if FEATURE_SVO_GI\nfloat value = gf_PI;\n#endif\n',
        encoding='utf8',
    )
    (tmp_path / 'Code' / 'UseUnqualified.cpp').write_text(
        '#include <Cry_Math.h>\ndouble value = g_PI;\n', encoding='utf8'
    )
    (tmp_path / 'Gems' / 'Example' / 'CMakeLists.txt').write_text(
        'ly_add_target(\n    NAME Example\n    BUILD_DEPENDENCIES PRIVATE Legacy::CryCommon\n)\n',
        encoding='utf8',
    )

    result = build_ledger(tmp_path)

    assert not validation_errors(result)
    assert result.explicit_include_counts['Cry_Math.h'] == 1
    assert result.unqualified_include_counts['Cry_Math.h'] == 1
    assert result.symbols['FEATURE_SVO_GI'].external_use_count == 1
    assert result.symbols['gf_PI'].external_use_count == 1
    assert len(result.dependency_records) == 1
    assert result.dependency_records[0].target == 'Example'
    assert csv_text(result) == csv_text(build_ledger(tmp_path))
