# Jolt performance qualification

Release microbenchmarks are the timing authority. Profile captures attribute time through the engine and native profiling scopes. A
matched result is accepted only after the comparator verifies workload identity, worker count, notification policy, sleep and continuous
collision policy, query cardinality, simulation quality, repetition count, and stability.

## Phase 7 benchmark policy

Matched throughput and frame-tail measurements are separate workloads. Throughput workloads execute 600 frames per repetition under
Google Benchmark's calibrated real-time measurement. Tail workloads execute exactly 4,096 consecutive frames per repetition and publish
every raw frame as `Frame{index}Ns`. Each provider runs 30 tail windows, yielding 122,880 raw frames per configuration without trimming.

The tail workload reports its within-window p50, p95, p99, and maximum. Its manual benchmark time is the p95 so the raw samples, summary
counters, and Google Benchmark result can be cross-checked. The comparator independently reconstructs every window p95, applies the 5%
CV gate to the 30 window p95 values, and applies the 1.10 tail-ratio gate to the pooled raw-frame p95. A percentile of per-repetition
averages is not accepted as frame-tail evidence.

Requested worker count means total execution participants. Every provider uses the caller plus `workerCount - 1` background workers. The
artifacts record both values and the comparator rejects an inconsistent topology. All timed steps, lifecycle mutations, raycasts,
batches, and overlaps contribute to explicit success counters; one valid final result cannot hide earlier failures. Jolt's timed
lifecycle path disables allocation instrumentation and reports it as a separate diagnostic workload.

Release captures use high process priority and exactly as many physical-core lanes as the requested worker count. A 10-second processor
utility and interrupt-rate sample on the Ryzen 9 7950X selected logical CPU 24 for one-worker captures; 17, 24, 26, and 29 for four-worker
captures; and 17, 19, 20, 23, 24, 26, 29, and 30 for eight-worker captures. Affinity is established before child-process creation so the
benchmark caller and every job worker inherit the intended topology. Each matched workload runs in its own fresh process so unrelated
allocator, cache, and power-state history cannot contaminate a later workload. Qualification records the policy, compiler,
configuration, source revision and complete dirty-source hash, runner, provider binary, runtime dependency hashes, workload signature,
repetition count, and minimum time. A report older than the runner, provider binary, runtime dependency, or current source state is
rejected.

### Windows desktop scheduling gate

Ordinary affinity is not exclusive ownership of a processor. Microsoft documents CPU Sets as soft affinity and reserves exclusive Core
Reservation assignment to system policy. The current host reports 32 CPU Sets with zero `Allocated` and zero
`AllocatedToTargetProcess`. Consequently, kernel interrupts, deferred procedure calls, and unrelated privileged work can still preempt a
benchmark on its selected processor. [CPU Sets](https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets)

The scheduler limitation was reproduced rather than inferred. An otherwise unchanged benchmark-only experiment elevated the timed caller and job
workers from `HIGH_PRIORITY_CLASS` with normal thread priority to `THREAD_PRIORITY_HIGHEST`. Every measured provider artifact confirmed
that the elevation took effect. Across 30 fresh processes and 122,880 retained frames per provider, the 1,024-body, one-worker p95-window
CV remained 9.23% for Box3D and 12.85% for PhysX. The policy was rejected because it did not remove the interruptions and base-priority-15
CPU-bound threads can interfere with normal system operation. No production or benchmark scheduling code retains the experiment.
[Scheduling Priorities](https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities)

That rejected capture is not qualification evidence. The final matched-provider runs isolate every workload in 30 fresh processes on
the measured quiet physical lanes and pass the 5% CV gate without trimming, replacing, or retrying samples. A later isolated recapture
also closes the four initially noisy capability and rollback series. Ordinary affinity still cannot prove the absence of privileged
preemption, so the exact lane policy and raw samples remain part of every artifact. Windows Performance Recorder could not capture an
additional CPU profile because the host rejected the request with `0xc5585011`; that trace remains an unavailable diagnostic, not a
substitute for or contradiction of the passing statistical evidence.

## Phase 7 Windows closeout

The final 2026-08-22 full Windows validator passed all 68 steps at revision `e23d77fe489207206e95f6c8153287a6296b8356`.
It covered the complete 251-test Jolt suite, 21 registered AutomatedTesting scenarios, Clang 22.1.8 and MSVC Debug/Profile/Release,
unity and non-unity builds, double precision, native diagnostics, clang-cl ASan, modular and monolithic targets, and source-tree plus
installed modular/monolithic public-only consumers. The retained JSON/JUnit summaries are under
`build/jolt-qualification/20260822-full-final-r2`. This is Windows functional and packaging qualification; performance has the
explicit exceptions below.

MSVC Profile is the broad Editor, Asset Processor, asset, and AutomatedTesting application build in that run. All Jolt-specific
clang-cl configurations pass, but a whole-engine clang-cl application build is unavailable because LLVM 22.1.8 rejects Assimp 6.0.4's
upstream Windows `VERSIONINFO` copyright byte. A bounded LLVM 22.1.8 resource-compiler probe reproduces the same failure independently
of Jolt. No Assimp or engine source is patched for this provider qualification.

The final MSVC Release matched-provider capture uses 30 fresh processes per throughput workload and 30 retained 4,096-frame windows per
tail workload. It is bound to revision `0372f05d889b895661fd40ab293aca2a1a573e3b`, `Jolt.Tests.Gem.dll`, `Jolt.API.dll`, the runner,
Box3D's corresponding module and runtime DLL, and the PhysX module. All workload, result, topology, 5% CV, and pooled-frame-tail checks
pass. The only comparator failure is the scalar closest-raycast throughput against PhysX:

| Workload | Jolt | Box3D | PhysX | Jolt/Box3D | Jolt/PhysX |
|---|---:|---:|---:|---:|---:|
| Step 128 bodies, 1 worker | 41.24 us | 104.63 us | 106.41 us | 0.394 | 0.388 |
| Step 128 bodies, 4 workers | 42.78 us | 74.90 us | 111.02 us | 0.571 | 0.385 |
| Step 128 bodies, 8 workers | 46.09 us | 90.70 us | 146.24 us | 0.508 | 0.315 |
| Step 1,024 bodies, 1 worker | 330.92 us | 845.77 us | 844.03 us | 0.391 | 0.392 |
| Step 1,024 bodies, 4 workers | 194.98 us | 300.17 us | 475.80 us | 0.650 | 0.410 |
| Step 1,024 bodies, 8 workers | 156.97 us | 205.04 us | 407.02 us | 0.766 | 0.386 |
| Create and destroy 128 bodies | 68.52 us | 75.10 us | 4,424.28 us | 0.912 | 0.015 |
| Create and destroy 1,024 bodies | 547.17 us | 764.74 us | 35,445.02 us | 0.715 | 0.015 |
| 128 closest raycasts, scalar | 21.40 us | 17.36 us | 17.36 us | 1.233 | 1.232 |
| 128 closest raycasts, batch | 19.29 us | 17.56 us | 22.41 us | 1.098 | 0.861 |
| 1,024 closest raycasts, batch | 46.13 us | 47.27 us | 197.04 us | 0.976 | 0.234 |
| Sphere overlap, 25 stable hits | 0.80 us | 0.85 us | 0.91 us | 0.937 | 0.882 |

The scalar-raycast median ratio is 1.232, its repetition-p95 ratio is 1.227, and its bootstrap upper ratio is 1.238, exceeding the
unchanged 1.00, 1.10, and 1.05 gates. The corresponding pre-refinement Clang 22.1.8 exact-source recapture passes: Jolt is 18.4764 us
with 2.001% CV,
Box3D is 19.5412 us, and PhysX is 18.6076 us. Jolt's Clang median ratio is 0.9930 against PhysX, its repetition-p95 ratio is 1.0234,
and its bootstrap upper ratio is 0.9966. That scalar gap is therefore MSVC-specific rather than an API-correctness or
workload-mismatch result.

Comparable non-LTCG objects show the public `World::RaycastClosest` at a 184-byte frame, 233 instructions, 10 calls, and 17 conditional
branches under MSVC, versus a 120-byte frame, 152 instructions, 10 calls, and 12 conditional branches under clang-cl. MSVC inlines
validation into the public operation; clang-cl emits a smaller public wrapper and tail-transfers through the default wrapper. A measured
forced-inline experiment did not improve either compiler and was reverted. Removing deterministic floating-point state, read locking,
or input/output validation would weaken required behavior.

A subsequent 2026-08-22 MSVC Release recapture measures the retained box fast-path refinement. The collector now preserves the winning
box hit's validated handles, material, and surface normal while the body is already available, avoiding a second body lookup and output
reconstruction. All three providers were rebuilt from a clean tree before 30 fresh-process repetitions of each affected query workload:

| Workload | Jolt | Box3D | PhysX | Jolt/Box3D | Jolt/PhysX |
|---|---:|---:|---:|---:|---:|
| 128 closest raycasts, scalar | 19.19 us | 17.24 us | 17.32 us | 1.113 | 1.108 |
| 128 closest raycasts, batch | 16.60 us | 17.53 us | 22.43 us | 0.947 | 0.740 |
| 1,024 closest raycasts, batch | 42.58 us | 46.87 us | 196.48 us | 0.908 | 0.217 |
| Sphere overlap, 25 stable hits | 0.77 us | 0.82 us | 0.90 us | 0.933 | 0.848 |

The scalar Jolt median improved 10.3% from the preceding 21.40 us capture, and both batch workloads remain faster than PhysX. The
scalar comparison still fails the unchanged gate: its median ratio is 1.108, repetition-p95 ratio is 1.110, and bootstrap upper ratio is
1.111. Each scalar repetition makes 128 independently guarded API calls; the measured canonical deterministic-float scope alone costs
12.475 ns per call, or 1.597 us across the workload. The batch operation applies that state and lock discipline once. The guarantee is
retained and callers with query collections should use the batch API.

A final 30-process MSVC decomposition on the retained implementation separates that cost from the native query and result path:

| Diagnostic workload | Median | CV |
|---|---:|---:|
| Deterministic floating-point scope | 12.561 ns | 0.243% |
| Uncontended query lock with floating-point scope | 14.252 ns | 0.203% |
| 128 empty-world closest raycasts | 3.193 us | 6.375% |
| 128 broadphase-only closest raycasts | 17.430 us | 2.593% |
| 128 full closest raycasts | 19.301 us | 0.784% |

The empty-world CV contains one retained 4.333 us process sample; its median remains stable. The guarded broadphase-only result is close
to the 17.32 us PhysX full-query median, while Jolt's narrowphase and AZ-facing hit construction add 1.871 us across 128 successful box
hits. This establishes that the remaining scalar gap is not safely removable by weakening the deterministic guard or read lock. The
diagnostic samples are under
`build/jolt-qualification/20260822-msvc-query-decomposition/Diagnostics` and are not substituted for the qualified matched-provider
artifact.

The compared provider contracts are close but not identical. The PhysX path takes `PHYSX_SCENE_READ_LOCK`, invokes `PxScene::raycast`,
and constructs a complete `SceneQueryHit`; it does not save and restore the caller's floating-point environment or impose Jolt's
canonical equal-fraction tie ordering. Jolt performs both additional guarantees on every scalar query. The 14.252 ns Jolt query-lock
scope is close to the measured 14.6-15.5 ns per-ray provider difference, but the source and decomposition do not prove that one scope
causes the entire gap. The raw scalar ratio therefore remains visible instead of being normalized or reclassified after measurement.

The query-only artifacts are under `build/jolt-qualification/20260822-msvc-query-closeout/BenchmarkResults`. They pass source/binary
freshness, workload, result, topology, and 5% CV validation. The full comparator intentionally also reports the absent step, lifecycle,
and frame-tail rows because this focused recapture does not replace the complete 2026-08-21 artifact.

An exact upstream box-normal specialization was also measured and rejected. It increased the scalar median from 19.1751 us to
19.3246 us, increased the mean by 1.18%, and increased CV from 0.486% to 1.293%. The experiment was removed without changing the
retained implementation.

Two exact axis-aligned box-intersection specializations were also rejected. The first dynamic-index implementation measured 22.125 us;
the forced-inline direct-component variant measured 21.719 us. Both were materially slower than the retained 19.301 us path and were
removed completely. The retained generic SIMD slab test therefore remains the smallest and fastest validated implementation on MSVC.

The initial clean MSVC absolute recapture passed every workload, result, allocation, retained-memory, no-growth, and latency threshold,
but missed four 5% CV gates when Google Benchmark repeated all workloads inside one long-lived process. A focused rerun preserved the
same Release binary, correctness counters, minimum sample duration, and 30-repetition gate while starting a fresh process for every
sample and constraining each process to the physical lanes required by its worker policy. All capability and rollback gates pass:

| Workload | Median | CV |
|---|---:|---:|
| Capability acquisition | 1.950 ns | 1.793% |
| Filtered recapture, 128 bodies | 20.19 us | 0.725% |
| Filtered recapture, 1,024 bodies | 169.36 us | 1.537% |
| Transactional filtered restore, 128 bodies | 35.70 us | 0.632% |
| Transactional filtered restore, 1,024 bodies | 347.13 us | 0.649% |
| Validated filtered restore, 128 bodies | 15.08 us | 2.375% |
| Validated filtered restore, 1,024 bodies | 122.32 us | 0.940% |

The capability median satisfies the 3 ns acquisition gate. The deterministic-float and uncontended-query-lock medians remain 12.475 ns
and 14.162 ns. The qualified isolated artifact is under
`build/jolt-qualification/20260822-msvc-isolated-variability-closeout/BenchmarkResults`; the full absolute and matched-provider artifacts
remain under `build/jolt-qualification/20260822-msvc-absolute-clean/BenchmarkResults` and
`build/jolt-qualification/20260821-msvc-phase7-final/BenchmarkResults`. No threshold was relaxed and no sample was trimmed, replaced,
or retried.

## Historical Phase 5 Windows baseline

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

Jolt is faster than PhysX for every workload. Its samples pass the Phase 5 workload, result, median, bootstrap, repetition-p95, and 5%
CV gates.
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

These reports predate the Phase 7 raw-frame tail and exact execution-topology requirements. They remain historical optimization evidence,
not current qualification artifacts.

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
so `llvm-objdump` cannot inspect its intermediate whole-program IR as a normal COFF object. A retained non-LTCG scratch compile now
provides comparable MSVC and clang-cl object disassembly for selected query functions without claiming whole-program code generation.
The reports are `build/jolt-qualification/msvc-raycast-assembly/report.md` and
`build/jolt-qualification/clang-raycast-assembly/report.md`.

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

Compiled scene and skeleton products treat canonical source data as authoritative and carry native archives only as optional acceleration
caches. A cache is used only when its platform and native build fingerprint match exactly; missing, incompatible, or corrupt caches rebuild
from canonical data. The table above therefore describes the compatible-cache fast path, not the portable reconstruction path. Asset-load
qualification must report both paths separately and must not compare a relabelled host-native archive against a target-native product.

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
mutation. Every external participant completes a non-mutating `PrepareRestoreState` pass before native state changes, then performs an
infallible `CommitRestoreState` pass after the native, character, soft-body, Hair, and contact-provenance state is complete. Contact-cache
provenance is also prepared in retained scratch and swapped only at commit. A failed restore recovers the pre-operation state and returns
`StateRestoreStatus::Rejected`; failed recovery returns `StateRestoreStatus::StateIndeterminate` and quarantines the world so only
destruction remains available. Imported archives always use transactional recovery regardless of the policy recorded by their source.

`RestoreSafety::Validated` is the explicit lower-latency policy for internally owned rollback snapshots after topology and participant-state
validation. It avoids the recovery capture while retaining all pre-mutation validation and exact stream-consumption checks. Snapshots that
contain caller-owned participant or Hair state still require recovery because those commits cross an external failure boundary.

The same 30-repetition Release process measured filtered body-and-constraint restores after warming snapshot storage. All runs used one
worker and reported `QualityValid=1`.

| Policy | 128 bodies, 127 constraints | CV | 1,024 bodies, 1,023 constraints | CV |
|---|---:|---:|---:|---:|
| Transactional | 35.7 us | 0.49% | 363 us | 1.95% |
| Validated | 14.6 us | 0.64% | 122 us | 2.07% |

At 128 bodies the recovery capture accounts for more than half of transactional restore time. At 1,024 bodies validated mode removes the
recovery capture and reduces the median by 66.4%. The safety policy is captured with the snapshot, reflected for scripts, and required to
match across every part of a multipart restore. These measurements predate the two-phase participant and contact-provenance remediation;
Phase 7 must recapture them before they are treated as current qualification evidence.

The Phase 7 native patch also retains the body-ID workspace used to notify broadphase bounds after restore. The first restore grows that
world-owned array; subsequent restores clear and reuse it. The final smoke covered recapture plus transactional and validated restore at
128 and 1,024 bodies. All six workloads reported zero native allocations, frees, reallocations, native-byte growth, temporary-capacity
growth, wrapper-capacity growth, and snapshot failures after warmup. The 24-byte array object and its high-water BodyID storage are owned
by the native world and released with it. Final 30-repetition timing remains the authority for the performance table.

## Authored Profile capture

`AutomatedTesting::JoltTests_Benchmark` creates an 8 by 8 by 4 stack, waits for 120 simulation ticks, keeps every body active, and captures
30 CPU frame samples through the engine profiling capture bus. The current Windows Profile run kept all 256 bodies active, reported no
physics update errors, and measured 3.8841 ms median whole-Editor frame time. The minimum, mean, and maximum were 2.3750 ms, 3.7406 ms, and
5.6622 ms. Rendering and Editor overhead are deliberately included, so this capture is diagnostic rather than the provider timing gate.

The capture must run in Profile. Release intentionally excludes `StatisticalProfilerProxySystemComponent`, so the engine capture API writes
zero CPU frame times in that configuration. The test rejects those zeroes instead of reporting unsupported measurements as evidence.

## Opt-in telemetry

`Diagnostics::ConfigurePerformanceStatistics` enables only the requested counter groups for a world. Disabled query, event, snapshot, job,
lock, Hair, and simulation counters do not read clocks. Lock timing uses the deterministic world mutex's contended path, job timing is
captured by the AZ job adapter, and native allocation counters are process-wide because Jolt's allocator hooks are process-wide. Resource
counts and retained capacities are read into a caller-owned `WorldPerformanceStatistics`; the read itself does not allocate. Resetting a
snapshot clears interval counters and high-water marks without releasing retained storage. Resetting native allocation counters from one
world begins a new process-wide memory interval for every world that has memory statistics enabled.

The resource report covers bodies, shapes, constraints, characters, vehicles, ragdolls, soft bodies, CPU Hair, scenes, and unified state
snapshots. Wrapper-retained bytes also include query workspaces, snapshot scratch, contact provenance, event queues, lookup structures, CPU Hair
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

`prepare_benchmark_artifact.py` refuses a non-Release report, a report older than its provider binary, runtime dependencies, or runner,
a provider binary or runtime dependency older than the current source state, or a missing input. It can merge disjoint captures and
explicitly reindex repeated fresh-process batches only when their benchmark contexts match, with the same 1% frequency tolerance used by
provider comparison. Local repetition indices must be contiguous and the merged result must contain exactly 30 repetitions. Batch-local
aggregate rows are discarded because they do not describe the pooled sample; the comparator recomputes every statistic from the retained
raw iterations. Every original capture context remains in the merged artifact. It hashes every raw report, the runner, provider binary,
runtime dependency, workload definition, tracked diff, and every untracked non-ignored source file. The artifact also records the
revision, compiler/configuration, affinity policy, filter, minimum time, repetition count, reindexing policy, and raw-sample policy. The
comparator rejects stale or absent runtime dependency metadata, stale signatures, differing
runner/source/configuration/affinity metadata, duplicate or missing repetitions, mismatched worker and affinity counts, invalid topology
and quality counters, non-finite samples, missing raw frames, and the median, bootstrap, repetition-p95, pooled-frame-tail, and CV gates.

Each workload begins with a separate recorded warmup process, then uses one fresh process for each of its 30 repetitions. Repeated world
destruction and recreation is a different lifecycle workload and measurably contaminates both throughput and frame-tail variance. This
preserves all 30 samples and all 30 measured 4,096-frame windows. No sample is trimmed or discarded, and warmup reports are retained and
hashed without entering measured results.

Throughput workloads use a 0.5-second minimum measurement window. A 0.05-second probe admitted isolated scheduler interruptions into an
entire repetition and produced a 5.10% raycast CV; the same 30 fresh-process repetitions measured 0.77% CV at 0.5 seconds. Tail workloads
retain every raw frame instead of relying on a longer aggregate interval. A 4,096-frame window reduced the measured PhysX four-worker tail
CV from 9.03% at 1,024 frames to 1.40% without trimming a sample.

The Windows recipe uses logical processors selected from a 10-second processor-utility and interrupt-rate sample, with one sibling per
physical core. Re-measure quiet physical lanes on different hardware and record the exact selection in the affinity policy. Qualification
uses response files for the hundreds of fresh-process report paths so Windows command-line length does not limit complete captures.
Do not poll the benchmark process or run other local commands while a timed capture is active. Session polling reproduced a 6.43% Jolt
batch-raycast CV and 9.01% PhysX tail-window CV; complete unpolled 30-process recaptures measured 1.46% and 0.84%, respectively, without
replacing or trimming individual samples.

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

The current Phase 7 report is `build/jolt-phase7/assembly/Jolt.Release.Clang22.md`. The exported capability seam has no virtual dispatch:
`WorldSimulation::Get` is 10 instructions with no stack frame or call, while `StepWorldDetailed`, `GetEvents`, `RaycastClosest`, and the
common rollback overloads are 12-instruction, zero-frame tail transfers into `RuntimeImplementation`. The inspected batch-raycast wrapper
has a 104-byte frame and two direct calls. `Internal::WaitForOperation` has a 56-byte frame and three calls, including the intentional
worker-aware wait/assistance path. These counts are diagnostic compiler-output evidence, not brittle opcode gates.

The default scalar raycast path checks for an active debug capture before entering the cold capture helper. Clang 22.1.8 passes the
current exact-source scalar-query gate, while MSVC retains the compiler-specific gap documented in the Phase 7 Windows closeout. The
separate compiler artifacts and comparable object disassembly supersede the older single-compiler optimization checkpoint.

## Allocator alignment validation

ASan and LLDB isolated an intermittent `vmovaps` fault in AzCore's HPHA placement buffer: the public byte buffer was 16-byte aligned while
the hidden allocator implementation contains 64-byte-aligned bucket state. The buffer is now 128-byte aligned, and a compile-time check
prevents the implementation alignment from exceeding its storage alignment. The repaired Release binary passed the full 227-test Jolt
suite and 120 consecutive stress-process launches. This is an engine allocator correctness fix exposed by the Phase 5 no-growth stress,
not a Jolt-specific alignment workaround.

## Reproduction

```powershell
$buildDirectory = 'build/windows_jolt_clangcl_ninja'
cmake --build $buildDirectory --config Release --target Jolt.Tests Box3D.Tests PhysX5.Tests --parallel 16

ctest --test-dir $buildDirectory -C Release -R 'Gem::Jolt\.Tests\.main' --output-on-failure

$binaryDirectory = Resolve-Path "$buildDirectory/bin/Release"
$runner = Resolve-Path "$binaryDirectory/AzTestRunner.exe"
$benchmarkResults = New-Item -ItemType Directory -Force 'build/jolt-phase7/BenchmarkResults'
$batchCount = 30
$repetitionsPerBatch = 1
$matchedSuffix =
    '(Step/SettledBoxes|Tail/Step/SettledBoxes|Lifecycle/CreateDestroyBodies|' +
    'Query/RaycastGrid|Query/RaycastClosestBatchGrid/1024/(128/4|1024/4)|Query/OverlapSphereGrid)'
$matchedWorkloads = @(
    [pscustomobject]@{ Name = 'Step128W1'; Filter = 'Step/SettledBoxes/128/1/iterations:600/real_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Step128W4'; Filter = 'Step/SettledBoxes/128/4/iterations:600/real_time'; Mask = 0x25020000 }
    [pscustomobject]@{ Name = 'Step128W8'; Filter = 'Step/SettledBoxes/128/8/iterations:600/real_time'; Mask = 0x659A0000 }
    [pscustomobject]@{ Name = 'Step1024W1'; Filter = 'Step/SettledBoxes/1024/1/iterations:600/real_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Step1024W4'; Filter = 'Step/SettledBoxes/1024/4/iterations:600/real_time'; Mask = 0x25020000 }
    [pscustomobject]@{ Name = 'Step1024W8'; Filter = 'Step/SettledBoxes/1024/8/iterations:600/real_time'; Mask = 0x659A0000 }
    [pscustomobject]@{ Name = 'Lifecycle128'; Filter = 'Lifecycle/CreateDestroyBodies/128/1/real_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Lifecycle1024'; Filter = 'Lifecycle/CreateDestroyBodies/1024/1/real_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Raycast'; Filter = 'Query/RaycastGrid/1024/128/1/real_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'BatchRaycast128'; Filter = 'Query/RaycastClosestBatchGrid/1024/128/4/real_time'; Mask = 0x25020000 }
    [pscustomobject]@{ Name = 'BatchRaycast1024'; Filter = 'Query/RaycastClosestBatchGrid/1024/1024/4/real_time'; Mask = 0x25020000 }
    [pscustomobject]@{ Name = 'Overlap'; Filter = 'Query/OverlapSphereGrid/1024/1/1/real_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Tail128W1'; Filter = 'Tail/Step/SettledBoxes/128/1/iterations:1/manual_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Tail128W4'; Filter = 'Tail/Step/SettledBoxes/128/4/iterations:1/manual_time'; Mask = 0x25020000 }
    [pscustomobject]@{ Name = 'Tail128W8'; Filter = 'Tail/Step/SettledBoxes/128/8/iterations:1/manual_time'; Mask = 0x659A0000 }
    [pscustomobject]@{ Name = 'Tail1024W1'; Filter = 'Tail/Step/SettledBoxes/1024/1/iterations:1/manual_time'; Mask = 0x01000000 }
    [pscustomobject]@{ Name = 'Tail1024W4'; Filter = 'Tail/Step/SettledBoxes/1024/4/iterations:1/manual_time'; Mask = 0x25020000 }
    [pscustomobject]@{ Name = 'Tail1024W8'; Filter = 'Tail/Step/SettledBoxes/1024/8/iterations:1/manual_time'; Mask = 0x659A0000 }
)

function Invoke-ProviderBenchmark(
    [string] $provider,
    [string] $module,
    [string] $filterSuffix,
    [double] $minimumTime,
    [string] $outputPath,
    [Int64] $affinityMask,
    [int] $repetitions)
{
    $filter = "^$provider/$filterSuffix"
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $runner
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.WorkingDirectory = $binaryDirectory
    $startInfo.ArgumentList.Add((Resolve-Path "$binaryDirectory/$module"))
    $startInfo.ArgumentList.Add('AzRunBenchmarks')
    $startInfo.ArgumentList.Add("--benchmark_filter=$filter")
    $startInfo.ArgumentList.Add("--benchmark_min_time=$minimumTime")
    $startInfo.ArgumentList.Add("--benchmark_repetitions=$repetitions")
    $startInfo.ArgumentList.Add('--benchmark_report_aggregates_only=false')
    $startInfo.ArgumentList.Add('--benchmark_display_aggregates_only=true')
    $startInfo.ArgumentList.Add("--benchmark_out=$outputPath")
    $startInfo.ArgumentList.Add('--benchmark_out_format=json')

    $benchmarkHost = [System.Diagnostics.Process]::GetCurrentProcess()
    $previousAffinity = $benchmarkHost.ProcessorAffinity
    try
    {
        $benchmarkHost.ProcessorAffinity = [IntPtr]$affinityMask
        $process = [System.Diagnostics.Process]::Start($startInfo)
    }
    finally
    {
        $benchmarkHost.ProcessorAffinity = $previousAffinity
    }
    $process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High
    if ($process.PriorityClass -ne [System.Diagnostics.ProcessPriorityClass]::High)
    {
        throw "$provider benchmark process did not retain High priority."
    }
    if ([Int64]$process.ProcessorAffinity -ne $affinityMask)
    {
        throw "$provider benchmark process did not retain the requested affinity."
    }
    $standardOutput = $process.StandardOutput.ReadToEndAsync()
    $standardError = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $standardOutput.GetAwaiter().GetResult() | Out-Null
    $errorText = $standardError.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0)
    {
        throw "$provider benchmark failed: $errorText"
    }
    if (!(Test-Path -LiteralPath $outputPath) -or (Get-Item -LiteralPath $outputPath).Length -eq 0)
    {
        throw "$provider benchmark produced no report: $outputPath"
    }
}

$absoluteSuffix =
    '(Diagnostic/AcquireRuntimeConfigurationCapability|Diagnostic/RaycastEmptyWorld|' +
    'Query/OverlapSphereGrid/1024/1/1|Query/RaycastGrid/1024/128/1|' +
    'Rollback/(RecaptureFilteredState|RestoreFilteredStateTransactional|RestoreFilteredStateValidated))'

$joltAbsoluteRaw = "$benchmarkResults/Jolt.Phase7.Absolute.Raw30.json"
$providers = @(
    [pscustomobject]@{ Name = 'Jolt'; Module = 'Jolt.Tests.Gem.dll'; Raw = [System.Collections.Generic.List[string]]::new(); Warmup = [System.Collections.Generic.List[string]]::new() }
    [pscustomobject]@{ Name = 'Box3D'; Module = 'Box3D.Tests.Gem.dll'; Raw = [System.Collections.Generic.List[string]]::new(); Warmup = [System.Collections.Generic.List[string]]::new() }
    [pscustomobject]@{ Name = 'PhysX'; Module = 'PhysX5.Tests.Gem.dll'; Raw = [System.Collections.Generic.List[string]]::new(); Warmup = [System.Collections.Generic.List[string]]::new() }
)

$benchmarkHost = [System.Diagnostics.Process]::GetCurrentProcess()
$previousPriority = $benchmarkHost.PriorityClass
try
{
    $benchmarkHost.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High
    foreach ($provider in $providers)
    {
        foreach ($workload in $matchedWorkloads)
        {
            $warmupPath = "$benchmarkResults/$($provider.Name).Phase7.$($workload.Name).Warmup.json"
            Invoke-ProviderBenchmark $provider.Name $provider.Module $workload.Filter 0.5 $warmupPath $workload.Mask 1
            $provider.Warmup.Add($warmupPath)
            for ($batchIndex = 0; $batchIndex -lt $batchCount; ++$batchIndex)
            {
                $rawPath =
                    "$benchmarkResults/$($provider.Name).Phase7.$($workload.Name).Raw$repetitionsPerBatch.Batch$batchIndex.json"
                Invoke-ProviderBenchmark `
                    $provider.Name `
                    $provider.Module `
                    $workload.Filter `
                    0.5 `
                    $rawPath `
                    $workload.Mask `
                    $repetitionsPerBatch
                $provider.Raw.Add($rawPath)
            }
        }
    }
    Invoke-ProviderBenchmark Jolt Jolt.Tests.Gem.dll $absoluteSuffix 0.2 $joltAbsoluteRaw 0x01000000 30
}
finally
{
    $benchmarkHost.PriorityClass = $previousPriority
}

$joltRaw = $providers[0].Raw.ToArray()
$box3dRaw = $providers[1].Raw.ToArray()
$physxRaw = $providers[2].Raw.ToArray()
$joltWarmup = $providers[0].Warmup.ToArray()
$box3dWarmup = $providers[1].Warmup.ToArray()
$physxWarmup = $providers[2].Warmup.ToArray()

function Get-AdditionalRawReportArguments([string[]] $rawPaths)
{
    $arguments = [System.Collections.Generic.List[string]]::new()
    for ($rawPathIndex = 1; $rawPathIndex -lt $rawPaths.Count; ++$rawPathIndex)
    {
        $arguments.Add('--additional-raw-report')
        $arguments.Add($rawPaths[$rawPathIndex])
    }
    return $arguments.ToArray()
}

function Get-WarmupReportArguments([string[]] $warmupPaths)
{
    $arguments = [System.Collections.Generic.List[string]]::new()
    foreach ($warmupPath in $warmupPaths)
    {
        $arguments.Add('--warmup-report')
        $arguments.Add($warmupPath)
    }
    return $arguments.ToArray()
}

$affinityPolicy = 'Ryzen 9 7950X measured quiet physical lanes: 1=24, 4=17,24,26,29, 8=17,19,20,23,24,26,29,30; inherited affinity; high priority'
$joltTests = Resolve-Path "$binaryDirectory/Jolt.Tests.Gem.dll"
$filter = "Jolt/$matchedSuffix"
$joltAdditionalRawArguments = Get-AdditionalRawReportArguments $joltRaw
$joltWarmupArguments = Get-WarmupReportArguments $joltWarmup
$joltResponseFile = "$benchmarkResults/Jolt.Phase7.Qualification.rsp"
[System.IO.File]::WriteAllLines($joltResponseFile, [string[]]($joltAdditionalRawArguments + $joltWarmupArguments))

python Gems/Physics/Jolt/Code/Tests/prepare_benchmark_artifact.py `
    $joltRaw[0] `
    $benchmarkResults/Jolt.Phase7.MatchedAndTail.Qualified.json `
    $joltTests `
    "@$joltResponseFile" `
    --runner $runner `
    --source-root . `
    --provider Jolt `
    --compiler-id Clang `
    --compiler-version 22.1.8 `
    --build-configuration Release `
    --cpu-affinity-policy $affinityPolicy `
    --benchmark-filter $filter `
    --minimum-time 0.5 `
    --repetitions 30 `
    --reindex-repetitions `
    --runtime-dependency "$binaryDirectory/Jolt.API.dll"

$filter = "Jolt/$absoluteSuffix"
python Gems/Physics/Jolt/Code/Tests/prepare_benchmark_artifact.py `
    $joltAbsoluteRaw `
    $benchmarkResults/Jolt.Phase7.Absolute.Qualified.json `
    $joltTests `
    --runner $runner `
    --source-root . `
    --provider Jolt `
    --compiler-id Clang `
    --compiler-version 22.1.8 `
    --build-configuration Release `
    --cpu-affinity-policy $affinityPolicy `
    --benchmark-filter $filter `
    --minimum-time 0.2 `
    --repetitions 30 `
    --runtime-dependency "$binaryDirectory/Jolt.API.dll"

$box3dTests = Resolve-Path "$binaryDirectory/Box3D.Tests.Gem.dll"
$filter = "Box3D/$matchedSuffix"
$box3dAdditionalRawArguments = Get-AdditionalRawReportArguments $box3dRaw
$box3dWarmupArguments = Get-WarmupReportArguments $box3dWarmup
$box3dResponseFile = "$benchmarkResults/Box3D.Phase7.Qualification.rsp"
[System.IO.File]::WriteAllLines($box3dResponseFile, [string[]]($box3dAdditionalRawArguments + $box3dWarmupArguments))
python Gems/Physics/Jolt/Code/Tests/prepare_benchmark_artifact.py `
    $box3dRaw[0] `
    $benchmarkResults/Box3D.Phase7.MatchedAndTail.Qualified.json `
    $box3dTests `
    "@$box3dResponseFile" `
    --runner $runner `
    --source-root . `
    --provider Box3D `
    --compiler-id Clang `
    --compiler-version 22.1.8 `
    --build-configuration Release `
    --cpu-affinity-policy $affinityPolicy `
    --benchmark-filter $filter `
    --minimum-time 0.5 `
    --repetitions 30 `
    --reindex-repetitions `
    --runtime-dependency "$binaryDirectory/Box3D.API.dll"

$physxTests = Resolve-Path "$binaryDirectory/PhysX5.Tests.Gem.dll"
$filter = "PhysX/$matchedSuffix"
$physxAdditionalRawArguments = Get-AdditionalRawReportArguments $physxRaw
$physxWarmupArguments = Get-WarmupReportArguments $physxWarmup
$physxResponseFile = "$benchmarkResults/PhysX.Phase7.Qualification.rsp"
[System.IO.File]::WriteAllLines($physxResponseFile, [string[]]($physxAdditionalRawArguments + $physxWarmupArguments))
python Gems/Physics/Jolt/Code/Tests/prepare_benchmark_artifact.py `
    $physxRaw[0] `
    $benchmarkResults/PhysX.Phase7.MatchedAndTail.Qualified.json `
    $physxTests `
    "@$physxResponseFile" `
    --runner $runner `
    --source-root . `
    --provider PhysX `
    --compiler-id Clang `
    --compiler-version 22.1.8 `
    --build-configuration Release `
    --cpu-affinity-policy $affinityPolicy `
    --benchmark-filter $filter `
    --minimum-time 0.5 `
    --repetitions 30 `
    --reindex-repetitions

python Gems/Physics/Jolt/Code/Tests/compare_provider_benchmarks.py `
    build/jolt-phase7/BenchmarkResults/Jolt.Phase7.MatchedAndTail.Qualified.json `
    build/jolt-phase7/BenchmarkResults/Box3D.Phase7.MatchedAndTail.Qualified.json `
    build/jolt-phase7/BenchmarkResults/PhysX.Phase7.MatchedAndTail.Qualified.json `
    --gate-provider PhysX `
    --repetitions 30 `
    --maximum-median-ratio 1.0 `
    --maximum-bootstrap-ratio 1.05 `
    --maximum-repetition-p95-ratio 1.10 `
    --maximum-frame-tail-ratio 1.10 `
    --maximum-cv 0.05

python Gems/Physics/Jolt/Code/Tests/validate_jolt_benchmarks.py `
    build/jolt-phase7/BenchmarkResults/Jolt.Phase7.Absolute.Qualified.json `
    --repetitions 30 `
    --maximum-capability-nanoseconds 3.0 `
    --maximum-capability-operation-ratio 0.02 `
    --maximum-cv 0.05

python Gems/Physics/Jolt/Code/Tests/inspect_hot_assembly.py `
    "$binaryDirectory/Jolt.API.dll" `
    --objdump D:/LLVM/22.1.8/bin/llvm-objdump.exe `
    --symbol 'Jolt::WorldSimulation::Get\(' `
    --symbol 'Jolt::WorldSimulation::StepWorldDetailed' `
    --symbol 'Jolt::WorldQueries::RaycastClosest\(' `
    --symbol 'Jolt::WorldQueries::RaycastClosestBatch\(' `
    --symbol 'Jolt::Internal::WaitForOperation' `
    --symbol 'Jolt::WorldSimulation::GetEvents' `
    --symbol 'Jolt::Rollback::CaptureWorldState\(' `
    --symbol 'Jolt::Rollback::RestoreWorldState\(' `
    --json-output build/jolt-phase7/assembly/Jolt.Release.Clang22.json `
    --markdown-output build/jolt-phase7/assembly/Jolt.Release.Clang22.md
```

The comparator fails closed when any required workload or counter is missing, signatures differ, the providers report different policies,
the workload misses its quality gate, or timing exceeds a configured threshold.

The current fresh LLVM Release target and the narrower Profile `Jolt.Module` and `Jolt.Editor` targets build normally through
`cmake --build`. Building the entire fresh Profile Editor host with LLVM 22.1.8 currently stops in the unrelated third-party Assimp
Windows-resource step because `llvm-rc` rejects its non-Unicode copyright byte. The Jolt diagnostics scenario was therefore run in the
already complete Profile host after replacing its freshly rebuilt AzCore and Jolt modules; all 21 independent checks passed. This is not
treated as a Jolt source failure or suppressed in third-party code.
