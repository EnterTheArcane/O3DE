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
| TySan | Profile main, sandbox, and benchmarks | Advisory only: source review found an aggregate/member-construction false positive in `ConsoleDataWrapper<bool>` and allocator-storage-reuse reports when HPHA reused list-node storage; neither was corroborated by UBSan, ASan, MSan, or TSan |
| Initialization stress | Separate pattern- and zero-initialized Profile main and sandbox runs | No outcome divergence |

The final Profile main suite passed 11,477/11,477 under strict UBSan and TSan. The ASan/LSan
process passed 11,470/11,470 after seven allocator/death tests that deliberately fault were
isolated from the shared run. Debug passed 11,454 tests and retained the same 24 existing
math/ray assertion failures before and after remediation. Sandbox passed 3/3 in every final
gate. These ordinary test results are not sanitizer findings.

The shortened full benchmark suite exercised 1,331 registered benchmarks. Dynamic tools only
establish cleanliness for executed paths.

The defined-risk pass completed the full Profile main suite and attributed five reports to
LLVM libc++ 23 internals: four integral `from_chars` conversions and one unsigned negation in
`bitset`. Focused AzCore source and benchmark reruns were clean. TySan's recovery run also
eventually crashed after flooding on valid HPHA storage reuse; because TySan is experimental
and documents aggregate/union limitations, these reports remain tool limitations rather than
source-confirmed AzCore findings.

## Linux MemorySanitizer follow-up

The Linux uninitialized-read pass uses the native arm64 Docker image in
[`Docker/azcore-sanitizers`](../../../../Docker/azcore-sanitizers). Ubuntu 24.04 LTS was
selected over the newer Ubuntu 26.04 LTS because it matches O3DE's Noble dependency set and
apt.llvm.org provides an explicitly versioned Clang 23 archive for Noble. This keeps the
historical Clang 23 audit pinned instead of depending on a moving default compiler package.

The image source-builds LLVM 23.1.0's libc++, libc++abi, and libunwind with the upstream
`MemoryWithOrigins` configuration. AzCore's statically linked Lua 5.4.4 dependency is also
built from source under MSan using O3DE's existing patch. The executable and every AzCore,
AzTest, and AzTestRunner translation unit are instrumented with origin tracking level 2;
`poison_in_dtor=1` keeps use-after-destruction checks enabled. A minimal source overlay avoids
loading unrelated prebuilt engine and GUI libraries into the process.

Source-confirmed MSan findings covered:

- uninitialized allocator bookkeeping, metrics arguments, version parsing, serialization test
  data, and failed-stream output buffers;
- missing exact allocation sizes while freeing pool pages, task-worker arrays, and erased
  `GenericClassInfo` implementations;
- a partially initialized ignored NEON lane; every bit of that lane is now made true before
  reduction. The MSan branch leaves this data flow barrier-free so shadow propagation proves
  the result, while unsanitized builds use zero-byte value barriers to retain the original
  two-instruction hot path;
- formatted trace-buffer termination and a fortified-glibc interceptor boundary;
- assembly-written unwind contexts and uninstrumented system-libunwind procedure lookup.

The final unwind handling checks every libunwind result, unpoisons only the context populated
by assembly, and narrows disabled MSan interceptor checks to the external procedure-name call.
No MSan ignorelist or `no_sanitize` function annotation is used. LLVM's source-built
instrumented libunwind remains advisory on glibc because `_dl_find_object` itself writes
outside MSan instrumentation.

The final Profile audit selected 11,479 main tests across four isolated GoogleTest shards.
Shards 0 and 1 passed 2,870/2,870; shard 2 passed 2,862 with eight assertion-capture/order
failures; shard 3 passed 2,868 with one such failure. All nine shard-order failures passed
9/9 when replayed unsharded, and no shard contained an MSan report. Sandbox passed 3/3.
The shortened Linux benchmark registry exercised 1,194 results with no MSan report. Of 21
disabled tests run in isolated processes, seven passed, 13 produced their ordinary expected
assertion result, and the intentionally heavy AssetContainer stress case reached its
180-second timeout; none reported uninitialized memory. After the last NEON code-generation
adjustment, the exact 45-test Vector3 filter and its two affected benchmarks were rebuilt and
rerun clean under strict MSan.

## Linux x86-64 follow-up

The same Ubuntu 24.04 image was rebuilt for `linux/amd64` and executed through Docker
Desktop's Rosetta support on the Apple Silicon host. Clang reported target
`x86_64-pc-linux-gnu`, so these results exercise x86-64 code generation rather than the host's
arm64 paths.

The Profile strict-UBSan main pass selected and passed 11,470 tests; sandbox passed 3/3 on
three repetitions, and the shortened complete benchmark registry finished without a report.
ASan/LSan plus strict UBSan passed 11,463 selected main tests, sandbox 3/3 on three
repetitions, and the benchmark registry. The seven omitted main tests are the same intentional
allocator/death cases isolated from the shared ASan process on macOS. MSan with origin
tracking passed 11,470 selected main tests, the benchmark registry, and sandbox 3/3 on ten
consecutive repetitions after remediation. Ordinary architecture-specific exact-estimate and
long-executable-path exclusions were recorded separately from sanitizer results.

The x86-64 passes reproduced five issues that the arm64 passes did not expose, or made easier
to diagnose:

- RapidJSON allocator adaptation performed null pointer arithmetic and passed a null source
  to a zero-length `memcpy`;
- `ByteContainerStream::Write` passed a null source to a zero-length `memcpy`;
- the profiler proxy registered and unregistered an `AZ::Interface` specialization from
  different dynamic objects;
- four-byte stamp fields left poisoned x86-64 tail padding in lock-free atomic structures;
- an asset sandbox listener published its interface from a base-class constructor before the
  derived object and synchronization state were fully alive.

The fixes use explicit null boundaries, same-module out-of-line proxy lifetime operations,
native-width stamps, value initialization, and explicit register/unregister ordering. Each
issue has focused regression coverage. The complete sanitizer sweeps first passed with the
strictest guard forms; after the final stream-guard code-generation refinement, all three
variants were rebuilt from the same source and the 11 focused regression tests were replayed
with halt-on-error enabled.

An x86-64 TSan smoke process could not start under Rosetta, including with Docker's seccomp
profile disabled. Rosetta occupies `0x800000000000`, colliding with compiler-rt's fixed TSan
shadow mapping. This is an execution-environment limitation, not a clean or failed AzCore
result. The existing native macOS arm64 TSan gate remains clean; x86-64 TSan still requires
native x86-64 Linux or a VM/emulator with a compatible address layout.

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
| `__TEXT` | 13,156,352 | 13,352,960 | +196,608 (+1.49%) |
| `__text` | 11,102,244 | 11,308,452 | +206,208 (+1.86%) |
| `__data` | 511,244 | 391,724 | -119,520 (-23.38%) |
| `__bss` | 92,720 | 92,864 | +144 (+0.16%) |

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

The Linux-MSan remediation and subsequent branch-wide conditional-operator cleanup were also
compared against a separately preserved, clean Clang 23 Profile artifact from immediately
before that follow-up:

| Section | Follow-up baseline | Final | Delta |
|---|---:|---:|---:|
| `__TEXT` | 13,385,728 | 13,352,960 | -32,768 (-0.24%) |
| `__text` | 11,336,996 | 11,308,452 | -28,544 (-0.25%) |
| `__data` | 391,724 | 391,724 | 0 |
| `__bss` | 92,736 | 92,864 | +128 (+0.14%) |

The final linked `__TEXT` segment is 32 KiB smaller than the follow-up baseline. At
the MSan-remediation checkpoint, before the explicit-control-flow cleanup, object `__text`
deltas for the touched follow-up units were: AllocatorManager +12 bytes, PoolAllocator -24,
TaskExecutor 0, SerializeContext +3,788, Trace +72, Vector4 0,
SettingsRegistryMergeUtils -256, EventLoggerReflectUtils +12, and StackTracer +36. The dominant
growth was the cold erased-type size/alignment registry in SerializeContext; sized TaskExecutor
destruction added four linked bytes on a cold teardown path. The later cleanup preserved the
`__TEXT` sizes of the directly affected Trace, PoolAllocator, and StackTracer objects while
branch-wide recompilation of common inline paths reduced the final linked text size.

The NEON first-three-lane fix was reworked twice after measurement rejected longer reductions.
The accepted all-ones mask plus zero-byte value barriers produces exactly the preserved hot
loop (`fcmeq`, `orr`, `uminv`, `fmov`, `cmp`, `cset`); normalized baseline/final instructions
are identical, and `llvm-mca -mcpu=apple-m2` reports the same 1.2-cycle block throughput. The
30-repetition follow-up benchmark comparison was:

| Focus | Median delta | Mann-Whitney p | Classification |
|---|---:|---:|---|
| Vector3 equality | +47.82% | <0.001 | Test-module link-placement artifact: the identical loop moved from within one instruction-cache line to straddling a line |
| Vector3 less-than | +0.56% | 0.119 | No significant regression; uses the same reducer |
| JSON trace event | +0.09% | 0.695 | No regression |
| Slice ID/reference fixup | +2.44% | <0.001 | Significant but below 5% |
| TaskGraph queue/dequeue | +4.91% | 0.002 | Significant but below 5% |

The equality result is retained in the record because discarding it would hide a real
measurement. It does not correspond to added production instructions or modeled throughput:
the final inner loop is instruction-for-instruction identical and the adjacent comparison
benchmark using the same inline reducer remains stable.

The source checkpoint immediately before the x86-64 remediation was preserved as another
clean macOS arm64 Clang 23 Profile non-unity artifact. The final production library changed
as follows:

| Section | Pre-x86-remediation | Final | Delta |
|---|---:|---:|---:|
| `__TEXT` | 13,352,960 | 13,352,960 | 0 |
| `__text` | 11,307,876 | 11,308,452 | +576 (+0.0051%) |
| `__data` | 391,724 | 391,724 | 0 |
| `__bss` | 92,864 | 92,864 | 0 |

The stream null boundary adds one cold-taken branch. Four measured template instantiations
grew by 4, 4, 8, and 8 bytes respectively, with the largest relative change 3.33%. A diagnostic
guard was rejected during measurement because it added 60--96 bytes per instantiation; the
accepted expression has no logging or formatting path.

The lock-free stamp change was also compiled into identical unsanitized x86-64 wrapper
objects using the preserved and final headers. Stamped push shrank from 285 to 237 bytes
(-16.8%) and stamped pop from 226 to 205 bytes (-9.3%); empty remained 22 bytes. Total wrapper
text shrank from 750 to 662 bytes (-11.7%), while instruction counts fell from 80 to 72 for
push and from 70 to 61 for pop. On arm64, stamped-stack pop retained the same instruction
layout with the intended native-width add, and stamped-queue pop shrank by 16 bytes. The
profiler constructor/destructor move affects only cold setup and teardown. No changed hot path
grew by more than eight bytes or showed a modeled throughput regression; the stamped paths
became materially smaller.

## Acceptance scope

The final source builds and runs under unsanitized Clang 23 Debug/Profile with unity both off
and on, and under supported AppleClang Debug/Profile. Valid sanitizer combinations configure
and link their compiler-rt runtimes; invalid address/thread/memory combinations, mixed TySan,
and simultaneous legacy/new ASan selectors fail during CMake configuration with actionable
diagnostics.

MemorySanitizer cannot run on Darwin with Clang 23, so its acceptance pass uses the separate
Linux container and instrumented dependencies described above. See
[Sanitizers.md](Sanitizers.md) for the reproducible configuration and runtime commands.
