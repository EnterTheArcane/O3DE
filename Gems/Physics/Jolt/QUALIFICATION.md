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
| `J3-AUD-016` | Ragdoll definitions construct every advertised constraint type and reject unsupported drive mappings before mutation. | 5 | **Open** — all 13 types, linked local identities, providers/paths, archives, snapshots, editor, and pose-drive tests required. |
| `J3-AUD-017` | Physical characters expose state-preserving add/remove simulation operations. | 5 | **Open** — identity, state, snapshot, event, and 1/4/8-worker replay tests required. |
| `J3-AUD-018` | One Path resource owns geometry and transform; active updates commit transactionally at a safe boundary. | 5 | **Open** — translation/rotation frame update, scale rebuild, state preservation, and failure rollback tests required. |
| `J3-AUD-019` | Custom-shape source dependencies participate in analysis invalidation and deterministic job/product fingerprints. | 5 | **Open** — real dependency edit/delete/recovery and recook tests required. |
| `J3-AUD-020` | Every claimed clang-cl ASan configuration is instrumented and deployed correctly or rejected during configure. | 1 | **Closed** — clang-cl 22.1.8 permits only Profile ASan trees because the Windows ASan runtime rejects the debug CRT; a fresh Ninja build proved compile instrumentation, dynamic runtime/thunk linkage, runtime deployment, the full Jolt and AzCore suites, and a symbolized heap-use-after-free sentinel. |
| `J3-AUD-021` | Operation creation and completion reclamation are bounded and never wait for unrelated work. | 6 | **Open** — complexity, cancellation, concurrent creator, shutdown, and worker-saturation tests/benchmarks required. |
| `J3-AUD-022` | Event and operation caches have count/byte ceilings and accurately report live, cached, outstanding, retained, and high-water memory. | 6 | **Open** — burst/release/reuse/oversize/teardown statistics and allocator evidence required. |
| `J3-AUD-023` | Steady-state allocation claims cover native, wrapper, AZ job, and closure domains. | 6 | **Open** — warm 1/4/8-worker stepping, automatic worlds, queries, and CPU Hair allocation evidence required. |
| `J3-AUD-024` | Contact publication meets the declared contention gate without weakening deterministic order or event completeness. | 6 | **Open** — wait/hold/growth instrumentation and contact-density W1/W4/W8 evidence required before changing the mutex design. |
| `J3-AUD-025` | `validate.py review/full` schedules, validates, and retains both native and AutomatedTesting performance artifacts. | 7 | **Open** — dry-run scheduling and fresh real-run artifact proof required. |
| `J3-AUD-026` | Every scenario has meaningful minimum assertions and a parsed, verified, retained result envelope. | 7 | **Open** — corrected saved/filter/reflection/gallery/stress tests and zero-check failure sentinel required. |
| `J3-AUD-027` | Requested and effective 1/4/8-worker runs compare complete world/subsystem digests and semantic outputs. | 7 | **Open** — rigid, constraint, character, vehicle, ragdoll, soft-body, custom-provider, CPU-Hair, rollback, and event proof required. |
| `J3-AUD-028` | Performance qualification is current, broad, statistically valid, and never trades away accuracy or deterministic contracts. | 7 | **Open** — 30-process matched/absolute/tail, allocation, pool, ECS, contention, and compiler evidence required. |
| `C-AUD-001` | Restoring a group filter refreshes every affected world's contact cache after commit. | 3 | **Implemented** — successful custom-filter restore invalidates affected contact caches and activates non-static bodies while the world is already locked. Mutable filters can belong to only one world, and a global-only snapshot test proves restored callbacks re-enable its body and publish a new contact on the next step. Final MSVC qualification remains. |
| `C-AUD-002` | Static-body component teardown retains ownership after a failed destroy. | 3 | **Implemented** — an ordinary disable is atomic under a dependency veto, while mandatory entity teardown discovers the same vetoed dependency closure and completes it without abandoning the body. Final MSVC qualification remains. |
| `C-AUD-003` | Path component teardown retains ownership after a failed destroy. | 3 | **Implemented** — mandatory path teardown ignores a client veto only after complete discovery, reserves the dependent native constraint closure, and commits the path after its constraints. Final MSVC qualification remains. |
| `C-AUD-004` | Active paths have one authoritative transform and cannot snapshot inconsistent geometry and frames. | 5 | **Open** — activation skew and live-update tests required. |
| `C-AUD-005` | Gameplay scripts cannot own world lifecycle, explicit stepping, or restore. | 4 | **Implemented** — lifecycle/configuration, explicit stepping, and rollback buses carry Automation scope while query and diagnostics buses alone carry Common scope. Reflection tests assert each boundary, retained-shape operations remain C++-only, script parity rejects stale or misplaced calls, and the focused clang-cl Release scenario passes. |
| `C-AUD-006` | Creating an async operation never joins or waits for unrelated queued/running work. | 6 | **Open** — blocked-worker latency and lock-context watchdog tests required. |
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

## Qualification platforms

- **Required:** fresh Windows MSVC and clang-cl/Ninja matrices, then a dedicated `--no-local` WSL ext4 Clang clone.
- **External gates:** macOS, native Linux, ARM hardware, iOS, Android, WebAssembly, and additional GPU/CPU architectures.
- **Known external blocker:** GCC currently fails before Jolt on existing engine warnings promoted to errors; record it without treating it as
  Clang/WSL evidence.

The final qualification revision must be clean, contain reviewable phase commits, and match every committed command/result claim. No
remote push or branch split belongs to this qualification.
