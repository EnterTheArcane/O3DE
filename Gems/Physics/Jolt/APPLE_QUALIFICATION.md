# Apple CPU qualification

This guide is the handoff for an agent qualifying the standalone Jolt Gem on Apple hardware. The Gem owns its worlds, resources,
components, scripting, editor integration, assets, diagnostics, and deterministic CPU Hair without registering AzPhysics singletons.
JoltPhysics 5.6.0 is fetched and built privately by `3rdParty/JoltNative.cmake`; native headers, targets, ABI types, and GPU handles must
not cross the Gem boundary. GPU Hair is deliberately outside this qualification. The immediate scope is macOS CPU; report iOS as a
separate external gate.

Windows currently passes the complete 68-step CPU-only functional and packaging matrix with clang-cl and MSVC, including
Debug/Profile/Release, unity/non-unity, double precision, diagnostics, ASan, modular/monolithic, and installed consumers. An earlier WSL2
Clang Release checkpoint passed its then-current CPU suite, but the current Linux rerun is deferred and its multiworker benchmark data
remains too variable to qualify native Linux. None of these results is Apple evidence. Do not mark an Apple ledger row complete without
running it on the recorded hardware and current working tree.

## Rules of engagement

- Record the exact commit and complete dirty diff before and after the run. Do not discard or reformat unrelated work.
- Fix source defects instead of suppressing warnings. Scope unavoidable third-party suppressions to the private native target and explain
  the exact upstream diagnostic.
- Put Apple-specific implementation behind the normal PAL files and explicit CMake manifests. Do not scatter `__APPLE__` branches through
  platform-neutral code.
- Preserve the private native boundary. Do not add `FindJolt.cmake`, `3rdParty::Jolt`, installed native headers, or public `JPH` types.
- Preserve deterministic floating-point state, canonical ordering, and exact 1/4/8-worker results. A platform-specific epsilon is not a
  substitute for investigating a divergent digest.
- Update explicit source manifests, focused tests, and `FEATURE_COVERAGE.md` with every source change. State unavailable hardware,
  device, SDK, sanitizer, or analysis gates plainly.
- Use all available compile cores. The commands below use Ninja with `--parallel`; replace the count only when thermal stability requires
  an explicitly documented limit.

## Record the host and source state

Run from the engine root and retain the output with the qualification artifacts:

```bash
mkdir -p build/jolt_apple_results

git rev-parse HEAD | tee build/jolt_apple_results/revision.txt
git status --short | tee build/jolt_apple_results/status-before.txt
git diff --binary | tee build/jolt_apple_results/dirty-before.patch

sw_vers | tee build/jolt_apple_results/macos.txt
xcodebuild -version | tee build/jolt_apple_results/xcode.txt
xcrun clang++ --version | tee build/jolt_apple_results/appleclang.txt
xcrun --show-sdk-path | tee build/jolt_apple_results/sdk-path.txt
uname -a | tee build/jolt_apple_results/uname.txt
system_profiler SPHardwareDataType | tee build/jolt_apple_results/hardware.txt
sysctl -n hw.model hw.machine hw.logicalcpu hw.physicalcpu hw.memsize \
    | tee build/jolt_apple_results/cpu-memory.txt
pmset -g batt | tee build/jolt_apple_results/power-before.txt
pmset -g therm | tee build/jolt_apple_results/thermal-before.txt
cmake --version | tee build/jolt_apple_results/cmake.txt
ninja --version | tee build/jolt_apple_results/ninja.txt
```

Also record the Mac model, Apple Silicon or Intel architecture, total memory, power connection, Low Power Mode state, cooling setup,
background workload, and whether FileVault indexing or another service was active. Never combine Rosetta and native results in one table.

## Configure the primary build matrix

The root presets define `mac-ninja`, `mac-ninja-no-unity`, `mac-default`, and `mac-mono-default`. Use separate build directories so options
and fetched native objects cannot contaminate one another.

```bash
jobs=$(sysctl -n hw.logicalcpu)

cmake --preset mac-ninja -S . -B build/jolt_mac \
    -DLY_PROJECTS=AutomatedTesting

cmake --preset mac-ninja-no-unity -S . -B build/jolt_mac_no_unity \
    -DLY_PROJECTS=AutomatedTesting

cmake --preset mac-ninja -S . -B build/jolt_mac_double \
    -DLY_PROJECTS=AutomatedTesting \
    -DLY_JOLT_DOUBLE_PRECISION=ON

cmake --preset mac-ninja -S . -B build/jolt_mac_mono \
    -DLY_PROJECTS=AutomatedTesting \
    -DLY_MONOLITHIC_GAME=ON
```

Save every configure command and cache. Confirm these invariants before compiling:

```bash
grep -E 'LY_JOLT_|LY_MONOLITHIC_GAME|LY_UNITY_BUILD|CMAKE_(CXX_)?COMPILER' \
    build/jolt_mac/CMakeCache.txt \
    | tee build/jolt_apple_results/primary-options.txt

cmake --build build/jolt_mac --config Profile --target help \
    | grep -E 'Jolt|Editor|AssetProcessor' \
    | tee build/jolt_apple_results/jolt-targets.txt
```

The configure must fetch or reuse Jolt through the Gem's private content path. It must not discover a system Jolt installation. Inspect the
generated dependency graph if a host package manager has installed Jolt.

## Compile and test

Build both Profile and Release in the primary tree, then compile the no-unity, double-precision, and monolithic permutations. Building
`Jolt.Tests` also compiles every public header in an isolated, non-unity translation unit.

```bash
cmake --build build/jolt_mac --config Profile \
    --target Jolt.Tests Editor AssetProcessor \
    --parallel "$jobs"

cmake --build build/jolt_mac --config Release \
    --target Jolt.Tests Editor AssetProcessor \
    --parallel "$jobs"

cmake --build build/jolt_mac_no_unity --config Profile \
    --target Jolt.Tests \
    --parallel "$jobs"

cmake --build build/jolt_mac_double --config Profile \
    --target Jolt.Tests \
    --parallel "$jobs"

cmake --build build/jolt_mac_mono --config Profile \
    --target Jolt.Tests AutomatedTesting.GameLauncher \
    --parallel "$jobs"
```

If a generated target name differs on the current branch, use `cmake --build <tree> --target help` and record the resolved target; do not
silently skip it. The host-tools `Jolt.Tests` target contains the runtime, editor, and builder tests. Run it in every applicable configuration:

```bash
for configuration in Profile Release; do
    ctest --test-dir build/jolt_mac --build-config "$configuration" \
        -R '^Gem::Jolt\.Tests\.main::TEST_RUN$' \
        --output-on-failure
done

ctest --test-dir build/jolt_mac_no_unity --build-config Profile \
    -R '^Gem::Jolt\.Tests\.main::TEST_RUN$' \
    --output-on-failure

ctest --test-dir build/jolt_mac_double --build-config Profile \
    -R '^Gem::Jolt\.Tests\.main::TEST_RUN$' \
    --output-on-failure
```

Record test counts, disabled tests, failures, and durations. Public-header isolation is not complete if unity accidentally absorbs a
header translation unit; inspect `Code/jolt_tests_files.cmake` and the no-unity compile commands when diagnosing a false pass.

## Assets, Editor, and AutomatedTesting

Build assets through the current `*.jolt.json` builder. Both scene and skeleton sources emit typed `.jolt` products; the product catalog
type, not the extension, identifies the runtime asset.

```bash
build/jolt_mac/bin/profile/AssetProcessorBatch \
    --project-path="$PWD/AutomatedTesting" \
    --platforms=mac

ctest --test-dir build/jolt_mac --build-config Profile \
    -R '^AutomatedTesting::JoltTests_Main\.main::TEST_RUN$' \
    --output-on-failure

ctest --test-dir build/jolt_mac --build-config Profile \
    -R '^AutomatedTesting::JoltTests_Benchmark\.benchmark::TEST_RUN$' \
    --output-on-failure
```

If the application bundle layout moves `AssetProcessorBatch`, locate the executable under `build/jolt_mac/bin/profile` and record the
resolved path. Inspect the Asset Processor database/logs and prove that `test_scene.jolt.json` and `test_skeleton.jolt.json` each produce a
non-empty `.jolt` product with the correct catalog asset type and no native header product.

The current main suite has six coarse null-renderer scenarios. As Phase 4 lands, run every focused `Jolt_*` scenario, the saved feature
gallery, the 600/3,600/36,000-tick stress modes appropriate to the qualification tier, save/reload, prefab instantiation, play, and clean
shutdown. Record each scenario independently; one passing editor launch does not qualify hidden assertions.

## Determinism

The named runtime tests internally compare 1/4/8 workers. Run them repeatedly in both precision modes and preserve every digest:

```bash
runner=$(find build/jolt_mac/bin/profile -type f -name AzTestRunner -print -quit)
module=$(find build/jolt_mac/bin/profile -type f \
    \( -name 'libJolt.Tests.Gem.dylib' -o -name 'Jolt.Tests.Gem.dylib' \) -print -quit)

for run in $(seq 1 100); do
    "$runner" "$module" AzRunUnitTests \
        --gtest_filter='SimulationTests.StateDigestIsDeterministicAcrossWorkerCounts:SimulationTests.HairCpuComputeIsExactAcrossWorkerCountsAndRestoresWorkerFloatEnvironment:SimulationTests.SoftBodySkinningAppliesBlendedInverseBindsAndRestoresExactly'
done
```

Repeat against `build/jolt_mac_double`. As additional deterministic tests land, include rollback/replay, custom shapes, transformed pair
queries, constraints, vehicles, ragdolls, soft bodies, events, and the stress digest. Poison caller and worker floating-point state where
the focused tests provide that mode. A failure report must include the seed, worker count, first divergent frame, expected/actual digest,
floating-point control state, precision, SIMD configuration, and whether the run used Rosetta.

## Sanitizers and static analysis

Use a dedicated no-unity AddressSanitizer tree. Do not reuse it for timing:

```bash
cmake --preset mac-ninja-no-unity -S . -B build/jolt_mac_asan \
    -DLY_PROJECTS=AutomatedTesting \
    -DLY_BUILD_WITH_ADDRESS_SANITIZER=ON

cmake --build build/jolt_mac_asan --config Profile \
    --target Jolt.Tests \
    --parallel "$jobs"

ctest --test-dir build/jolt_mac_asan --build-config Profile \
    -R '^Gem::Jolt\.Tests\.main::TEST_RUN$' \
    --output-on-failure
```

Run available AppleClang static analysis from an Xcode tree and record unavailable checks:

```bash
cmake --preset mac-default -S . -B build/jolt_mac_xcode \
    -DLY_PROJECTS=AutomatedTesting

xcodebuild -list -project build/jolt_mac_xcode/O3DE.xcodeproj
xcodebuild -project build/jolt_mac_xcode/O3DE.xcodeproj \
    -scheme Jolt.Tests -configuration Profile analyze \
    CODE_SIGNING_ALLOWED=NO
```

Also run the Phase-6 IWYU/header-isolation, reflection parity, native-leakage, serialization fuzz/truncation, manifest, and unrelated-diff
checks when those entry-point modes exist. If UBSan is supported by the current engine/AppleClang combination, use a separate tree and
record the exact flags; do not claim it from ASan coverage.

## Build-option matrix

Qualify each supported option in its own build tree. Phase 2 adds the broadphase and narrowphase statistics options; until then, record
them as unavailable rather than passing them implicitly.

| Option | Required configurations |
|---|---|
| `LY_JOLT_DOUBLE_PRECISION` | `OFF` and `ON` |
| `LY_JOLT_ENABLE_DEBUG_RENDERING` | `OFF` and `ON` |
| `LY_JOLT_ENABLE_DETAILED_PROFILING` | `OFF` and `ON` |
| `LY_JOLT_ENABLE_SIMULATION_STATISTICS` | `OFF` and `ON` |
| `LY_JOLT_ENABLE_BROADPHASE_STATISTICS` | `OFF` and `ON` after Phase 2 |
| `LY_JOLT_ENABLE_NARROWPHASE_STATISTICS` | `OFF` and `ON` after Phase 2 |
| Unity | `mac-ninja` and `mac-ninja-no-unity` |
| Linkage | modular and `LY_MONOLITHIC_GAME=ON` |
| Configuration | Debug compile smoke, Profile qualification, Release timing |
| Architecture | native arm64; x86_64 only on actual Intel hardware or as a separately labeled Rosetta compile/run experiment |

For Apple Silicon, verify the effective private SIMD configuration in `RuntimeInfo` and native compile commands. `JoltNative.cmake`
uses the engine's fixed platform baseline: SSE2 on x86, NEON on ARM, and SIMD128 on WebAssembly.

## Private FetchContent and installed-engine proof

Use an empty dedicated tree for the clean-fetch run. Preserve the fetched revision, patch hash, configure log, and patched-source diff.
Reconfigure the same tree twice, then refresh from a pristine cached archive and prove that the patch is idempotent.

```bash
cmake --preset mac-ninja -S . -B build/jolt_mac_clean_fetch \
    -DLY_PROJECTS=AutomatedTesting

cmake --preset mac-ninja -S . -B build/jolt_mac_clean_fetch \
    -DLY_PROJECTS=AutomatedTesting

cmake --build build/jolt_mac_clean_fetch --config Profile \
    --target Jolt.Tests \
    --parallel "$jobs"
```

For the installed engine, use a separate prefix and build both modular and monolithic installs. The exact public-only consumer is a Phase-6
deliverable; once present, configure it only against the installed engine and fail if it can include native Jolt headers.

```bash
cmake --preset mac-ninja -S . -B build/jolt_mac_install \
    -DLY_PROJECTS=AutomatedTesting \
    -DCMAKE_INSTALL_PREFIX="$PWD/build/jolt_install"

cmake --build build/jolt_mac_install --config Profile \
    --target install \
    --parallel "$jobs"

find build/jolt_install -type f \
    \( -path '*/Jolt/Jolt.h' -o -path '*/Jolt/Physics/*' \) \
    -print
```

The final `find` must print nothing. Repeat with `LY_MONOLITHIC_GAME=ON`. Then configure and run the checked-in installed consumer when it
exists, including one world, body, shape, step, query, and shutdown through public AZ-facing headers only. Do not add an installed native
lookup module to make the consumer pass.

## Performance and memory qualification

Only Release results are timing evidence. Use a power-connected, idle machine with Low Power Mode disabled. Let the machine reach a stable
thermal state before each provider, record `pmset -g therm` before and after, and stop when throttling or background work invalidates the
comparison. Use the same native architecture, compiler, configuration, CPU scheduling policy, worker count, workload signature,
iterations, and repetitions for every provider. macOS has no supported general-purpose affinity contract comparable to the Windows gate;
record that fact and never imply fixed affinity.

Run at least 30 repetitions and retain raw JSON, not aggregates alone:

```bash
mkdir -p build/jolt_apple_results/benchmarks

runner=$(find build/jolt_mac/bin/release -type f -name AzTestRunner -print -quit)
module=$(find build/jolt_mac/bin/release -type f \
    \( -name 'libJolt.Tests.Gem.dylib' -o -name 'Jolt.Tests.Gem.dylib' \) -print -quit)

"$runner" "$module" AzRunBenchmarks \
    --benchmark_min_time=0.5 \
    --benchmark_repetitions=30 \
    --benchmark_report_aggregates_only=false \
    --benchmark_out=build/jolt_apple_results/benchmarks/Jolt.Raw30.json \
    --benchmark_out_format=json
```

Report median, p95/p99 where raw per-frame samples exist, bootstrap ratio bounds, and coefficient of variation. A valid gated workload has
CV at most 5%. Keep 1-, 4-, and 8-worker results separate; never average them together. Run matched Jolt/Box3D/PhysX workloads only when
all three providers compile on the same machine, and pass their raw files through `Code/Tests/compare_provider_benchmarks.py` with the
unchanged median 1.0, bootstrap 1.05, tail 1.10, 30-repetition, and 5% CV gates.
Do not poll the benchmark process or run other local commands during timed capture; inspect the completed artifact afterward.

Run Jolt-specific absolute workloads for constraints, characters, vehicles, ragdolls, soft bodies, CPU Hair, scenes, custom providers,
events, sensors, CCD, sleep/wake, broadphase rebuild/origin shift, rollback, assets, topology churn, and 1/4/8-worker scaling as they land.
Measure persistent bytes, temporary peaks, allocation counts, snapshot reuse, cooking, scene instantiation, event/query delivery,
destruction/slot reuse, and no-growth steady state. Profile or Instruments traces may attribute time and allocation sites, but they do not
replace Release timing.

Inspect selected hot functions without exact-opcode pass/fail rules:

```bash
binary=$(find build/jolt_mac/bin/release -type f \
    \( -name 'libJolt*.dylib' -o -name 'Jolt.Tests.Gem.dylib' \) -print -quit)

xcrun nm -nm "$binary" \
    | c++filt \
    | grep -E 'Jolt::(World::Step|World::RaycastClosest|World::Overlap|World::CaptureState)' \
    | tee build/jolt_apple_results/hot-symbols.txt

xcrun llvm-objdump --demangle --disassemble "$binary" \
    > build/jolt_apple_results/jolt-release-disassembly.txt
```

For each selected function, report stack-frame size, out-of-line calls, branch count, scalar/vector conversions, SIMD/vectorization, large
copies, and inlining boundaries. Compare before/after assembly and benchmark data for every performance-sensitive fix.

## Results template

Copy this section into the final Apple report and fill every field:

```text
Revision:
Dirty-diff hash and summary:
macOS / Xcode / AppleClang / SDK:
Mac model / architecture / CPU / memory:
Power, Low Power Mode, cooling, thermal observations:
CMake generator, configuration, unity, linkage, precision, SIMD, diagnostic options:

Configure and build commands:
Build results and warnings:
Runtime tests: passed / failed / disabled / duration:
Editor tests: passed / failed / disabled / duration:
Builder and asset tests: passed / failed / disabled / duration:
AutomatedTesting scenarios and assertion counts:
Feature-gallery result:
Stress mode, ticks, seed, workers, peak memory, final resource counts:

Determinism seeds and 1/4/8-worker hashes:
Snapshot/replay first-divergence data, if any:
ASan / UBSan / static-analysis results:
Public-header / IWYU / native-leakage / reflection results:
FetchContent pristine, repeat-configure, cache-refresh results:
Installed modular / monolithic / public-consumer results:

Release benchmark artifact paths:
Workload | workers | samples | median | p95 | p99 | CV | comparison ratio | gate
Allocation and memory artifact paths:
Resource/workload | persistent bytes | temporary peak | allocations | steady-state growth
Assembly report paths and findings:

Source changes made and focused proof:
FEATURE_COVERAGE.md rows changed:
Unavailable tools or hardware:
Remaining macOS limitations:
Separately reported iOS limitations:
```

Finish by recording `git status --short`, the complete final diff, test logs, JSON/JUnit results, raw benchmark files, environment metadata,
and every unavailable gate. macOS CPU qualification does not imply iOS, Metal, Android, WebAssembly, Intel, Rosetta, or other-device
qualification.
