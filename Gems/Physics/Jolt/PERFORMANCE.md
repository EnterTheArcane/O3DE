# Jolt performance qualification

Release microbenchmarks are the timing authority. Profile captures attribute time through the engine and native profiling scopes. A
matched result is accepted only after the comparator verifies workload identity, worker count, notification policy, sleep and continuous
collision policy, query cardinality, simulation quality, repetition count, and stability.

## Current Windows baseline

The following medians were captured on 2026-08-16 in isolated Release processes with 30 repetitions per provider. Every workload used the
same provider policy and reported the expected resource and result counts. Ratios below 1.0 favor Jolt.

| Workload | Jolt | Box3D | PhysX | Jolt/Box3D | Jolt/PhysX |
|---|---:|---:|---:|---:|---:|
| Step 128 bodies, 1 worker | 33.90 us | 93.05 us | 118.29 us | 0.364 | 0.287 |
| Step 128 bodies, 4 workers | 32.98 us | 72.44 us | 160.19 us | 0.455 | 0.206 |
| Step 128 bodies, 8 workers | 43.32 us | 88.98 us | 182.85 us | 0.487 | 0.237 |
| Step 1024 bodies, 1 worker | 268.10 us | 749.00 us | 935.47 us | 0.358 | 0.287 |
| Step 1024 bodies, 4 workers | 171.87 us | 258.99 us | 541.85 us | 0.664 | 0.317 |
| Step 1024 bodies, 8 workers | 152.09 us | 191.51 us | 443.51 us | 0.794 | 0.343 |
| Create and destroy 128 bodies | 63.52 us | 59.35 us | 475.24 us | 1.070 | 0.134 |
| Create and destroy 1024 bodies | 510.97 us | 616.58 us | 3767.26 us | 0.829 | 0.136 |
| 128 closest raycasts, scalar | 16.04 us | 19.26 us | 21.92 us | 0.833 | 0.732 |
| 128 closest raycasts, batch | 14.32 us | 17.24 us | 25.73 us | 0.831 | 0.556 |
| 1024 closest raycasts, batch | 38.95 us | 45.31 us | 257.64 us | 0.860 | 0.151 |
| Sphere overlap, 25 stable hits | 0.62 us | 0.66 us | 1.05 us | 0.951 | 0.596 |

Jolt is faster than PhysX for every workload. Its samples pass the workload, result, median, bootstrap, repetition-tail, and 5% CV gates.
The complete three-provider comparison is not accepted as a qualification artifact because the unchanged comparator correctly rejects
six PhysX series whose CV exceeds 5% on this host. The affected PhysX CVs range from 5.105% to 8.555%. No samples were trimmed and no
threshold was relaxed. Jolt is also faster than Box3D in every measured workload except the 128-body lifecycle case, which is 1.070x;
PhysX remains the configured provider gate for this comparison.

The retained reports are:

- `build/windows_jolt_clangcl_ninja/BenchmarkResults/Jolt.Phase5.Matched.Final.Raw30.json`
- `build/windows_jolt_clangcl_ninja/BenchmarkResults/Box3D.Phase5.Matched.Final.Raw30.json`
- `build/windows_jolt_clangcl_ninja/BenchmarkResults/PhysX.Phase5.Matched.Final2.Raw30.json`

The corresponding `.Qualified.json` files contain the source, dirty-diff, binary, compiler, configuration, policy, and workload
fingerprints consumed by the fail-closed comparator.

## MSVC and WSL Clang checkpoint

The final Phase 5 source was also built with MSVC 19.44.35228 and Clang 20.1.8 under Ubuntu WSL2. Both Release trees use Ninja through
`cmake --build --parallel 24`, compile the public-header isolation sources, and pass the consolidated 227-test runtime, editor, builder,
and cross-module suite. The WSL build lives on the distribution's ext4 filesystem because FetchContent directory replacement and object
creation on `/mnt/d` are prohibitively slow and unreliable.

The following table compares each final 30-repetition matched Jolt capture with the Windows Clang 22.1.8 baseline. Times are medians;
ratios above 1.0 are slower than Windows Clang.

| Workload | Windows Clang | MSVC | MSVC/Clang | WSL Clang | WSL/Windows Clang |
|---|---:|---:|---:|---:|---:|
| Step 128 bodies, 1 worker | 33.90 us | 43.52 us | 1.284 | 34.78 us | 1.026 |
| Step 128 bodies, 4 workers | 32.98 us | 42.26 us | 1.281 | 68.57 us | 2.079 |
| Step 128 bodies, 8 workers | 43.32 us | 61.11 us | 1.411 | 138.83 us | 3.205 |
| Step 1,024 bodies, 1 worker | 268.10 us | 429.45 us | 1.602 | 282.86 us | 1.055 |
| Step 1,024 bodies, 4 workers | 171.87 us | 224.54 us | 1.306 | 272.36 us | 1.585 |
| Step 1,024 bodies, 8 workers | 152.09 us | 193.18 us | 1.270 | 359.33 us | 2.363 |
| Create and destroy 128 bodies | 63.52 us | 92.34 us | 1.454 | 64.44 us | 1.015 |
| Create and destroy 1,024 bodies | 510.97 us | 750.49 us | 1.469 | 520.60 us | 1.019 |
| 128 closest raycasts, scalar | 16.04 us | 28.13 us | 1.753 | 18.77 us | 1.170 |
| 128 closest raycasts, batch | 14.32 us | 25.24 us | 1.763 | 16.46 us | 1.150 |
| 1,024 closest raycasts, batch | 38.95 us | 65.07 us | 1.671 | 108.18 us | 2.778 |
| Sphere overlap, 25 stable hits | 0.62 us | 1.22 us | 1.953 | 0.65 us | 1.037 |

Across the 61 common Jolt-specific absolute workloads, the median MSVC/Windows-Clang ratio is 1.416. MSVC is faster in 5 workloads and
slower in 56. The largest repeatable regressions are virtual-character update at 2.984x and CPU Hair update at 1.709x to 1.962x; retained
shape-pair casts are 0.799x and 0.823x. Forty-two MSVC series exceed 5% CV, so this cross-compiler run is diagnostic and does not replace
the stable Windows Clang timing authority. The canonical deterministic-float scope is 11.97 ns under MSVC and the complete uncontended
query-lock path is 15.86 ns, compared with 9.22 ns and approximately 11.3 ns under Windows Clang.

The WSL single-worker matched results are within 1.5% to 17.0% of native Windows Clang, but its multiworker results are 1.585x to 3.205x
and noisy. Across the same 61 absolute workloads its median ratio is 1.039 and 22 workloads are faster, but 47 series exceed 5% CV.
Linux affinity support constrains the benchmark caller and AZ workers to eight distinct physical-core siblings, yet WSL2 scheduling and
virtualization remain part of every sample. These results validate the Linux code paths and expose scheduling behavior; they are not a
native-Linux qualification or a Windows compiler comparison. The Linux deterministic-float scope is 18.8 ns and the query-lock path is
22.5 ns. The additional x87 status handling is required to clear native exception state while restoring the caller's exact environment.

The retained final reports are:

- `build/windows_jolt_phase5_msvc_v2/BenchmarkResults/Jolt.MSVC.Phase5.Matched.Final.Raw30.json`
- `build/windows_jolt_phase5_msvc_v2/BenchmarkResults/Jolt.MSVC.Phase5.Absolute.Final.Raw30.json`
- `build/windows_jolt_phase5_msvc_v2/BenchmarkResults/Jolt.MSVC.Phase5.FloatAndLock.Final.Raw30.json`
- `build/windows_jolt_phase5_msvc_v2/BenchmarkResults/WSL/Jolt.WSL.Clang.Phase5.Matched.PinnedAffinity8.Raw30.json`
- `build/windows_jolt_phase5_msvc_v2/BenchmarkResults/WSL/Jolt.WSL.Clang.Phase5.Absolute.PinnedAffinity8.Raw30.json`
- `build/windows_jolt_phase5_msvc_v2/BenchmarkResults/WSL/Jolt.WSL.Clang.Phase5.FloatAndLock.PinnedAffinity8.Raw30.json`

Each report has a corresponding qualified artifact with the final binary hash and dirty-source fingerprint. The MSVC module uses LTCG,
so `llvm-objdump` cannot inspect its intermediate whole-program IR as a normal COFF object. The retained Clang 22 reports remain the
instruction-level evidence; adding an MSVC linker/PDB disassembly route is a diagnostic Phase 6 task rather than an opcode gate.

## Automatic multi-world scaling

Independent single-worker worlds are stepped concurrently through the engine job system. The scheduler keeps one world on the calling
thread and assigns the remaining worlds to background jobs. All dependents are attached before any job starts, and result aggregation uses
relaxed atomics instead of per-world result storage. Worlds with native worker counts above one retain serial outer scheduling to avoid
nested use of the same worker pool. Debug-render and debug-capture worlds also retain serial ordering.

A 30-repetition high-priority Release benchmark stepped 1,024 active bodies per world with one native worker per world. Every run reported
valid body counts and no update failures.

| Worlds | Total bodies | Median | Real-time CV | Throughput relative to one world |
|---:|---:|---:|---:|---:|
| 1 | 1,024 | 835 us | 1.35% | 1.00x |
| 2 | 2,048 | 851 us | 1.13% | 1.96x |
| 4 | 4,096 | 881 us | 1.34% | 3.79x |

The original serial path was measured separately with 128 bodies per world and scaled from 44.6 us for one world to 89.0 us for two and
178 us for four, making aggregate work almost exactly linear. The current 1,024-body results preserve single-world latency while scaling
independent-world throughput. A focused unit test blocks two worlds inside native step listeners and proves they overlap before either is
allowed to complete. Profile disassembly also confirmed that replacing fixed per-world result arrays with atomic aggregation reduced the
scheduler stack frame from `0x6f0` to `0x2b0`.

The retained raw reports are:

- `build/windows/BenchmarkResults/Jolt.AutomaticWorlds.Raw30.json`
- `build/windows/BenchmarkResults/Jolt.AutomaticWorlds.1024.Atomic.HighPriority.Raw30.json`

## Concurrent world queries

Geometric queries use a shared topology gate, while structural mutations use its writer-preferred exclusive path. Simulation retains the
operation lock but releases topology ownership after publishing an update-in-progress flag and draining earlier readers. Queries that
start during the update use Jolt's broad-phase and body read locks; otherwise the scalar closest-ray path retains its no-lock traversal.
This keeps native body and shape storage stable without serializing independent readers or preventing supported query/update overlap.

The Release assembly report confirms that the forced-inline closest-ray core reads the update flag and branches directly to the existing
no-lock native call when idle. A retained closest-ray collector reuses the precomputed native ray and inverse direction instead of
rebuilding them for every leaf. The final matched artifact measures the complete public path at 16.04 us for 128 scalar rays. Unit coverage
blocks two filters simultaneously, verifies canonical and restored per-thread floating-point state, proves a topology writer waits for an
active query, and completes a broad-phase query while simulation is paused inside a native step listener.

## Deterministic floating-point scope

The canonical fast path now compares only floating-point control state. Sticky exception-status flags do not make the environment
noncanonical: entry clears them directly and exit restores the caller's complete state. Noncanonical rounding, mask, denormal, or
precision state still uses the full save-and-restore fallback. The canonical scope benchmark improved from 101 ns to 9.22 ns, and the
complete uncontended deterministic query-lock path improved from approximately 105 ns to 11.3 ns. The scalar 128-ray workload improved
from 28.82 us before this optimization sequence to 16.04 us in the final matched artifact.

`NativeRuntimeTests.DeterministicFloatScopeRestoresExceptionFlags` poisons the caller status flags and verifies both the canonical native
environment and exact caller restoration. Query and job tests independently poison worker-thread state so the guarantee is not limited to
the calling thread.

## Cooked soft-body loading

The scene pipeline now stores optimized native soft-body definitions instead of rebuilding face constraints, rest constants, skin normals,
and solver ordering during every runtime load. A 30-repetition Release benchmark used the same 32 by 32 cloth definition for both paths:
1,024 vertices, 1,922 faces, and 1,984 authored edges.

| Path | Median | CV | Relative time |
|---|---:|---:|---:|
| Build and optimize authored definition | 1298 us | 3.67% | 63.9x |
| Import 153.852 KB cooked archive | 20.3 us | 3.36% | 1.0x |

The import path validates the archive format, native build fingerprint, material count, content hash, and exact stream consumption before
publishing a definition handle. This benchmark measures definition creation and destruction only; scene material creation and body
instantiation are outside both timed loops.

## Ragdoll simulation membership

Disabling a ragdoll now removes its native bodies and constraints from simulation without destroying or rebuilding them. The provider
preserves velocities across Jolt's removal-time deactivation and reuses a lazily allocated motion-state scratch buffer on later cycles.
A 30-repetition Release benchmark measured the same 64-part articulated ragdoll in both paths.

| Path | Median | CV | Relative time |
|---|---:|---:|---:|
| Remove and re-add existing ragdoll | 8.11 us | 0.66% | 1.0x |
| Destroy and recreate native ragdoll | 27.4 us | 4.77% | 3.38x |

The membership path preserves the ragdoll, body, and constraint handles, pose, and linear/angular velocities. The benchmark warms one
membership cycle before timing so the retained path measures its allocation-free steady state. Both loops perform one removal and one
addition per iteration with 1 worker and the same definition, shapes, constraints, and activation policy.

## Filtered rollback recapture

Filtered body-and-constraint snapshots pass the provider's already-known selected-constraint count into Jolt's state recorder. Jolt
therefore serializes selected constraints directly instead of allocating, filling, walking, and freeing a second pointer array. Unknown
third-party filters retain the original allocation-backed path. Contact snapshots reuse deterministic body-pair and manifold ordering
buffers owned by the native contact cache, so a prewarmed contacts-only recapture adds no native allocations. Body filtering sorts a
preallocated pointer workspace owned by the recorder. This preserves the one-pass body traversal without allocating or freeing native
scratch storage after warmup.

The retained 30-repetition Release benchmark captures all bodies and point constraints in one filtered partition after warming both
alternating provider snapshot buffers.

| Partition | Median | Native allocations | Native frees | Native reallocations |
|---|---:|---:|---:|---:|
| 128 bodies, 127 constraints | 17.84 us | 0 | 0 | 0 |
| 1,024 bodies, 1,023 constraints | 152.63 us | 0 | 0 | 0 |

The final artifact is `Jolt.Phase5.Rollback.NoGrowth.Final.Raw30.json`. The exact-count branch writes the count and walks the constraint
array directly, while the provider-owned recorder retains the sorted body workspace. The focused test checks 32 consecutive captures and
fails on any native allocation, free, or reallocation after warmup.
`PrewarmedContactStateRecaptureDoesNotAllocateNativeScratch` separately checks 32 contacts-only captures after warming the reusable sort
buffers. Jolt's Release object is whole-program IR before the final link, so allocator absence is enforced by these hooked native allocator
tests rather than inferred from an unlinked object disassembly.

## Rollback restore safety

Restore defaults to transactional safety: the provider captures the affected state into a reusable scratch snapshot immediately before
mutation, restores the requested snapshot, and recovers the pre-operation state if any native, character, soft-body, or hair restore fails.
`RestoreSafety::Validated` is the explicit lower-latency policy for internally owned rollback snapshots after topology and callback-state
validation. It avoids the recovery capture while retaining all pre-mutation validation and exact stream-consumption checks.

The same 30-repetition Release process measured filtered body-and-constraint restores after warming snapshot storage. All runs used one
worker and reported `QualityValid=1`.

| Policy | 128 bodies, 127 constraints | CV | 1,024 bodies, 1,023 constraints | CV |
|---|---:|---:|---:|---:|
| Transactional | 35.7 us | 0.49% | 363 us | 1.95% |
| Validated | 14.6 us | 0.64% | 122 us | 2.07% |

At 128 bodies the recovery capture accounts for more than half of transactional restore time. At 1,024 bodies validated mode removes the
recovery capture and reduces the median by 66.4%. The safety policy is captured with the snapshot, reflected for scripts, and required to
match across every part of a multipart restore.

## Authored Profile capture

`AutomatedTesting::JoltTests_Benchmark` creates an 8 by 8 by 4 stack, waits for 120 simulation ticks, keeps every body active, and captures
30 CPU frame samples through the engine profiling capture bus. The current Windows Profile run kept all 256 bodies active, reported no
physics update errors, and measured 3.8841 ms median whole-Editor frame time. The minimum, mean, and maximum were 2.3750 ms, 3.7406 ms, and
5.6622 ms. Rendering and Editor overhead are deliberately included, so this capture is diagnostic rather than the provider timing gate.

The capture must run in Profile. Release intentionally excludes `StatisticalProfilerProxySystemComponent`, so the engine capture API writes
zero CPU frame times in that configuration. The test rejects those zeroes instead of reporting unsupported measurements as evidence.

## Opt-in telemetry

`ISystem::ConfigurePerformanceStatistics` enables only the requested counter groups for a world. Disabled query, event, snapshot, job,
lock, Hair, and simulation counters do not read clocks. Lock timing uses the deterministic world mutex's contended path, job timing is
captured by the AZ job adapter, and native allocation counters are process-wide because Jolt's allocator hooks are process-wide. Resource
counts and retained capacities are read into a caller-owned `WorldPerformanceStatistics`; the read itself does not allocate. Resetting a
snapshot clears interval counters and high-water marks without releasing retained storage. Resetting native allocation counters from one
world begins a new process-wide memory interval for every world that has memory statistics enabled.

The resource report covers bodies, shapes, constraints, characters, vehicles, ragdolls, soft bodies, CPU Hair, scenes, body snapshots,
and world snapshots. Wrapper-retained bytes also include query workspaces, snapshot scratch, event queues, lookup structures, CPU Hair
joint storage, step listeners, and bounded debug capture. Vector capacities are exact; unordered-map nodes are a documented structural
estimate because the standard container does not expose allocator-retained bytes. Native current/peak bytes,
allocation/free/reallocation counts, and temporary allocator current/capacity/peak bytes remain separate so wrapper and native ownership
are not conflated.

`SimulationTests.PerformanceStatisticsAreOptInResettableAndAllocationFreeToRead` verifies opt-in/reset behavior and repeated caller-buffer
reads. `SimulationTests.SnapshotRecaptureReusesRetainedStorage` proves stable wrapper capacity after warmup. The lifecycle and rollback
benchmarks publish native traffic, temporary peaks, snapshot bytes, and wrapper-retained bytes as counters on every result.

## Phase 5 absolute workload matrix

The Jolt-specific benchmark suite includes:

- every primary constraint solver family plus point-constraint 1/4/8-worker scaling;
- physical and virtual characters, all three vehicle controllers, ragdolls, soft-body simulation, skeleton mapping, and CPU Hair
  update/readback;
- general custom shapes loaded through `Jolt.TestProviders` across a real module boundary;
- retained shape-pair collision/casts plus matched scalar, batch, broadphase, overlap, and count-only queries;
- contact/event-heavy, sensor, CCD, and sleep/wake policies;
- heightfield updates, broadphase optimization, scene instantiation/destruction, lifecycle churn, and filtered snapshot capture/restore;
- statistics read overhead and native/wrapper allocation counters.

Profile smoke runs establish correctness and counter availability only. Qualification timings come from Release with 30 raw repetitions.
The worker count, affinity state, resource count, update errors, and workload-specific quality fields are emitted beside each timing so a
fast but invalid workload cannot pass.

## Benchmark artifact integrity

`prepare_benchmark_artifact.py` refuses a non-Release report, a report older than its binary, or a missing binary. It adds SHA-256 hashes
for the benchmark binary and workload definition, source revision and dirty-diff hash, compiler/configuration, affinity policy, filter,
minimum time, repetition count, and raw-sample policy. The comparator rejects stale signatures, duplicate binary hashes, differing
revisions or dirty states, mismatched compiler/build/affinity policies, duplicate or missing repetition indices, non-finite samples, and
the existing median, bootstrap, repetition-tail, and CV gates.

`inspect_hot_assembly.py` runs LLVM objdump over selected functions and emits JSON plus Markdown counts for stack frames, instructions,
out-of-line calls, conditional branches, conversions, vector instructions, and likely copies. These are diagnostic heuristics, not exact
opcode gates. The report is intended to focus review on unexpected code generation without making compiler-version changes fail CI.

The final LLVM 22.1.8 Release reports are in `build/windows_jolt_phase5_clangcl/BenchmarkResults`. Representative results are:

| Function | Stack bytes | Instructions | Calls | Conditional branches | Vector instructions |
|---|---:|---:|---:|---:|---:|
| `World::RaycastClosestDefaultCoreUnlocked` | 360 | 136 | 4 | 7 | 26 |
| `World::RaycastClosestDefaultUnlocked` | 0 | 2 | 0 | 0 | 0 |
| `World::RaycastClosestFilteredUnlocked` | 248 | 129 | 3 | 7 | 2 |
| `World::StepDetailed` | 88 | 67 | 7 | 6 | 8 |
| `JobSystem::QueueJob` | 56 | 20 | 2 | 1 | 0 |
| `FloatEnvironment::Enter` | 56 | 40 | 4 | 3 | 2 |
| `FloatEnvironment::Leave` | 56 | 33 | 2 | 4 | 2 |

The floating-point counts include the cold noncanonical fallback calls; the 9.22 ns benchmark measures the common canonical branch.
`World::GetPerformanceStatistics` is intentionally a cold, opt-in read path and remains the largest inspected function at a 1,032-byte
frame. Its repeated-read unit test proves that this reporting complexity does not allocate.

## Allocator alignment validation

ASan and LLDB isolated an intermittent `vmovaps` fault in AzCore's HPHA placement buffer: the public byte buffer was 16-byte aligned while
the hidden allocator implementation contains 64-byte-aligned bucket state. The buffer is now 128-byte aligned, and a compile-time check
prevents the implementation alignment from exceeding its storage alignment. The repaired Release binary passed the full 227-test Jolt
suite and 120 consecutive stress-process launches. This is an engine allocator correctness fix exposed by the Phase 5 no-growth stress,
not a Jolt-specific alignment workaround.

## Reproduction

```powershell
$buildDirectory = 'build/windows_jolt_phase5_clangcl'
cmake --build $buildDirectory --config Release --target Jolt.Tests Jolt.TestProviders --parallel 24

ctest --test-dir $buildDirectory -C Release -R 'Gem::Jolt\.Tests\.main' --output-on-failure

$runner = Resolve-Path "$buildDirectory/bin/Release/AzTestRunner.exe"
$joltTests = Resolve-Path "$buildDirectory/bin/Release/Jolt.Tests.Gem.dll"
$benchmarkResults = New-Item -ItemType Directory -Force "$buildDirectory/BenchmarkResults"
$filter = 'Jolt/(Step/FallingBoxes|Lifecycle/CreateDestroyBodies|Query/RaycastGrid|Query/RaycastClosestBatchGrid/1024/(128/4|1024/4)|Query/OverlapSphereGrid)'

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $runner
$startInfo.UseShellExecute = $false
$startInfo.WorkingDirectory = Split-Path $runner
$startInfo.ArgumentList.Add($joltTests)
$startInfo.ArgumentList.Add('AzRunBenchmarks')
$startInfo.ArgumentList.Add("--benchmark_filter=$filter")
$startInfo.ArgumentList.Add('--benchmark_min_time=0.05')
$startInfo.ArgumentList.Add('--benchmark_repetitions=30')
$startInfo.ArgumentList.Add('--benchmark_report_aggregates_only=false')
$startInfo.ArgumentList.Add("--benchmark_out=$benchmarkResults/Jolt.Phase5.Matched.Final.Raw30.json")
$startInfo.ArgumentList.Add('--benchmark_out_format=json')
$process = [System.Diagnostics.Process]::Start($startInfo)
$process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High
$process.WaitForExit()

python Gems/Physics/Jolt/Code/Tests/prepare_benchmark_artifact.py `
    $benchmarkResults/Jolt.Phase5.Matched.Final.Raw30.json `
    $benchmarkResults/Jolt.Phase5.Matched.Final.Qualified.json `
    $joltTests `
    --source-root . `
    --provider Jolt `
    --compiler-id Clang `
    --compiler-version 22.1.8 `
    --build-configuration Release `
    --cpu-affinity-policy "Eight physical cores, high process priority" `
    --benchmark-filter $filter `
    --minimum-time 0.05 `
    --repetitions 30

& $runner $joltTests `
    AzRunBenchmarks `
    --benchmark_filter="Jolt/Rollback/RecaptureFilteredState" `
    --benchmark_min_time=0.05 `
    --benchmark_repetitions=30 `
    --benchmark_report_aggregates_only=false `
    --benchmark_out="$benchmarkResults/Jolt.Phase5.Rollback.NoGrowth.Final.Raw30.json" `
    --benchmark_out_format=json

python Gems/Physics/Jolt/Code/Tests/compare_provider_benchmarks.py `
    build/windows_jolt_clangcl_ninja/BenchmarkResults/Jolt.Phase5.Matched.Final.Qualified.json `
    build/windows_jolt_clangcl_ninja/BenchmarkResults/Box3D.Phase5.Matched.Final.Qualified.json `
    build/windows_jolt_clangcl_ninja/BenchmarkResults/PhysX.Phase5.Matched.Final2.Qualified.json `
    --gate-provider PhysX `
    --repetitions 30 `
    --maximum-median-ratio 1.0 `
    --maximum-bootstrap-ratio 1.05 `
    --maximum-repetition-tail-ratio 1.10 `
    --maximum-cv 0.05

$joltCodeBuild = Resolve-Path "$buildDirectory/External/Jolt-*/Code"
$worldBitcode = "$joltCodeBuild/CMakeFiles/Jolt.Private.Static.dir/release/Unity/unity_6_cxx.cxx.obj"
$floatAndJobsBitcode = "$joltCodeBuild/CMakeFiles/Jolt.Private.Static.dir/release/Unity/unity_2_cxx.cxx.obj"
& D:/LLVM/22.1.8/bin/clang.exe -c -x ir --target=x86_64-pc-windows-msvc -O3 -fno-lto `
    $worldBitcode -o "$benchmarkResults/Jolt.World.Assembly.obj"
& D:/LLVM/22.1.8/bin/clang.exe -c -x ir --target=x86_64-pc-windows-msvc -O3 -fno-lto `
    $floatAndJobsBitcode -o "$benchmarkResults/Jolt.FloatAndJobs.Assembly.obj"

python Gems/Physics/Jolt/Code/Tests/inspect_hot_assembly.py `
    $benchmarkResults/Jolt.World.Assembly.obj `
    --objdump D:/LLVM/22.1.8/bin/llvm-objdump.exe `
    --symbol "Jolt::World::RaycastClosest" `
    --symbol "Jolt::World::StepDetailed" `
    --symbol "Jolt::World::GetPerformanceStatistics" `
    --json-output $benchmarkResults/Jolt.Release.World.Assembly.json `
    --markdown-output $benchmarkResults/Jolt.Release.World.Assembly.md

python Gems/Physics/Jolt/Code/Tests/inspect_hot_assembly.py `
    $benchmarkResults/Jolt.FloatAndJobs.Assembly.obj `
    --objdump D:/LLVM/22.1.8/bin/llvm-objdump.exe `
    --symbol "Jolt::JobSystem::QueueJob" `
    --symbol "Jolt::FloatEnvironment::Enter" `
    --symbol "Jolt::FloatEnvironment::Leave" `
    --json-output $benchmarkResults/Jolt.Release.FloatAndJobs.Assembly.json `
    --markdown-output $benchmarkResults/Jolt.Release.FloatAndJobs.Assembly.md
```

The comparator fails closed when any required workload or counter is missing, signatures differ, the providers report different policies,
the workload misses its quality gate, or timing exceeds a configured threshold.

The current fresh LLVM Release target and the narrower Profile `Jolt.Module` and `Jolt.Editor` targets build normally through
`cmake --build`. Building the entire fresh Profile Editor host with LLVM 22.1.8 currently stops in the unrelated third-party Assimp
Windows-resource step because `llvm-rc` rejects its non-Unicode copyright byte. The Jolt diagnostics scenario was therefore run in the
already complete Profile host after replacing its freshly rebuilt AzCore and Jolt modules; all 21 independent checks passed. This is not
treated as a Jolt source failure or suppressed in third-party code.
