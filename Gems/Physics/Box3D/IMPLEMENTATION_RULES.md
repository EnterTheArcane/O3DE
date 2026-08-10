# Box3D implementation rules

This checklist condenses the project requirements and the current upstream [coding standard], [best-practices guide], and [API reference
guide]. Apply it to every change in this Gem and to shared physics APIs changed for this work. `Required` rules in the upstream documents
remain mandatory; this file adds stricter project-specific constraints.

The source snapshot is SIG-Core commit [`cacff6e`]. Apply rules in this order: upstream `Required` rules, upstream `Recommended` rules
unless they have a concrete technical cost, then stricter rules in this file. Written rules outrank non-normative examples. Where the
snapshot's C++17 or legacy-toolchain guidance conflicts with this checkout, the repository's C++20 build configuration wins.

## Readability and naming

- Make control flow self-documenting: validate preconditions early, return early, keep the success path shallow, and split genuinely
  independent logic. Do not trade measurable performance or binary size for cosmetic structure.
- Test pointers and pointer-like values directly (`if (pointer)` / `if (!pointer)`); do not compare them with `nullptr`. A type that exposes
  `IsValid()` also exposes an `explicit operator bool()` when validity is an unambiguous contextual condition.
- Do not use conditional (`?:`) expressions. Use explicit control flow and a narrowly scoped result instead. An exception requires measured
  code-generation or lifetime evidence showing that an equivalent `if` statement has a material cost.
- Keep code used at one call site in the calling function. Extract a function only for actual reuse, an API-required callback, RAII/lifetime
  control, platform separation, independent testing, or enough algorithmic complexity to justify a separately named operation.
- Name the exact domain concept, ownership, units, and lifecycle. Rename a symbol when its meaning changes. Avoid abbreviations; treat
  acronyms as words (`Api`, `Id`, `Json`).
- Spell the operation `Raycast` in new Box-owned APIs and identifiers. Retain legacy `RayCast` only when overriding or consuming an existing
  engine or scripting contract.
- Read every name as a qualified phrase. Do not repeat information already supplied by its namespace, class, or directory:
  `Box3D::StaticRigidBodyComponent`, not `Box3D::Box3DStaticRigidBodyComponent`.
- Use UpperCamelCase for types, namespaces, functions, constants with static storage, and enumerators. Use lowerCamelCase for locals and
  parameters, `m_` for members, and `s_` for static members. Prefix abstract interface and trait types with `I` so the unprefixed name
  remains available to a concrete implementation; retain established EBus `Requests` and `Notifications` suffixes. Name the header for
  the capability without the `I` prefix (`Cooking.h` contains `ICooking`). Do not use other Hungarian notation or class-letter prefixes.
- Fully qualify cross-namespace ownership boundaries (`AZ::`, `AZStd::`) while omitting redundant qualification inside the
  current namespace. Never use `using namespace` in a public header or at translation-unit scope.
- Use scoped enums unless bit-field ergonomics justify an unscoped enum. Prefer typed constants over macros; macros are a last resort and
  require a subsystem prefix plus uppercase spelling.
- Order enum values by domain sequence when one exists and alphabetically otherwise. Preserve native, serialized, networked, variant-index,
  and bit-mask values ahead of cosmetic ordering. Reserve zero for `None` when no selection is a real state; otherwise start valid values at
  one only when that does not break an established numeric contract. Boolean-like and lifecycle enums do not acquire artificial `None`
  values.
- Use `auto` only when the type is obvious from the expression or it improves correctness and maintenance. Do not hide important ownership,
  precision, or domain types.

## Code and API design

- The repository targets C++20 in `cmake/Configurations.cmake`. Use supported C++20 features when they make ownership, constraints,
  correctness, or performance clearer;
  keep code warning-free across every supported compiler and platform.
- Use `AZ`/`AZStd` facilities, fixed-width integers, `nullptr`, C++ casts, `aznumeric_cast` for checked numeric conversions, and `AZ_RTTI`
  where runtime type information is required. Do not use exceptions, native RTTI, `dynamic_cast`, raw `new`, or `malloc`.
- Pass trivial values of at most 64 bits by value. Use `AZStd::span` for borrowed contiguous ranges instead of container references, and use
  `AZStd::string_view` for non-owning string input that is not retained. Keep owning, serialized, and resizable output containers explicit.
  Print string views with `AZ_STRING_FORMAT`/`AZ_STRING_ARG`.
- Use `AZ::Name` for stable interned identity and frequent equality checks when `NameDictionary` lifetime is guaranteed. Use owning strings
  for serialized/display text and string views for transient native calls; do not intern text merely because it has a name.
- Declare UUID constants as `inline constexpr AZ::TypeId`, not character pointers. Mark leaf value structs `final`; leave only deliberate
  polymorphic bases inheritable.
- Express ownership: values first, `AZStd::unique_ptr` for one owner, `AZStd::shared_ptr` only for real shared ownership, and raw pointers or
  references only for non-owning access. Allocate through the engine allocator system and give the Gem a child allocator for native data.
- Avoid allocating global/function-static class objects, allocator-backed containers, and objects that connect to buses. Initialize every
  object and variable at declaration in the narrowest useful scope.
- Keep the V1 public API provider-owned. Do not claim the generic `Physics` service or register `AzPhysics`, `Physics`, default-world, or
  related singleton interfaces. Coexist with PhysX and use unique component services and script bus names.
- Keep every native Box3D type, identifier, macro, header, and calling convention private. Public headers expose strong engine types only.
  Treat runtime handles as transient capabilities, never persistent/network identities.
- Make deterministic behavior explicit: fixed integer ticks, canonical mutation/event/query ordering, stable keys, a compatibility
  fingerprint, and per-tick digests. Every calling, AZ job, and native scheduler thread that performs physics work must canonicalize and
  restore its floating-point environment, including x86 MXCSR and ARM64 FPCR modes. Do not claim rollback semantics that Box3D recording
  does not provide.

## Headers, layout, and builds

- Every header uses `#pragma once`, forward-slash package-qualified includes, and IWYU. A translation unit that includes only that public
  header must compile. Forward declare only when a complete type is not required.
- Forward declare only legally forward-declarable project types used through pointers or references. Never manually forward-declare
  standard-library or alias-backed `AZStd` types; include their defining header directly.
- Keep implementations out of public headers except templates and trivial accessors. Prefer PIMPL/private implementation targets where it
  materially reduces dependency fan-out. Never rely on unity-build include leakage.
- Public containers/strings use `AZStd`. Lay out members for stable size and cache locality; keep data used together together. Put functions
  before data within each access section and separate data into semantic groups. Verify ABI assumptions with focused `static_assert`s in a
  dedicated `.cpp`; retain assertions in headers only when template instantiation or immediate consumer ABI enforcement requires them.
- Do not use public or reflected bit-fields: they cannot provide the member pointers required by reflection and their packing is
  implementation-defined. Consider packed flags only in private hot data after measuring object size, cache behavior, and generated code;
  ordinary booleans can be faster when a bit-field would require read-modify-write operations.
- Place a public `Reflect` declaration immediately after the constructor/destructor block, isolated by one blank line before and after it.
  Omit reflection `Version(...)` declarations until the V1 serialization contract is intentionally stabilized.
- Put `[[nodiscard]]` on its own line. Keep `[[maybe_unused]]` attached to the parameter it qualifies. When a parameter list is wrapped,
  put every parameter on its own line; prefer that chopped form for declarations with two or more parameters. Put a continued binary or
  logical operator at the start of the continuation line.
- Use block indentation for wrapped call arguments: break immediately after the opening parenthesis and indent each argument one level
  (four spaces) from the owning expression. Do not align continuation arguments to the opening parenthesis or a previous argument. Apply
  this recursively to nested calls, and put each argument on its own line.
- Keep an assignment operator on the declaration or expression line. If the right-hand side wraps, begin it on the following line with one
  additional indentation level; never place `=` at the beginning of a continuation line.
- When a logical condition wraps, put every operand on its own line and place each `&&` or `||` at the beginning of its operand's line.
  Do not retain multiple operands on either the first line or a continuation line.
- Keep a serialized class reflection chain contiguous from `Class<...>()` through its final `Field(...)`. Put one blank line between complete
  class reflection chains, not within a chain.
- Keep a simple macro invocation on one line when it fits. For wrapped template argument and type lists, break after the opening angle
  bracket and indent every entry by one level instead of aligning entries to the first argument.
- Use logical method groups first, mirror base-class order for overrides, and alphabetize members within a group when no stronger domain
  order exists. Keep tightly coupled pairs such as enable/disable or getter/update adjacent. Put one blank line after every member-function
  declaration so attributes, comments, and wrapped signatures remain visually isolated. Keep only the constructor, destructor, copy, and
  move special-member block contiguous. Group data members semantically instead of separating every field.
- Prefer designated initializers for long aggregates. Do not put spaces just inside initializer braces. Add a trailing comma to multiline
  enums and initializer lists, but omit it from compact single-line initializers.
- Prefer a qualified explicit specialization such as `struct AZStd::hash<Box3D::Handle<Tag>>` over opening a namespace solely for one
  specialization.
- Use four spaces, Allman braces, braces for every control-flow body, one statement per line, left-bound `*`/`&`, and a 140-column limit.
  Put a class inheritance list on the line after the class name, even when it would fit on one line. Run the repository `.clang-format`
  only on the changed ranges of touched C++ files; never reformat unchanged code. Before handoff, compare tracked files with their base and
  restore format-only edits so reviewers see only intentional changes.
- Preserve disk/CMake/IDE hierarchy and explicit source manifests. Put platform implementations behind PAL files instead of adding platform
  conditionals to shared code.
- Mirror the include hierarchy under `Source/Box3D`; private includes remain package-qualified. Keep editor implementation under
  `Source/Box3D/Editor` and editor types in `namespace Box3D::Editor`, using compact nested-namespace syntax.
- Keep the Gem at `Gems/Physics/Box3D` and its level/Python tests under matching `Physics/Box3D` hierarchy. Because Gem discovery is not
  recursive, register the concrete Gem path explicitly rather than relying on the organizational directory.
- Source and installed builds must both work. Keep `FindBox3D.cmake` in the source and installer locations, preserve the precision ABI marker,
  and keep all third-party compile policy target-local.

## Documentation and diagnostics

- Comments add information the code cannot: why, invariants, units, ownership, lifetime, constraints, non-obvious cost, or algorithm choice.
  Delete narration that merely restates names or syntax. Use complete, objective sentences and do not leave TODO comments.
- Public API comments use `//!`; the first sentence is the brief and does not use `@brief`. Document only meaningful parameter constraints,
  return behavior, side effects, and failure modes. Keep implementation detail in `.cpp` files.
- Separate top-level BehaviorContext enum, class, and EBus reflection chains with one blank line so each script contract is visually distinct.
- Use `AZ_Printf`, `AZ_TracePrintf`, `AZ_Warning`, `AZ_Error`, `AZ_Assert`, and existing profiler/telemetry facilities. Do not invent a parallel
  tracing system.

## Verification gates

- Unit-test every public contract, native conversion boundary, invalid/stale/cross-scene handle case, failure path, event ordering rule,
  feature flag, shape, body mode, joint, query, character behavior, recording behavior, and serialization migration.
- Prefer memory-backed test data and mocks over disk IO. Use trace suppression for expected errors and death tests for expected assertions.
- Add end-to-end coverage as Box3D-specific levels and prefabs in the existing `AutomatedTesting` project. Tests must prove that provider
  components remain independent when PhysX is also present.
- Test unity and non-unity builds, standalone public-header compilation, source and installed engines, client/server/tools variants, supported
  platforms, and Debug/Profile/Release behavior.
- Benchmark providers in separate processes with equivalent scenes. Record median, p95, p99, throughput confidence bounds, allocations,
  memory, task counts, and instrumentation overhead. A feature is incomplete until deterministic replay and agreed performance gates pass.

[`cacff6e`]: https://github.com/o3de/sig-core/commit/cacff6e84b5ef33a6cfa0f511ec9ad0fc9c35437
[coding standard]: https://github.com/o3de/sig-core/blob/cacff6e84b5ef33a6cfa0f511ec9ad0fc9c35437/governance/Coding-Standards-and-Style-Guide.md
[best-practices guide]: https://github.com/o3de/sig-core/blob/cacff6e84b5ef33a6cfa0f511ec9ad0fc9c35437/governance/C%2B%2B-Best-Practices-Guide.md
[API reference guide]: https://github.com/o3de/sig-core/blob/cacff6e84b5ef33a6cfa0f511ec9ad0fc9c35437/governance/API-Ref-Guidelines-Update.md
