# AzCore Clang 23 sanitizer audit

This record summarizes the macOS arm64 AzCore audit performed with Homebrew Clang 23.1.0.
The audit used fresh, separate non-unity build trees for every instrumented mode, plus fresh
unity, non-unity, and AppleClang acceptance builds. Third-party source was not modified and no
sanitizer ignorelist was checked in.

## Dynamic results

| Pass | Coverage | Result |
|---|---|---|
| Strict UBSan | Debug and Profile main, sandbox, shortened full benchmark suite, and 28 disabled tests in isolation | No AzCore sanitizer report |
| Defined-risk checks | Profile main, sandbox, and benchmarks | Intentional modular arithmetic classified separately; no unexplained source-confirmed finding |
| ASan/LSan plus strict UBSan | Debug and Profile main, sandbox, and benchmarks | No AzCore report after isolating intentional allocator death tests |
| TSan | Profile main and sandbox three times; benchmark coverage once | No AzCore report |
| TySan | Profile main, sandbox, and benchmarks | One source-reviewed aggregate/member-construction report in `ConsoleDataWrapper<bool>`; retained as an experimental-tool limitation rather than suppressing or changing valid object construction |
| Initialization stress | Separate pattern- and zero-initialized Profile main and sandbox runs | No outcome divergence |

The Profile main suite executed 11,475 tests: 11,474 passed and the existing
`MATH_IntersectSegmentTriangleTest/RayTriangleTests.RegressionTestForSpecificSegmentsAndTriangles/2`
assertion failed in both sanitized and unsanitized builds. Debug had the same 24 existing
math/ray assertion failures before and after remediation. Sandbox passed 3/3. These ordinary
test results are not sanitizer findings.

The shortened full benchmark suite exercised 1,331 registered benchmarks. Dynamic tools only
establish cleanliness for executed paths.

## Static results

The Clang Static Analyzer ran the stable `core.uninitialized`, `cplusplus.InnerPointer`,
`cplusplus.NewDelete`, and `cplusplus.NewDeleteLeaks` checks over 297 AzCore production
translation units and completed with no remaining report. `-Wlifetime-safety` emitted expected
container-invalidation contracts and negative-test diagnostics; they remain advisory rather
than becoming build errors.

Source-confirmed findings included uninitialized parse output, an unchecked reflected call
result, null-pointer offset expressions, malformed text asset parsing, and a union-based endian
conversion. They were replaced with explicit initialization, checked results, `offsetof`, input
guards, and `memcpy` respectively, with focused regression coverage.

The cumulative benchmark sweep also exposed signed overflow in an EBus test-only execution
counter after more than `INT_MAX` dispatches. Its ordering contract is modulo arithmetic, so the
counter now uses defined unsigned wraparound; the complete shortened sweep then ran clean.

## Remediation themes

The audit made undefined intent explicit across AzCore without changing public APIs, ABI, or
serialized formats. The principal fixes were:

- defined unsigned or widened intermediate arithmetic for overflow-prone math, hashing, random,
  allocator, statistics, and serialization paths;
- explicit object-lifetime storage and construction for reflected arguments, containers,
  delegates, and allocators;
- `memcpy`-based representation access in place of aliasing or inactive-union reads;
- bounds, null, malformed-input, and divide guards at source boundaries;
- acquire/release synchronization for allocator, EBus environment, job cancellation, profiler,
  asset, and streaming shared state;
- deterministic initialization and narrowly focused tests for each reproduced finding.

No `no_sanitize` annotation or checked-in suppression was required.

## Code generation and performance

Fresh unsanitized Clang 23 Profile baselines used identical compiler, flags, generator, and
non-unity layout. Final `libAzCore.dylib` section sizes were:

| Section | Baseline | Final | Delta |
|---|---:|---:|---:|
| `__TEXT` | 13,156,352 | 13,385,728 | +229,376 (+1.74%) |
| `__text` | 11,102,244 | 11,336,996 | +234,752 (+2.11%) |
| `__data` | 511,244 | 391,724 | -119,520 (-23.38%) |
| `__bss` | 92,720 | 92,736 | +16 (+0.02%) |

Most text growth is in cold, thread-safe initialization paths made explicit by the audit.
Normalized Clang 23 arm64 assembly for endian conversion, hash combination, and the changed
unsigned divide-and-round-up path showed no hot-path regression. `llvm-mca -mcpu=apple-m2`
kept divide-and-round-up block throughput at 13 cycles and reduced the modeled serial sequence
from 16 to 15 cycles.

The final 30-repetition reverse-order benchmark comparison found no statistically significant
median CPU regression greater than 5%:

| Focus | Median delta | Mann-Whitney p |
|---|---:|---:|
| HPHA allocate | -0.26% | 0.610 |
| HPHA deallocate | +0.24% | 0.819 |
| EBus event | +0.44% | 0.559 |
| EBus raw event | -0.44% | 0.029 |
| EBus broadcast | -8.54% | <0.001 |
| RTTI type check | -12.92% | <0.001 |
| unordered-map insert | -0.09% | 0.530 |
| unordered-map lookup | -2.06% | <0.001 |
| lightweight jobs | -2.07% | 0.501 |
| TaskGraph queue | -6.42% | 0.007 |

Focused benchmarks were added for job-cancellation traversal and unsigned divide-and-round-up.
The EBus default-context fix uses an inline acquire-load fast path and moves locking and
initialization to a cold path; this removed the initial benchmark regression.

## Acceptance scope

The final source builds and runs under unsanitized Clang 23 Debug/Profile with unity both off
and on, and under supported AppleClang Debug/Profile. Valid sanitizer combinations configure
and link their compiler-rt runtimes; invalid address/thread/memory combinations, mixed TySan,
and simultaneous legacy/new ASan selectors fail during CMake configuration with actionable
diagnostics.

MemorySanitizer remains a separate Linux follow-up because Clang 23 does not provide a Darwin
MSan runtime. See [Sanitizers.md](Sanitizers.md) for the reproducible configuration and runtime
commands.
