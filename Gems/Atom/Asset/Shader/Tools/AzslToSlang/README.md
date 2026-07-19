# AzslToSlang

Source-to-source porting of Atom's AZSL shaders to Slang. Each `.azsl`/`.azsli`/`.srgi` gets a
`.slang`/`.slangi` counterpart written **beside** it; the AZSL originals are never modified, so the
two can be diffed side by side during review.

## Running it

```bash
python -m azsl2slang                     # convert the whole engine (skips existing outputs)
python -m azsl2slang --dry-run           # show diffs, write nothing
python -m azsl2slang --force             # regenerate, overwriting existing outputs
python -m azsl2slang --check             # CI: non-zero exit if output differs from what is committed
python -m azsl2slang <path> [<path>...]  # convert a subset (the scan still covers the whole tree)
```

Requires `antlr4-python3-runtime==4.13.2` (`pip install -r requirements.txt`). Java is **not**
needed — the generated lexer is checked in under `generated/`.

Verification:

```bash
python tests/roundtrip_check.py             # lex + re-emit every source; must be byte-identical
python tests/verify_slangc.py --sample 25   # type-check converted output with slangc
```

## How it works

The tool lexes with a vendored copy of AZSLC's `azslLexer.g4`, then rewrites only the tokens it
recognizes. Everything else is copied through byte for byte, which is what keeps the ports diffable.

Two things are worth knowing before changing it:

**It uses the lexer, not the parser.** AZSLC's grammar only has rules for `#pragma` and `#line`
because it consumes post-MCPP input. Raw AZSL is full of `#include`/`#if`/`#define`, so the vendored
grammar adds one `PreprocessorDirective` rule routing whole directive lines to the PREPROCESSOR
channel. The parser is never run: every construct the tool rewrites is recognizable from the token
stream with brace tracking, and lexing raw source means **both arms of an `#if` get converted** —
which preprocessing first would make impossible.

**Pass 1 scans the whole tree before anything is rewritten.** Options are declared in one header and
used from every file that includes it, and `PassSrg::` can only be told apart from `ColorSpaceId::`
by knowing which names are SRGs. Converting a subset still scans everything.

Regenerating the lexer after editing `azslLexer.g4`:

```bash
"$JAVA_HOME/bin/java" -jar <antlr4-4.13.2-complete.jar> -Dlanguage=Python3 -o generated azslLexer.g4
```

## Conversion rules

| AZSL | Slang |
|---|---|
| `ShaderResourceGroup PassSrg : SRG_PerPass { … }` | `[AtomShaderResourceGroup(SrgBindingSlot.Pass)]` + `struct PassSrgLayout { … };` + `ParameterBlock<PassSrgLayout> PassSrg;` |
| `PassSrg::m_x` | `PassSrg.m_x` |
| `option bool o_x = true;` | `[AtomOption(true)] public extern bool o_x();` |
| `option enum class E {A,B} o_x;` | `public enum E { A, B };` + `[AtomOption(E.A)] public extern E o_x();` |
| use site `o_x` | `o_x()` |
| `[[range(1,8)]]` | `[AtomRange(1, 8)]` |
| `rootconstant float4 s_x;` | `struct ShaderRootConstants {…}` + `[[vk::push_constant]] ConstantBuffer<…>` |
| `[[pad_to(16)]]` | explicit `float3 m_pad;` sized by HLSL packing rules |
| `Sampler s { MinFilter = Linear; … };` | `[AtomStaticSampler(…10 args…)] SamplerState s;` |
| `class X : Y` | `struct X : Y` |
| `float4 X::Method(…) { … }` | `extension X { static float4 Method(…) { … } }`, in-class prototype removed |
| `ShaderResourceGroupSemantic … { … };` | removed (the slot rides on the attribute) |
| `#include <F.azsli>` | `#include <F.slangi>` |

The ParameterBlock instance keeps the **original SRG name** because that name is runtime-facing:
`SlangReflectionWalker` names the group after the ParameterBlock variable, which becomes
`ShaderResourceGroupLayout::GetName()` and is what `FindShaderResourceGroupLayout(Name{"ObjectSrg"})`
matches. The struct takes the `Layout` suffix instead.

Left deliberately untouched: raw HLSL types (`float4`, not `Vector4F` — ports mirror their AZSL
original), `::` on enums and namespaces (valid Slang, verified), intrinsics, semantics, `[numthreads]`,
comments, and all preprocessor logic.

## Output tiers

Every file lands in one of four tiers, reported per run and in `--report` JSON:

- **converted** — no human review needed beyond the diff.
- **converted-with-todos** — emitted with `// TODO(slang-port): …` markers in place. Nothing is ever
  silently dropped.
- **needs-manual** — not emitted. `partial ShaderResourceGroup` fragments (Scene/View/Bindless
  composition) land here: Slang has no `partial`, so composition must be restructured into an
  aggregate struct by hand. See `Atom/Features/Srg/SceneSrg.slangi` for the target shape.
- **failed** — the file could not be lexed, or two rules claimed the same characters.

## Known gaps

**The Scene/View partial-SRG hand-port is not reconciled with the bulk port.** This is the single
remaining blocker, and every current slangc failure traces to it:

- `ViewSrg`/`SceneSrg` are undefined in ported shaders (E30015). Their declarations live in the
  `partial ShaderResourceGroup` aggregation files, which are in the needs-manual tier and therefore
  never emitted, so `ViewSrg.m_viewMatrixInverse` has nothing to resolve against.
- The hand-ported `Srg/SceneSrgElements.slangi` declares structs that converted ShaderLib files also
  declare (`ReflectionProbeData`, `DirectionalLight`, `Decal`, …), which Slang reports as an
  ambiguous reference or conflicting declaration (E39999, E30200).

Both disappear once the aggregate Scene/View `.slangi` files are the single declaration site and the
21 needs-manual fragments are composed into them.

Smaller items:

- `UNBOUNDED_SIZE` and the `AZ_TRAIT_*` defines from
  `Atom/RPI/Platform/<OS>/AzslcPlatformHeader.azsli` have no Slang counterpart yet;
  `tests/verify_slangc.py` injects them so it measures the conversion rather than that gap. The
  `real` family is handled — it now lives in `Atom/RPI/Prelude.slang`.
- Root constants are emitted in the shape Gate7 verified, but `SlangReflectionWalker` does not
  reflect them yet, so all five sites carry TODOs.
- Unused includes are kept. Deciding an include is dead needs usage analysis, so the tool is
  conservative and leaves it to review.
