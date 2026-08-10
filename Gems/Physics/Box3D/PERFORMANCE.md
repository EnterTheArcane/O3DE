# Box3D performance qualification

This document records reproducible performance evidence for the provider. Release microbenchmarks are the timing authority; Profile
captures exist to attribute time to engine and native scopes. Correctness, worker count, notification policy, sleep policy, CCD policy, and
steady-state quality are validated before a matched result is accepted.

## Current Windows baseline

The following medians were captured on 2026-08-09 with a 32-logical-processor Windows host. Each provider ran in a separate Release process
for 30 repetitions. Matched simulations use the same fixed step, body count, reported worker count, disabled sleeping, disabled CCD, no
notifications, and a displacement/penetration quality gate. Batch queries receive the same unmeasured 100 ms scheduler preconditioning,
reported as `WarmupMs`, before samples are accepted.

| Workload | Box3D | PhysX | Ratio | Result |
|---|---:|---:|---:|---|
| 128 falling spheres, 4 workers | 146.86 us | 192.40 us | 0.763 | 1.31x faster |
| 1024 falling spheres, 4 workers | 579.24 us | 1313.98 us | 0.441 | 2.27x faster |
| Create and destroy 128 bodies | 75.91 us | 4342.55 us | 0.017 | 57.2x faster |
| Create and destroy 1024 bodies | 764.73 us | 34887.90 us | 0.022 | 45.6x faster |
| 128 closest raycasts, scalar API | 16.59 us | 17.63 us | 0.941 | 5.9% lower latency |
| 128 closest raycasts, batch API | 18.11 us | 22.94 us | 0.790 | 21.0% lower latency |
| 1024 closest raycasts, batch API | 51.02 us | 217.29 us | 0.235 | 4.26x faster |
| Sphere overlap returning 25 stable-order hits | 0.809 us | 0.897 us | 0.902 | 9.8% lower latency |

The strict comparator passes every median, repetition-tail, bootstrap, policy, signature, and stability gate. The largest Box3D/PhysX
bootstrap upper bound is 0.945 for scalar raycasting, and the largest coefficient of variation in either report is 3.938%. The comparator
requires the reported worker count, notification policy, simulation policy, quality counters, query cardinality, and batch warmup to match;
missing or inconsistent counters fail the comparison.

The closest-ray path retains validation, deterministic synchronization, full hit data, and initial-overlap semantics. A cached
`IWorldQueries` view removes repeated world lookup for hot callers, a generational native-ID table replaces user-data round trips, and the
native closest callback writes its result directly. Axis-aligned traversal prevalidates child bounds while the parent is cache-hot, visits
the only matching child without calculating an irrelevant ordering key, and revalidates deferred children after a closer hit clips the ray.
Tests compare closest and all-hit results for all six axis directions, collision filters, initial overlaps, arbitrary rays, and recycled
shape IDs. The current native median is 15.02 us for 128 rays with 11 node visits and one leaf visit; the complete public path is 16.59 us.
The validation-only floor is 1.46 us for 128 rejected requests. A valid empty-world query takes 3.87 us.

The exact overlap path classifies box hulls once at creation, uses sphere-versus-box distance with native overlap slop, and retains GJK for
general hulls. Native overlap takes 0.552 us while visiting 107 nodes and accepting 25 leaves. The compact stable-order public result takes
0.809 us; the full `QueryHit` diagnostic takes 0.920 us.

Rejected alternatives remain useful negative evidence. Removing synchronization won a narrow microbenchmark but permitted mutation races;
spin-based synchronization deadlocked recursive paths; larger ray-stack entries, tail traversal, generic entry-fraction ordering, and an
uncontended reader protocol regressed wall time; bypassing validation and splitting the wrapper hot/cold path were neutral; and expanding
the native result ABI for provider identity was not repeatably beneficial. Release IPO, compact native identities, direct closest dispatch,
child prevalidation, and the allocation-free batch surface were retained because they improved complete public calls without weakening the
contract. Auditing upstream `main` at `3fc20f5b453ba9e14cdf54ecafa87a2a4bcdf53c` found no equivalent query changes.

The final MSVC Release linker map and disassembly confirm that the closest-ray hot path retains packed SIMD validation, one native query,
dense native-ID and generation checks, and direct result writes after link-time code generation. It contains no allocator calls, string
work, native user-data round trips, or virtual dispatch in result mapping. Private slot-size assertions keep frequently traversed body,
shape, joint, character, material, and cooked-shape identities within their documented cache budgets while cold configurations and owned
resources remain in parallel storage.

Batch queries keep workloads of 128 requests or fewer on the caller thread; larger batches allocate at least 128 requests to each AZ job.
This avoids thread wake-up overhead for short batches while preserving four-way execution for 512- and 1024-query workloads. The worker
partitions establish and restore the deterministic floating-point environment once per partition.

## Linux corroboration

The final Clang 21 Release build is non-unity. Runtime and editor suites, 64 repetitions of the determinism and concurrent-batch stress
tests, and an installed-engine consumer all pass. Affinity-matched 30-repetition reports use four distinct physical cores. Every Box3D
median is below its PhysX match: simulation ratios are 0.639-0.740, lifecycle ratios are 0.125-0.172, and query ratios are 0.450-0.969.
Every Box3D workload remains below the 5% variation limit. One PhysX 128-body lifecycle repetition was preempted by the Windows host and
raises that reference workload's coefficient of variation to 7.495%; therefore Windows remains the timing authority instead of weakening
the comparator's stability gate. The preserved reports are `build/linux_box3d/BenchmarkResults/Box3D.FinalAffinity.Samples30.json` and
`build/linux_box3d/BenchmarkResults/PhysX.FinalAffinity.Samples30.json`.

## Authored workload capture

`AutomatedTesting::Box3DTests_Benchmark` creates an 8 by 8 by 4 stack of dynamic boxes, waits for 120 simulation ticks, then keeps the bodies
awake while capturing 30 real DX12 CPU frames through the engine profiling capture bus. The test disables VSync and frame limiting, and
rejects null-renderer zeroes, missing output, and any sample with fewer than 95% of the bodies awake. The current run kept at least 255 of
256 bodies awake and measured 5.329 ms median and 8.243 ms p95 whole-Editor frame time. The capture is diagnostic because rendering and
Editor overhead are intentionally included.

The provider supplies Physics profiler scopes for world stepping, native stepping and tasks, event gathering, bodies, shapes, joints,
characters, queries, effects, materials, cooking, recording, replay, debug drawing, and static-tree rebuilds. Opt-in datapoints expose body,
shape, joint, island, contact, task, memory, and separating-axis-cache statistics. The platform currently rejects Windows Performance Recorder
CPU tracing with policy error `0xc5585011`; engine-native captures and native step profiles remain available.

## Reproduction

```powershell
cmake --build build/box3d_automatedtesting --config Release --target Box3D.Tests PhysX5.Tests -- /m:1

build/box3d_automatedtesting/bin/release/AzTestRunner.exe `
    build/box3d_automatedtesting/bin/release/Box3D.Tests.Gem.dll AzRunBenchmarks `
    "--benchmark_filter=^Box3D/(Step|Lifecycle|Query/(RaycastGrid|RaycastClosestBatchGrid|OverlapSphereGrid))" `
    --benchmark_repetitions=30 --benchmark_out_format=json --benchmark_out=Box3D.json

build/box3d_automatedtesting/bin/release/AzTestRunner.exe `
    build/box3d_automatedtesting/bin/release/PhysX5.Tests.Gem.dll AzRunBenchmarks `
    "--benchmark_filter=^PhysX/(Step|Lifecycle|Query/(RaycastGrid|RaycastClosestBatchGrid|OverlapSphereGrid))" `
    --benchmark_repetitions=30 --benchmark_out_format=json --benchmark_out=PhysX5.json

python Gems/Physics/Box3D/Code/Tests/compare_benchmarks.py Box3D.json PhysX5.json `
    --worker-count 4 --minimum-repetitions 30 --maximum-ratio 1.0

ctest --test-dir build/box3d_automatedtesting -C Profile `
    -R "AutomatedTesting::Box3DTests_Benchmark" --output-on-failure
```

The comparator exits with failure while a required workload exceeds its threshold, a workload signature differs, either provider exceeds
the variation gate, or the bootstrap upper bound regresses. The current passing raw reports are
`build/windows/BenchmarkResults/Box3D.CurrentFinal.Samples30.json` and
`build/windows/BenchmarkResults/PhysX.CurrentFinal.Samples30.json`. Native and fixed-cost attribution is retained in
`build/windows/BenchmarkResults/Box3D.CurrentFinal.Diagnostics.Samples30.json`. The Release linker map used for assembly inspection is
`build/windows/BenchmarkResults/Box3D.Release.map`.
