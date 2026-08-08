# CryCommon migration validation

This document records the evidence and remaining gates for the `no-cry` migration. It is intentionally a status report, not a claim that compiling alone proves behavioral equivalence.

## Candidate and scope

- Development comparison point: `upstream/development` (`1fe32b68...` when this audit began).
- Original `no-cry` commit: `809fcd82c1ff3d7a215f934f6acd0876144762f4`.
- The PR candidate must be frozen to a commit after these working-tree remediations are committed. Regenerate the hunk manifest at that SHA.
- The Mac test workaround from `fix-mac-tests` (`c7e1355cdf...`) was reproduced only in generated build output and is not part of this source diff.

Foundational Cry math, color, conversion, and random headers were restored from development for downstream source compatibility. Production consumers remain prohibited from adding new uses. AZ-native font, XML, serialization, spline, and indexed-mesh interfaces remain intentionally migrated.

## Inventory and disposition ledger

The original plan inventory reported 128 explicit CryCommon includes in 93 external files and 89 dependency declarations in 48 CMake files. The current working candidate has 97 explicit `CryCommon/...` includes in 66 external files and 68 dependency declarations in 34 CMake files. Recount these values after freezing the candidate.

The generated declaration/use ledger covers 85 public headers, 1,575 aggregated identifiers at
2,310 declaration sites, 488 resolved external include edges (97 explicit and 391 unqualified),
and every remaining CMake dependency. Its CSV records declaration sites, direct/transitive include
closure, and bounded per-symbol/file reference records; 29 lexically unresolved declaration
candidates and ambiguous identifier matches are retained and labeled instead of being treated as
deletion evidence. Generate or verify it with:

```text
python3 scripts/commit_validation/generate_crycommon_ledger.py
python3 scripts/commit_validation/generate_crycommon_ledger.py --check
```

| Surface | Disposition |
|---|---|
| `Vec2/3/4`, matrices, quaternions, colors, conversions, random compatibility | Restored as exported source compatibility; forbidden in migrated production consumers |
| GPU/file wire vectors | `AZ::PackedVector2f` / `AZ::PackedVector3f`, with size/alignment/offset assertions and byte tests |
| `stl::push_back_unique`, find, first erase, map lookup | Replaced with explicit AZStd/container operations preserving first-match and return semantics |
| `stl::free_container` | Default-container swap for standard containers; `CryMT::queue::free_memory()` for the synchronized queue |
| Legacy string hash/case helpers | Bounded local compatibility functors preserving the polynomial hash and comparison behavior |
| `MiniQueue<CTimeValue, 32>` | `AZStd::fixed_vector` with explicit oldest-entry eviction |
| Float clamps and degree/radian macros | Bounded expressions retaining legacy NaN, signed-zero, and evaluation-order behavior |
| Legacy Vec2/Vec3/Vec4 serialized node UUIDs | Lifecycle-owned by AzCore math reflection and converted with checked reads/writes |
| Legacy outer Vec2 spline-track UUIDs | Explicit deprecated-class converters in LyShine and Maestro |
| `PathUtil` aliases/localization and live console bridges | Deferred until edge behavior and ownership/lifecycle are proven |
| `gEnv`, `CTimeValue`, lifecycle buses/listeners, CrySystem console bridge | Deferred systemic work |

The full-tree and commit guards live in `scripts/commit_validation`. They reject new high-signal retired headers, unique types, constants, helpers, macros, global-qualified legacy forms, and target dependencies. Intentional compatibility declarations/tests are capped per file and may decrease but not increase. Ambiguous unqualified names shared by Cry and AZ or subsystem-local APIs (for example `Plane` and `Lerp`) require the declaration ledger/AST-aware review; the lexical guard deliberately does not claim it can infer symbol ownership.

## Corrected release blockers

- Restored all reviewed 12/16/24-byte vertex layouts across SSE and NEON; `SVF_P3F_C4B_T2F` remains 24 bytes.
- Removed uninitialized `AZ::Vector2` component reads in AtomFont.
- Restored exact float clamp and degree/radian operation ordering where observable behavior differed.
- Preserved zero and very-small-vector normalization behavior, including signed zero.
- Replaced the migrated spline adapter's aligned `AZ::Vector2` reinterpret casts with explicit
  `StoreToFloat2`/`CreateFromFloat2` conversion, including deterministic spare lanes.
- Restored `int_round` half-neighbor behavior for localized numeric strings and reflected-property sliders; removed an unused narrowing rounding helper.
- Preserved the raw legacy `UiAnimParamData` Vec2 UUID compatibility exception.
- Added checked legacy Vec2/Vec3/Vec4 conversion and migration for both generated outer spline-track UUIDs.
- Moved shared legacy vector converters to AzCore math reflection so module teardown cannot leave dangling converter callbacks.
- Restored CryLegacy equivalence tests and the unrelated patch-script failure behavior.
- Removed safe, bounded dead includes/dependencies and all live external `stl::`/`MiniQueue` uses; lifecycle-changing migrations remain deferred.

## Durable evidence

- Static assertions cover every changed exported vertex structure's size, alignment, and member offsets.
- Raw byte tests cover AtomFont vertex formats and LyShine UI primitive vertices.
- Legacy vector converters test valid values, malformed nodes, and reflection teardown.
- LyShine and Maestro tests cover legacy outer spline UUID conversion and the raw legacy animation parameter UUID.
- Every checked-in legacy `.uicanvas` fixture is loaded without ignoring unknown classes, saved, reloaded, and compared through an animation manifest containing sequence ranges, events, nodes, tracks, parameters, raw per-index key values and tangents, duplicate-time keys, flags, times, and deterministic inter-key samples.
- A synthetic legacy outer-track fixture carries a nonzero `DefaultValue` encoded with the old Vec2 UUID, proving that the outer and nested converters compose and preserve empty-track evaluation.
- A deliberately float-aligned, SIMD-misaligned `ISplineInterpolator::ValueType` test exercises the `AZ::Vector2` round trip and verifies zeroed spare lanes.
- Cry random output is checked against a fixed baseline sequence, not another candidate generator.
- The compatibility test translation unit exercises retained Cry constructors, methods, conversions, math, color, and random behavior.

## Validation commands

```text
python3 scripts/commit_validation/check_crycommon_usage.py
python/python.sh -m pytest -q scripts/commit_validation/commit_validation/tests/test_crycommon_usage_validator.py
cmake --build build/mac_ninja --target EditorLib.Tests AtomFont.Tests LyShine.Tests Maestro.Tests -j 8
cmake --preset mac-ninja-no-unity -B build/mac_ninja_nounity
cmake --build build/mac_ninja_nounity --config profile --target EditorLib.Tests AtomFont.Tests LyShine.Tests Maestro.Tests -j 8
```

Results on the latest reviewed source during this audit:

- Debug unity AtomFont, Maestro, LyShine, and EditorLib composites: passed. LyShine includes the strengthened strict canvas manifest and final spline storage/default-value tests.
- Profile no-unity AtomFont, Maestro, LyShine, and EditorLib composites: passed on the same final source.
- The complete 97-entry Mac profile no-unity inventory was built and run. 83 entries passed and 14 failed; both long-running benchmark suites and all four migration-sensitive composites passed. The failing entries were AzCore, AzFramework, AzToolsFramework, AzNetworking main/sandbox, AssetProcessor main/periodic, ImageProcessingAtom.Editor, GradientSignal.Benchmarks, AtomToolsFramework, EMotionFX.Editor, SaveData, PhysX5.Editor, and ScriptCanvasTesting.Editor. Their observed failures are outside the changed production areas, but a matching baseline run is still required before classifying them as pre-existing.
- EditorLib's 74 tests require `QT_QPA_PLATFORM=minimal` in this headless environment; the default invocation aborts because no screen is available.
- The Mac AddressSanitizer build completed. Full AtomFont and Maestro composites passed, as did all 7 CryLegacy compatibility tests and the 11 LyShine migration/serialization tests under focused filters. Full EditorLib and LyShine composite runs encounter an AzCore thread-name lifetime finding in `pthread_setname_np`; it is outside this diff but requires a baseline run before the candidate can claim it introduced no sanitizer finding. Leak detection is unsupported by the Mac sanitizer runtime used here.
- Commit guard and declaration-ledger tests: 7 passed.
- Full-tree CryCommon guard and `git diff --check`: passed at the current working candidate.

## Gates not satisfied locally

Do not describe this branch as fully behaviorally equivalent until all of these are recorded at the frozen commit:

- Baseline-versus-candidate differential executables and corpora were not run in isolated worktrees.
- The durable canvas tests now capture raw spline values and `ds`/`dd` tangents, duplicate-time keys, a nonzero default, sequence ranges, and deterministic inter-key samples, but the isolated baseline-versus-candidate comparison remains outstanding.
- Generated AtomFont glyph/index buffers and text measurement still need baseline capture and comparison; declaration byte tests alone do not cover that path.
- The full Mac test inventory build/run is complete. Fourteen failures outside the migration-sensitive composites require a matching baseline run for formal classification; Editor/UI rendering smoke scenarios remain outstanding.
- AddressSanitizer found an AzCore thread-name lifetime error outside this diff during unrelated EditorLib/LyShine fixtures. The migration-focused filters pass, but a matching baseline run is required to classify that engine-wide finding formally.
- Linux and Windows normal/no-unity CI remain external gates.
- The hunk manifest's two review columns remain pending until both reviewers sign off the final frozen SHA.

Any failure at these gates requires correcting, restoring, or deferring the affected migration rather than weakening the check.

## Reproducible hunk coverage

Generate the PR manifest after the final commit:

```text
python3 scripts/commit_validation/generate_crycommon_hunk_manifest.py \
  --base upstream/development \
  --output Code/Legacy/CryCommon/CryCommonHunkManifest.csv
```

The current CSV contains 2,182 unique final-diff hunk records, with upstream-identical compatibility restorations and the generated manifest itself excluded from the final PR diff model. It records each hunk's semantic group, contract claim, evidence pointer, disposition, and both independent-review columns; those columns remain explicitly pending frozen-SHA signoff. Regenerate it whenever the candidate changes.
