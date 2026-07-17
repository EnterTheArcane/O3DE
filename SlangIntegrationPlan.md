# Slang Integration Plan for O3DE / Atom

**Goal:** Add [Slang](https://github.com/shader-slang/slang) as a second shader source language for Atom, running **in parallel** with the existing AZSL/AZSLC pipeline, using the **Slang C++ API in-process** (no `slangc.exe` subprocess), supporting both **SRG-based** and **bindless** shaders, with a path toward eventually replacing AZSLC.

**Slang SDK:** prebuilt release, **pinned to `v2026.13.1`** (latest; `slang.h`, `slang-compiler.lib`/`slang-compiler.dll` — `slang.dll` is a legacy proxy, see D8 — plus optional support DLLs). The investigation below was performed against a `2026.12.0.1` demo drop (`External/Slang`, since removed), which published no macOS archives; the production pin is the latest release with the full windows/linux/macos × x86_64/aarch64 archive matrix. The production integration fetches the prebuilt binaries from the [GitHub release](https://github.com/shader-slang/slang/releases/tag/v2026.13.1) via `o3de_fetch_content` (see D8); the Phase 0A gates re-validate the pinned SDK on every bump.

---

## Part 1 — How the current shader pipeline works (deep-dive findings)

### 1.1 The builders

`AzslShaderBuilderSystemComponent` (`Gems/Atom/Asset/Shader/Code/Source/Editor/AzslShaderBuilderSystemComponent.cpp:87`) registers four Asset Processor builders:

| Builder | Source pattern | Output |
|---|---|---|
| `ShaderAssetBuilder` | `*.shader` | `.azshader` (`RPI::ShaderAsset`) + root `.azshadervariant` + intermediate artifacts |
| `ShaderVariantListBuilder` | `*.shadervariantlist` | hashed variant list intermediates |
| `ShaderVariantAssetBuilder` | `*.hashedvariantlist` / `*.hashedvariantinfo` | `.azshadervarianttree` + non-root `.azshadervariant` |
| `PrecompiledShaderBuilder` | `*.precompiledshader` | wraps prebuilt bytecode |

A `.shader` file is JSON (`RPI::ShaderSourceData`, `Gems/Atom/RPI/Code/Include/Atom/RPI.Edit/Shader/ShaderSourceData.h`): points at one `Source` AZSL file and declares entry points + stages, draw list, render states, shader-option defaults, supervariants (each = extra preprocessor definitions / build-arg deltas), and disabled RHI backends.

### 1.2 ShaderAssetBuilder::ProcessJob — the core flow

From `ShaderAssetBuilder.cpp:327` (the "big picture" comment at line 395 is authoritative). For **each enabled RHI backend** (`ShaderPlatformInterface`, discovered via `ShaderPlatformInterfaceRequestBus`) and **each supervariant**:

```
.azsl source
  │  1. Prepend per-API header (RHI::PrependFile)
  │     e.g. Gems/Atom/Asset/Shader/Code/AZSL/Platform/Windows/Vulkan/AzslcHeader.azsli
  ▼
  2. C-preprocess with MCPP  ──────────────  in-process library call (mcpp_lib_main),
  │     + include paths + supervariant/.shader -D definitions      serialized by a global mutex
  ▼     (Preprocessor.cpp:82)
 .azslin  (flat AZSL, one per supervariant per API)
  │  3. AZSLC transpile: AzslCompiler::EmitFullData (AzslCompiler.cpp:57)
  │     subprocess:  Builders/AZSLc/azslc.exe --full --Zpr --W1 --strip-unused-srgs
  │                  --root-const=128 --sc-options [--namespace=dx | --namespace=vk --unique-idx]
  ▼
  ├── .hlsl                 (pure HLSL, SRGs flattened to register(bN/tN/uN/sN, spaceN))
  ├── .ia.json              (input assembly: vertex entry inputs, semantics)
  ├── .om.json              (output merger: fragment outputs / render target count)
  ├── .srg.json             (SRG reflection: resources, constants, registers, spaces, fallback key)
  ├── .options.json         (shader options: names, values, bit ranges, specializationId)
  └── .bindingdep.json      (which functions reference which bindings → per-stage masks)
  │
  │  4. ShaderBuilderUtility::PopulateAzslDataFromJsonFiles (ShaderBuilderUtility.cpp:134)
  │     → SrgLayoutUtility::LoadShaderResourceGroupLayouts (SrgLayoutUtility.cpp:92)
  │        builds RHI::ShaderResourceGroupLayout list (names, types, counts, registerId, spaceId,
  │        static samplers, SRG constants, unbounded arrays, variant-key fallback)
  │     → AzslCompiler::ParseOptionsPopulateOptionGroupLayout (AzslCompiler.cpp:903)
  │        builds RPI::ShaderOptionGroupLayout (+ "specializationConstants" flag)
  │     → binding dependencies + root constants
  ▼
  5. ShaderBuilderUtility::BuildPipelineLayoutDescriptorForApi (ShaderBuilderUtility.cpp:546)
     - prunes non-entry functions, converts binding deps → per-resource ShaderStageMask
     - RHI::PipelineLayoutDescriptor + per-API extras via
       ShaderPlatformInterface::BuildPipelineLayoutDescriptor (DX12 builds root signature data here)
  6. ShaderBuilderUtility::CreateShaderInputAndOutputContracts (ShaderBuilderUtility.cpp:809)
     - RPI::ShaderInputContract / ShaderOutputContract from .ia/.om JSON
  ▼
  7. ShaderVariantAssetBuilder::CreateShaderVariantAsset (ShaderVariantAssetBuilder.cpp:926)
     for the ROOT variant: for each entry point →
       ShaderPlatformInterface::CompilePlatformInternal(hlslPath, entry, stage, buildArgs, useSC)
       (subprocess DXC → DXIL or SPIR-V; Metal continues → spirv-cross → xcrun metal/metallib)
       → StageDescriptor{byteCode} → CreateShaderStageFunction → RHI::ShaderStageFunction
  ▼
.azshader (ShaderAsset: per-API, per-supervariant: SRG layouts, pipeline layout, contracts,
           option layout, render states, ref to root .azshadervariant)
```

Key detail: the `.hlsl` and the reflection JSONs are emitted as **job products** (sub-ID scheme `RPI::ShaderAsset::MakeProductAssetSubId(apiUniqueIndex, supervariantIndex, subProduct)`), and `ShaderVariantAssetBuilder` later **fetches those cached products** (`ObtainBuildArtifactPathFromShaderAssetBuilder`) to compile non-root variants without re-running MCPP/AZSLC.

### 1.3 Per-RHI backends (`RHI::ShaderPlatformInterface`)

Interface: `Gems/Atom/RHI/Code/Include/Atom/RHI.Edit/ShaderPlatformInterface.h`. Implementations in each RHI gem under `Code/Source/RHI.Builders/ShaderPlatformInterface.cpp`:

- **DX12**: DXC → DXIL (`-T vs_6_x/ps_6_x/cs_6_x/lib_6_3`). Builds root-signature-shaped `PipelineLayoutDescriptor`. When specialization constants are on, runs `dxsc.exe` to **patch DXIL**: option constants are compiled with a sentinel value, `dxsc` records byte offsets into an `offsets.json`, stored as `StageDescriptor::m_extraData`, and the runtime patches bytecode at PSO creation (DX12 `ShaderPlatformInterface.cpp:336`).
- **Vulkan**: DXC `-spirv -fvk-use-dx-layout`, per-stage flags `-fvk-invert-y` (VS/GS), `-fvk-use-dx-position-w` (PS). Options→real SPIR-V specialization constants. AZSLC `--unique-idx` gives unique binding indices across resource types per set.
- **Metal**: DXC → SPIR-V → `spirv-cross --msl --msl-argument-buffers ...` → `xcrun metal`/`metallib` (macOS-hosted builds).
- **Null**: no-op.

Each backend also supplies the **prepend header** (`GetAzslHeader`) that defines per-platform macros (e.g. `UNBOUNDED_SIZE`, `AZ_TRAIT_CONSTANT_BUFFER_ALIGNMENT`) before MCPP runs.

Build arguments come from a scoped stack (`ShaderBuildArgumentsManager`) merging `Config/Shader/shader_build_options.settings` files: global → platform (Windows/Android/...) → RHI API (dx12/vulkan) → `.shader` → supervariant. Argument groups: `preprocessor`, `azslc`, `dxc`, `spirv-cross`, `metal-air`, `metal-lib` (`Atom/RHI.Edit/ShaderBuildArguments.h`).

### 1.4 Shader options, variants, supervariants

- **Options** are declared in AZSL (`option bool x;`). AZSLC reflects them into `.options.json` with a packed bit layout (`keyOffset`/`keySize` in a 128-bit `ShaderVariantKey`), and generates HLSL where each option is either:
  - a macro-overridable constant: variant compiles prepend `#define <name>_OPTION_DEF <value>` (`ShaderVariantAssetBuilder.cpp:984`);
  - read at runtime from the **shader variant key fallback** — a designated SRG constant blob (dynamic branches); or
  - with `--sc-options`: a **specialization constant** (`specializationId` per option). Vulkan/Metal use native spec constants; DX12 uses the `dxsc` bytecode-patching scheme above. Fully-specialized shaders skip building variant trees entirely (`ShaderVariantAssetBuilder.cpp:546`).
- **Variants** (`.shadervariantlist`) = baked combinations of option values → separate bytecode, selected at runtime via `ShaderVariantTreeAsset` search with fallback-key dynamic branching for misses.
- **Supervariants** = preprocessor-level forks of the same source (different `-D` sets / build args) compiled as separate HLSL+reflection inside the same `.azshader`. The `ShaderOptionGroupLayout` must hash-match across supervariants.

### 1.5 Material pipeline code generation

`MaterialTypeBuilder` (`Gems/Atom/RPI/Code/Source/RPI.Builders/Material/MaterialTypeBuilder.cpp:664`) **generates intermediate `.azsl` + `.shader` sources** by pure C-preprocessor stitching: it writes a small file that `#define`s `MATERIAL_TYPE_AZSLI_FILE_PATH`, `MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS` (SRG member injection!), `MATERIAL_PARAMETERS_AZSLI_FILE_PATH`, etc., then `#include`s the material pipeline's template `.azsli`. It also generates a material-parameter struct `.azsli` (`WriteMaterialParameterStructureAzsli`). These intermediates then flow through the normal `ShaderAssetBuilder`. **Implication:** any Slang path must tolerate heavy C-preprocessor use and macro-injected SRG members (Slang's preprocessor is C-compatible, so this pattern ports).

### 1.6 Bindless today

`Gems/Atom/Feature/Common/Assets/ShaderLib/Atom/Features/Bindless.azsli`: a `partial ShaderResourceGroup Bindless : SRG_Bindless` holding unbounded arrays (`Texture2D m_Texture2D[UNBOUNDED_SIZE]`, etc.) with accessor functions. Unbounded arrays flow through reflection as `ShaderInputImageUnboundedArrayDescriptor`/`ShaderInputBufferUnboundedArrayDescriptor` (`SrgLayoutUtility.cpp:134-187`). So "bindless" in Atom is *itself SRG-based* — one SRG of unbounded arrays — plus RHI-side indices.

### 1.7 The critical contract

**The runtime never sees AZSL or HLSL.** Everything the runtime consumes is serialized asset data:

1. `ShaderResourceGroupLayout` list (names→register/space, types, counts, static samplers, fallback key),
2. `PipelineLayoutDescriptor` (+ per-API payload, e.g. DX12 root signature layout),
3. `ShaderInputContract`/`ShaderOutputContract`,
4. `ShaderOptionGroupLayout` (+ specialization IDs),
5. Root constants layout,
6. `RHI::ShaderStageFunction` per stage (bytecode + per-API extras like DX12 SC offsets).

**Any compiler that produces these six data sets with correct values is a drop-in replacement.** This is the whole basis of the parallel-integration strategy: integrate at the *builder* level; zero runtime changes required — even specialization constants reuse the existing asset plumbing (`specializationId` in the option layout, DX12 offset tables in `m_extraData`).

---

## Part 2 — What Slang offers for each pipeline stage

Verified against `External/Slang/include/slang.h` (v2026.12.0.1):

| Current stage | Slang replacement | API |
|---|---|---|
| MCPP preprocess + per-API prepend header | Slang's built-in C-compatible preprocessor; per-API macros via session | `SessionDesc::preprocessorMacros` (`slang.h:4275`), `ISession::loadModuleFromSourceString` (`slang.h:4498`) |
| AZSLC frontend (parse, SRG flattening, register allocation) | Slang frontend; `ParameterBlock<T>` ≙ SRG (one descriptor set / register space per block); automatic or explicit binding layout | `IModule`, `ParameterBlock`, layout rules per target |
| AZSLC reflection JSONs | In-process reflection object model — no JSON round-trip | `IComponentType::getLayout` (`slang.h:5227`) → `ProgramLayout`/`VariableLayoutReflection` (binding index/space, categories incl. `SLANG_PARAMETER_CATEGORY_SPECIALIZATION_CONSTANT`, semantics for IA/OM, user-defined attributes) |
| DXC subprocess (DXIL) | Slang DXIL backend — an embedded DXC runs **in-process** as a "downstream compiler" (verified: DXIL emits with no external `dxcompiler.dll`; Appendix C) | `SLANG_DXIL` (`slang.h:626`), `DownstreamArgs` option (`slang.h:1039`) |
| DXC subprocess (SPIR-V) | **Direct SPIR-V emission** (no DXC at all) | `SLANG_SPIRV` (`slang.h:622`); `VulkanInvertY`/`VulkanUseDxPositionW`/`VulkanBindShift` options (`slang.h:1018-1022`) replace `-fvk-*` flags |
| spirv-cross → MSL → metallib | Direct MSL / Metal lib emission (or keep SPIR-V→spirv-cross as fallback) | `SLANG_METAL`, `SLANG_METAL_LIB` (`slang.h:642-644`) |
| `#define X_OPTION_DEF` variant recompiles from HLSL text | **Link-time constants / link-time specialization** against a *serialized module* — no re-parse | `IModule::serialize`/`writeToFile` (`slang.h:5477`), `ISession::loadModuleFromIRBlob` (`slang.h:4481`), compose + `link()` + `getEntryPointCode` (`slang.h:5255`) |
| `--sc-options` spec constants | Native `[SpecializationConstant]` / `[vk::constant_id]` attributes | reflection category `SpecializationConstant` (`slang.h:2601`) |
| Include-dependency regex scan (`IncludedFilesParser`) | Exact dependency list from the compiler | `IModule::getDependencyFileCount/getDependencyFilePath` (`slang.h:5503`); `ISlangFileSystem` hook (`slang.h:1526`) for full control |
| `.shader` entry point list | Validate/bind entry points by name even without `[shader(...)]` | `IModule::findAndCheckEntryPoint` (`slang.h:5493`) |
| Row-major `--Zpr`/`-Zpr`, debug info, opt levels | Session compiler options | `CompilerOptionName::MatrixLayoutRow`, `DebugInformation`, `Optimization` (`slang.h:964+`) |
| SRG semantics metadata (binding slot, fallback key, static samplers) | **User-defined attributes**, reflectable | `getUserAttributeCount/getUserAttributeByIndex` (`slang.h:2556`) |

Extras we get "for free": generics/interfaces (replaces macro-based material shader customization long-term), modules with separate compilation (faster builds than re-preprocessing the world per supervariant), autodiff, WGSL target (future Emscripten/WebGPU RHI), language server for IDE support.

---

## Part 3 — Design

**Guiding principles for this effort**

- **Language-agnostic seams.** Nothing outside a language backend may know a specific shader language exists. Atom already carries too many AZSL-shaped expectations (`GetAzslHeader`, the `azslc` argument group, `CompilePlatformInternal`'s "input is HLSL text" assumption); this effort must not add more. Whenever existing code has to change, hoist the AZSL-specific concept into a general one that serves AZSL, Slang, *and a hypothetical future language* — adding one should mean "implement the backend interface, register a source extension" and nothing else.
- **Naming:** "frontend"/"Frontend" is one word. New identifiers spell out `ShaderResourceGroup` — no new `Srg` abbreviations (existing names like `SrgData` stay until organically touched).
- **API style:** `AZStd::string_view` over `const char*` in new interfaces.
- **New Slang shader code** uses the global prelude's type aliases (D4).

### D1. Integration point: language backends inside the existing builders

Keep **one** `ShaderAssetBuilder` and **one** `.shader` format. The `Source` file extension selects a backend from a registry:

- `"Source": "Foo.azsl"` → `AzslcBackend`: existing MCPP → AZSLC → per-RHI DXC path (unchanged).
- `"Source": "Foo.slang"` → `SlangBackend`: new, in-process `slang-compiler.dll`.

No new asset types: the Slang path fills the **same** `ShaderAssetCreator` / `ShaderVariantAssetCreator` inputs (the six data sets from §1.7). `.slang`/`.slangi` never need their own builder — like `.azsl`, they are source dependencies of a `.shader`.

File locations follow the `.azsl` convention exactly — there is no new central "Slang folder". Shader sources sit beside their `.shader` descriptor (`Gems/<Gem>/Assets/Shaders/...`, project asset folders), and shared modules go in the existing `ShaderLib` include trees (`Gems/Atom/RPI/Assets/ShaderLib/Atom/RPI/...`, `Gems/Atom/Feature/Common/Assets/ShaderLib/Atom/Features/...`). Slang `import` resolves module paths (dots → directory separators) against the same ShaderLib search roots the preprocessor uses for `#include` today.

Why not a separate builder: the supervariant loop, build-args stack, job-product sub-ID scheme, render-state handling and asset creation (~600 lines of `ShaderAssetBuilder.cpp`) are language-independent and must not fork. Refactor step: extract the language-specific middle ("source → per-entry bytecode + reflection") behind an internal interface:

```cpp
// Gems/Atom/Asset/Shader/Code/Source/Editor/ShaderCompilerBackend.h (new)
class IShaderCompilerBackend
{
public:
    virtual ~IShaderCompilerBackend() = default;

    virtual AZStd::string_view GetName() const = 0;                                  // "azslc", "slang", ...
    virtual AZStd::span<const AZStd::string_view> GetSourceExtensions() const = 0;   // {".azsl"}, {".slang"}
    // Capability negotiation lives on the language side (never on the RHI): can this
    // backend produce the RHI's declared target? Gives Null/WGSL/not-yet-supported
    // combinations a clean skip/error path.
    virtual bool CanCompileTarget(const RHI::ShaderTargetDescriptor&) const = 0;
    // Runs the frontend once per (API, supervariant): preprocess/parse/reflect.
    virtual Outcome<FrontendResult> CompileFrontend(const FrontendInput&) = 0;
    // Compiles one entry point of one variant to target bytecode.
    virtual Outcome<StageResult> CompileStage(const StageInput&) = 0;
};
// Each language ships its own system component (AzslcBackendSystemComponent, later
// SlangBackendSystemComponent) that broadcasts its backend on ShaderCompilerBackendBus during
// activation (mirroring how RHI backends register via ShaderPlatformInterfaceRegisterBus) —
// a future language ships as a new component, with no edits to the builders or the RHI.
// The handling ShaderBuilderSystemComponent rejects duplicate extension claims
// deterministically (hard error, not last-writer-wins).
```

`FrontendResult` carries the canonical `ShaderReflectionData` (D5) — SRG reflection, option layout, binding info + stage masks, root constants, IA/OM contracts — plus backend-specific intermediate products to cache (AZSLC: `.hlsl`+JSONs; Slang: the serialized module closure, see D7). Conversion from `ShaderReflectionData` to the final RHI/RPI objects happens once, in shared builder code, after the backend returns.

### D2. Per-RHI plumbing: generic target descriptors — no language-specific virtuals

The RHI layer must stay shader-language-agnostic: a `SupportsSlang()` virtual on `RHI::ShaderPlatformInterface` would bake a language name into the RHI interface, repeating the AZSL-shaped coupling this effort is trying to shed. Instead, invert the relationship — **RHI backends describe *what* they need; language backends decide *how* to produce it.**

- **`RHI::ShaderTargetDescriptor`** (new, language-neutral data), split by mutability:
  - **Immutable target facts** come from one new *language-neutral* virtual, `ShaderPlatformInterface::GetShaderTargetDescriptor(platform)`: target IR (`DXIL`, `SPIR-V`, `MSL`/`metallib`, `WGSL`), a **stage → profile map** (a single profile string is wrong even for today's DX12: raster/compute use SM 6.2 while ray tracing uses `lib_6_3` — see the `stageToProfileName` tables in both DX12 and Vulkan `ShaderPlatformInterface.cpp`), and hard conventions (invert Y, DX-style position W, DX memory layout, unique binding indices per set, root-constant capacity, constant-buffer alignment). These are facts about what the RHI consumes — putting them in user-editable settings would permit nonsense like "Metal produces DXIL". A language-neutral virtual is consistent with the no-language-names rule; it's `SupportsSlang()` that was the red flag. The base implementation returns "no target declared" so RHIs that haven't opted in (Metal until Phase 3, out-of-tree RHIs) build unchanged; whether a given language backend can produce a declared target is the *backend's* answer, via `IShaderCompilerBackend::CanCompileTarget` (D1).
  - **Tunables** stay in the data-driven `shader_build_options.settings` scope stack (global → platform → API → `.shader` → supervariant), including per-language argument groups (`"slang"` alongside `"azslc"`/`"dxc"`). One plumbing fact forces the shape here (verified): every scope in the stack — `ShaderBuildOptions`, `ShaderBuildArgumentsManager`, and the `.shader`/supervariant overrides in `ShaderSourceData` — carries the concrete `RHI::ShaderBuildArguments` type, so a group parsed only by the builder gem would silently not participate in `.shader`/supervariant add/remove semantics. Therefore `ShaderBuildArguments` gains a **generic named argument-group map** in Phase 1A (`"slang" → [args]`, opaque to the RHI, which stores and merges but never interprets it); the hardcoded `m_azslcArguments`/`m_dxcArguments` members migrate into the map over time for the same reason.
  - Before finalizing the descriptor's field list, audit every existing per-platform trait/flag (Android and Metal carry behavior like descriptor-space limits, 16-bit types, root-constant padding, subpass-input offsets, framebuffer fetch) so the descriptor is complete enough for mobile from day one.
- The `SlangBackend` maps the descriptor onto Slang session options (`SLANG_SPIRV` + `VulkanInvertY` + `VulkanUseDxPositionW` + `MatrixLayoutRow`, ...). A future backend maps the same descriptor onto its own knobs. Over time the `AzslcBackend` migrates onto the same descriptor — hoisting today's hardcoded `--namespace=dx/vk`, `-fvk-invert-y`, `-fvk-use-dx-position-w` flags into derived data instead of duplicated argument lists.
- **Stays on `ShaderPlatformInterface`** (already language-agnostic): `BuildPipelineLayoutDescriptor` (DX12 root signature), `CreateShaderStageFunction` (bytecode wrapping, DX12 SC offsets via `m_extraData`), `Is*Stage*` queries, API names/indices. This is what keeps DX12/Vulkan/Metal runtime behavior identical across languages.
- **Hoisted out of it over time:** `CompilePlatformInternal` today fuses two jobs — "HLSL text → bytecode via DXC subprocess" (language-pipeline work) and RHI post-processing (DX12's `dxsc` patch, Metal's spirv-cross → metallib chain). Split them: producing target IR becomes the language backend's job; post-processing becomes a language-neutral RHI step shared by all pipelines — with real context, not just bytes: `Outcome<StageResult> PostProcessStage(const StagePostProcessInput&, StageResult&&)`, where the input carries platform/target, entry point, stage, temp path, applicable build arguments, specialization metadata, and requested diagnostics (everything `CompilePlatformInternal` receives today — `dxsc` patching and the Metal chain need most of it). Likewise `GetAzslHeader` generalizes to per-(language, API) prelude files resolved from configuration — no virtual with a language name in it.

**Skip vs. fail is explicit.** "Intentionally disabled", "not migrated", and "unsupported" must never be conflated:

1. An RHI disabled in the `.shader` (`m_disabledRhiBackends`) is skipped normally, exactly as today.
2. An AZSL shader on an RHI with no generalized target descriptor continues through the existing legacy compile path — "not migrated" is not an error.
3. A Slang shader on an *enabled* RHI whose declared target no registered backend `CanCompileTarget()` accepts **fails the job with a clear diagnostic** — silently skipping would hide missing platform support behind a green build.
4. A Slang shader compiles only through a backend whose `CanCompileTarget()` accepted the descriptor.

Only the shader builder gem (`Atom_Asset_Shader.Builders`) links the Slang library. RHI `*.Builders` never do — under this split they only describe targets and post-process bytecode, so they need no compiler at all.

Phase 1A does this minimally: `GetShaderTargetDescriptor()` for DX12/Vulkan/Null plus settings-sourced tunables; AZSL behavior and products are preserved exactly, converging on the new seams later.

### D3. SRGs in Slang: `ParameterBlock<T>` + user attributes + a core module

The Atom SRG concept maps naturally onto Slang's `ParameterBlock` (which was *designed* for the descriptor-set-per-frequency pattern). Authoring idiom, provided by a new first-party module at `Gems/Atom/RPI/Assets/ShaderLib/Atom/RPI/ShaderResourceGroup.slang` (`import Atom.RPI.ShaderResourceGroup;`) — same ShaderLib layout as `Atom/RPI/Math.azsli` and `Atom/RPI/ShaderResourceGroups/`:

```slang
// AZSL today:
ShaderResourceGroupSemantic SRG_PerMaterial { FrequencyId = 2; };
ShaderResourceGroup MaterialSrg : SRG_PerMaterial
{
    Texture2D m_diffuse;
    Sampler m_sampler { MaxAnisotropy = 16; };
    float4 m_color;
};

// Slang tomorrow (syntax verified against the bundled 2026.12.0.1 compiler — see Appendix C):
[AtomShaderResourceGroup(2 /*PerMaterial binding slot*/)]
struct MaterialShaderResourceGroup
{
    Texture2D m_diffuse;

    [AtomStaticSampler(16 /*maxAnisotropy*/, "Wrap" /*addressMode*/ /*, ...*/)]
    SamplerState m_sampler;

    Vector4F m_color;                            // loose members → SRG-constants constant buffer
};
ParameterBlock<MaterialShaderResourceGroup> MaterialSrg;   // instance name = runtime SRG name;
                                                           // existing content looks up "MaterialSrg"
```

Two syntax facts, established by compiling against the bundled SDK: Slang user-defined attributes take **positional arguments only** (`name: value` named-argument syntax is a parse error), and the static-sampler attribute goes **on the sampler member itself** (an attribute argument can't reference a sibling member). Both verified working, including reflection: `userAttribs` shows up on the member with its argument values intact.

- `[AtomShaderResourceGroup]`, `[AtomStaticSampler]`, `[AtomVariantFallback(128 /*sizeBits*/)]`, `[AtomRootConstants]` are **user-defined attributes** (`[__AttributeUsage]`), read back through Slang reflection — this is how the builder recovers binding slots, static-sampler descriptors, and the variant-key fallback designation that AZSL expressed with dedicated syntax. (Parameter-block *instance* names are runtime-facing — `RPI::ShaderResourceGroup` looks layouts up by name — so ports of existing SRGs keep names like `MaterialSrg` even though new *type and attribute* identifiers spell out `ShaderResourceGroup`.)
- Loose `float4 m_color` members of a `ParameterBlock` automatically land in an implicit constant buffer — exactly AZSL's "SRG constants" (`m_srgConstantData*`). Slang reflection reports the offsets/sizes needed for `ShaderInputConstantDescriptor`.
- **Bindless is a design decision, not a direct port.** `Bindless.azsli` holds five unsized arrays in one SRG (on Windows `UNBOUNDED_SIZE` expands to nothing), and Slang rejects more than one unsized member per struct (`E30070: unsized member must be last` — verified, Appendix C), so the `ParameterBlock` form cannot express it. The verified-working alternative: **global-scope unsized arrays with explicit bindings**, grouped into the logical Bindless SRG via attributes. Probes confirm global unsized arrays compile for both SPIR-V (sequential bindings in one set — exactly today's Vulkan Bindless shape) and DXIL (each array auto-assigned its own register space — exactly today's DX12 unbounded-array semantics), and that explicit `[[vk::binding(n, set)]]` + `register(tN, spaceN)` annotations are honored, which lets the port pin the same numbering the AZSL Bindless SRG produces. `Bindless.slang` lands beside the `.azsli` at `Atom/Features/Bindless.slang`; validating its layout hash against the AZSL-produced layout is Phase 0A gate 1. (Longer term, DX12's `ResourceDescriptorHeap`/Slang `DescriptorHandle<T>` can make bindless access cheaper, but that's an optimization, not required for parity.)
- `partial ShaderResourceGroup` (used for SceneSrg/ViewSrg composition and material-pipeline member injection) has no Slang `partial` equivalent — but composition today is actually done with **preprocessor macros** (`MATERIAL_PIPELINE_OBJECT_SRG_MEMBERS`, `SceneSrgIncludesAll.azsli` accumulation), and Slang's preprocessor handles that pattern unchanged. Struct-`#include`-into-body remains legal in Slang files.

**Binding assignment strategy — two tiers.** "Reflection is the truth" is only sufficient for *shader-private* SRGs (per-material/per-pass SRGs no other shader shares): there, Slang may auto-assign and the builder reads the result back into `ShaderReflectionData` (and from it, the SRG layout and binding info). **Shared SRGs are a binding ABI.** `SceneSrg`, `ViewSrg`, `Bindless`, and shared pass SRGs are instantiated *once* against one layout and bound to pipelines from many shaders; descriptor-set compatibility requires every consuming shader to expect identical bindings, and Atom enforces this — the SRG layout hash includes every input's `m_registerId` (verified: `ShaderResourceGroupLayoutDescriptor.cpp` hashes `m_registerId` in every descriptor's `GetHash`). During incremental migration, Slang- and AZSL-compiled shaders will consume these SRGs interchangeably, so the Slang declarations of shared SRGs must **pin explicit bindings** matching what AZSLC produces (explicit `[[vk::binding]]`/`register(space)` annotations are honored by Slang — verified). Byte-for-byte layout/hash parity for Scene/View/Bindless SRGs is Phase 0A gate 1. The ABI needs an explicit owner, or the pinned Slang declarations silently go stale the first time someone edits an AZSL Scene/View SRG: check in a **language-neutral binding-ABI manifest** (per shared SRG: inputs, types, counts, register/space per API), a maintainer-run generator that emits/updates the pinned `.slangi` declarations from it, and CI that compares *both* compilers' reflection output against the manifest so drift fails the build rather than shipping. During migration AZSL remains the editorial source feeding the manifest; ownership can flip later without changing the mechanism. Per-resource **stage masks** (today from `.bindingdep.json`) come from per-entry-point usage queries (`IMetadata::isParameterLocationUsed`). Be explicit about the contract: conservative "all stages" masks are functionally safe but change stage-visibility inputs to DX12 root-signature construction, so **pipeline-layout parity cannot be claimed with them**. They're bring-up scaffolding only; exact `IMetadata`-derived masks are a Phase 1B requirement for the parity test shaders.

### D4. Global prelude: one module every Slang file sees

All Slang compiles implicitly include a first-party prelude module (`Gems/Atom/RPI/Assets/ShaderLib/Atom/RPI/Prelude.slang`) so authors get a consistent type vocabulary with zero per-file boilerplate. Injection mirrors today's `RHI::PrependFile` mechanism: the builder prepends `import Atom.RPI.Prelude;` (plus a `#line` directive so diagnostics keep original file/line numbers) when loading source into the session, and the module resolves through the standard ShaderLib search paths. The per-API preludes (D2) stack on the same mechanism.

The prelude establishes explicitly sized, spelled-out type aliases, and **all new first-party Slang code (ShaderLib ports, material templates, samples) uses them** instead of raw `float3`/`float4x4`:

```slang
// SCALAR
public typealias i8  = int8_t;    public typealias u8  = uint8_t;
public typealias i16 = int16_t;   public typealias u16 = uint16_t;
public typealias i32 = int32_t;   public typealias u32 = uint32_t;
public typealias i64 = int64_t;   public typealias u64 = uint64_t;

public typealias f16 = float16_t;
public typealias f32 = float32_t;
public typealias f64 = float64_t;

// VECTOR
public typealias Vector<T, let N : int> = vector<T, N>;

public typealias Vector2<T> = Vector<T, 2>;   // + Vector3<T>, Vector4<T>
public typealias Vector2I = Vector2<i32>;     // and per width: I (i32), U (u32),
public typealias Vector2U = Vector2<u32>;     //                F (f32), D (f64)
public typealias Vector2F = Vector2<f32>;     // e.g. Vector3F, Vector4U, ...
public typealias Vector2D = Vector2<f64>;

// MATRIX
public typealias Matrix<T, let R : int, let C : int> = matrix<T, R, C>;
public typealias Matrix2x2<T> = Matrix<T, 2, 2>;
// ... Matrix2x3 through Matrix4x4, all R/C combinations of 2, 3, 4
public typealias Matrix4x4<T> = Matrix<T, 4, 4>;
```

### D5. Reflection: consume Slang metadata directly

No JSON intermediary, and one canonical contract **in memory as well as on disk**: `ShaderReflectionData` (below) is what both backends produce — the `SlangBackend` populates it by walking `ProgramLayout`/`VariableLayoutReflection` in-process; the `AzslcBackend` populates it from its JSONs (making `PopulateAzslDataFromJsonFiles` an AZSLC-internal adapter). Shared converters then produce the final RHI/RPI objects — SRG layouts (via the logic in `SrgLayoutUtility`), pipeline-layout binding info, IA/OM contracts, root constants, `ShaderOptionGroupLayout`. The legacy `SrgData`/`AzslData` shapes must not survive as the de facto interface between backends and builders. (An AZSLC-JSON-emitting adapter for the Slang side was considered for de-risking and rejected: it would freeze AZSLC's serialization quirks — the very expectations we're shedding — into the new path; the five-JSON detour only exists because AZSLC is an out-of-process transpiler.)

Two consequences:

- **Persistence between builders:** `ShaderVariantAssetBuilder` re-reads reflection today by re-parsing the cached JSON products. The Slang path instead emits one **AZ-serialized reflection product** — `ShaderReflectionData`, a **new, versioned, builder-owned DTO** with `AZ_TYPE_INFO`/`Reflect`, stable primitive fields, an embedded schema version, and explicit converters to the final RHI/RPI objects. It must *not* be the legacy `SrgData`/`AzslData` shapes: those are internal AZSLC parsing structures with no serialization reflection, and `AzslData` is already marked `DEPRECATED [ATOM-15472]` (`AzslData.h:84`). The DTO gets its own new `RPI::ShaderAssetSubId` entries rather than squatting on language-shaped IDs like `SrgJson`/`GeneratedHlslSource`. This is itself a hoisting win (guiding principles): once `ShaderReflectionData` exists, `PopulateAzslDataFromJsonFiles` becomes an `AzslcBackend` implementation detail that *produces* the same structure.
- **Debuggability/parity:** keep a debug-only text/JSON dump of `ShaderReflectionData` behind a build flag, and validate parity in tests by comparing layout hashes and field-level structures between an AZSL shader and its Slang port (Phase 1B.5) — not by diffing AZSLC's JSON shape.

### D6. Shader options and variants in Slang

Options are the least direct mapping — AZSL's `option` keyword is bespoke. A hard requirement shapes the whole design: **one authoring form must lower to all three modes.** AZSLC gives an `option` three lives — baked constant (variants), specialization constant (`--sc-options`), or dynamic read from the variant-key fallback — and shader code referencing the option is identical in each. A plain `extern static const` covers only the baked/link-time case; a `[SpecializationConstant]` declaration covers only that mode; neither can *become* a buffer read for the dynamic fallback. So the shape is fixed even though the syntax isn't: an option *declaration* the builder discovers via reflection, a single use-site expression, and a builder-synthesized per-mode implementation behind it. Proving one boolean and one enum option in **all three lowerings** (including DX12's `dxsc` patching) is Phase 0A gate 2 — it is the highest-uncertainty piece of the language mapping. Per-mode model:

- **Authoring form — a candidate, not yet the design.** The naive `[AtomOption] extern static const T o_thing;` (Slang link-time constants) covers the baked mode cleanly but **cannot** satisfy single-form on its own: a `static const` can be given a link-time value or a specialization-constant lowering, but it can never *become* a runtime buffer read — the dynamic mode would require rewriting every use site. The leading candidate is therefore an **accessor/wrapper layer**: use sites reference one expression (`o_thing` via a builder-generated property/accessor, or `Options.thing`), and its *implementation* — generated by the builder into the options module — returns a link-time constant, a `[SpecializationConstant]`, or a fallback-buffer load depending on mode. Whether the wrapper can keep AZSL-like bare-identifier ergonomics (global `property` support, link-time-specialized implementation types) is exactly what Phase 0A gate 2 exists to answer; the gate output is the chosen representation.
- **Baked variants:** `ShaderVariantAssetBuilder` links the cached module closure against a tiny generated module providing the option values, then `getEntryPointCode`. No text prepending; avoids re-parsing and is expected to improve large variant-list builds — benchmark per D7 before claiming numbers.
- **Specialization-constant mode** (default today via `--sc-options`): the same authored declaration lowers to a native spec constant in the generated implementation (IDs assigned by the builder); reflection reports them (`SLANG_PARAMETER_CATEGORY_SPECIALIZATION_CONSTANT`). DX12 keeps the existing `dxsc.exe` sentinel-patch trick: bake the sentinel into the spec-constant default, then run the unchanged `dxsc` patch + `offsets.json` + `m_extraData` flow.
- **Dynamic options (root variant / variant misses):** AZSLC generates "read the option from the variant-key fallback SRG constant" code. In Slang, generate the equivalent access function in the builder-generated options module (reading the `[AtomVariantFallback]` blob with the same bit packing as `ShaderOptionGroupLayout`). Bit layout stays identical → `ShaderVariantKey`, variant trees, and the RPI runtime remain untouched.
- The builder assembles `ShaderOptionGroupLayout` itself (it already owns bit packing rules from `keyOffset`/`keySize` parsing — we now *produce* those instead of parsing them).

Supervariants: unchanged concept — each supervariant is a distinct Slang session (different `preprocessorMacros`, possibly different modules), same loop structure as today.

### D7. Variant builds without re-running the frontend

**Verified constraint (Appendix C): a serialized module does not contain its imports.** Serializing the shader's root module and reloading it in a fresh process fails with `cannot open file '<import>.slang'` — a ShaderLib-heavy shader's cached module is useless without its transitive closure. So the cached product is a **module-closure bundle**: every module loaded in the session (enumerated via `ISession::getLoadedModule`, `slang.h:4488`), plus a manifest of module names/virtual paths and a fingerprint (the *loaded* compiler's identity via `IGlobalSession::getBuildTagString()` (`slang.h:4004`) — not just the compile-time `SLANG_TAG_VERSION` header constant, which describes the headers we built against rather than the DLL actually running — plus effective compiler options, target descriptor, and reflection schema version). Reject-and-recompile on any mismatch — no optimistic loads.

`ShaderAssetBuilder`'s Slang path emits, per (API, supervariant):
1. the module-closure bundle (new sub-ID) — replaces the cached `.hlsl` product,
2. the AZ-serialized `ShaderReflectionData` product (D5, new sub-ID) — `ObtainBuildArtifactPathFromShaderAssetBuilder` fetches both by sub-ID exactly as it fetches the JSONs today.

`ShaderVariantAssetBuilder`'s Slang path: load closure via `loadModuleFromIRBlob` → compose with the generated option-values module → `link` → per-entry `getEntryPointCode` → existing `CreateShaderStageFunction`.

Treat this as a **build-speed optimization with a mandatory fallback**, not a correctness dependency: precompiled modules and link-time specialization sit on the experimental end of Slang's feature-maturity spectrum, so the variant builder must also be able to recompile from source (it has the source dependencies anyway). Benchmark cold/warm variant builds against the AZSLC path before advertising build-time wins.

### D8. In-process embedding (the "no subprocess" requirement)

- Link **`slang-compiler.lib`** and deploy **`slang-compiler.dll`** — in the 2026.12 SDK, `slang.dll` is a 0.15 MB compatibility proxy over the 33 MB `slang-compiler.dll` (verified in the bundled bin/) and is on its way out; new integrations should not depend on the proxy. Precedent for in-process compilers: MCPP is already an in-process library in the same builder gem (with a global mutex — `Preprocessor.cpp`).
- **One `IGlobalSession` per builder process** (expensive: loads the core module), created lazily, destroyed at shutdown (`slang_shutdown`). One `ISession` per job/supervariant/API (cheap). AssetBuilder worker processes are separate OS processes, so global-session sharing across jobs is per-process only — exactly what we want.
- **Thread safety:** treat `IGlobalSession::createSession` and session use as externally synchronized until verified against this Slang version; guard with a mutex like MCPP does. AP parallelism is process-based, so this costs little.
- **DXIL specifics:** Slang drives DXC as an in-process *downstream* compiler (not a subprocess). Probes against the bundled SDK show DXIL emission works with **no** `dxcompiler.dll` in the bin directory — a DXC ("dxc 1.10") and its validator appear to be embedded in `slang-compiler.dll` in this release. Phase 0 must confirm the emitted DXIL is validated/signed for non-dev-mode D3D12; if a specific external DXC version is ever required, `IGlobalSession::setDownstreamCompilerPath` points Slang at DLLs we deploy. SPIR-V needs nothing (direct emission — verified).
- **Crash blast radius:** a compiler crash now takes down an AssetBuilder worker instead of a child `azslc.exe`. AP already survives builder-process crashes (job fails, worker respawns); acceptable, but log the active shader path around Slang calls to keep triage easy.
- CMake: `cmake/3rdParty/FindSlang.cmake` (engine-level Find module beside `FindWwise.cmake`; `cmake/3rdParty` is already on `CMAKE_MODULE_PATH`) using `o3de_fetch_content` against the prebuilt per-host archives of the [GitHub release](https://github.com/shader-slang/slang/releases/tag/v2026.13.1) (`URL_HASH`-pinned from the release API's sha256 digests, license metadata, all six windows/linux/macos × x86_64/aarch64 archives selected by host OS + architecture). Declare an imported `3rdParty::Slang` target (SHARED IMPORTED: include dir + `slang-compiler` implib/dll), wired for `RUNTIME_DEPENDENCIES` deployment the same way `3rdParty::DirectXShaderCompilerDxc`/`3rdParty::azslc` are consumed by `Atom_Asset_Shader.Builders` today (`Gems/Atom/Asset/Shader/Code/CMakeLists.txt:108`). **Only the shader builder gem links it** — never RHI builders (D2), never runtime targets. Ship a *trimmed* DLL closure: `slang-compiler.dll` is required; `slang-llvm.dll` (105 MB) is CPU-target-only and excluded; verify whether `slang-glslang.dll` (11 MB) is needed for SPIR-V legalization or only for GLSL input, and exclude if unused. If offline builds/internal mirrors become a requirement later, the fetch can be promoted to a full `3p-package-source` package without touching consumers — they only see `3rdParty::Slang`.

### D9. Dependencies, includes, and CreateJobs

- **Builder-injected dependencies never appear in a source scan.** The global prelude (D4), the per-API prelude (D2), their transitive imports, and the binding-ABI manifest / generated shared-SRG declarations (D3) are injected or consumed by the builder itself — no `.slang` source mentions them, so no scanner will find them. `CreateJobs` must add them as source dependencies explicitly, or editing `Prelude.slang` would rebuild nothing.
- **`CreateJobs` is the only source-dependency channel.** `ProcessJobResponse` has no source-dependency field (verified — `AssetBuilderSDK.h:767`: products, reprocess triggers, nothing else), so the AP's rebuild graph is only as good as the CreateJobs scan. The scanner must be authoritative and conservative: extend `IncludedFilesParser` to also match Slang `import X;` / `__include` statements, using **one shared module resolver** for both the scanner and the compile-time `ISlangFileSystem` hook so they can never disagree. Module-name resolution is not just dots-to-slashes — Slang also maps `_` in module names to `-` in filenames and accepts string-form imports (`import "file-name.slang";`); the shared resolver owns all of those rules. It must also handle **search-root shadowing**: for a module resolved from a lower-priority root, register the nonexistent higher-priority candidate paths as dependencies too (the `#include` scanner already does exactly this via `AppendListOfPossibleFutureLocations`, `ShaderAssetBuilder.cpp:91`), so a same-named module appearing in a higher-priority root later triggers a rebuild. Coarse over-inclusion remains acceptable (it already is, per the code comments).
- `ProcessJob` uses `IModule::getDependencyFilePath` as a **verifier**: compare Slang's actual loaded-file list against what the scanner declared and fail the job loudly on any file the scanner missed — a missed dependency means stale-asset bugs later, and failing early keeps the resolver honest.
- Add `.slang`/`.slangi` to the Editor/AP source-file registry (file browser, "shader source" recognition — `RPI.Edit` file utilities, `AzslShaderBuilderSystemComponent` job dependency wiring), and list all new shader source files in explicit `FILES_CMAKE` asset manifests per repo convention (e.g. `atom_rpi_asset_files.cmake`) — no globs.

### D10. Metal / mobile strategy

Phase the risk: start Metal on **Slang → SPIR-V → existing spirv-cross → metal toolchain** (only the front half changes; `spirv-cross`/`metal-air`/`metal-lib` arg groups and code paths already exist), then evaluate native `SLANG_METAL_LIB` once DX12+Vulkan parity is proven. Same idea applies to any future WGSL/Emscripten target (`SLANG_WGSL`).

---

## Part 4 — Execution plan

Phases are ordered so each lands independently behind the language dispatch (no flag days; AZSL behavior and products preserved throughout).

### Phase 0A — Feasibility gates (≈2 weeks; go/no-go)

Narrow, disposable compiler probes that must pass before committing to the reflection schema or the builder refactor. Several are **already answered** by the probes run for this plan (Appendix C): attribute syntax + reflection (positional args work, `userAttribs` reflect), global-scope unsized arrays for Bindless on SPIR-V + DXIL, explicit binding pinning, and module serialization excluding imports. Remaining gates:

1. **Shared-SRG ABI parity:** declare SceneSrg/ViewSrg/Bindless in Slang with pinned bindings and prove byte-for-byte `ShaderResourceGroupLayout` hash parity against the AZSLC-built layouts on DX12 + Vulkan (D3). For Bindless this includes the **grouping step**: prove the reflection walker synthesizes one logical Bindless SRG (a single `ShaderResourceGroupLayout` with unbounded-array inputs) from the attribute-grouped globals — correct bytecode bindings alone don't demonstrate that.
2. **Options in all three modes:** one boolean + one enum option authored once, lowered to baked (link-time), specialization constant (Vulkan native + DX12 `dxsc` sentinel patching of Slang-produced DXIL), and dynamic fallback-key read (D6). Includes bit-packing/ordering determinism.
3. **DXIL validation/signing:** confirm the embedded DXC validates/signs for non-dev-mode D3D12 (D8).
4. **Module-closure reload:** serialize a session's full module closure, reload in a fresh process, link a variant, emit bytecode (D7).
5. **CreateJobs invalidation:** prove `import`-only edits retrigger builds through the shared resolver (D9), including the **search-root shadowing** sequence — a module resolves from a lower-priority ShaderLib root, a same-named module later appears in a higher-priority root, and the AP rebuilds the dependent shader (establishes whether the resolver must register nonexistent higher-priority candidate paths, as `AppendListOfPossibleFutureLocations` already does for unresolved `#include`s — `ShaderAssetBuilder.cpp:91`). Also cover missing imports and Windows/Linux filename-case behavior, and verify builds are reproducible under two different checkout roots.
6. **Thread-safety characterization** of `IGlobalSession`/`ISession` in AssetBuilder-like conditions → sets the mutex policy.
7. **Root constants + static samplers end-to-end:** confirm Slang's push-constant/root-constant reflection satisfies the `RootConstantsInfo` contract (register/space/size, `--root-const=128` capacity) on both APIs, and drive one `[AtomStaticSampler]` from attribute reflection into a working static sampler — these are two of the six critical runtime datasets and must not remain open questions past Phase 0.

### Phase 0B — Service & packaging (≈1–2 weeks)
1. `cmake/3rdParty/FindSlang.cmake`: `o3de_fetch_content` of the prebuilt per-host release archives (`v2026.13.1`, `URL_HASH`-pinned, license metadata, all six windows/linux/macos × x86_64/aarch64 archives); imported `3rdParty::Slang` target linking `slang-compiler` with a trimmed, verified DLL closure (D8). Consumed only by the shader builder gem, which sits behind the existing host-tool/platform guards.
2. `SlangCompilerService` in the shader builder gem: global-session lifetime, session creation from `ShaderTargetDescriptor` (D2), diagnostics → `AZ_Error`/`AZ_Warning` bridging, mutex policy from gate 6, compiler/options/schema fingerprint (D7) in builder fingerprints.
3. Unit test target proving in-process compile of a trivial compute kernel to DXIL + SPIR-V inside `AssetBuilder`-like conditions.

### Phase 1A — Pure AZSL-preserving refactor (≈2 weeks)
1. `IShaderCompilerBackend` + extension-keyed backend registry; extract `AzslcBackend` from `ShaderAssetBuilder::ProcessJob` (mechanical — current code behind the seam). No Slang in this change.
2. `ShaderPlatformInterface::GetShaderTargetDescriptor()` (language-neutral, D2) with a defaulted "no target declared" base implementation — Metal and out-of-tree RHIs keep building unchanged — and stage→profile maps for DX12/Vulkan/Null. Add the generic named argument-group map to `RHI::ShaderBuildArguments` (D2) so the `"slang"` group participates in every settings/`.shader`/supervariant scope without the RHI interpreting it.
3. `ShaderReflectionData` DTO (language-neutral, D5) + the AZSLC JSON→`ShaderReflectionData` adapter, so `AzslcBackend` produces the canonical contract before any Slang code exists.
4. **Parity verification before merging:** identical AZSL products, sub-IDs, source dependencies, and diagnostics before/after the refactor (asset-cache diff across a full AtomSampleViewer build) — identical *except* the deliberate invalidation from bumping builder versions, which participate in Asset Processor fingerprints. Bump *all* affected builder version constants (`ShaderAssetBuilder`, `ShaderVariantAssetBuilder`, `ShaderVariantListBuilder` register independent versions — `AzslShaderBuilderSystemComponent.cpp`).

### Phase 1B — Minimal Slang path (.slang → .azshader, DX12 + Vulkan, PC) (≈3–4 weeks)
1. `SlangBackend` through the Phase 1A seam; per-API `.slang` prelude files (generalizing `AzslcHeader.azsli`'s role).
2. `Atom/RPI/Prelude.slang` (D4 type aliases) + `Atom/RPI/ShaderResourceGroup.slang` core modules in the RPI ShaderLib: `[AtomShaderResourceGroup]`, `[AtomStaticSampler]`, `[AtomVariantFallback]`, `[AtomRootConstants]` attributes (positional-arg syntax per Appendix C); `Bindless.slang` at `Atom/Features/Bindless.slang` using the Phase 0A-validated global-array design.
3. Reflection walker (D5): `ProgramLayout` → `ShaderReflectionData` → shared converters (stage masks: conservative scaffolding during bring-up only; exact `IMetadata`-derived masks for the parity shaders per D3; options empty until Phase 2); preserve all `StageDescriptor` byproducts (source text, debug artifacts, `m_extraData`) in the generic backend result.
4. Root-variant-only compile of all entry points; wire into `ShaderVariantAssetCreator` + existing `CreateShaderStageFunction`.
5. **Validation harness:** a test suite of shaders authored in both languages; reflection-diff harness asserting SRG layout hashes, pipeline layouts, and contracts match between backends (via the D5 debug dump); render a fullscreen triangle + a bindless texture sample via `.slang` in AtomSampleViewer-style test levels.

**Exit criteria:** a hand-written `.slang` + `.shader` renders through both DX12 and Vulkan exercising private SRGs, *shared* SRGs (Scene/View), bindless access, root constants, and a static sampler — with exact `IMetadata`-derived stage masks on the parity shaders — and every pre-existing AZSL shader builds bit-identically. "No subprocess" here means the Slang compile itself is in-process (the stated goal was eliminating the `azslc.exe`-style transpiler subprocess); Phase 2's DX12 `dxsc.exe` patching intentionally remains a subprocess.

### Phase 2 — Options, variants, supervariants (≈3–4 weeks)
1. Productize the Phase 0A option prototype: single `[AtomOption]` authoring form (D6) + builder-side `ShaderOptionGroupLayout` construction (deterministic bit packing/ordering, defaults from `.shader`, enum symbolic values, mixed specialized/dynamic behavior, default-override validation).
2. Dynamic-option fallback codegen (variant-key read functions) — parity with AZSLC's fallback path.
3. Spec-constant mode: Vulkan native; DX12 sentinel + existing `dxsc` patch + `offsets.json`/`m_extraData`.
4. Module-closure bundle product (D7) + `ShaderVariantAssetBuilder` Slang path (load closure → link option values → bytecode), with the source-recompile fallback; benchmark cold/warm variant builds vs. the AZSLC path.
5. Supervariant loop with per-supervariant sessions/macros; option-layout hash equality enforcement (same rule as today).

### Phase 3 — Metal + remaining platforms (≈2–3 weeks)
1. Metal via Slang→SPIR-V→spirv-cross (reuse existing back half), mobile Vulkan preludes (`UNBOUNDED_SIZE` analog, subpass inputs — verify Slang's `SubpassInput`/`[vk::input_attachment_index]` support).
2. Android/iOS/Linux/Mac builder traits; evaluate native `SLANG_METAL_LIB` afterward.

### Phase 4 — Ecosystem & authoring surface (≈4+ weeks, parallelizable)
1. Port a representative slice of `ShaderLib` (`SrgSemantics`, SceneSrg/ViewSrg definitions, math/PBR headers) to `.slangi` — this is the long pole for authoring real shaders (~527 azsl/azsli files in Gems; port on demand, not wholesale).
2. Material pipeline: `MaterialTypeBuilder` needs a language-aware generation seam, not just an extension rename — it explicitly emits AZSL (`#define`/`#include` stitching, the generated material-parameter struct via `WriteMaterialParameterStructureAzsli`, and source-path rewriting into the generated `.shader`). Introduce a small generator interface (AZSL + Slang implementations) covering template stitching and parameter-struct emission (`.slangi` with prelude aliases).
3. Tooling: Shader Management Console variant workflows against Slang-built shaders; Material Canvas (hardcodes `azsl`/`azsli`/`srgi` file-type filters — `MaterialCanvasMainWindow.cpp:39` — and generates shader source from graph templates); `.slang` in editor file pickers; docs + authoring guide (AZSL↔Slang construct table).
4. CI: dual-language shader test suite; reflection-diff regression harness from Phase 1B.5.

### Phase 5 — Convergence experiments (later, optional)
- AZSL-on-Slang: evaluate compiling *legacy AZSL* by translating SRG syntax with a lightweight preprocessor shim over Slang's HLSL compatibility mode — if viable, AZSLC can be retired without rewriting all content.
- Replace macro-driven material customization with Slang interfaces/generics; module-level precompilation of ShaderLib for build-time wins; `DescriptorHandle`-based bindless.

---

## Part 5 — Risks and open questions

| # | Risk | Mitigation |
|---|---|---|
| 1 | **Shared-SRG binding ABI**: Scene/View/Pass/Bindless layout hashes include every register ID (verified), and one SRG instance serves pipelines from both languages during migration | Two-tier strategy (D3): pinned explicit bindings for shared SRGs (explicit `[[vk::binding]]`/`register(space)` verified honored), auto-assign + reflect for private SRGs; byte-for-byte hash parity is Phase 0A gate 1 |
| 2 | Slang thread-safety inside AP worker processes | Per-process global session + mutex (MCPP precedent); AP parallelism is process-level anyway |
| 3 | DXIL path depends on Slang's embedded DXC (verified: DXIL emits with no external `dxcompiler.dll`); version control and signing are opaque | Phase 0A gate 3 confirms validation/signing for non-dev D3D12; `setDownstreamCompilerPath` escape hatch to pin an external DXC version |
| 4 | Serialized modules **exclude their imports** (verified) and the format churns across Slang upgrades; precompiled modules/link-time specialization are on the experimental end of Slang's maturity spectrum | Module-closure bundle + full fingerprint, reject-and-recompile on mismatch (D7); mandatory source-recompile fallback; benchmark before claiming wins |
| 5 | Metal target maturity | Phase 3 keeps spirv-cross back half until native MSL proves out |
| 6 | AZSL features without 1:1 Slang syntax (`option`, SRG semantics, static samplers, `partial` SRGs, `rootconstant`) | Attributes + generated modules (D3/D6); attribute syntax and reflection already verified (positional args, member-level static sampler); the single-authoring-form option design is the trickiest piece — Phase 0A gate 2 |
| 7 | Per-resource shader-stage masks (bindingdep parity) | Start conservative (all-entry-point masks are functionally safe), tighten via `IMetadata::isParameterLocationUsed` |
| 8 | Two languages to maintain indefinitely | Explicit convergence phase (5); per-shader migration is opt-in and diffable |
| 9 | 16-bit types / `-enable-16bit-types` parity on DX12, `-fvk-use-dx-layout` memory layout parity | Covered by Slang profile/`MatrixLayoutRow`/layout options; verify in Phase 1B tests with constant-offset asserts from reflection |
| 10 | Specialization on DX12 relies on `dxsc.exe` patching pipeline built around DXC output shape | Keep sentinel approach; verify Slang-produced DXIL is patchable, else fall back to link-time-constant per-variant compiles on DX12 |

**Open questions to resolve during Phase 0 prototyping**
1. Exact Slang thread-safety guarantees in 2026.12 (docs vs. practice) — decides mutex granularity (Phase 0A gate 6).
2. Does `IMetadata` usage-tracking cover SRG-constant-buffer members individually (needed for eventual exact stage masks)?
3. Slang `SubpassInput` parity for mobile forward+ passes.
4. `[AtomStaticSampler]` as a member attribute is verified working and reflectable; remaining question is authoring preference only — attribute in source vs. data in the `.shader` file (the latter needs no language-side declaration but splits the SRG definition across two files).
5. Is `slang-glslang.dll` needed for SPIR-V legalization in this release, or only for GLSL input? Determines the shipped DLL closure (D8).

---

## Appendix A — Key source references

| Area | Location |
|---|---|
| Builder registration | `Gems/Atom/Asset/Shader/Code/Source/Editor/AzslShaderBuilderSystemComponent.cpp:87` |
| Main build flow | `Gems/Atom/Asset/Shader/Code/Source/Editor/ShaderAssetBuilder.cpp:327` (pipeline comment at `:395`) |
| AZSLC invocation (subprocess) | `Gems/Atom/Asset/Shader/Code/Source/Editor/AzslCompiler.cpp:57` |
| MCPP in-process preprocess | `Gems/Atom/Asset/Shader/Code/Source/Editor/CommonFiles/Preprocessor.cpp:82` |
| Reflection JSON → runtime data | `ShaderBuilderUtility.cpp:134` (populate), `:546` (pipeline layout), `:809` (IA/OM contracts) |
| SRG layout construction | `Gems/Atom/Asset/Shader/Code/Source/Editor/SrgLayoutUtility.cpp:92` |
| Options JSON parsing | `Gems/Atom/Asset/Shader/Code/Source/Editor/AzslCompiler.cpp:903` |
| Variant compilation (`#define` prepend) | `Gems/Atom/Asset/Shader/Code/Source/Editor/ShaderVariantAssetBuilder.cpp:926` |
| RHI backend interface | `Gems/Atom/RHI/Code/Include/Atom/RHI.Edit/ShaderPlatformInterface.h` |
| Vulkan DXC compile | `Gems/Atom/RHI/Vulkan/Code/Source/RHI.Builders/ShaderPlatformInterface.cpp:153` |
| DX12 spec-constant DXIL patching | `Gems/Atom/RHI/DX12/Code/Source/RHI.Builders/ShaderPlatformInterface.cpp:336` |
| Build args config | `Gems/Atom/Asset/Shader/Assets/Config/Shader/**/shader_build_options.settings` |
| Per-API AZSL preludes | `Gems/Atom/Asset/Shader/Code/AZSL/Platform/<OS>/<API>/AzslcHeader.azsli` |
| Material pipeline codegen | `Gems/Atom/RPI/Code/Source/RPI.Builders/Material/MaterialTypeBuilder.cpp:664` |
| Bindless SRG | `Gems/Atom/Feature/Common/Assets/ShaderLib/Atom/Features/Bindless.azsli` |
| Slang API header | `External/Slang/include/slang.h` (v2026.12.0.1) |

## Appendix B — Content scale (Gems only)

`*.azsl`: 161 · `*.azsli`: 366 · `*.shader`: 176 — plus project/sample content. Parallel operation is mandatory; migration is per-shader and incremental.

## Appendix C — Compiler probe results (2026-07-16, bundled `slangc` 2026.12.0.1)

Empirical results that shaped the design above. All probes used the SDK in `External/Slang` (same frontend as the programmatic API).

| # | Probe | Result |
|---|---|---|
| 1 | Five unsized arrays inside a `ParameterBlock<T>` struct (the `Bindless.azsli` shape; on Windows `UNBOUNDED_SIZE` expands to nothing) | **Rejected** — `E30070: unsized member must be last` per member. `ParameterBlock` cannot express today's Bindless SRG |
| 2 | Same five unsized arrays at **global scope**, SPIR-V target | **Compiles** — sequential bindings in one descriptor set (bindings 0–4, set 0): matches today's Vulkan Bindless layout shape |
| 3 | Same, DXIL target (`cs_6_2`, explicit entry) | **Compiles** — each unbounded array auto-assigned its own register space (`space1`–`space5`): matches DX12 unbounded-array semantics. Notably, **no `dxcompiler.dll` exists in the SDK's bin/** — an embedded DXC 1.10 (with validator) did the work |
| 4 | User attribute with **named** arguments: `[AtomShaderResourceGroup(bindingSlot: 2)]` | **Rejected** — `E20001: unexpected ':', expected ','`. Attributes are positional-only |
| 5 | User attributes with **positional** args, `[AtomStaticSampler(16, "Wrap")]` placed on the sampler member | **Compiles**, and `-reflection-json` shows `userAttribs` on the member with name + argument values intact |
| 6 | Explicit `[[vk::binding(n, 4)]]` + `register(tN, spaceN)` on global unsized arrays | **Honored exactly** (SPIR-V `DescriptorSet 4`, bindings as annotated) — explicit pinning available for the shared-SRG ABI |
| 7 | Serialize module `rootmod` (imports `depmod`), reload `rootmod.slang-module` in an isolated directory | **Fails** — `cannot open file 'depmod.slang'`, even with `depmod.slang-module` beside it (via `slangc` search paths). Serialized modules exclude their imports; caching needs the full closure |
| 8 | SDK binary shape | `slang.dll` = 0.15 MB proxy; `slang-compiler.dll` = 33 MB (link this); `slang-llvm.dll` = 105 MB (CPU targets only); `slang-glslang.dll` = 11 MB (necessity unverified — open question 5) |

Repo-side verifications: SRG layout hash includes every input's `m_registerId` (`ShaderResourceGroupLayoutDescriptor.cpp` `GetHash` methods; accumulated in `ShaderResourceGroupLayout.cpp:320`); `ProcessJobResponse` carries no source dependencies (`AssetBuilderSDK.h:767`) so CreateJobs is the only dependency channel; `AzslData` is marked `DEPRECATED [ATOM-15472]` and `SrgData` has no serialization reflection (`AzslData.h`); Material Canvas hardcodes `azsl`/`azsli`/`srgi` file filters (`MaterialCanvasMainWindow.cpp:39`); `Atom_Asset_Shader.Builders` already consumes compiler binaries via `RUNTIME_DEPENDENCIES` on `3rdParty::` targets (`CMakeLists.txt:108`).
