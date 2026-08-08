#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Generate a best-effort declaration and use ledger for CryCommon.

This module deliberately does not pretend to be a C++ compiler.  It inventories the public
CryCommon headers with deterministic lexical heuristics, records every declaration candidate it
cannot classify, and correlates the result with source references, include edges, and CMake target
dependencies.  The unresolved rows are part of the contract: they make parser blind spots visible
instead of silently treating them as unused declarations.
"""

from __future__ import annotations

import csv
import io
import re
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import DefaultDict, Dict, Iterable, List, Sequence, Set, Tuple


SOURCE_EXTENSIONS = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hpp', '.hxx', '.inl', '.m', '.mm'}
HEADER_EXTENSIONS = {'.h', '.hpp', '.hxx', '.inl'}

COMPATIBILITY_HEADERS = {
    'Cry_Color.h',
    'Cry_Math.h',
    'Cry_Matrix33.h',
    'Cry_Matrix34.h',
    'Cry_Matrix44.h',
    'Cry_Quat.h',
    'Cry_Vector2.h',
    'Cry_Vector3.h',
    'Cry_Vector4.h',
    'CryRandomInternal.h',
    'Cry_ValidNumber.h',
    'LCGRandom.h',
    'MathConversion.h',
}

DEFERRED_SYSTEMIC_HEADERS = {
    'CryListenerSet.h',
    'CrySystemBus.h',
    'IConsole.h',
    'IGem.h',
    'ISystem.h',
    'TimeValue.h',
}

RETIREMENT_HEADERS = {'MiniQueue.h', 'StlUtils.h'}

# Identifiers this short or generic cannot be attributed reliably without an AST.  They are still
# counted, but their disposition and confidence make the ambiguity explicit.
AMBIGUOUS_IDENTIFIERS = {
    'Add', 'Begin', 'Clear', 'Clone', 'Create', 'Data', 'Delete', 'Empty', 'End', 'Find', 'Get',
    'GetName', 'GetType', 'Init', 'Insert', 'IsValid', 'Load', 'Name', 'None', 'Object', 'Read',
    'Register', 'Release', 'Remove', 'Reset', 'Save', 'Set', 'SetName', 'SetType', 'Size', 'String',
    'Type', 'Update', 'Value', 'Write', 'begin', 'data', 'end', 'size', 'value',
}

CONTROL_WORDS = {
    'alignof', 'catch', 'const_cast', 'decltype', 'do', 'dynamic_cast', 'else', 'for', 'if',
    'noexcept', 'reinterpret_cast', 'return', 'sizeof', 'static_assert', 'static_cast', 'switch',
    'throw', 'while',
}

INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
MACRO_PATTERN = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)(\s*\([^)]*\))?')
NAMESPACE_PATTERN = re.compile(
    r'^\s*namespace(?:\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*))?\s*(\{)?\s*$'
)
TYPE_PATTERN = re.compile(
    r'^\s*(?:(?:template\s*<.*?>)\s*)?(class|struct|union)\s+'
    r'(?:(?:[A-Z][A-Z0-9_]*_API|AZCORE_API|CRYCOMMON_API)\s+)?([A-Za-z_]\w*)'
)
ENUM_PATTERN = re.compile(
    r'^\s*(?:(?:template\s*<.*?>)\s*)?enum(?:\s+class|\s+struct)?\s+'
    r'(?:(?:[A-Z][A-Z0-9_]*_API|AZCORE_API|CRYCOMMON_API)\s+)?([A-Za-z_]\w*)'
)
USING_PATTERN = re.compile(r'^\s*(?:(?:template\s*<.*?>)\s*)?using\s+([A-Za-z_]\w*)\s*=')
CONSTANT_PATTERN = re.compile(
    r'^\s*(?:(?:extern|inline|static)\s+)*'
    r'(?:(?:const|constexpr|constinit)\b.+?|.+?\bconst\b.+?)'
    r'\b([A-Za-z_]\w*)\s*(?:\[[^]]*\])?\s*(?:=|;)'
)
IDENTIFIER_PATTERN = re.compile(r'\b[A-Za-z_]\w*\b')


@dataclass(frozen=True)
class DeclarationSite:
    header: str
    line: int
    kind: str
    qualified_name: str
    snippet: str
    confidence: str = 'medium'
    note: str = ''


@dataclass
class SymbolLedgerEntry:
    identifier: str
    sites: List[DeclarationSite] = field(default_factory=list)
    external_use_count: int = 0
    external_file_use_counts: Dict[str, int] = field(default_factory=dict)
    external_files: Set[str] = field(default_factory=set)
    direct_reference_files: Set[str] = field(default_factory=set)
    inferred_transitive_files: Set[str] = field(default_factory=set)
    unresolved_reference_files: Set[str] = field(default_factory=set)


@dataclass(frozen=True)
class DependencyRecord:
    path: str
    line: int
    target: str
    text: str


@dataclass(frozen=True)
class IncludeRecord:
    path: str
    line: int
    operand: str
    resolved_header: str
    kind: str
    confidence: str


@dataclass
class LedgerResult:
    repository_root: Path
    crycommon_root: Path
    headers: List[str]
    symbols: Dict[str, SymbolLedgerEntry]
    unresolved_declarations: List[DeclarationSite]
    intra_header_includes: Dict[str, Set[str]]
    transitive_header_includes: Dict[str, Set[str]]
    include_records: List[IncludeRecord]
    explicit_include_counts: Counter
    unqualified_include_counts: Counter
    explicit_include_files: DefaultDict[str, Set[str]]
    unqualified_include_files: DefaultDict[str, Set[str]]
    ambiguous_include_count: int
    dependency_records: List[DependencyRecord]


def _sanitize_cpp_lines(text: str) -> List[str]:
    """Remove comments and literals while preserving line and column boundaries."""

    result: List[str] = []
    in_block_comment = False
    quote = ''
    escaped = False

    for raw_line in text.splitlines():
        output = list(raw_line)
        index = 0
        while index < len(raw_line):
            char = raw_line[index]
            next_char = raw_line[index + 1] if index + 1 < len(raw_line) else ''

            if in_block_comment:
                output[index] = ' '
                if char == '*' and next_char == '/':
                    output[index + 1] = ' '
                    in_block_comment = False
                    index += 2
                else:
                    index += 1
                continue

            if quote:
                output[index] = ' '
                if escaped:
                    escaped = False
                elif char == '\\':
                    escaped = True
                elif char == quote:
                    quote = ''
                index += 1
                continue

            if char == '/' and next_char == '/':
                for remainder in range(index, len(output)):
                    output[remainder] = ' '
                break
            if char == '/' and next_char == '*':
                output[index] = output[index + 1] = ' '
                in_block_comment = True
                index += 2
                continue
            if char in ('"', "'"):
                output[index] = ' '
                quote = char
                index += 1
                continue
            index += 1

        # A normal C++ string/character literal cannot cross a physical line without a continuation.
        # Retain the state for a trailing backslash; otherwise reset it to avoid masking later lines.
        if quote and not raw_line.rstrip().endswith('\\'):
            quote = ''
            escaped = False
        result.append(''.join(output))
    return result


def _header_guard_names(raw_lines: Sequence[str]) -> Set[str]:
    guards: Set[str] = set()
    for index, line in enumerate(raw_lines[:40]):
        match = re.match(r'^\s*#\s*ifndef\s+([A-Za-z_]\w*)', line)
        if not match:
            continue
        candidate = match.group(1)
        for following in raw_lines[index + 1:index + 5]:
            if re.match(rf'^\s*#\s*define\s+{re.escape(candidate)}\b', following):
                guards.add(candidate)
                break
    return guards


def _qualified_name(scopes: Sequence[Tuple[str, str, int]], name: str) -> str:
    named_scopes = [scope_name for _, scope_name, _ in scopes if scope_name and scope_name != '<anonymous>']
    return '::'.join([*named_scopes, name]) if named_scopes else name


def _function_name(statement: str) -> str | None:
    if '(' not in statement or statement.lstrip().startswith(
        ('typedef ', 'using ', 'enum ', 'class ', 'struct ', 'union ')
    ):
        return None
    if statement.lstrip().startswith(('#', 'static_assert')):
        return None

    # Only inspect the declarator. Function bodies commonly contain named casts and calls which
    # are not candidates for the declaration name.
    declaration = statement.split('{', 1)[0].split(';', 1)[0]

    # A function returning a pointer-to-array has an extra pair of parentheses before its name.
    # Prefer the declarator name rather than mistaking the return type (for example ``char``) for
    # the function.
    pointer_declarator = re.search(r'\(\s*[*&]\s*([A-Za-z_]\w*)\s*\(', declaration)
    if pointer_declarator:
        return pointer_declarator.group(1)

    operator_match = re.search(
        r'\b(operator\s*(?:<=>|==|!=|<=|>=|<<=?|>>=?|\[\]|\(\)|->\*?|[-+*/%&|^]=?|[<>=~!,]))\s*\(',
        declaration,
    )
    if operator_match and '::' not in declaration[max(0, operator_match.start() - 3):operator_match.start()]:
        return re.sub(r'\s+', '', operator_match.group(1))

    specialization_match = re.search(r'\b([A-Za-z_]\w*)\s*<[^;{}()]+>\s*\(', declaration)
    if specialization_match:
        name = specialization_match.group(1)
        prefix = declaration[max(0, specialization_match.start() - 3):specialization_match.start()]
        if (
            name not in ('const_cast', 'dynamic_cast', 'reinterpret_cast', 'static_cast')
            and not prefix.endswith(('::', '->', '.'))
        ):
            return specialization_match.group(1)

    for match in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', declaration):
        name = match.group(1)
        prefix = declaration[max(0, match.start() - 3):match.start()]
        if name in CONTROL_WORDS or name.isupper() or prefix.endswith(('::', '->', '.')):
            continue
        return name
    return None


def _typedef_names(statement: str) -> List[str]:
    """Return every alias introduced by a lexical typedef declaration.

    C and platform headers commonly place several pointer aliases in one declaration.  A regex
    that selects only the last identifier silently loses the first aliases, so split only on
    top-level commas and inspect each declarator.
    """

    stripped = statement.strip()
    if not stripped.startswith('typedef ') or ';' not in stripped:
        return []

    function_pointer_names = re.findall(r'\(\s*(?:[A-Za-z_]\w*::)?[*&]\s*([A-Za-z_]\w*)\s*\)', stripped)
    if function_pointer_names:
        return function_pointer_names

    body = stripped[len('typedef '):].rsplit(';', 1)[0]
    pieces: List[str] = []
    current: List[str] = []
    depths = {'<': 0, '(': 0, '[': 0, '{': 0}
    opening = {'<': '<', '(': '(', '[': '[', '{': '{'}
    closing = {'>': '<', ')': '(', ']': '[', '}': '{'}
    for char in body:
        if char in opening:
            depths[opening[char]] += 1
        elif char in closing:
            depths[closing[char]] = max(0, depths[closing[char]] - 1)
        if char == ',' and not any(depths.values()):
            pieces.append(''.join(current))
            current = []
        else:
            current.append(char)
    pieces.append(''.join(current))

    names: List[str] = []
    for piece in pieces:
        declarator = re.sub(r'\[[^]]*\]\s*$', '', piece.strip())
        identifiers = IDENTIFIER_PATTERN.findall(declarator)
        if identifiers:
            names.append(identifiers[-1])
    return names


def _is_out_of_class_member(statement: str) -> bool:
    declaration = statement.split('{', 1)[0].split(';', 1)[0]
    owner = r'[A-Za-z_]\w*(?:\s*<[^;{}()]*>)?'
    member = r'(?:~?[A-Za-z_]\w*|operator\s*(?:\[\]|\(\)|[-+*/%<>&|^~=!,]+))'
    return bool(re.search(rf'\b(?:{owner}::)+{member}\s*\(', declaration))


def _constant_name(statement: str) -> str | None:
    """Classify a namespace constant without reading const-qualified function bodies as data."""

    declaration = statement.split('{', 1)[0].strip()
    constant_match = CONSTANT_PATTERN.match(declaration)
    if constant_match:
        name = constant_match.group(1)
        prefix = declaration[:constant_match.start(1)]
        # Parentheses before the declarator generally indicate a function or function pointer.
        # Parentheses in an initializer occur after the name and are harmless here.
        if '(' not in prefix:
            return name

    # Legacy math objects also use direct initialization: ``const Vec2 Vec2_Zero(0, 0);``.
    # This is unavoidably heuristic, so only admit an identifier-looking argument list with no
    # cv/ref/type declaration tokens; ambiguous cases remain unresolved rather than being called
    # constants.
    direct_initializer = re.match(
        r'^\s*(?:(?:extern|inline|static)\s+)*(?:const|constexpr|constinit)\b'
        r'.*?\b([A-Za-z_]\w*)\s*\(([^()]*)\)\s*;\s*$', declaration
    )
    if direct_initializer:
        arguments = direct_initializer.group(2)
        type_tokens = r'\b(?:const|volatile|class|struct|typename|void|bool|char|short|int|long|float|double)\b'
        if not re.search(rf'{type_tokens}|[&*]', arguments):
            return direct_initializer.group(1)
    return None


def _extract_enum_values(text: str) -> Iterable[str]:
    for piece in text.split(','):
        candidate = piece.strip()
        if not candidate:
            continue
        match = re.match(r'^([A-Za-z_]\w*)\s*(?:=|$)', candidate)
        if match:
            yield match.group(1)


def _source_reference_tokens(clean_line: str) -> Iterable[str]:
    """Yield source identifiers, including uses in conditional and replacement macros."""

    stripped = clean_line.lstrip()
    if not stripped.startswith('#'):
        yield from IDENTIFIER_PATTERN.findall(clean_line)
        return

    directive_match = re.match(r'#\s*([A-Za-z_]\w*)\b(.*)', stripped)
    if not directive_match or directive_match.group(1) == 'include':
        return
    directive, body = directive_match.groups()
    if directive == 'define':
        declared_macro = re.match(r'\s*[A-Za-z_]\w*(?:\s*\([^)]*\))?', body)
        if declared_macro:
            body = body[declared_macro.end():]
    yield from IDENTIFIER_PATTERN.findall(body)


def _parse_header(
    header_path: Path, relative_header: str
) -> Tuple[List[DeclarationSite], List[DeclarationSite], Set[str]]:
    raw_text = header_path.read_text(encoding='utf8', errors='replace')
    raw_lines = raw_text.splitlines()
    clean_lines = _sanitize_cpp_lines(raw_text)
    guards = _header_guard_names(raw_lines)
    declarations: List[DeclarationSite] = []
    unresolved: List[DeclarationSite] = []
    includes: Set[str] = set()

    scopes: List[Tuple[str, str, int]] = []
    brace_depth = 0
    pending_template = False
    template_angle_depth = 0
    pending_scope: Tuple[str, str] | None = None
    in_preprocessor_continuation = False
    statement = ''
    statement_line = 0
    statement_is_template = False

    for line_number, (raw_line, clean_line) in enumerate(zip(raw_lines, clean_lines), 1):
        while scopes and brace_depth < scopes[-1][2]:
            scopes.pop()

        include_match = INCLUDE_PATTERN.match(raw_line)
        if include_match:
            includes.add(include_match.group(1).replace('\\', '/'))

        macro_match = MACRO_PATTERN.match(raw_line)
        if macro_match and macro_match.group(1) not in guards:
            name = macro_match.group(1)
            kind = 'function_macro' if macro_match.group(2) else 'object_macro'
            declarations.append(
                DeclarationSite(relative_header, line_number, kind, name, raw_line.strip(), 'high')
            )

        if in_preprocessor_continuation:
            in_preprocessor_continuation = raw_line.rstrip().endswith('\\')
            continue
        if raw_line.lstrip().startswith('#'):
            in_preprocessor_continuation = raw_line.rstrip().endswith('\\')
            continue

        stripped = clean_line.strip()
        if not stripped:
            continue

        # Most CryCommon namespaces and types put the opening brace on the next line.  Keep that
        # pending structural scope so member declarations are not mistaken for namespace-level
        # functions, and so qualified names remain useful when identifiers collide.
        if pending_scope:
            if '{' in clean_line:
                scope_kind, scope_name = pending_scope
                opened_scope = (scope_kind, scope_name, brace_depth + 1)
                scopes.append(opened_scope)
                if scope_kind == 'enum':
                    enum_tail = clean_line.split('{', 1)[1].split('}', 1)[0]
                    for value in _extract_enum_values(enum_tail):
                        declarations.append(
                            DeclarationSite(
                                relative_header, line_number, 'enum_value',
                                _qualified_name(scopes, value), raw_line.strip(), 'low',
                                'Enumerator use attribution is name-based and may collide with other enums.'
                            )
                        )
                brace_depth += clean_line.count('{') - clean_line.count('}')
                pending_scope = None
                statement = ''
                continue
            if ';' in clean_line:
                pending_scope = None
            else:
                # A base-class or underlying-type clause may span several physical lines.
                continue

        inline_namespace_match = re.match(
            r'^\s*namespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\{(.*)\}\s*$', clean_line
        )
        if inline_namespace_match:
            inline_scopes = [*scopes]
            for part in inline_namespace_match.group(1).split('::'):
                inline_scopes.append(('namespace', part, brace_depth + 1))
            body = inline_namespace_match.group(2)
            classified = False
            for candidate in body.split(';'):
                candidate = candidate.strip()
                if not candidate:
                    continue
                type_match = TYPE_PATTERN.match(candidate)
                enum_match = ENUM_PATTERN.match(candidate)
                using_match = USING_PATTERN.match(candidate)
                if type_match:
                    name = type_match.group(2)
                    declarations.append(
                        DeclarationSite(
                            relative_header, line_number, type_match.group(1),
                            _qualified_name(inline_scopes, name), raw_line.strip(), 'medium',
                            'Declaration appears in a one-line namespace and is classified lexically.'
                        )
                    )
                    classified = True
                elif enum_match:
                    name = enum_match.group(1)
                    declarations.append(
                        DeclarationSite(
                            relative_header, line_number, 'enum', _qualified_name(inline_scopes, name),
                            raw_line.strip(), 'medium',
                            'Declaration appears in a one-line namespace and is classified lexically.'
                        )
                    )
                    classified = True
                elif using_match:
                    name = using_match.group(1)
                    declarations.append(
                        DeclarationSite(
                            relative_header, line_number, 'alias', _qualified_name(inline_scopes, name),
                            raw_line.strip(), 'medium',
                            'Declaration appears in a one-line namespace and is classified lexically.'
                        )
                    )
                    classified = True
            if body.strip() and not classified:
                unresolved.append(
                    DeclarationSite(
                        relative_header, line_number, 'unresolved_declaration',
                        f'<unresolved@{relative_header}:{line_number}>', raw_line.strip(), 'low',
                        'One-line namespace body was not classified by the lexical parser.'
                    )
                )
            statement = ''
            continue

        namespace_match = NAMESPACE_PATTERN.match(clean_line)
        if namespace_match:
            namespace_name = namespace_match.group(1) or '<anonymous>'
            if namespace_match.group(2):
                for part in namespace_name.split('::'):
                    scopes.append(('namespace', part, brace_depth + 1))
                brace_depth += clean_line.count('{') - clean_line.count('}')
            else:
                pending_scope = ('namespace', namespace_name)
            statement = ''
            continue

        template_on_line = False
        parse_line = clean_line.strip()
        if template_angle_depth:
            template_angle_depth += parse_line.count('<') - parse_line.count('>')
            if template_angle_depth > 0:
                continue
            template_angle_depth = 0
            parse_line = parse_line.rsplit('>', 1)[-1].strip()
            template_on_line = True
            if not parse_line:
                pending_template = True
                continue
        elif pending_template:
            template_on_line = True
            pending_template = False
        elif re.match(r'^\s*template\s*<', parse_line):
            template_angle_depth = parse_line.count('<') - parse_line.count('>')
            template_on_line = True
            if template_angle_depth > 0:
                continue
            parse_line = parse_line.rsplit('>', 1)[-1].strip()
            if not parse_line:
                pending_template = True
                continue

        structural_scope_depth = max((scope[2] for scope in scopes), default=0)
        at_declaration_level = brace_depth == structural_scope_depth
        type_match = TYPE_PATTERN.match(parse_line) if at_declaration_level else None
        enum_match = ENUM_PATTERN.match(parse_line) if at_declaration_level else None
        new_scope: Tuple[str, str, int] | None = None
        recognized_declaration = False

        if type_match:
            name = type_match.group(2)
            kind = 'template_type' if template_on_line else type_match.group(1)
            declarations.append(
                DeclarationSite(
                    relative_header, line_number, kind, _qualified_name(scopes, name), raw_line.strip(),
                    'medium', 'Visibility is inferred lexically for nested declarations.'
                )
            )
            if '{' in clean_line:
                new_scope = ('type', name, brace_depth + 1)
            elif ';' not in clean_line:
                pending_scope = ('type', name)
            pending_template = False
            statement = ''
            recognized_declaration = True
        elif enum_match:
            name = enum_match.group(1)
            declarations.append(
                DeclarationSite(
                    relative_header, line_number, 'enum', _qualified_name(scopes, name), raw_line.strip(),
                    'medium', 'Visibility is inferred lexically for nested declarations.'
                )
            )
            if '{' in clean_line:
                new_scope = ('enum', name, brace_depth + 1)
                enum_tail = clean_line.split('{', 1)[1].split('}', 1)[0]
                for value in _extract_enum_values(enum_tail):
                    declarations.append(
                        DeclarationSite(
                            relative_header, line_number, 'enum_value',
                            _qualified_name([*scopes, new_scope], value), raw_line.strip(), 'low',
                            'Enumerator use attribution is name-based and may collide with other enums.'
                        )
                    )
            elif ';' not in clean_line:
                pending_scope = ('enum', name)
            pending_template = False
            statement = ''
            recognized_declaration = True
        else:
            using_match = USING_PATTERN.match(parse_line) if at_declaration_level else None
            typedef_names = _typedef_names(parse_line) if at_declaration_level else []
            alias_names = [using_match.group(1)] if using_match else typedef_names
            if alias_names:
                for name in alias_names:
                    kind = 'template_alias' if template_on_line else 'alias'
                    declarations.append(
                        DeclarationSite(
                            relative_header, line_number, kind, _qualified_name(scopes, name), raw_line.strip(),
                            'medium', 'Access/export visibility is inferred lexically for nested aliases.'
                        )
                    )
                pending_template = False
                recognized_declaration = True
                statement = ''

        enum_scopes = [scope for scope in scopes if scope[0] == 'enum']
        if enum_scopes and not enum_match:
            for value in _extract_enum_values(clean_line.split('}', 1)[0]):
                declarations.append(
                    DeclarationSite(
                        relative_header, line_number, 'enum_value', _qualified_name(scopes, value), raw_line.strip(),
                        'low', 'Enumerator use attribution is name-based and may collide with other enums.'
                    )
                )

        in_named_type = any(scope[0] in ('type', 'enum') for scope in scopes)
        in_anonymous_namespace = any(scope[0] == 'namespace' and scope[1] == '<anonymous>' for scope in scopes)
        namespace_depth = max((scope[2] for scope in scopes if scope[0] == 'namespace'), default=0)
        at_namespace_level = not in_named_type and not in_anonymous_namespace and brace_depth == namespace_depth

        if at_namespace_level and not recognized_declaration:
            if not statement:
                statement_line = line_number
                statement_is_template = template_on_line
            else:
                statement_is_template = statement_is_template or template_on_line
            statement = f'{statement} {stripped}'.strip()
            terminates = ';' in stripped or '{' in stripped
            if terminates:
                normalized = re.sub(r'\s+', ' ', statement)
                constant_name = _constant_name(normalized)
                function_name = _function_name(normalized)
                if constant_name:
                    declarations.append(
                        DeclarationSite(
                            relative_header, statement_line, 'constant', _qualified_name(scopes, constant_name),
                            normalized[:300], 'medium', 'Constant classification is lexical.'
                        )
                    )
                    pending_template = False
                elif function_name and not _is_out_of_class_member(normalized):
                    kind = 'template_function' if statement_is_template else 'free_function'
                    declarations.append(
                        DeclarationSite(
                            relative_header, statement_line, kind, _qualified_name(scopes, function_name),
                            normalized[:300], 'medium', 'Free-function classification is lexical.'
                        )
                    )
                    pending_template = False
                elif (
                    ('(' in normalized or normalized.startswith('template'))
                    and not _is_out_of_class_member(normalized)
                    and not normalized.startswith(
                        ('namespace ', 'class ', 'struct ', 'union ', 'enum ', 'using ', 'typedef ', 'static_assert')
                    )
                ):
                    unresolved.append(
                        DeclarationSite(
                            relative_header, statement_line, 'unresolved_declaration',
                            f'<unresolved@{relative_header}:{statement_line}>', normalized[:300], 'low',
                            'Top-level declaration candidate was not classified by the lexical parser.'
                        )
                    )
                statement = ''
                statement_is_template = False

        open_count = clean_line.count('{')
        close_count = clean_line.count('}')
        brace_depth += open_count - close_count
        if new_scope:
            scopes.append(new_scope)

    return declarations, unresolved, includes


def _resolve_include(
    operand: str,
    headers: Set[str],
    unique_basenames: Dict[str, str],
) -> Tuple[str | None, str]:
    normalized = operand.replace('\\', '/')
    if normalized.startswith('CryCommon/'):
        candidate = normalized[len('CryCommon/'):]
        return (candidate if candidate in headers else None), 'explicit'
    if normalized in headers:
        return normalized, 'unqualified'
    basename = Path(normalized).name
    # Only a bare include operand is an unqualified CryCommon include.  Treating
    # ``SomeOtherLibrary/IConsole.h`` as CryCommon merely because its basename is unique creates a
    # convincing-looking false edge.
    if '/' not in normalized and basename in unique_basenames:
        return unique_basenames[basename], 'unqualified'
    return None, 'unresolved'


def _source_files(repository_root: Path) -> Iterable[Path]:
    for source_root in ('Code', 'Gems'):
        root = repository_root / source_root
        for path in root.rglob('*'):
            if not path.is_file() or path.suffix.lower() not in SOURCE_EXTENSIONS:
                continue
            relative = path.relative_to(repository_root)
            if relative.as_posix().startswith('Code/Legacy/CryCommon/'):
                continue
            if 'build' in relative.parts or 'Cache' in relative.parts or 'External' in relative.parts:
                continue
            yield path


def _transitive_closure(include_graph: Dict[str, Set[str]]) -> Dict[str, Set[str]]:
    closure: Dict[str, Set[str]] = {}
    for header in sorted(include_graph):
        visited: Set[str] = set()
        pending = list(include_graph.get(header, set()))
        while pending:
            included = pending.pop()
            if included in visited:
                continue
            visited.add(included)
            pending.extend(include_graph.get(included, set()) - visited)
        closure[header] = visited
    return closure


def _dependency_records(repository_root: Path) -> List[DependencyRecord]:
    records: List[DependencyRecord] = []
    for source_root in ('Code', 'Gems'):
        for path in (repository_root / source_root).rglob('CMakeLists.txt'):
            relative = path.relative_to(repository_root).as_posix()
            lines = path.read_text(encoding='utf8', errors='replace').splitlines()
            in_target = False
            parenthesis_depth = 0
            target_name = '<unresolved-target>'
            set_context = ''
            for line_number, line in enumerate(lines, 1):
                if 'ly_add_target(' in line:
                    in_target = True
                    parenthesis_depth = line.count('(') - line.count(')')
                    target_name = '<unresolved-target>'
                elif in_target:
                    parenthesis_depth += line.count('(') - line.count(')')

                if in_target:
                    name_match = re.match(r'^\s*NAME\s+(.+?)\s*$', line)
                    if name_match and target_name == '<unresolved-target>':
                        # The tokens after NAME are target type/options; the first token is the
                        # actual CMake target name (and may itself be a variable expression).
                        target_name = name_match.group(1).strip().split()[0]
                else:
                    set_match = re.match(r'^\s*set\s*\(\s*([A-Za-z_]\w*)', line)
                    if set_match:
                        set_context = f'set:{set_match.group(1)}'

                occurrence_count = line.count('Legacy::CryCommon')
                for _ in range(occurrence_count):
                    records.append(
                        DependencyRecord(
                            relative,
                            line_number,
                            target_name if in_target else (set_context or '<unresolved-target>'),
                            line.strip(),
                        )
                    )

                if in_target and parenthesis_depth <= 0:
                    in_target = False
                    target_name = '<unresolved-target>'
    return sorted(records, key=lambda record: (record.path, record.line, record.target))


def build_ledger(repository_root: Path) -> LedgerResult:
    repository_root = repository_root.resolve()
    crycommon_root = repository_root / 'Code' / 'Legacy' / 'CryCommon'
    header_paths = sorted(
        path for path in crycommon_root.rglob('*') if path.is_file() and path.suffix.lower() in HEADER_EXTENSIONS
    )
    headers = [path.relative_to(crycommon_root).as_posix() for path in header_paths]
    header_set = set(headers)
    basename_groups: DefaultDict[str, List[str]] = defaultdict(list)
    for header in headers:
        basename_groups[Path(header).name].append(header)
    unique_basenames = {name: paths[0] for name, paths in basename_groups.items() if len(paths) == 1}

    all_declarations: List[DeclarationSite] = []
    unresolved_declarations: List[DeclarationSite] = []
    raw_include_graph: Dict[str, Set[str]] = {}
    for header_path, relative_header in zip(header_paths, headers):
        declarations, unresolved, includes = _parse_header(header_path, relative_header)
        all_declarations.extend(declarations)
        unresolved_declarations.extend(unresolved)
        raw_include_graph[relative_header] = includes

    intra_header_includes: Dict[str, Set[str]] = {header: set() for header in headers}
    for header, operands in raw_include_graph.items():
        for operand in operands:
            resolved, _ = _resolve_include(operand, header_set, unique_basenames)
            if resolved:
                intra_header_includes[header].add(resolved)
    closure = _transitive_closure(intra_header_includes)

    symbols: Dict[str, SymbolLedgerEntry] = {}
    for site in all_declarations:
        # The unqualified identifier is used for best-effort source correlation.  Qualified names and
        # every declaration site remain in the CSV so collisions are never hidden.
        identifier = site.qualified_name.split('::')[-1]
        symbols.setdefault(identifier, SymbolLedgerEntry(identifier)).sites.append(site)

    explicit_include_counts: Counter = Counter()
    unqualified_include_counts: Counter = Counter()
    explicit_include_files: DefaultDict[str, Set[str]] = defaultdict(set)
    unqualified_include_files: DefaultDict[str, Set[str]] = defaultdict(set)
    ambiguous_include_count = 0
    include_records: List[IncludeRecord] = []
    symbol_names = set(symbols)

    for source_path in _source_files(repository_root):
        relative_source = source_path.relative_to(repository_root).as_posix()
        text = source_path.read_text(encoding='utf8', errors='replace')
        raw_lines = text.splitlines()
        included_headers: Set[str] = set()
        for line_number, line in enumerate(raw_lines, 1):
            include_match = INCLUDE_PATTERN.match(line)
            if not include_match:
                continue
            operand = include_match.group(1)
            resolved, include_kind = _resolve_include(operand, header_set, unique_basenames)
            if resolved:
                included_headers.add(resolved)
                include_records.append(
                    IncludeRecord(relative_source, line_number, operand, resolved, include_kind, 'high')
                )
                if include_kind == 'explicit':
                    explicit_include_counts[resolved] += 1
                    explicit_include_files[resolved].add(relative_source)
                else:
                    unqualified_include_counts[resolved] += 1
                    unqualified_include_files[resolved].add(relative_source)
            elif operand.startswith('CryCommon/') or ('/' not in operand and Path(operand).name in basename_groups):
                ambiguous_include_count += 1
                include_records.append(
                    IncludeRecord(relative_source, line_number, operand, '', 'unresolved', 'low')
                )

        inferred_headers = set(included_headers)
        for included in included_headers:
            inferred_headers.update(closure.get(included, set()))

        clean_lines = _sanitize_cpp_lines(text)
        per_file_counts: Counter = Counter()
        for clean_line in clean_lines:
            for token in _source_reference_tokens(clean_line):
                if token in symbol_names:
                    per_file_counts[token] += 1

        for identifier, count in per_file_counts.items():
            entry = symbols[identifier]
            entry.external_use_count += count
            entry.external_file_use_counts[relative_source] = count
            entry.external_files.add(relative_source)
            declaration_headers = {site.header for site in entry.sites}
            if declaration_headers & included_headers:
                entry.direct_reference_files.add(relative_source)
            elif declaration_headers & inferred_headers:
                entry.inferred_transitive_files.add(relative_source)
            else:
                entry.unresolved_reference_files.add(relative_source)

    for entry in symbols.values():
        entry.sites.sort(key=lambda site: (site.header, site.line, site.kind, site.qualified_name))

    return LedgerResult(
        repository_root=repository_root,
        crycommon_root=crycommon_root,
        headers=headers,
        symbols=symbols,
        unresolved_declarations=sorted(unresolved_declarations, key=lambda site: (site.header, site.line)),
        intra_header_includes=intra_header_includes,
        transitive_header_includes=closure,
        include_records=sorted(include_records, key=lambda record: (record.path, record.line, record.operand)),
        explicit_include_counts=explicit_include_counts,
        unqualified_include_counts=unqualified_include_counts,
        explicit_include_files=explicit_include_files,
        unqualified_include_files=unqualified_include_files,
        ambiguous_include_count=ambiguous_include_count,
        dependency_records=_dependency_records(repository_root),
    )


def validation_errors(result: LedgerResult) -> List[str]:
    """Return invariants that catch known high-impact lexical-parser regressions."""

    errors: List[str] = []
    if 'Cry_Math.h' in result.headers:
        for expected_constant in ('gf_PI', 'g_PI'):
            entry = result.symbols.get(expected_constant)
            if not entry or not any(
                site.header == 'Cry_Math.h' and site.kind == 'constant' for site in entry.sites
            ):
                errors.append(f'missing Cry_Math.h constant declaration: {expected_constant}')

        f32_entry = result.symbols.get('f32')
        if f32_entry and any(
            site.header == 'Cry_Math.h' and site.kind in ('free_function', 'template_function')
            for site in f32_entry.sites
        ):
            errors.append('Cry_Math.h initializer cast was misclassified as an f32 function')

    if any(Path(header).name in ('AppleSpecific.h', 'LinuxSpecific.h') for header in result.headers):
        char_entry = result.symbols.get('char')
        if char_entry and any(
            Path(site.header).name in ('AppleSpecific.h', 'LinuxSpecific.h')
            and site.kind in ('free_function', 'template_function')
            for site in char_entry.sites
        ):
            errors.append('RtlpNumberOf pointer-to-array declaration was misclassified as a char function')

    if 'platform.h' in result.headers:
        for expected_helper in ('ZeroStruct', 'non_const'):
            entry = result.symbols.get(expected_helper)
            if not entry or not any(
                site.header == 'platform.h' and site.kind == 'template_function' for site in entry.sites
            ):
                errors.append(f'missing platform.h template helper declaration: {expected_helper}')
        for cast_keyword in ('const_cast', 'dynamic_cast', 'reinterpret_cast', 'static_cast'):
            entry = result.symbols.get(cast_keyword)
            if entry and any(
                site.header == 'platform.h' and site.kind in ('free_function', 'template_function')
                for site in entry.sites
            ):
                errors.append(f'platform.h named cast was misclassified as a function: {cast_keyword}')
    return errors


def _header_category(header: str) -> str:
    basename = Path(header).name
    if basename in COMPATIBILITY_HEADERS:
        return 'compatibility_retained'
    if basename in DEFERRED_SYSTEMIC_HEADERS:
        return 'deferred_systemic'
    if basename in RETIREMENT_HEADERS:
        return 'retirement_in_progress'
    return 'general_public_surface'


def _is_dependency_forward_declaration(site: DeclarationSite) -> bool:
    if site.kind not in ('class', 'struct', 'union'):
        return False
    identifier = site.qualified_name.split('::')[-1]
    return bool(
        re.search(rf'\b(?:class|struct|union)\s+{re.escape(identifier)}\s*;', site.snippet)
    )


def _symbol_disposition(entry: SymbolLedgerEntry) -> Tuple[str, str, str]:
    headers = {site.header for site in entry.sites}
    categories = {_header_category(header) for header in headers}
    kinds = {site.kind for site in entry.sites}
    ambiguous = (
        len(entry.identifier) <= 3
        or entry.identifier in AMBIGUOUS_IDENTIFIERS
        or len({site.qualified_name for site in entry.sites}) > 1
        or 'enum_value' in kinds
        or entry.identifier.startswith('operator')
    )
    notes: List[str] = []
    if len(entry.sites) > 1:
        notes.append(f'{len(entry.sites)} declaration sites are aggregated by unqualified identifier')
    if entry.unresolved_reference_files:
        notes.append('some references have no directly or internally inferred CryCommon include')
    if entry.identifier.startswith('operator'):
        notes.append('operator use counts cannot be correlated by identifier token')

    if all(_is_dependency_forward_declaration(site) for site in entry.sites):
        disposition = 'dependency_forward_declaration'
        notes.append('declaration supplies a dependency name only; identifier matches are not CryCommon use evidence')
    elif 'compatibility_retained' in categories:
        disposition = 'compatibility_retained'
    elif 'deferred_systemic' in categories:
        disposition = 'deferred_systemic'
    elif 'retirement_in_progress' in categories:
        disposition = 'retirement_in_progress' if entry.external_use_count else 'zero_use_retirement_candidate'
    elif ambiguous:
        disposition = 'ambiguous_external_use' if entry.external_use_count else 'ambiguous_zero_use'
    elif entry.external_use_count:
        disposition = 'active_external_use'
    else:
        disposition = 'zero_external_use_unresolved'

    confidence = 'low' if ambiguous or any(site.confidence == 'low' for site in entry.sites) else 'medium'
    if all(site.confidence == 'high' for site in entry.sites) and not ambiguous:
        confidence = 'high'
    return disposition, confidence, '; '.join(notes)


CSV_FIELDS = (
    'record_type', 'identifier', 'qualified_names', 'declaration_kinds', 'header', 'line',
    'declaration_sites', 'external_use_count', 'external_file_count', 'direct_reference_file_count',
    'inferred_transitive_file_count', 'unresolved_reference_file_count', 'explicit_include_count',
    'explicit_include_file_count', 'unqualified_include_count', 'unqualified_include_file_count',
    'cmake_dependency_count', 'disposition', 'confidence', 'notes',
)


def csv_text(result: LedgerResult) -> str:
    output = io.StringIO(newline='')
    writer = csv.DictWriter(output, fieldnames=CSV_FIELDS, lineterminator='\n')
    writer.writeheader()

    for identifier, entry in sorted(result.symbols.items()):
        disposition, confidence, disposition_notes = _symbol_disposition(entry)
        sites = entry.sites
        headers = sorted({site.header for site in sites})
        line = min(site.line for site in sites)
        notes = [site.note for site in sites if site.note]
        if disposition_notes:
            notes.append(disposition_notes)
        writer.writerow({
            'record_type': 'symbol',
            'identifier': identifier,
            'qualified_names': ';'.join(sorted({site.qualified_name for site in sites})),
            'declaration_kinds': ';'.join(sorted({site.kind for site in sites})),
            'header': ';'.join(headers),
            'line': line,
            'declaration_sites': ';'.join(f'{site.header}:{site.line}' for site in sites),
            'external_use_count': entry.external_use_count,
            'external_file_count': len(entry.external_files),
            'direct_reference_file_count': len(entry.direct_reference_files),
            'inferred_transitive_file_count': len(entry.inferred_transitive_files),
            'unresolved_reference_file_count': len(entry.unresolved_reference_files),
            'explicit_include_count': sum(result.explicit_include_counts[header] for header in headers),
            'explicit_include_file_count': len(
                set().union(*(result.explicit_include_files[header] for header in headers))
            ),
            'unqualified_include_count': sum(result.unqualified_include_counts[header] for header in headers),
            'unqualified_include_file_count': len(
                set().union(*(result.unqualified_include_files[header] for header in headers))
            ),
            'cmake_dependency_count': '',
            'disposition': disposition,
            'confidence': confidence,
            'notes': '; '.join(dict.fromkeys(notes)),
        })

        for path, count in sorted(entry.external_file_use_counts.items()):
            if path in entry.direct_reference_files:
                attribution = 'direct_reference'
                confidence = 'high'
            elif path in entry.inferred_transitive_files:
                attribution = 'inferred_transitive_reference'
                confidence = 'medium'
            else:
                attribution = 'unresolved_identifier_match'
                confidence = 'low'
            writer.writerow({
                'record_type': 'symbol_reference',
                'identifier': identifier,
                'qualified_names': ';'.join(sorted({site.qualified_name for site in sites})),
                'declaration_kinds': ';'.join(sorted({site.kind for site in sites})),
                'header': ';'.join(headers),
                'declaration_sites': path,
                'external_use_count': count,
                'external_file_count': 1,
                'disposition': attribution,
                'confidence': confidence,
                'notes': 'Exact per-file lexical match record; unresolved matches may be coincidental.',
            })

    for site in result.unresolved_declarations:
        writer.writerow({
            'record_type': 'unresolved_declaration',
            'identifier': site.qualified_name,
            'qualified_names': site.qualified_name,
            'declaration_kinds': site.kind,
            'header': site.header,
            'line': site.line,
            'declaration_sites': f'{site.header}:{site.line}',
            'external_use_count': '',
            'external_file_count': '',
            'direct_reference_file_count': '',
            'inferred_transitive_file_count': '',
            'unresolved_reference_file_count': '',
            'explicit_include_count': result.explicit_include_counts[site.header],
            'explicit_include_file_count': len(result.explicit_include_files[site.header]),
            'unqualified_include_count': result.unqualified_include_counts[site.header],
            'unqualified_include_file_count': len(result.unqualified_include_files[site.header]),
            'cmake_dependency_count': '',
            'disposition': 'ambiguous_unresolved_declaration',
            'confidence': 'low',
            'notes': f'{site.note} Candidate: {site.snippet}',
        })

    for header in result.headers:
        explicit_count = result.explicit_include_counts[header]
        unqualified_count = result.unqualified_include_counts[header]
        category = _header_category(header)
        if category == 'general_public_surface':
            disposition = (
                'active_external_include'
                if explicit_count or unqualified_count
                else 'zero_direct_include_review_required'
            )
        else:
            disposition = category
        writer.writerow({
            'record_type': 'header',
            'identifier': header,
            'qualified_names': '',
            'declaration_kinds': 'public_header',
            'header': header,
            'line': '',
            'declaration_sites': '',
            'external_use_count': '',
            'external_file_count': '',
            'direct_reference_file_count': '',
            'inferred_transitive_file_count': '',
            'unresolved_reference_file_count': '',
            'explicit_include_count': explicit_count,
            'explicit_include_file_count': len(result.explicit_include_files[header]),
            'unqualified_include_count': unqualified_count,
            'unqualified_include_file_count': len(result.unqualified_include_files[header]),
            'cmake_dependency_count': '',
            'disposition': disposition,
            'confidence': 'high',
            'notes': f'{len(result.intra_header_includes[header])} direct intra-CryCommon include edge(s)',
        })

    for include in result.include_records:
        writer.writerow({
            'record_type': 'external_include_edge',
            'identifier': include.resolved_header or '<unresolved-header>',
            'qualified_names': include.operand,
            'declaration_kinds': include.kind,
            'header': include.resolved_header,
            'line': include.line,
            'declaration_sites': f'{include.path}:{include.line}',
            'disposition': f'{include.kind}_include',
            'confidence': include.confidence,
            'notes': 'External include operand and resolution; exact source edge.',
        })

    for source_header in result.headers:
        for included_header in sorted(result.intra_header_includes[source_header]):
            writer.writerow({
                'record_type': 'intra_header_include_edge',
                'identifier': included_header,
                'declaration_kinds': 'direct_include',
                'header': source_header,
                'declaration_sites': source_header,
                'disposition': 'direct_intra_crycommon_include',
                'confidence': 'high',
                'notes': f'{source_header} directly includes {included_header}',
            })
        indirect_headers = (
            result.transitive_header_includes[source_header] - result.intra_header_includes[source_header]
        )
        for included_header in sorted(indirect_headers):
            writer.writerow({
                'record_type': 'transitive_include_edge',
                'identifier': included_header,
                'declaration_kinds': 'transitive_include',
                'header': source_header,
                'declaration_sites': source_header,
                'disposition': 'inferred_transitive_intra_crycommon_include',
                'confidence': 'medium',
                'notes': f'{source_header} reaches {included_header} through the lexical include graph',
            })

    dependency_count = len(result.dependency_records)
    for dependency in result.dependency_records:
        writer.writerow({
            'record_type': 'cmake_dependency',
            'identifier': dependency.target,
            'qualified_names': '',
            'declaration_kinds': 'Legacy::CryCommon',
            'header': '',
            'line': dependency.line,
            'declaration_sites': f'{dependency.path}:{dependency.line}',
            'external_use_count': '',
            'external_file_count': '',
            'direct_reference_file_count': '',
            'inferred_transitive_file_count': '',
            'unresolved_reference_file_count': '',
            'explicit_include_count': '',
            'explicit_include_file_count': '',
            'unqualified_include_count': '',
            'unqualified_include_file_count': '',
            'cmake_dependency_count': dependency_count,
            'disposition': 'allowlisted_dependency_review_required',
            'confidence': 'medium' if dependency.target != '<unresolved-target>' else 'low',
            'notes': dependency.text,
        })
    return output.getvalue()


def _markdown_escape(value: object) -> str:
    return str(value).replace('|', '\\|').replace('\n', ' ')


def report_text(result: LedgerResult) -> str:
    disposition_counts: Counter = Counter()
    for entry in result.symbols.values():
        disposition_counts[_symbol_disposition(entry)[0]] += 1

    declaration_site_count = sum(len(entry.sites) for entry in result.symbols.values())
    explicit_include_count = sum(result.explicit_include_counts.values())
    unqualified_include_count = sum(result.unqualified_include_counts.values())
    dependency_file_count = len({record.path for record in result.dependency_records})
    ambiguous_symbols = [
        entry for entry in result.symbols.values() if _symbol_disposition(entry)[1] == 'low'
    ]
    zero_use_symbols = [entry for entry in result.symbols.values() if not entry.external_use_count]
    active_symbols = sorted(
        result.symbols.values(), key=lambda entry: (-entry.external_use_count, entry.identifier)
    )

    lines = [
        '# CryCommon declaration and use ledger',
        '',
        'Generated deterministically by `scripts/commit_validation/generate_crycommon_ledger.py`.',
        '',
        '## Coverage summary',
        '',
        '| Metric | Count |',
        '| --- | ---: |',
        f'| CryCommon public header files scanned | {len(result.headers)} |',
        f'| Aggregated declaration identifiers | {len(result.symbols)} |',
        f'| Declaration sites | {declaration_site_count} |',
        f'| Unclassified declaration candidates | {len(result.unresolved_declarations)} |',
        f'| Explicit `CryCommon/...` include directives outside CryCommon | {explicit_include_count} |',
        f'| Resolved unqualified CryCommon include directives outside CryCommon | {unqualified_include_count} |',
        f'| Ambiguous/unresolved CryCommon-like include directives | {result.ambiguous_include_count} |',
        f'| `Legacy::CryCommon` dependency declarations | {len(result.dependency_records)} |',
        f'| CMake files containing those dependencies | {dependency_file_count} |',
        f'| Symbols with best-effort zero external references | {len(zero_use_symbols)} |',
        f'| Symbols flagged low-confidence/ambiguous | {len(ambiguous_symbols)} |',
        '',
        '## Interpretation limits',
        '',
        '- Declarations are found lexically in installed/public CryCommon headers; this is not a Clang AST.',
        '- Nested declaration visibility, overload identity, aliases, conditional compilation, '
        'generated/restricted source, and operator uses can be ambiguous.',
        '- External references are identifier matches with comments, literals, include directives, '
        'locally declared macro names, build output, Cache, and third-party `External` trees removed; '
        'uses in other preprocessor directives and macro replacement bodies are counted.',
        '- A reference is “inferred transitive” only when the source includes a CryCommon header whose '
        'in-tree include closure reaches the declaration header.',
        '- “Unresolved reference” includes PCH, target-propagated, generated, and coincidentally '
        'same-named symbols. Zero matches never authorize deletion by themselves.',
        '- The CSV records exact external include edges, direct and inferred intra-CryCommon include '
        'edges, and one bounded row per symbol/file match; aggregate report numbers can therefore be '
        'audited back to paths.',
        '- Serialized names in assets and runtime-generated source are outside the symbol counter and '
        'require the separate serialization/layout validation plan.',
        '',
        '## Symbol dispositions',
        '',
        '| Disposition | Symbols | Meaning |',
        '| --- | ---: | --- |',
    ]
    meanings = {
        'active_external_use': 'Best-effort external references exist.',
        'ambiguous_external_use': 'References exist, but the identifier cannot be attributed reliably.',
        'ambiguous_zero_use': 'No match was found for an intrinsically ambiguous identifier.',
        'compatibility_retained': 'Foundational source-compatibility declaration; not a deletion candidate.',
        'deferred_systemic': 'Ownership/lifecycle/system migration is deferred.',
        'dependency_forward_declaration': (
            'Dependency name declared by a CryCommon header; matches are not attributed as '
            'CryCommon uses.'
        ),
        'retirement_in_progress': 'Declared in StlUtils/MiniQueue and still externally referenced.',
        'zero_external_use_unresolved': 'No match found; export/install/transitive review still required.',
        'zero_use_retirement_candidate': 'Retirement header declaration with no best-effort external match.',
    }
    for disposition, count in sorted(disposition_counts.items()):
        lines.append(f'| `{disposition}` | {count} | {meanings.get(disposition, "Review required.")} |')

    lines.extend([
        '',
        '## Headers with external include edges',
        '',
        '| Header | Explicit directives/files | Unqualified directives/files | Category |',
        '| --- | ---: | ---: | --- |',
    ])
    for header in result.headers:
        explicit = result.explicit_include_counts[header]
        unqualified = result.unqualified_include_counts[header]
        if not explicit and not unqualified:
            continue
        lines.append(
            f'| `{_markdown_escape(header)}` | {explicit}/{len(result.explicit_include_files[header])} | '
            f'{unqualified}/{len(result.unqualified_include_files[header])} | `{_header_category(header)}` |'
        )

    lines.extend([
        '',
        '## Most referenced declaration identifiers',
        '',
        'Dependency-only forward declarations remain in the CSV but are omitted from this ranking '
        'because unrelated use of the dependency type is not evidence of CryCommon use.',
        '',
        '| Identifier | Kind(s) | Header(s) | Uses/files | Direct/transitive/unresolved files | Disposition |',
        '| --- | --- | --- | ---: | ---: | --- |',
    ])
    displayed_active_count = 0
    for entry in active_symbols:
        if not entry.external_use_count:
            break
        disposition, _, _ = _symbol_disposition(entry)
        if disposition == 'dependency_forward_declaration':
            continue
        lines.append(
            f'| `{_markdown_escape(entry.identifier)}` | '
            f'`{_markdown_escape(";".join(sorted({site.kind for site in entry.sites})))}` | '
            f'`{_markdown_escape(";".join(sorted({site.header for site in entry.sites})))}` | '
            f'{entry.external_use_count}/{len(entry.external_files)} | '
            f'{len(entry.direct_reference_files)}/{len(entry.inferred_transitive_files)}/'
            f'{len(entry.unresolved_reference_files)} | '
            f'`{disposition}` |'
        )
        displayed_active_count += 1
        if displayed_active_count == 100:
            break

    lines.extend([
        '',
        '## CMake dependency ledger',
        '',
        '| Target (best effort) | Location |',
        '| --- | --- |',
    ])
    for dependency in result.dependency_records:
        lines.append(
            f'| `{_markdown_escape(dependency.target)}` | `{_markdown_escape(dependency.path)}:{dependency.line}` |'
        )

    lines.extend([
        '',
        '## Unresolved declaration candidates',
        '',
        'These candidates are deliberately retained in the ledger instead of being silently classified as unused.',
        '',
        '| Location | Candidate |',
        '| --- | --- |',
    ])
    if result.unresolved_declarations:
        for site in result.unresolved_declarations:
            lines.append(f'| `{site.header}:{site.line}` | `{_markdown_escape(site.snippet)}` |')
    else:
        lines.append('| — | No unresolved candidates found by the current heuristic. |')

    lines.extend([
        '',
        'The CSV beside this report is the full per-symbol/header/dependency ledger. Regenerate both artifacts with:',
        '',
        '```console',
        'python3 scripts/commit_validation/generate_crycommon_ledger.py',
        '```',
        '',
        'Verify that checked-in artifacts are current with:',
        '',
        '```console',
        'python3 scripts/commit_validation/generate_crycommon_ledger.py --check',
        '```',
        '',
    ])
    return '\n'.join(lines)
