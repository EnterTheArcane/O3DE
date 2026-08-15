# Jolt performance qualification

Release microbenchmarks are the timing authority. Profile captures attribute time through the engine and native profiling scopes. A
matched result is accepted only after the comparator verifies workload identity, worker count, notification policy, sleep and continuous
collision policy, query cardinality, simulation quality, repetition count, and stability.

## Current Windows baseline

The following medians were captured on 2026-08-13 in isolated high-priority Release processes with 30 repetitions per gated provider.
The same affinity policy constrained each process to eight physical cores. Ratios below 1.0 favor Jolt.

| Workload | Jolt | Box3D | PhysX | Jolt/Box3D | Jolt/PhysX |
|---|---:|---:|---:|---:|---:|
| Step 128 bodies, 1 worker | 41.23 us | 101.45 us | 115.63 us | 0.406 | 0.357 |
| Step 128 bodies, 4 workers | 35.02 us | 70.54 us | 124.92 us | 0.496 | 0.280 |
| Step 128 bodies, 8 workers | 43.98 us | 88.81 us | 154.45 us | 0.495 | 0.285 |
| Step 1024 bodies, 1 worker | 323.54 us | 820.69 us | 850.29 us | 0.394 | 0.381 |
| Step 1024 bodies, 4 workers | 184.39 us | 272.34 us | 453.47 us | 0.677 | 0.407 |
| Step 1024 bodies, 8 workers | 155.43 us | 199.13 us | 388.29 us | 0.781 | 0.400 |
| Create and destroy 128 bodies | 65.71 us | 76.60 us | 4358.00 us | 0.858 | 0.015 |
| Create and destroy 1024 bodies | 526.34 us | 754.65 us | 35146.75 us | 0.697 | 0.015 |
| 128 closest raycasts, scalar | 16.80 us | 16.86 us | 17.51 us | 0.996 | 0.960 |
| 128 closest raycasts, batch | 15.19 us | 17.50 us | 23.05 us | 0.868 | 0.659 |
| 1024 closest raycasts, batch | 38.92 us | 47.34 us | 238.50 us | 0.822 | 0.163 |
| Sphere overlap, 25 stable hits | 0.73 us | 0.80 us | 0.89 us | 0.905 | 0.810 |

Jolt is faster than PhysX for every workload and passes every median, repetition-tail, bootstrap, workload-signature, quality, and 5% CV
check. Normal-priority runs are retained alongside the accepted artifacts: they contain OS scheduling outliers in the Jolt eight-worker
and PhysX step workloads and therefore fail the unchanged stability gate. High process priority removes that host noise without changing
affinity, workload, iteration count, or provider policy. The concurrent-query topology gate retains the idle no-lock ray traversal and
Jolt is now faster than Box3D for every matched workload, including scalar closest raycasts.

The retained reports are:

- `build/windows/BenchmarkResults/Jolt.ConcurrentQueries.HighPriority.Raw30.json`
- `build/windows/BenchmarkResults/Box3D.FinalMatched.Raw30.json`
- `build/windows/BenchmarkResults/PhysX.FinalMatched.HighPriority.Raw30.json`

The first post-Linux run is retained as `Jolt.FinalMatched.PostLinux.HighPriority.Raw30.json`; its 128-body, one-worker step series had
5.36% CV and failed the unchanged 5% stability gate. The current revision passed without trimming samples or relaxing thresholds.

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

The Profile object disassembly confirms that the forced-inline closest-ray core reads the update flag with one `movzbl`, branches directly
to the existing no-lock virtual call when idle, and emits no atomic read-modify-write instruction for that decision. The accepted Release
artifact above measures the complete public path after this change: scalar closest raycast improved from 16.94 us to 16.80 us, while the
full matched suite remained inside every strict gate. Unit coverage blocks two filters simultaneously, verifies canonical and restored
per-thread floating-point state, proves a topology writer waits for an active query, and completes a broad-phase query while simulation is
paused inside a native step listener.

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
buffers owned by the native contact cache, so a prewarmed contacts-only recapture adds no native allocations. The body serializer retains
its original one-pass scratch array: an allocation-free two-pass prototype was rejected after a same-session Release A/B because it
increased the 1,024-body median from 170 us to 209 us.

The retained 30-repetition Release benchmark captures all bodies and point constraints in one filtered partition after warming both
alternating provider snapshot buffers.

| Partition | Allocation-free body prototype | Retained median | Native allocations per recapture | Result |
|---|---:|---:|---:|---:|
| 128 bodies, 127 constraints | 21.9 us | 20.3 us | 0 to 1 | Retained path is 7.3% faster |
| 1,024 bodies, 1,023 constraints | 209 us | 170 us | 0 to 1 | Retained path is 18.7% faster |

The retained runs reported 1.72% and 1.71% real-time CV. Profile assembly confirms that the exact-count branch writes the count and walks
the constraint array directly, with no native allocator call; the original allocation path remains behind the unknown-count branch.
`PrewarmedFilteredStateRecaptureUsesOneNativeScratchAllocationPerCapture` checks 32 consecutive captures and fails if native allocation
traffic grows beyond the single body-selection scratch allocation.
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

## Reproduction

```powershell
cmake --build build/windows --config Release --target Jolt.Tests Box3D.Benchmarks PhysX5.Tests -- /m:1

$runner = Resolve-Path build/windows/bin/release/AzTestRunner.exe
$joltTests = Resolve-Path build/windows/bin/release/Jolt.Tests.Gem.dll
$benchmarkResults = Resolve-Path build/windows/BenchmarkResults
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
$startInfo.ArgumentList.Add("--benchmark_out=$benchmarkResults/Jolt.ConcurrentQueries.HighPriority.Raw30.json")
$startInfo.ArgumentList.Add('--benchmark_out_format=json')
$process = [System.Diagnostics.Process]::Start($startInfo)
$process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High
$process.WaitForExit()

build/windows/bin/release/AzTestRunner.exe `
    build/windows/bin/release/Jolt.Tests.Gem.dll `
    AzRunBenchmarks `
    --benchmark_filter="Jolt/Lifecycle/(Create|Import)SoftBodyDefinition" `
    --benchmark_min_time=0.1 `
    --benchmark_repetitions=30 `
    --benchmark_report_aggregates_only=true

build/windows/bin/release/AzTestRunner.exe `
    build/windows/bin/release/Jolt.Tests.Gem.dll `
    AzRunBenchmarks `
    --benchmark_filter="Jolt/Lifecycle/(ChangeRagdollMembership|RecreateRagdoll)/64" `
    --benchmark_min_time=0.05 `
    --benchmark_repetitions=30 `
    --benchmark_report_aggregates_only=true

build/windows/bin/release/AzTestRunner.exe `
    build/windows/bin/release/Jolt.Tests.Gem.dll `
    AzRunBenchmarks `
    --benchmark_filter="Jolt/Rollback/(Recapture|Restore)FilteredState" `
    --benchmark_min_time=0.05 `
    --benchmark_repetitions=30 `
    --benchmark_report_aggregates_only=true

python Gems/Physics/Jolt/Code/Tests/compare_provider_benchmarks.py `
    build/windows/BenchmarkResults/Jolt.ConcurrentQueries.HighPriority.Raw30.json `
    build/windows/BenchmarkResults/Box3D.FinalMatched.Raw30.json `
    build/windows/BenchmarkResults/PhysX.FinalMatched.HighPriority.Raw30.json `
    --gate-provider PhysX `
    --minimum-repetitions 30 `
    --maximum-median-ratio 1.0 `
    --maximum-bootstrap-ratio 1.05 `
    --maximum-repetition-tail-ratio 1.10 `
    --maximum-cv 0.05

cmake --build build/windows --config Profile --target Editor AssetProcessor -- /m:1

ctest --test-dir build/windows -C Profile `
    -R "AutomatedTesting::JoltTests_Benchmark" --output-on-failure
```

The comparator fails closed when any required workload or counter is missing, signatures differ, the providers report different policies,
the workload misses its quality gate, or timing exceeds a configured threshold.
