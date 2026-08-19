# Jolt implementation rules

Apply these rules to every Jolt change. They condense the project requirements and the SIG-Core coding standard, best-practices guide,
and API reference guide. Upstream `Required` rules remain mandatory; this file adds stricter local constraints. The repository's C++20
configuration wins over legacy language-version examples.

## Architecture

- Keep V1 standalone. Claim only Jolt-specific services and interfaces; do not register AzPhysics, Physics, DefaultWorld, or other generic
  provider singletons. The Gem must coexist with PhysX and Box3D.
- Keep every `JPH` type, header, identifier, macro, and calling convention private. Public headers expose only engine-friendly strong types.
- Treat runtime handles as transient capabilities, never serialized or network identities. Use provider-owned 64-bit generational handles
  with `Handle<Tag>::Invalid` and an `explicit operator bool()`.
- Use `AZ::Transform` for entity transforms and a separate non-uniform-scale component/bus only where a shape supports it. Do not burden all
  component instances with non-uniform-scale storage.
- Make deterministic behavior explicit: fixed integer ticks, canonical mutation/event/query ordering, stable keys, compatibility
  fingerprints, and per-tick digests. Canonicalize and restore the floating-point environment on every calling and worker thread that
  performs physics work. Include CPU hair in snapshots and deterministic validation, but certify it only for the same binary until the
  supported platform matrix proves bitwise cross-platform equivalence. CPU Hair is the only supported Hair backend.
- Keep the native Jolt allocator, job system, tracing, profiling, and assertion boundaries integrated with existing AzCore facilities.
  Do not invent parallel infrastructure.

## Naming and readability

- Prefer self-documenting control flow: validate early, return early, and keep the success path shallow. Extract a function only for actual
  reuse, an API callback, RAII/lifetime control, platform separation, independent testing, or enough algorithmic complexity to justify it.
- Test pointers directly (`if (pointer)` / `if (!pointer)`); do not compare them with `nullptr`.
- Do not use conditional (`?:`) expressions without measured code-generation or lifetime evidence that explicit control flow is worse.
- Prefix abstract interfaces and traits with `I`, but name their files for the capability (`Cooking.h` contains `ICooking`). Retain the
  established EBus `Requests` and `Notifications` suffixes.
- Read names as qualified phrases and omit repeated context: `Jolt::RigidBodyComponent`, not `Jolt::JoltRigidBodyComponent`.
- Use `Raycast` in new APIs. Use UpperCamelCase for types/functions/enumerators/constants, lowerCamelCase for locals/parameters, `m_` for
  members, and `s_` for static members. Treat acronyms as words (`Api`, `Id`, `Json`).
- Order enums by domain sequence when one exists and alphabetically otherwise. Preserve native, serialized, networked, variant-index, and
  bit-mask values ahead of cosmetic ordering. Reserve zero for `None` when no selection is a real state; otherwise start valid values at one
  only when that is semantically useful and contract-safe.
- Comments explain why, invariants, units, ownership, lifetime, non-obvious cost, or algorithm choice. Delete narration of syntax. Do not
  leave TODO comments. Public API comments use `//!` and document only meaningful constraints, side effects, failure modes, and lifetimes.

## C++ and API design

- Use supported C++20 where it improves ownership, constraints, correctness, or performance. Use AZ/AZStd facilities, fixed-width integers,
  `nullptr`, C++ casts, and `aznumeric_cast`; do not use exceptions, native RTTI, `dynamic_cast`, raw `new`, or `malloc`.
- Pass trivial values up to 64 bits by value. Use `AZStd::span` for borrowed contiguous ranges and `AZStd::string_view` for transient text.
  Use `AZ::Name` only for stable interned identity when `NameDictionary` lifetime is guaranteed.
- Express ownership with values first, `AZStd::unique_ptr` for one owner, shared ownership only when real, and raw pointers/references only
  for observation. Allocate through engine allocators and the Gem's native child allocator.
- Declare UUID constants as `inline constexpr AZ::TypeId`. Mark leaf value structs `final`; leave only deliberate polymorphic bases
  inheritable. Use `AZ_DISABLE_COPY`, `AZ_DISABLE_MOVE`, or `AZ_DISABLE_COPY_MOVE` rather than spelling deleted special members individually.
- Keep implementations out of public headers except templates and trivial accessors. Public headers are IWYU-clean and compile standalone;
  never depend on unity-build leakage or manually forward-declare standard/AZStd aliases.
- Use `AZStd` containers publicly. Lay out members for size, alignment, and locality; keep fields used together together. Put methods before
  data in each access section and separate data into semantic groups.
- Do not use public/reflected bit-fields. Consider private packed flags only after measuring size, cache behavior, and generated code.
- Put non-template `static_assert`s in a dedicated `.cpp` so every consumer does not reevaluate them.

## Formatting and source layout

- Use four spaces, Allman braces, braces for all control flow, one statement per line, left-bound `*`/`&`, and a 140-column limit. Put class
  inheritance on the following line even when it fits. Never reformat unrelated code.
- Put a public `Reflect` immediately after the constructor/destructor block and isolate it with blank lines. Do not add reflection
  `Version(...)` until the V1 serialization contract is intentionally stabilized.
- Put `[[nodiscard]]` on its own line; keep `[[maybe_unused]]` attached to its parameter.
- Chop declaration and definition parameter lists when their types and names benefit from vertical scanning. Prefer the chopped form for
  declarations with two or more parameters. Keep calls, return expressions, and initializers on one line when they fit the configured
  line limit. When a call must wrap, break immediately after `(`, put each argument on its own line, and use ordinary four-space
  indentation rather than aligning to earlier text.
- Keep `=` on the declaration/expression line and begin a wrapped right-hand side on the next line. When a condition wraps, put every
  operand on its own line and place each `&&` or `||` at the beginning of its operand's line.
- Put one blank line after every member-function declaration except the contiguous special-member block. Group methods logically, mirror
  base-class order for overrides, and alphabetize only when no stronger domain order exists. Keep coupled pairs adjacent.
- Keep each serialized reflection chain contiguous and put one blank line between complete chains. Separate top-level BehaviorContext enum,
  class, and EBus chains with one blank line. Omit needless wrapping of simple macros.
- Prefer designated initializers for long aggregates. Do not place spaces just inside initializer braces. Add trailing commas to multiline
  enums and initializers, but omit them from compact single-line initializers.
- Keep private sources under `Source/Jolt`, editor sources under `Source/Jolt/Editor`, and editor types in `namespace Jolt::Editor`. Mirror the
  public include hierarchy and use package-qualified private includes.
- Preserve explicit CMake manifests and IDE hierarchy. Put platform implementations behind PAL files instead of shared-file conditionals.
- Store JSON-authored Jolt source documents as `*.jolt.json`. Route the closed document set by its stable root `ClassName` and reject unknown
  classes before processing. Compile each document to a `.jolt` product and use its catalog asset type, not its extension, to identify the
  runtime schema. Reserve product sub-ID zero for the engine's generic JSON product and use stable nonzero sub-IDs for compiled products.
- Source and installed engines must both work through the private `JoltNative.cmake` integration. Do not add a `FindJolt.cmake`, publish a
  `3rdParty::Jolt` target, install native headers, or expose native ABI policy to consumers. Keep all third-party compile policy target-local.

## Verification

- Unit-test every public contract, native conversion boundary, invalid/stale/cross-world handle, failure path, event ordering rule, feature
  flag, shape, body mode, constraint, query, character behavior, serialization path, and deterministic state rule.
- Add Jolt-specific levels and prefabs under `AutomatedTesting/Physics/Jolt`. Prove the standalone provider remains independent when PhysX
  and Box3D are present.
- Test unity and non-unity builds, standalone public headers, source and installed engines, client/server/tools variants, supported PALs,
  and Debug/Profile/Release behavior. Defer slow Linux qualification until the final validation phase.
- Benchmark providers in separate processes with equivalent scenes. Record median, p95, p99, throughput confidence bounds, allocations,
  memory, task counts, and instrumentation overhead. Inspect generated assembly for hot paths and retain changes only with correctness and
  performance evidence. Never claim a performance ranking without current comparable results.

Primary references:

- [Coding Standards and Style Guide](https://github.com/o3de/sig-core/blob/main/governance/Coding-Standards-and-Style-Guide.md)
- [C++ Best Practices Guide](https://github.com/o3de/sig-core/blob/main/governance/C%2B%2B-Best-Practices-Guide.md)
- [API Reference Guidelines](https://github.com/o3de/sig-core/blob/main/governance/API-Ref-Guidelines-Update.md)
