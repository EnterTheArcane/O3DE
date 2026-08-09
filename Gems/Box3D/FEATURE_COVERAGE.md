# Box3D feature coverage

This is the release audit for the pinned Box3D API. `Open` and `Partial` rows are release blockers. Native C identifiers remain private;
public surfaces use engine types and qualified names without redundant words.

| Capability | Provider-owned surface | Current evidence | Status |
|---|---|---|---|
| Source dependency | Source-tree `FindBox3D.cmake`, pinned v0.1.0 source, audited consolidated patch | Windows and Linux source configure, non-unity build, and unit tests | Verified |
| Installed dependency | Installed `FindBox3D.cmake` and packaged source/header metadata | Installed-engine consumer configure and build pass on Windows | Verified |
| Coexistence | Unique `Box3DSystemService`; no generic physics service or singleton registration | PhysX and Box3D configure together; unit test preserves any existing `AzPhysics::SystemInterface` registration | Verified |
| World lifetime and stepping | `Box3D::ISystem`, strong generational handles | Default/custom world, stale handle, live configuration, fixed-step tests | Verified |
| Static, kinematic, and dynamic bodies | Provider-owned configurations, state, properties, mass, force, impulse, velocity, transform, and closest-point APIs | Conversion, mutation, force/impulse, kinematic, bulk-mass, contact, sensor, naming, and unsupported-type transaction tests | Verified |
| Geometry | Sphere, capsule, box, cylinder, convex hull, triangle mesh, heightfield, and compound configurations | Every family creates through the private native boundary; mutable heightfield component test | Verified |
| Multiple shapes | Ordered `AZStd::span` views and stable shape handles | Runtime collider attachment, allocation-free body enumeration, stale-handle, material-lifetime, and mutation tests | Verified |
| Materials | Inline authoring configurations and transient shared handles | Reference lifetime, live update propagation, and component material resolution tests | Verified |
| Filtering | Provider-owned collision layers, groups, query filters, pair callback, and pre-solve callback | Sensor inclusion, body-type/query callbacks, collision-group overrides, pair callbacks, and pre-solve callbacks are covered | Verified |
| Continuous collision and solver controls | Per-world and per-body settings | Live updates cover worker dispatch, length scale, speed clamping, sleep/wake, hit thresholds, and fast-body tunneling with continuous collision disabled and enabled; deterministic fingerprints cover the complete solver policy | Verified |
| Queries | Closest raycast, bounded raycast, shape cast, overlap, AABB overlap, body/shape bounds, closest point | All convex query geometries, scaling, filters, sensor policy, initial-overlap parity, six-axis closest/all-hit parity, recycled native IDs, stable ordering, overflow, body-only queries, material identity, and invalid inputs are covered | Verified |
| Contact, sensor, hit, movement, and joint events | Borrowed per-step views and component notifications | Every event family, runtime sensor mutation, destroyed-shape provenance, recycled-slot safety, catch-up accumulation, disabled collection, and bus dispatch are covered | Verified |
| Joints | Nine typed configurations, transactional updates, typed measurements, threshold events | Every kind creates, steps, reports its typed state, updates transactionally, exports from the editor, and runs in AutomatedTesting; threshold and stale-handle behavior are covered | Verified |
| Characters | Capsule mover, slope/step/support/filter settings, component transform synchronization | Ground sticking, step traversal, steep-slope support, dynamic-body pushing, retained configuration, and live component motion are covered | Verified |
| Effects | Explosion and wind interfaces, components, and request buses | Masked radial impulse, aerodynamic force, invalid input, editor export, viewport drawing, and live AutomatedTesting behavior are covered | Verified |
| Heightfields | Full replacement and rectangular height/material updates with stable wrapper handles | Component test covers corner update, invalid range, material update, and handle lifetime | Verified |
| Diagnostics | Typed counters/profile/capacity data, debug drawing, static-tree rebuild | Statistics, drawing, rebuild, replay inspection, and benchmark quality counters are covered | Verified |
| Recording and replay | Caller-owned byte buffers, validation, replay player, keyframe policy | Recording, seeking, body/query inspection, drawing, callback rejection, and repeated serial/native/AZ-job validation are covered | Verified |
| Determinism | Strict floating-point flags, canonical per-thread x86 MXCSR and ARM64 FPCR environments, fixed integer ticks, state digest, compatibility fingerprint | Repeated serial and one-worker/four-worker recording equality tests plus caller floating-point environment restoration pass on Windows Profile and Linux Release; MSVC and Clang ARM64 register paths compile-probe successfully and have an architecture-gated restoration test | Verified |
| Job integration | `AZ::JobContext` bridge with native fallback for standalone use | Caller owns worker zero, one-worker worlds execute inline, and existing worlds retain AZ jobs through worker-count reconfiguration | Verified |
| Editor | Provider-native authoring components under `Box3D::Editor` | Runtime export, viewport drawing and selection, collider geometry/offset modes, character capsule mode, effect position/radius/velocity modes, and parent/child joint-frame translation and rotation modes are covered by tools-application tests | Verified |
| Scripting | Provider-qualified request and notification buses | Typed world queries/events, all joint configurations and states, collider geometry/material mutation, material lifetime, cooked geometry, statistics, recording validation, replay control and inspection, and static-tree rebuild are reflected without native variants or spans crossing the script boundary | Verified |
| AutomatedTesting | Dedicated Box3D levels and prefabs in the existing `AutomatedTesting` project | Live authored scenarios cover mutable terrain, body/material/collider mutation, cooking, queries, every notification family, statistics, recording/replay, static-tree rebuild, all nine joint families, character motion, wind, and explosion | Verified on Windows Profile |
| Performance | Physics profiler scopes, opt-out simulation datapoints, matched provider benchmarks, native-boundary diagnostics, and authored DX12 capture | The strict 30-repetition Release comparator passes every median, repetition-tail, bootstrap, workload-signature, and stability gate. Simulation is 1.71-1.75x faster, lifecycle is 44.8-58.3x faster, scalar and batch raycasts are 1.07-3.33x faster, and exact stable-order overlap has 15.1% lower latency. `PERFORMANCE.md` retains the raw-report paths and attribution evidence | Verified |
| Public-header hygiene | AzCore-only API target, no native declarations, isolated translation units | Every public header compiles alone in the non-unity target on Windows Profile and Linux Release | Verified |
| Platform matrix | Windows, Linux, macOS, and supported consoles | Windows Debug, Profile, and Release builds/tests pass; Linux Release non-unity build and unit tests pass with Clang 21; ARM64 runtime, macOS, and console validation remain unavailable locally | Open |

## Representation policy

- Default native definition factories become initialized value types.
- Native identifiers and pointers remain private; public handles are validated transient capabilities, not serialization or network IDs.
- Native getters and setters are grouped into coherent configuration/state values when that reduces virtual calls and preserves transactional updates.
- Native recording files remain caller-owned byte buffers so file policy stays with the application.
- Native dump functions map to typed diagnostics, tracing, profiling, and debug rendering rather than an additional logging system.
