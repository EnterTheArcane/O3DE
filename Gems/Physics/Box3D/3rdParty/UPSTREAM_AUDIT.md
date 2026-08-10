# Box3D upstream audit

Audited 2026-08-09 against released commit [`8441b4a`] (`v0.1.0`) and upstream `main` commit [`3fc20f5`]. Keep the exact release pin plus the
versioned consolidated patch; do not replace it with a floating branch. `main` is 18 commits ahead but still needs nearly
every semantic patch family, while its unreleased public API and recording format have diverged.

| Local patch family | Status at `3fc20f5` |
|---|---|
| Contact and body-cast compound-child provenance | Missing |
| Capacity-aware mover collision, ignored bodies, sensors, and overflow | Missing; upstream still uses a fixed 64-plane buffer |
| Deep-overlap mover handling for hulls, meshes, and heightfields | Missing |
| Compound material-count validation before allocation | Missing |
| Runtime compound material mutation | No supported in-place API; friction, restitution, surface-material, and mesh-material setters still reject compounds |
| Allocator alignment, overflow, and profiling macro | Alignment fixed independently; overflow and profiling fixes missing |
| Joint reaction, sign, axis, warm-start, separation, and cone correctness | Missing; relevant joint files remain unchanged |
| Deterministic solver orchestrator ownership | Missing; worker 0 still races the stepping thread for the orchestrator role |
| Deterministic floating-point environment on step and scheduler threads | Missing |
| Invalid-world destruction count guard | Missing |
| Worker-count scheduler rebuild and external task ownership | Missing |
| Recording attachment ownership and safe destruction | Missing |
| Capacity-aware mover recording/replay schema | Missing; upstream recording schema diverged independently |
| Replay length-scale isolation | Missing |
| Sensor comparator equality and deterministic ordering | Missing |
| CCD pre-solve null callback guard | Missing |
| Closest-point shape radii | Missing |
| Zero-count snapshot deserialization guards | Missing |
| Cached box-hull classification and exact sphere overlap | Missing; released and current `main` route box hulls through generic GJK |
| Direct closest dispatch, axis child prevalidation, box slab cast, and forward-projection tree ordering | Missing; current `main` uses two callback layers, generic ray rejection, hull-plane casts, and Euclidean child ordering |

The patched recording format uses private `B3AZ` magic and its own `1.0` version sequence. This prevents patched recordings from being
misread as upstream's independently created 3.x or 4.x formats.

Provider-side compound material updates clone the immutable compound data with new creation-time materials and replace the native shape
while preserving its public generational handle. This uses the supported public API and avoids expanding the dependency patch.

The performance additions in `box3d-0.1.0-o3de.patch` remain internal to the native implementation. Box classification is cached when a shape is
created or its hull changes. Sphere overlap uses the analytic path only for verified orthogonal box planes, distinguishes local axis-aligned
boxes from oriented boxes, preserves `B3_OVERLAP_SLOP`, and falls back to the unchanged GJK path for every other hull. Axis-aligned rays avoid
a redundant separating-axis test after the segment AABB test, verified axis-aligned box hulls use an exact slab cast, and closest queries use
a direct native callback. The tree checks child bounds while the parent is cache-hot, processes a sole matching child without calculating an
ordering key, and revalidates deferred children after ray clipping. Initial overlaps remain excluded and general hulls retain the original
cast path.

Before changing the pin, compare every row above, rebase only the remaining hunks, run the dependency qualification and determinism suites,
and benchmark the candidate in separate processes against the current baseline. A new exact upstream commit is justified only by a measured
representative-workload win or a correctness result that cannot be carried safely on the release.

[`8441b4a`]: https://github.com/erincatto/box3d/commit/8441b4a06d6d09dcfb0b0f704df4d847d1437b92
[`3fc20f5`]: https://github.com/erincatto/box3d/commit/3fc20f5b453ba9e14cdf54ecafa87a2a4bcdf53c
