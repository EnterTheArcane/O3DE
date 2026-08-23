# Jolt shipping qualification

This document is the authoritative closure matrix for the third deep audit. It records shipping contracts and the evidence required
to close them; a historical passing build or test does not close a row whose behavior, workload, or instrumentation was incomplete.

The remediation baseline is branch `jolt`, revision `42dbab82f32f945b293e7c019c5a07282eb3df24`, tree
`f900eab995dd5a49836aa4112b520a2bf9f0517a`. Raw JSON, traces, disassembly, frame samples, and transient build metadata belong under
`build/jolt-production-readiness/` and are not committed.

## State contract

- **Open**: the defect or missing evidence is confirmed and its correction has not passed its complete gate.
- **Implemented**: the product correction exists, but one or more qualification gates remain.
- **Closed**: the final product, test, and evidence gates pass at the recorded qualification revision.
- **External gate**: the local design and cross-build checks pass, but required hardware or platform execution is unavailable.

No row may become **Closed** from source inspection alone. A row may cite a shared test or artifact only when that evidence exercises
the row's complete contract.

## Third-audit closure matrix

| Finding | Shipping contract | Stage | State and required evidence |
|---|---|---:|---|
| `J3-AUD-001` | One Runtime root is published only after complete activation and revoked once before draining; borrowed capabilities require quiescent teardown. | 2 | **Open** — replacement, concurrent acquisition, failed activation, teardown, assembly, and acquisition-latency proof required. |
| `J3-AUD-002` | Handles cannot alias across Runtime replacement, simultaneous isolated Runtimes, slot reuse, or generation exhaustion. | 2 | **Open** — module-lifetime per-kind generation-domain tests required. |
| `J3-AUD-003` | Every mutable rollback participant has one owning world and cannot enter concurrent world transactions. | 3 | **Open** — ownership, concurrent restore rejection, and complete cache-restoration proof required. |
| `J3-AUD-004` | Entity resource teardown prepares without mutation, commits once, and never abandons a live native resource. | 3 | **Open** — rigid, static, character, path, constraint, and vehicle component/direct-capability tests required. |
| `J3-AUD-005` | Vehicle creation on an unadded chassis fails without changing counts, revisions, or ownership. | 3 | **Open** — focused failure-atomicity and lifecycle proof required. |
| `J3-AUD-006` | Automatic simulation runs at physics tick order and publishes transforms before attachment and pre-render consumers. | 4 | **Open** — tick-order and same-frame transform visibility tests required. |
| `J3-AUD-007` | Native acquisition is provider-owned, versioned, fingerprinted from actual objects/options, and rejects foreign targets or populations. | 1 | **Open** — clean/cached fetch, foreign-target, patch, fingerprint, license, source/install, and offline tests required. |
| `J3-AUD-008` | Every installed public out-of-line callable is exported or private, with no native leakage. | 4 | **Open** — installed modular consumer must call every public symbol. |
| `J3-AUD-009` | ISA flags follow each target architecture, retain upstream exclusions, enforce SSE4.1 on x86, and disable AVX2/FMA. | 1 | **Implemented** — MSVC and clang-cl x64 native-object commands prove the exact SSE4.1 floor; Xcode slice generation and Apple execution remain external gates. |
| `J3-AUD-010` | IPO is support-checked, private to its owner, preserves platform guards, and is identical across source/install delivery. | 1 | **Implemented** — clang-cl modular IPO on/off and monolithic-off builds pass; MSVC modular `/GL` and `/LTCG` pass. Fresh installed-delivery and external-platform equality remain final gates. |
| `J3-AUD-011` | Jolt does not require unrelated installed-Python metadata or non-hermetic package provisioning. | 1 | **Implemented** — the branch-local Python install and launcher-discovery workarounds are removed, and the public source consumer passes. Fresh offline installed-consumer proof remains a final gate. |
| `J3-AUD-012` | Public retained-shape ABI is precision-independent and meets the selected representation's allocation and query-cost gates. | 4 | **Open** — 32/64-bit precision layout, modular ABI, allocation, and 30-process query comparison required. |
| `J3-AUD-013` | Contact points are addressable only through the immutable batch that produced their event. | 4 | **Open** — 64-bit batch provenance, cross-batch rejection, reuse, overflow, serialization, and stale-event tests required. |
| `J3-AUD-014` | Entity request buses have one owner and deterministic result routing. | 4 | **Open** — `Single` policy audit and duplicate-handler rejection tests required. |
| `J3-AUD-015` | World lifecycle, simulation, queries, rollback, and diagnostics use separate ownership/scoping boundaries. | 4 | **Open** — aggregate removal, bus-policy, BehaviorContext scope, and script-call parity proof required. |
| `J3-AUD-016` | Ragdoll definitions construct every advertised constraint type and reject unsupported drive mappings before mutation. | 5 | **Open** — all 13 types, linked local identities, providers/paths, archives, snapshots, editor, and pose-drive tests required. |
| `J3-AUD-017` | Physical characters expose state-preserving add/remove simulation operations. | 5 | **Open** — identity, state, snapshot, event, and 1/4/8-worker replay tests required. |
| `J3-AUD-018` | One Path resource owns geometry and transform; active updates commit transactionally at a safe boundary. | 5 | **Open** — translation/rotation frame update, scale rebuild, state preservation, and failure rollback tests required. |
| `J3-AUD-019` | Custom-shape source dependencies participate in analysis invalidation and deterministic job/product fingerprints. | 5 | **Open** — real dependency edit/delete/recovery and recook tests required. |
| `J3-AUD-020` | Every claimed clang-cl ASan configuration is instrumented and deployed correctly or rejected during configure. | 1 | **Open** — Debug/Profile flags, runtime deployment, detection sentinel, and normal Jolt suite required. |
| `J3-AUD-021` | Operation creation and completion reclamation are bounded and never wait for unrelated work. | 6 | **Open** — complexity, cancellation, concurrent creator, shutdown, and worker-saturation tests/benchmarks required. |
| `J3-AUD-022` | Event and operation caches have count/byte ceilings and accurately report live, cached, outstanding, retained, and high-water memory. | 6 | **Open** — burst/release/reuse/oversize/teardown statistics and allocator evidence required. |
| `J3-AUD-023` | Steady-state allocation claims cover native, wrapper, AZ job, and closure domains. | 6 | **Open** — warm 1/4/8-worker stepping, automatic worlds, queries, and CPU Hair allocation evidence required. |
| `J3-AUD-024` | Contact publication meets the declared contention gate without weakening deterministic order or event completeness. | 6 | **Open** — wait/hold/growth instrumentation and contact-density W1/W4/W8 evidence required before changing the mutex design. |
| `J3-AUD-025` | `validate.py review/full` schedules, validates, and retains both native and AutomatedTesting performance artifacts. | 7 | **Open** — dry-run scheduling and fresh real-run artifact proof required. |
| `J3-AUD-026` | Every scenario has meaningful minimum assertions and a parsed, verified, retained result envelope. | 7 | **Open** — corrected saved/filter/reflection/gallery/stress tests and zero-check failure sentinel required. |
| `J3-AUD-027` | Requested and effective 1/4/8-worker runs compare complete world/subsystem digests and semantic outputs. | 7 | **Open** — rigid, constraint, character, vehicle, ragdoll, soft-body, custom-provider, CPU-Hair, rollback, and event proof required. |
| `J3-AUD-028` | Performance qualification is current, broad, statistically valid, and never trades away accuracy or deterministic contracts. | 7 | **Open** — 30-process matched/absolute/tail, allocation, pool, ECS, contention, and compiler evidence required. |
| `C-AUD-001` | Restoring a group filter refreshes every affected world's contact cache after commit. | 3 | **Open** — cross-world restore and next-step contact/event proof required. |
| `C-AUD-002` | Static-body component teardown retains ownership after a failed destroy. | 3 | **Open** — direct dependency-veto and later successful cleanup proof required. |
| `C-AUD-003` | Path component teardown retains ownership after a failed destroy. | 3 | **Open** — direct dependency-veto and later successful cleanup proof required. |
| `C-AUD-004` | Active paths have one authoritative transform and cannot snapshot inconsistent geometry and frames. | 5 | **Open** — activation skew and live-update tests required. |
| `C-AUD-005` | Gameplay scripts cannot own world lifecycle, explicit stepping, or restore. | 4 | **Open** — reflection audit and attempted Common-scope invocation rejection required. |
| `C-AUD-006` | Creating an async operation never joins or waits for unrelated queued/running work. | 6 | **Open** — blocked-worker latency and lock-context watchdog tests required. |
| `C-AUD-007` | Parent/child allocator byte accounting uses the same size on allocation and deallocation in every malloc/ASan-header mode. | 1 | **Open** — `ChildAllocatorSchema` and `NativeAllocator` round-trip tests required. |

## Qualification platforms

- **Required:** fresh Windows MSVC and clang-cl/Ninja matrices, then a dedicated `--no-local` WSL ext4 Clang clone.
- **External gates:** macOS, native Linux, ARM hardware, iOS, Android, WebAssembly, and additional GPU/CPU architectures.
- **Known external blocker:** GCC currently fails before Jolt on existing engine warnings promoted to errors; record it without treating it as
  Clang/WSL evidence.

The final qualification revision must be clean, contain reviewable phase commits, and match every committed command/result claim. No
remote push or branch split belongs to this qualification.
