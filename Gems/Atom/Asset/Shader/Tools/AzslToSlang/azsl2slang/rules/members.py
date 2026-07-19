# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Member-level rules: root constants (7), pad_to (8) and inline sampler state (9)."""

from __future__ import annotations

from ..lexing import find_matching_brace
from ..packing import PackingError, padding_member_for
from ..rewrite import Rewriter, Severity

from azslLexer import azslLexer as L  # noqa: N814

# Order of the ten positional arguments of [AtomStaticSampler], from Atom/RPI/ShaderResourceGroup.slang.
_SAMPLER_DEFAULTS = {
    "filterMin": '"Point"',
    "filterMag": '"Point"',
    "filterMip": '"Point"',
    "addressU": '"Wrap"',
    "addressV": '"Wrap"',
    "addressW": '"Wrap"',
    "maxAnisotropy": "0",
    "comparisonFunc": '"Never"',
    "reductionType": '"Filter"',
    "borderColor": '"TransparentBlack"',
}
_SAMPLER_ARGUMENT_ORDER = tuple(_SAMPLER_DEFAULTS)

# AZSL sampler-state property names (both spellings the grammar accepts) to attribute arguments.
_SAMPLER_PROPERTY_TO_ARGUMENT = {
    "minfilter": "filterMin",
    "min_filter": "filterMin",
    "magfilter": "filterMag",
    "mag_filter": "filterMag",
    "mipfilter": "filterMip",
    "mip_filter": "filterMip",
    "addressu": "addressU",
    "address_u": "addressU",
    "addressv": "addressV",
    "address_v": "addressV",
    "addressw": "addressW",
    "address_w": "addressW",
    "maxanisotropy": "maxAnisotropy",
    "max_anisotropy": "maxAnisotropy",
    "comparisonfunc": "comparisonFunc",
    "comparison_func": "comparisonFunc",
    "reductiontype": "reductionType",
    "reduction_type": "reductionType",
    "bordercolor": "borderColor",
    "border_color": "borderColor",
}

_QUOTED_ARGUMENTS = frozenset(
    {"filterMin", "filterMag", "filterMip", "addressU", "addressV", "addressW",
     "comparisonFunc", "reductionType", "borderColor"}
)


def apply(lexed, registry, rewriter: Rewriter) -> None:
    code = lexed.code_tokens
    _convert_root_constants(code, rewriter)
    _convert_samplers(code, rewriter)
    _convert_pad_to(code, rewriter)


def _convert_root_constants(code, rewriter: Rewriter) -> None:
    """Rule 7: gather a run of `rootconstant` declarations into a push-constant buffer.

    The shape follows the Gate7 parity test: a plain struct plus
    `[[vk::push_constant]] ConstantBuffer<T>`. SlangReflectionWalker does not consume root
    constants yet, so each run is also flagged for review.
    """
    runs = _rootconstant_runs(code)
    root_names: set[str] = set()
    declared_indices: set[int] = set()
    for run in runs:
        first_index, last_index, declarations, name_indices = run
        for name_index in name_indices:
            root_names.add(code[name_index].text)
            declared_indices.add(name_index)
        body = "\n".join(f"    {declaration};" for declaration in declarations)
        replacement = (
            "// TODO(slang-port): root constants are not reflected by SlangReflectionWalker yet\n"
            "struct ShaderRootConstants\n"
            "{\n"
            f"{body}\n"
            "};\n"
            "[[vk::push_constant]]\n"
            "ConstantBuffer<ShaderRootConstants> RootConstants;"
        )
        rewriter.replace_span(code[first_index], code[last_index], replacement)
        rewriter.note(
            Severity.TODO,
            f"{len(declarations)} rootconstant declaration(s) folded into a push-constant buffer",
            code[first_index].line,
        )

    _requalify_root_constant_uses(code, root_names, declared_indices, rewriter)


def _requalify_root_constant_uses(code, root_names, declared_indices, rewriter: Rewriter) -> None:
    """Rewrite bare uses of a folded rootconstant to `RootConstants.<name>`.

    The declaration moved into the ShaderRootConstants push-constant buffer, so every other reference
    to the name has to go through the buffer. Skips the declaration itself and any qualified use
    (already `.name` / `::name`)."""
    if not root_names:
        return
    for index, token in enumerate(code):
        if token.type != L.Identifier or token.text not in root_names or index in declared_indices:
            continue
        if index > 0 and code[index - 1].type in (L.Dot, L.ColonColon):
            continue
        rewriter.insert_before(token, "RootConstants.")


def _rootconstant_runs(code) -> list[tuple[int, int, list[str], list[int]]]:
    """Consecutive `rootconstant ...;` declarations, coalesced into one struct each.

    Each run yields (start, end, declaration texts, token indices of the declared names).
    """
    runs: list[tuple[int, int, list[str], list[int]]] = []
    index = 0
    while index < len(code):
        if code[index].type != L.Rootconstant:
            index += 1
            continue

        start = index
        declarations: list[str] = []
        name_indices: list[int] = []
        end = index
        while index < len(code) and code[index].type == L.Rootconstant:
            semi = _next_of_type(code, index, L.Semi)
            if semi < 0:
                break
            declarations.append(
                " ".join(token.text for token in code[index + 1 : semi]).strip()
            )
            name_index = _declared_name_index(code, index + 1, semi)
            if name_index >= 0:
                name_indices.append(name_index)
            end = semi
            index = semi + 1

        if declarations:
            runs.append((start, end, declarations, name_indices))
        else:
            index += 1
    return runs


def _declared_name_index(code, start: int, semi: int) -> int:
    """The declared variable name in `Type name[..];`: the identifier before `[` or `;`."""
    for cursor in range(start, semi):
        if code[cursor].type == L.Identifier and code[cursor + 1].type in (L.LeftBracket, L.Semi):
            return cursor
    return -1


def _convert_samplers(code, rewriter: Rewriter) -> None:
    """Rule 9: `Sampler m_s { MinFilter = Linear; ... };` -> [AtomStaticSampler(...)] SamplerState."""
    index = 0
    while index < len(code):
        token = code[index]
        # AZSL spells the SRG sampler type several ways: `Sampler` (SamplerCapitalS) is the common
        # one in SRG bodies, alongside lowercase legacy `sampler` and the HLSL state types.
        if token.type not in (
            L.Sampler,
            L.SamplerCapitalS,
            L.SamplerState,
            L.SamplerStateCamel,
            L.SamplerComparisonState,
        ):
            index += 1
            continue
        if index + 1 >= len(code) or code[index + 1].type != L.Identifier:
            index += 1
            continue

        brace = index + 2
        if brace >= len(code) or code[brace].type != L.LeftBrace:
            # No inline state block, so there is no static sampler to describe — but AZSL's
            # `Sampler` spelling is not a Slang type, so the declaration still needs renaming.
            if token.type in (L.Sampler, L.SamplerCapitalS, L.SamplerStateCamel):
                rewriter.replace_token(token, "SamplerState")
            index += 1
            continue
        close = find_matching_brace(code, brace)
        if close < 0:
            index += 1
            continue

        properties = _sampler_properties(code, brace, close)
        arguments = [
            _sampler_argument(name, properties.get(name)) for name in _SAMPLER_ARGUMENT_ORDER
        ]
        name = code[index + 1].text

        end = close
        if close + 1 < len(code) and code[close + 1].type == L.Semi:
            end = close + 1

        rewriter.replace_span(
            code[index],
            code[end],
            f"[AtomStaticSampler({', '.join(arguments)})]\n    SamplerState {name};",
        )
        unknown = properties.get("__unknown__")
        if unknown:
            rewriter.note(
                Severity.TODO,
                f"sampler {name}: unmapped state {unknown}",
                code[index].line,
            )
        index = end + 1


def _sampler_properties(code, brace: int, close: int) -> dict[str, str]:
    """Read `Property = Value;` pairs out of an inline sampler body."""
    properties: dict[str, str] = {}
    unknown: list[str] = []
    cursor = brace + 1
    while cursor < close:
        assign = _next_of_type_bounded(code, cursor, L.Assign, close)
        if assign < 0:
            break
        semi = _next_of_type_bounded(code, assign, L.Semi, close)
        if semi < 0:
            semi = close

        property_name = code[assign - 1].text if assign - 1 >= brace else ""
        value = "".join(token.text for token in code[assign + 1 : semi]).strip()
        argument = _SAMPLER_PROPERTY_TO_ARGUMENT.get(property_name.lower())
        if argument is None:
            unknown.append(property_name)
        else:
            properties[argument] = value
        cursor = semi + 1

    if unknown:
        properties["__unknown__"] = ", ".join(unknown)
    return properties


def _sampler_argument(name: str, value: str | None) -> str:
    if value is None:
        return _SAMPLER_DEFAULTS[name]
    if name in _QUOTED_ARGUMENTS:
        return f'"{value}"'
    return value


def _convert_pad_to(code, rewriter: Rewriter) -> None:
    """Rule 8: replace `[[pad_to(N)]]` with an explicit member sized from the enclosing struct."""
    for index, token in enumerate(code):
        if token.type != L.LeftDoubleBracket:
            continue
        if index + 1 >= len(code) or code[index + 1].text != "pad_to":
            continue

        closing = _double_bracket_end(code, index)
        if closing < 0:
            continue

        target = None
        for cursor in range(index, closing):
            if code[cursor].type == L.IntegerLiteral:
                target = int(code[cursor].text, 0)
                break
        if target is None:
            continue

        try:
            declaration = padding_member_for(code, index, target)
        except PackingError as exc:
            rewriter.note(
                Severity.TODO,
                f"[[pad_to({target})]] could not be computed: {exc}",
                token.line,
            )
            rewriter.replace_span(
                code[index],
                code[closing],
                f"// TODO(slang-port): [[pad_to({target})]] - add explicit padding ({exc})",
            )
            continue

        if declaration is None:
            rewriter.replace_span(
                code[index], code[closing], f"// [[pad_to({target})]] - already aligned"
            )
            continue

        rewriter.replace_span(
            code[index], code[closing], f"{declaration} // [[pad_to({target})]]"
        )


def _double_bracket_end(code, start: int) -> int:
    """Index of the second `]` closing a `[[ ... ]]` specifier."""
    seen = 0
    for cursor in range(start + 1, len(code)):
        if code[cursor].type == L.RightBracket:
            seen += 1
            if seen == 2:
                return cursor
        elif code[cursor].type == L.LeftBrace:
            return -1
    return -1


def _next_of_type(code, start: int, token_type: int) -> int:
    for cursor in range(start, len(code)):
        if code[cursor].type == token_type:
            return cursor
    return -1


def _next_of_type_bounded(code, start: int, token_type: int, limit: int) -> int:
    for cursor in range(start, min(limit, len(code))):
        if code[cursor].type == token_type:
            return cursor
    return -1
