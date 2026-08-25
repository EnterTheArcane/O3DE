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
| `J3-AUD-001` | One Runtime root is published only after complete activation and revoked once before draining; borrowed capabilities require quiescent teardown. | 2 | **Implemented** — the single private root, complete lifetime tests, modular export check, clang-cl assembly, and focused latency gate pass. MSVC and monolithic/IPO repetitions remain final qualification gates. |
| `J3-AUD-002` | Handles cannot alias across Runtime replacement, simultaneous isolated Runtimes, slot reuse, or generation exhaustion. | 2 | **Implemented** — module-lifetime per-kind generation domains, exhaustion rejection, and integration tests cover every handle family across simultaneous and sequential Runtimes, slot reuse, retained results, events, and asynchronous results. Final MSVC and packaging qualification remain. |
| `J3-AUD-003` | Every mutable rollback participant has one owning world and cannot enter concurrent world transactions. | 3 | **Implemented** — nonzero-state callback and group-filter registrations are reference-counted within one owning world; a second-world bind fails before mutation. Zero-state registrations declare deterministic, thread-safe sharing. Focused tests cover rejection without digest/count changes, same-world reuse, transfer after release, concurrent stateless use, and unregistration barriers. Final MSVC, unload/reload, and stress qualification remain. |
| `J3-AUD-004` | Entity resource teardown prepares without mutation, commits once, and never abandons a live native resource. | 3 | **Implemented** — component clients only discover dependencies during preparation; Runtime reservations cover complete direct and component-owned closures before mutation. Rigid, static, character, path, constraint, vehicle, retained-shape, soft-body ownership, veto, callback-reentrancy, and pending/committed retry tests pass. Final MSVC and lifecycle stress qualification remain. |
| `J3-AUD-005` | Vehicle creation on an unadded chassis fails without changing counts, revisions, or ownership. | 3 | **Implemented** — wheeled, motorcycle, and tracked construction reject an unadded chassis before native or wrapper mutation. The focused test preserves the world digest and resource counts, then proves the chassis can still be added, removed, and destroyed normally. Final MSVC qualification remains. |
| `J3-AUD-006` | Automatic simulation runs at physics tick order and publishes transforms before attachment and pre-render consumers. | 4 | **Implemented** — the provider runs at `TICK_PHYSICS_SYSTEM`; a moving-body integration test observes the old transform immediately before physics and the updated transform at both attachment and pre-render orders in the same tick. Final MSVC qualification remains. |
| `J3-AUD-007` | Native acquisition is provider-owned, versioned, fingerprinted from actual objects/options, and rejects foreign targets or populations. | 1 | **Open** — clean/cached fetch, foreign-target, patch, fingerprint, license, source/install, and offline tests required. |
| `J3-AUD-008` | Every installed public out-of-line callable is exported or private, with no native leakage. | 4 | **Implemented** — all seven previously hidden callables are exported, present in the modular import library, and invoked by the clang-cl Release public consumer. The validator requires every call so the proof cannot regress silently. Fresh installed modular/monolithic and MSVC consumers remain final gates. |
| `J3-AUD-009` | ISA flags follow each target architecture, retain upstream exclusions, enforce SSE4.1 on x86, and disable AVX2/FMA. | 1 | **Implemented** — MSVC and clang-cl x64 native-object commands prove the exact SSE4.1 floor; Xcode slice generation and Apple execution remain external gates. |
| `J3-AUD-010` | IPO is support-checked, private to its owner, preserves platform guards, and is identical across source/install delivery. | 1 | **Implemented** — clang-cl modular IPO on/off and monolithic-off builds pass; MSVC modular `/GL` and `/LTCG` pass. Fresh installed-delivery and external-platform equality remain final gates. |
| `J3-AUD-011` | Jolt does not require unrelated installed-Python metadata or non-hermetic package provisioning. | 1 | **Implemented** — the branch-local Python install and launcher-discovery workarounds are removed, and the public source consumer passes. Fresh offline installed-consumer proof remains a final gate. |
| `J3-AUD-012` | Public retained-shape ABI is precision-independent and meets the selected representation's allocation and query-cost gates. | 4 | **Implemented** — the public lease is pointer-sized in every precision mode; its bounded private pool passes steady-state allocation, lifetime, clang-cl Release code-generation, and 30-process retained-query gates. Double-precision, MSVC, and installed modular/monolithic builds remain final qualification gates. |
| `J3-AUD-013` | Contact points are addressable only through the immutable batch that produced their event. | 4 | **Implemented** — module-lifetime 64-bit identities bind contacts to their producing batch, never wrap, and reject live foreign or recycled storage before point-range access. Copy/move, Runtime shutdown, overflow, serialization, reflection, and modular export tests pass. Final MSVC and installed-consumer qualification remain. |
| `J3-AUD-014` | Entity request buses have one owner and deterministic result routing. | 4 | **Implemented** — every entity request interface declares `Single` locally, every notification interface declares `Multiple`, and the shared EBus container rejects a duplicate single handler instead of silently replacing the owner. Focused dispatch, fanout, and clang-cl Release application tests pass; final MSVC and install gates remain. |
| `J3-AUD-015` | World lifecycle, simulation, queries, rollback, and diagnostics use separate ownership/scoping boundaries. | 4 | **Implemented** — five singular buses separate lifecycle/configuration, simulation, queries, rollback, and diagnostics. Reflection scopes destructive/control operations as Automation and read/query diagnostics as Common. A static parity gate accounts for all 59 reflected script operations, and the focused null-renderer scenario passes. The complete application matrix remains. |
| `J3-AUD-016` | Ragdoll definitions construct every advertised constraint type and reject unsupported drive mappings before mutation. | 5 | **Implemented** — all 13 native constraint alternatives construct through direct Runtime and authored component paths. Stable local UUIDs replace world handles for linked gear and rack-and-pinion constraints; custom providers and paths remain retained; invalid identities, links, providers, paths, poses, and drive combinations fail before mutation. JSON, binary/editor configuration, snapshot restore, dependency lifetime, simulation membership, and exact worker-digest tests pass in the 318-test clang-cl Release non-unity suite. Final MSVC and application qualification remain. |
| `J3-AUD-017` | Physical characters expose state-preserving add/remove simulation operations. | 5 | **Implemented** — character-specific add, remove, and membership queries preserve the character handle, backing body handle, transform, linear/angular velocity, move-event subscription, and detached snapshot behavior. Component disable/re-enable preserves resources without destruction notifications, the focused scenario checks the membership transition, and exact full-state digests match at effective 1/4/8 workers. The 322-test clang-cl Release non-unity suite and fresh source consumer pass; installed-engine, MSVC, and application qualification remain. |
| `J3-AUD-018` | One Path resource owns geometry and transform; active updates commit transactionally at a safe boundary. | 5 | **Implemented** — each Path retains canonical local geometry and one authoritative `AZ::Transform`. Queued updates coalesce before a safe-boundary transaction across every world; translation/rotation updates dependent frames, uniform-scale changes rebuild native geometry, and failed preparation leaves the previous state active. Direct, scene, ragdoll, custom-provider, component, snapshot, modular-consumer, and exact 1/4/8-worker tests pass. Final MSVC and application qualification remain. |
| `J3-AUD-019` | Custom-shape source dependencies participate in analysis invalidation and deterministic job/product fingerprints. | 5 | **Implemented** — scene sources declare canonical asset-database-relative dependency paths. `CreateJobs` publishes their exact absolute source dependencies and fingerprints current contents plus captured provider identity/version; `ProcessJob` independently reanalyzes and rejects missing files or provider output that disagrees with the declared path/hash set. Runtime cooking canonicalizes ordering and duplicates, while compiled scene data drops authoring-only path storage. `EditorAssetBuilderTests.TracksCustomShapeDependencyEditsDeletionAndRecovery` proves dependency-only fingerprint changes, stale-provider rejection, deletion tracking, successful recovery, provider-version invalidation, and recooking without changing the parent source. Invalid provider/path/hash contracts fail transactionally. Final Asset Processor application and MSVC qualification remain. |
| `J3-AUD-020` | Every claimed clang-cl ASan configuration is instrumented and deployed correctly or rejected during configure. | 1 | **Closed** — clang-cl 22.1.8 permits only Profile ASan trees because the Windows ASan runtime rejects the debug CRT; a fresh Ninja build proved compile instrumentation, dynamic runtime/thunk linkage, runtime deployment, the full Jolt and AzCore suites, and a symbolized heap-use-after-free sentinel. |
| `J3-AUD-021` | Operation creation and completion reclamation are bounded and never wait for unrelated work. | 6 | **Implemented** — creation performs one free-list lookup and never scans, joins, or waits for active work. Terminal detached operations nominate themselves for completion-driven maintenance, and intrusive links remove active and reap entries in constant time. Profile creation of 2,048 operations with all work blocked improved from a 5,011 us median to 147 us; optimized Release measures 122 us. Concurrent creation, detached cancellation/completion, shutdown, and saturated-worker watchdog tests pass. Cache byte/count ceilings and complete memory telemetry remain `J3-AUD-022`. Final MSVC and application qualification remain. |
| `J3-AUD-022` | Event and operation caches have count/byte ceilings and accurately report live, cached, outstanding, retained, and high-water memory. | 6 | **Implemented** — event batches retain at most four records, 8 MiB total, and 4 MiB per record. Operations retain at most 64 records and 1 MiB per type, with a 512 KiB record ceiling. Reflected telemetry partitions live storage into cached and outstanding count/bytes and reports resettable high-water values. Focused 1,024-record and 100,000-result burst/release/reuse/oversize/shutdown tests pass. Final Release, MSVC, application, and stress qualification remain. |
| `J3-AUD-023` | Steady-state allocation claims cover native, wrapper, AZ job, and closure domains. | 6 | **Implemented** — provider job tasks use fixed owner-allocated slots with a bounded retirement-handoff generation; automatic multiworld jobs use bounded stack storage; CPU Hair retains at most 16 generated wrappers and their buffer leases. Warmed parallel stepping, automatic worlds, batch queries, and Hair report zero `SystemAllocator` or `ThreadPoolAllocator` allocations; Hair also reports zero native allocations and stable wrapper creation/retained bytes. Final Release 1/4/8-worker and application evidence remain. |
| `J3-AUD-024` | Contact publication meets the declared contention gate without weakening deterministic order or event completeness. | 6 | **Implemented** — 64 stable body/subshape-pair shards replace the global producer lock; effective single-worker worlds use a direct lock-free publication path. Thirty fresh clang-cl Release processes preserve every contact/point count while aggregate wait remains 0.014%/0.291% at 128-body W4/W8 and 0.697%/3.986% at 1,024-body W4/W8. All rows pass 5% CV, W8 is at most 1.025x W4, and medians improve 4.66–37.65% from the single-lock baseline. Exact normalized event/point streams and final world digests match across effective 1/4/8 workers. Final MSVC, application, and stress qualification remain. |
| `J3-AUD-025` | `validate.py review/full` schedules, validates, and retains both native and AutomatedTesting performance artifacts. | 7 | **Implemented** — review mode captured three fresh processes for step, query, tail, capability, query-diagnostic, and all rollback workloads; the authored Release application scenario retained its verified result envelope, engine metadata, and 30 positive Jolt update samples. Empty native filters and incomplete application samples fail. Final full-mode 30-process evidence remains. |
| `J3-AUD-026` | Every scenario has meaningful minimum assertions and a parsed, verified, retained result envelope. | 7 | **Implemented** — all 20 main scenarios and the benchmark scenario have explicit minimum assertion counts. Legacy aggregated scenarios now record independent checks; zero-check, duplicate-name, missing, malformed, truncated, tampered, and failed-check evidence is rejected. Static parity validates all 21 registrations and the checked-in gallery/stress manifests. Final broad application execution remains. |
| `J3-AUD-027` | Requested and effective 1/4/8-worker runs compare complete world/subsystem digests and semantic outputs. | 7 | **Implemented** — the focused C++ suite executes actual effective 1/4/8-worker worlds for rigid bodies, constraints, characters, vehicles, ragdolls, soft bodies, paths, events, custom providers, CPU Hair, snapshots, and full state. Application stress rejects worker clamping and requires exact complete-digest equality. The five focused determinism tests pass in clang-cl Release; final full stress execution remains. |
| `J3-AUD-028` | Performance qualification is current, broad, statistically valid, and never trades away accuracy or deterministic contracts. | 7 | **Implemented** — one runner now owns fresh-process warmup, physical-core affinity, raw capture, source/binary fingerprints, context validation, matched comparison, Jolt-only correctness/allocation gates, and empty-filter rejection. Review-mode native and authored application smokes pass. Final 30-process MSVC/clang-cl matched, absolute, tail, allocation, ECS, stress, and compiler evidence remains. |
| `C-AUD-001` | Restoring a group filter refreshes every affected world's contact cache after commit. | 3 | **Implemented** — successful custom-filter restore invalidates affected contact caches and activates non-static bodies while the world is already locked. Mutable filters can belong to only one world, and a global-only snapshot test proves restored callbacks re-enable its body and publish a new contact on the next step. Final MSVC qualification remains. |
| `C-AUD-002` | Static-body component teardown retains ownership after a failed destroy. | 3 | **Implemented** — an ordinary disable is atomic under a dependency veto, while mandatory entity teardown discovers the same vetoed dependency closure and completes it without abandoning the body. Final MSVC qualification remains. |
| `C-AUD-003` | Path component teardown retains ownership after a failed destroy. | 3 | **Implemented** — mandatory path teardown ignores a client veto only after complete discovery, reserves the dependent native constraint closure, and commits the path after its constraints. Final MSVC qualification remains. |
| `C-AUD-004` | Active paths have one authoritative transform and cannot snapshot inconsistent geometry and frames. | 5 | **Implemented** — activation creates native geometry and its authoritative transform together; updates publish geometry, frames, path state, and world configuration revisions in one commit. Pre-update snapshots reject the changed topology, while byte-identical post-update archives and public state match across effective 1/4/8 workers. Final MSVC and application qualification remain. |
| `C-AUD-005` | Gameplay scripts cannot own world lifecycle, explicit stepping, or restore. | 4 | **Implemented** — lifecycle/configuration, explicit stepping, and rollback buses carry Automation scope while query and diagnostics buses alone carry Common scope. Reflection tests assert each boundary, retained-shape operations remain C++-only, script parity rejects stale or misplaced calls, and the focused clang-cl Release scenario passes. |
| `C-AUD-006` | Creating an async operation never joins or waits for unrelated queued/running work. | 6 | **Implemented** — a blocked-worker benchmark keeps every prior operation outstanding while timing creation, directly exposing the former active-list rescan. Creation no longer invokes maintenance. Completion-driven reaping considers only nominated terminal records and starts an already-ready completion outside the pool lock. Detached queued cancellation, concurrent creators, ordinary worker-aware waits, and the subprocess saturation watchdog pass. Final MSVC qualification remains. |
| `C-AUD-007` | Parent/child allocator byte accounting uses the same size on allocation and deallocation in every malloc/ASan-header mode. | 1 | **Closed** — `SystemAllocator` now reports and accounts the exact size returned by deallocation. `ChildAllocatorSchema` and Jolt `NativeAllocator` round trips pass with the Profile ASan header and with `AZCORE_USE_MALLOC_SYSTEM_ALLOCATOR=ON` without that header. |

## Stage 2 evidence

`J3-AUD-001` uses one file-local atomic Runtime root owned by `Jolt.API`; none of the 21 public capability classes owns storage.
The five focused `NativeRuntimeTests` cover exact adjusted capability addresses, stable concurrent readers, sequential replacement, failed
activation, isolation and duplicate-owner rejection, ordinary teardown, and revocation before a blocked provider operation can drain.
The clang-cl 22.1.8 modular Release suite passes all 254 tests, and the quick validator passes all 80 public-header probes and every
static validation step. The rebuilt DLL exports no capability or Runtime-root data symbol.

Optimized `RuntimeConfiguration::Get()` and `Vehicles::Get()` each have a zero-byte stack frame, three instructions, and no call,
branch, conversion, or copy candidate. The final affinity-pinned 30-repetition no-IPO checkpoint measures capability acquisition at a
1.01 ns median with 1.99% CV, below the 3 ns and 5% variability gates. Raw benchmark, disassembly, validator, and test evidence is
retained beneath `build/jolt-production-readiness/stage2/` and is intentionally not committed.

## Stage 3 rollback ownership evidence

Mutable rollback state is owned per extension registration. Binding a registration whose `GetStateByteCount()` is nonzero claims one
world at a cold configuration boundary; additional resources in that world share a checked reference count. A different world is
rejected before revisions, resource counts, or native state change. A zero byte count is the explicit stateless contract and permits
deterministic, thread-safe concurrent use. Native callback invocation still uses the retained direct pointer without a registry lookup
or ownership lock.

The clang-cl 22.1.8 Debug non-unity suite passes 300 tests with zero failures and one intentionally disabled test. The focused evidence
is `MutableRollbackParticipantRejectsASecondWorldWithoutMutation`,
`MutableGlobalCallbackTransfersOnlyAfterItsOwningWorldReleasesIt`,
`StatelessRollbackParticipantCanServeConcurrentWorlds`, `MutableGroupFilterRejectsASecondWorldUntilReleased`, and
`StatelessGroupFilterCanServeMultipleWorlds`. `GlobalGroupFilterRestoreRefreshesAffectedBodies` captures only global callback state,
then proves restore refreshes native contact state and produces the expected begin event. Group-filter mutation releases its filter lock
before visiting worlds, and restore uses the already-held world lock instead of reacquiring it. Transient test and validator evidence is
retained beneath `build/jolt-production-readiness/stage3/`.

## Stage 3 transactional teardown evidence

Component preparation is non-mutating and visits every registered client even when the first or middle client rejects an ordinary
request. Runtime preflight augments those component observations with the complete native body, constraint, vehicle, character, and
path dependency closure. Only a successful reservation marks resources as destroying. `Destroying` is published while the handle is
valid, the reserved native commit runs exactly once, and `Destroyed` is published after invalidation; all notifications execute outside
world and registry locks. Observer callbacks resolve the recorded entity and component identity before dereferencing their context, so
component removal during a notification cannot leave a dangling callback.

Mandatory teardown transfers exceptional pending or committed work to the Runtime. The deferred record owns its plan, retries the
reservation or commit at a safe boundary, and retains soft-body definition/material ownership until cleanup succeeds. Collider and
ragdoll shape sets likewise move to Runtime-owned retry storage when immutable leases temporarily retain them. A successful deferred
body or character commit immediately retries those shape sets, preventing released body references from leaving wrapper-owned shapes
stranded.

The clang-cl 22.1.8 Debug non-unity consolidated suite passes all 308 tests. The 34 focused `ComponentTests` include
`DependencyPreparationVisitsEveryClientAroundAnyVeto`,
`BodyDeactivationDestroysDirectDependenciesAndDefersRetainedShapes`,
`StaticBodyRequestedDisableIsAtomicAndMandatoryTeardownCannotBeVetoed`,
`RuntimeOwnsAndRetriesAnExceptionalReservedDestruction`,
`RuntimeCompletesAPendingDestructionReservation`,
`RuntimeRetainsSoftBodyResourcesAcrossDeferredDestruction`,
`PathDeactivationDestroysDirectConstraintDependencies`,
`CharacterDeactivationDestroysDirectBodyDependencies`, and
`ConstraintDeactivationDestroysDirectParentConstraints`. Body, path, constraint, and vehicle notification handlers attempt reentrant
destruction, proving the reservation rejects conflicting mutation without blocking or deadlocking.

## Stage 4 physics tick-order evidence

`SystemComponent` now reports `AZ::ComponentTickBus::TICK_PHYSICS_SYSTEM`, matching the engine's dedicated provider position instead of
the generic default. `ComponentTests.SystemTickPublishesTransformsBeforeAttachmentAndPreRender` drives a real dynamic body through
`AZ::TickBus` and captures its entity transform at the immediately preceding, attachment, and pre-render tick orders. The pre-physics
observer sees the original transform; attachment and pre-render observers see the same newly simulated transform. The focused test and
the complete clang-cl 22.1.8 Debug non-unity suite pass.

## Stage 4 modular public-export evidence

`ReflectDebugDraw`, `ReflectDiagnostics`, `ReflectEvents`, `ReflectQueries`, `ReflectWorldQueries`,
`ShapeQueryFaceBuffers::GetQueryFace`, and `ShapeQueryFaceBuffers::GetTargetFace` are exported by `Jolt.API`. The public headers include
their own export definition directly, and the modular import library contains all seven symbols. The source-tree public consumer calls
every function without native headers or types, links against the clang-cl 22.1.8 Release modular provider, runs a simulation, and
exits successfully. Static validation fails if any of those calls is removed. Fresh installed modular/monolithic consumers and the
MSVC matrix remain final qualification gates.

## Stage 4 retained-shape ABI evidence

`TransformedShape` now contains one opaque record pointer, so its public size and alignment equal those of a pointer without depending on
native precision, alignment, or layout. Native state and retained metadata live in a private 160-byte single-precision record; the
double-precision build asserts a maximum of 192 bytes and remains part of the final build matrix. Each World retains at most 64 released
records in an intrusive free list. The free-list link consumes existing tail padding, and an unused World does not allocate pool storage.

Copying a lease performs one relaxed atomic increment. Releasing a non-final copy performs one acquire-release atomic decrement and a
conditional branch. Final release decrements shape ownership exactly once, clears the native reference, returns the record to the bounded
cache or deletes it, releases the teardown-safe lease state, and retries deferred shape destruction. The focused allocation test warms 16
simultaneous leases, then completes 32 additional 16-lease batches without changing `SystemAllocator::NumAllocatedBytes()`.

The complete clang-cl 22.1.8 Debug and Release non-unity suites each pass all 310 tests. The Release test DLL was measured in 30 isolated
processes with a 0.2-second minimum per query operation:

| Retained pair query | Inline median | Pooled median | Median change | Pooled CV |
|---|---:|---:|---:|---:|
| Collision | 148.26 ns | 148.89 ns | +0.42% | 3.61% |
| Cast | 142.10 ns | 139.26 ns | -2.00% | 1.10% |

Both workloads satisfy the selected representation's maximum 1% median regression and 5% variability gates. Optimized object-code
inspection reports zero stack bytes and no calls for acquisition, non-final release, and record validation. Acquisition is four
instructions; release is five instructions with one conditional branch; validation is 16 instructions with three validation branches.
The final `Jolt.API.dll` SHA-256 is `DE798610C4D6EDB6B0F809B6036777A39551E0711EA4A4C540BC12E33F77FA49`; the measured
`Jolt.Tests.Gem.dll` SHA-256 is `577770F670B2338C5C7710F64C04489B472AE3E8D76CFB52C661084E307C0C01`. Raw reports and
disassembly remain beneath `build/jolt-production-readiness/stage4/` and are intentionally not committed.

## Stage 4 contact-event provenance evidence

Every acquired `EventBatch` receives a nonzero identity from one module-lifetime atomic domain. The compare-exchange source returns the
largest 64-bit identity once and then remains exhausted at zero instead of wrapping. Publication stamps each contact before its point
storage becomes visible, and `GetContactPoints` compares that identity before evaluating its range. A copied or moved batch retains the
same identity and storage; a live foreign batch, a later generation that recycles the storage, or an empty batch cannot address the
contact's points.

The identity occupies existing alignment space in `ContactEvent`, whose exact checked size remains 96 bytes. `EventBatch` remains one
pointer. `GetId` has a zero-byte stack frame, eight instructions, no calls, and one empty-batch branch. `GetContactPoints` has a zero-byte
stack frame, 29 instructions, no calls, four validation branches, and performs no allocation or locking. The module-wide source performs
one relaxed compare-exchange per published batch; its publication cost and cross-world contention remain explicitly assigned to
`J3-AUD-024` rather than inferred from code generation.

The complete clang-cl 22.1.8 Release non-unity suite passes all 315 tests with zero failures and one intentional disable. Focused evidence
is `ContactPointsRequireTheirProducingBatchAcrossCopiesAndRuntimeDestruction`, `RecycledEventStorageRejectsAStaleContact`,
`EventBatchIdentitySourceFailsInsteadOfWrapping`, and `ContactEventBatchProvenanceRoundTripsThroughJson`. Behavior reflection requires
the read-only `batchId` property, the public consumer calls `EventBatch::GetId`, and the modular DLL exports that symbol. Raw test XML,
export inspection, and disassembly remain beneath `build/jolt-production-readiness/stage4/` and are intentionally not committed. The
validated `Jolt.API.dll` SHA-256 is `13CAEEA8C32D00AC258937C516C4D1B8F635ADF376089361F4BD72242859A535`; the test DLL SHA-256 is
`CCF268488E044148F70B102EEF95FE301D5212AAA8EE1D7204C3D9A34C8006CA`.

## Stage 4 EBus ownership and script-boundary evidence

Every entity request interface inherits `AZ::ComponentBus` directly and declares `HandlerPolicy::Single`; every entity notification
interface declares `HandlerPolicy::Multiple`. Vehicle requests use the same ownership contract on an entity-addressed `AZ::EBusTraits`
bus. The shared single-handler container now rejects a second connection without replacing the original handler, so release builds retain
the same deterministic owner as assertion-enabled builds. Focused AzCore tests cover single- and multi-address containers, while
`ComponentTests.EntityRequestBusesRejectDuplicateOwnersAndNotificationsFanOut` proves Jolt request routing and notification fanout.

The former aggregate world request bus is split into lifecycle/configuration, simulation, queries, rollback, and diagnostics buses. All
five are singularly addressed and singularly handled. Lifecycle/configuration, explicit stepping, and rollback are Automation-only;
queries and diagnostics remain Common. `ComponentTests.BehaviorReflectionExposesEveryRuntimeComponentBus` verifies the exact event and
scope inventory, and retained transformed-shape operations remain absent from BehaviorContext.

The qualification validator parses the five reflection chains and every checked-in Jolt AutomatedTesting script. It currently accounts
for all 59 reflected operations and fails for either a reflected operation without a script call or a script call without reflection.
`Jolt_WorldQueriesAndSnapshots` exercises auxiliary-world creation/configuration/destruction, per-body queries, contact history, and
configured snapshot recapture rather than using placeholder calls. The clang-cl 22.1.8 Release non-unity suite passes 316 tests with
zero failures and one intentional disable. The focused null-renderer scenario passes in 14.5 seconds after preserving Asset Processor's
machine-consumed control and listening port announcements in Release. The final complete scenario, MSVC, and install matrices remain.

## Stage 5 physical-character membership evidence

`Characters` now owns explicit add, remove, and membership operations because generic body membership deliberately rejects bodies owned
by composite resources. Removal preserves both linear and angular velocity around Jolt's required deactivation, while the immutable
shape, transform, metadata, native body, wrapper handles, and move-event subscription remain owned in place. Re-addition applies the
caller's activation choice without recreating either handle. `CharacterState::m_isInSimulation` exposes the authoritative native
broadphase membership to C++ and script callers.

`SimulationTests.CharacterSimulationMembershipPreservesIdentityStateSnapshotsAndMoveEvents` proves invalid and cross-world rejection,
duplicate-operation rejection, stable character/body/shape identity, velocity preservation, stale added-state snapshot rejection,
detached snapshot capture/restore, event silence while detached, event resumption after re-add, and destruction from the detached state.
`ComponentTests.CharacterSimulationMembershipPreservesComponentResources` proves that component disable/re-enable retains the same
handles and state without publishing destruction notifications; entity deactivation still performs the complete ownership transaction.
`SimulationTests.CharacterMembershipTransitionsAreDeterministicAcrossWorkerCounts` performs the same fixed removal, detached mutation,
re-addition, and simulation workload in independent effective 1/4/8-worker worlds and compares the complete state digest exactly.

The clang-cl 22.1.8 Release non-unity consolidated suite passes 322 tests with zero failures and one intentional disable. The focused
`Jolt_Characters` scenario now observes the public disabled and re-enabled states through `CharacterBus`. The source/install consumer
calls every new public capability operation; its fresh installed modular and monolithic executions, the null-renderer scenario, and the
MSVC matrix remain final qualification gates.

## Stage 5 active-path evidence

`Paths` now retains the immutable Hermite or custom-provider source together with one authoritative `AZ::Transform`. Creation publishes
both or neither. Transform notifications queue the latest valid value, and explicit or automatic simulation flushes queued values while
holding the Runtime's world topology gate. Every affected world validates its complete constraint and ragdoll-definition graph before
any Path or constraint changes. A translation or rotation reuses the native geometry; a uniform-scale change builds replacement geometry
in scratch. Commit preserves handles, path fractions, motors, enabled and simulation-membership state, then advances every affected
world's configuration revision. The previous Path remains active if any world, provider, or fraction validation fails.

`SimulationTests.ActivePathTransformsCommitAtSafeBoundariesAndPreserveDependentState` proves coalescing, invalid-input rejection,
translation/rotation updates, scaled rebuilds, unchanged constraint identity and state, stale-snapshot rejection, and failure atomicity.
`SceneDefinitionsAndInstancesRetainUniquePathAndCollisionFilterResources` and
`RagdollsSupportEveryConstraintKindAndRetainDefinitionDependencies` prove the two indirect ownership paths update without replacing their
instance or constraint handles. `CustomPathsRetainProvidersAndExposeDeterministicSamples` covers provider-backed scaling and world-space
queries. `ActivePathUpdatesAreDeterministicAcrossWorkerCounts` compares exact public body/constraint state, state digests, and complete
snapshot archives across repeated internal serial, external serial, four-worker, and eight-worker executions.

The private native patch serializes `MotorSettings` field-by-field, removing three indeterminate padding bytes that previously made valid
PathConstraint archives depend on worker scheduling without changing simulation output. World preparation and pending-update storage now
retain reusable capacity instead of allocating fresh vectors for each steady transform update. The clang-cl 22.1.8 Release non-unity
suite passes 323 tests, the public-only modular consumer creates and updates a transformed Path successfully, and the quick validator
passes. Final allocator measurement, MSVC, installed-engine, and application qualification remain.

## Stage 5 custom-shape source-dependency evidence

Scene sources declare asset-database-relative files used by general custom-shape providers. `CreateJobs` resolves, hashes, deduplicates,
and publishes those files before creating any job descriptors. Its additional fingerprint includes each canonical path, current content,
existence state, provider identity, and provider version. `ProcessJob` performs an independent analysis and accepts the product only when
the provider reports the same canonical path/hash set in both portable and native archive metadata. Missing files, stale hashes, invalid
paths, unavailable providers, and conflicting provider output fail without publishing a product. Compiled scene source records release
the authoring-only dependency list after analysis.

`EditorAssetBuilderTests.TracksCustomShapeDependencyEditsDeletionAndRecovery` proves dependency-only edits, stale-provider rejection,
deletion tracking, recovery, provider-version invalidation, serialization, and recooking. The invalid-contract test covers conflicting
hashes, traversal, unsupported shape families, and unavailable providers. The clang-cl 22.1.8 Profile non-unity consolidated suite
discovers 326 tests, with zero failures and one intentional disable. The public modular consumer links and executes the new extension lookup,
and the quick validator passes all ledger, isolated-header, manifest, scenario, reflection, native-boundary, sanitizer-policy, consumer,
and Python checks. Final Asset Processor application and MSVC qualification remain.

## Stage 6 asynchronous-operation reclamation evidence

Operation creation now performs one type-keyed free-list lookup and publishes through an intrusive active list. It does not inspect or
join any unrelated record. When the caller releases the last public token, a terminal operation nominates itself for reclamation. The
pool visits only nominated records during explicit or automatic simulation maintenance, verifies that their AZ completion is already
ready, removes both list memberships in constant time, and joins outside the pool mutex. Running work retains its task reference, while
detached results retain the pool and their provider-owned immutable result until safe reclamation.

`Jolt/Diagnostic/Operation/CreateOutstanding` occupies the only background worker and times creation while every operation remains
queued. At 2,048 operations, the exact clang-cl 22.1.8 Profile median fell from 5,011 us to 147 us, a 34.0x improvement, with 0.57% CV.
The optimized Release implementation measures 122 us with 0.50% CV, or 16.75 million published operations per second. Cleanup, waits,
and cooked-shape destruction are outside the timed region, while every result is subsequently validated and destroyed.

`SimulationTests.ConcurrentOperationCreationAndDetachedCompletionRemainSafe` drives four creator threads through repeated mixed
explicit-wait and detached-completion cycles. `QueuedOperationCanBeCanceledWithoutRunning` drops one canceled queued token without a
wait while retaining and waiting on a second. Existing ownership, running-token destruction, result-lifetime, and saturated-worker
subprocess tests cover the remaining completion and shutdown paths. The complete Profile and Release non-unity suites each pass 326
tests with zero failures and one intentional disable. Raw JSON and XML evidence remains beneath
`build/jolt-production-readiness/stage6/operation-pool/` and is intentionally not committed.

## Stage 6 bounded storage and adapter-allocation evidence

Event-batch storage has independent per-record, total-byte, and record-count ceilings. Operation caches apply the same policy per result
type so one large query cannot permanently raise the steady-state footprint. Oversized and excess records are deleted on release rather
than shrunk on every reuse. `PoolStatistics` reports the complete live allocation set, partitions it into cached and caller-held storage,
and retains count and byte high-water values across reset intervals. Runtime-wide operation storage is labelled as such because reading or
resetting it through any world observes the same pool.

Jolt background tasks now use fixed Runtime-owned storage instead of allocating one self-deleting AZ job for every scheduling wave. Two
bounded slot generations cover the interval between a task relinquishing scheduling capacity and the AZ job delete hook returning its
storage. Automatic multiworld stepping constructs only the required caller-owned jobs in a bounded `fixed_vector`. CPU Hair retains its
generated shader wrappers, exact binding-buffer leases, and binding signatures across updates, with a maximum of 16 cached shaders; a
larger future shader set remains correct but uses transient wrappers beyond that ceiling.

`EventBatchPoolBoundsCachedStorageAndReportsCompleteMemory` and
`OperationPoolBoundsPerTypeCachesAndEvictsOversizeResults` exercise 1,024 simultaneous records, a 100,000-point event batch, a
100,000-result asynchronous query, reuse, reset, oversize eviction, shutdown, and immutable outstanding lifetime. Warmed parallel world
stepping, automatic multiworld stepping, caller-buffer batch queries, and CPU Hair updates report zero `SystemAllocator` or
`ThreadPoolAllocator` allocation calls. Hair additionally reports zero native allocation/reallocation calls, stable generated-wrapper
creation count, and stable retained bytes.
The task-handoff correction was stressed by ten consecutive executions of the complete 328-test clang-cl Profile non-unity suite with
zero failures. Final Release, MSVC, 1/4/8-worker application, and soak evidence remain qualification gates.

## Qualification platforms

- **Required:** fresh Windows MSVC and clang-cl/Ninja matrices, then a dedicated `--no-local` WSL ext4 Clang clone.
- **External gates:** macOS, native Linux, ARM hardware, iOS, Android, WebAssembly, and additional GPU/CPU architectures.
- **Known external blocker:** GCC currently fails before Jolt on existing engine warnings promoted to errors; record it without treating it as
  Clang/WSL evidence.

The final qualification revision must be clean, contain reviewable phase commits, and match every committed command/result claim. No
remote push or branch split belongs to this qualification.
