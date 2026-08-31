# Clang sanitizers

O3DE exposes `LY_CLANG_SANITIZERS` as a default-off, comma-separated list of checks passed
to Clang's `-fsanitize=` option. The selected checks instrument Debug and Profile; Release
remains unsanitized. Use a separate build directory for every sanitizer combination because
instrumentation changes every compiled object and linked image.

`LY_BUILD_WITH_ADDRESS_SANITIZER` remains available for legacy Debug-only ASan builds.
Do not combine it with `LY_CLANG_SANITIZERS`; use `LY_CLANG_SANITIZERS=address` instead.

The current AzCore audit results are in [AzCoreSanitizerAudit.md](AzCoreSanitizerAudit.md).

## Homebrew LLVM on macOS

The arm64 macOS platform accepts both AppleClang and upstream Clang. The following example
configures a non-unity AzCore build with Homebrew LLVM 23.1.0:

```sh
LLVM_PREFIX="$(brew --prefix llvm)"
cmake -S . -B build/mac-clang-ubsan -G "Ninja Multi-Config" \
    -DCMAKE_C_COMPILER="$LLVM_PREFIX/bin/clang" \
    -DCMAKE_CXX_COMPILER="$LLVM_PREFIX/bin/clang++" \
    -DLY_UNITY_BUILD=OFF \
    -DLY_CLANG_SANITIZERS=undefined,local-bounds,vptr
cmake --build build/mac-clang-ubsan --config profile --target AzCore.Tests
```

Instrumented Debug and Profile builds include debug information, frame pointers, and
`-fno-sanitize-merge` for actionable reports. Address-enabled modes also use
`-fsanitize-address-use-after-scope` and select AzCore's malloc-backed system allocator.
CMake validates incompatible combinations and verifies that the selected compiler can compile
and link its compiler-rt runtime before generating the engine build.

Recommended AzCore audit combinations are:

| Purpose | `LY_CLANG_SANITIZERS` | Configurations |
|---|---|---|
| Strict undefined behavior | `undefined,local-bounds,vptr` | Debug, Profile |
| Defined but suspicious conversions/arithmetic | `unsigned-integer-overflow,implicit-conversion,nullability,float-divide-by-zero` | Profile |
| Address, leak, and strict UB verification | `address,undefined,local-bounds,vptr` | Debug, Profile |
| Data races | `thread` | Profile |
| Experimental strict-aliasing checks | `type` | Profile |
| Uninitialized reads on supported Linux targets | `memory` | Debug, Profile |

Address, Thread, and Memory Sanitizer cannot be combined with each other. TypeSanitizer must
be used alone. MemorySanitizer has no Darwin runtime; use a fully instrumented Linux build for
that pass.

## Linux MemorySanitizer container

The reproducible Linux pass is documented in
[Docker/azcore-sanitizers/README.md](../../../../Docker/azcore-sanitizers/README.md). The image
uses Ubuntu 24.04 LTS, Clang 23, an MSan-instrumented LLVM 23.1.0 libc++/libc++abi runtime,
and an instrumented build of AzCore's Lua dependency. It deliberately uses separate Linux
build and third-party cache directories and a minimal source overlay containing AzCore,
AzTest, and AzTestRunner.

Run MSan with origin tracking and use-after-destruction poisoning. The container supplies
these defaults and places the matching symbolizer in `PATH`:

```sh
MSAN_OPTIONS=halt_on_error=1:exit_code=86:poison_in_dtor=1:symbolize=1
MSAN_SYMBOLIZER_PATH=/usr/lib/llvm-23/bin/llvm-symbolizer
```

MSan requires initialization-flow dependencies to be instrumented. Do not substitute the
host C++ runtime or O3DE's prebuilt Lua library in this pass. Reports that cross another
uninstrumented system or third-party boundary require source review before an AzCore change.

## Runtime configuration

Put the matching `llvm-symbolizer` first in `PATH`. Start UBSan in recovery mode to collect
and deduplicate reports, then use halt-on-error for the final clean gate.

```sh
LLVM_PREFIX="$(brew --prefix llvm)"
export PATH="$LLVM_PREFIX/bin:$PATH"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:external_symbolizer_path=$LLVM_PREFIX/bin/llvm-symbolizer"

ctest --test-dir build/mac-clang-ubsan -C profile \
    -R '^AZ::AzCore\.(Tests\.(main|sandbox)|Benchmarks\.benchmark)::TEST_RUN$' \
    --output-on-failure

export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:external_symbolizer_path=$LLVM_PREFIX/bin/llvm-symbolizer"
```

On macOS, explicitly enable leak, initialization-order, and stack-use-after-return detection
for the combined ASan/UBSan build. Stack-use-after-scope detection is enabled at compile time.

```sh
export ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:halt_on_error=1:external_symbolizer_path=$LLVM_PREFIX/bin/llvm-symbolizer"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:external_symbolizer_path=$LLVM_PREFIX/bin/llvm-symbolizer"

ctest --test-dir build/mac-clang-asan -C profile \
    -R '^AZ::AzCore\.(Tests\.(main|sandbox)|Benchmarks\.benchmark)::TEST_RUN$' \
    --output-on-failure
```

The AzCore allocator death tests intentionally provoke invalid accesses. Exclude those cases
from the full ASan process, then run each one separately and verify the expected failure. This
prevents an intentional abort from hiding later findings.

For ThreadSanitizer, run main and sandbox three times to exercise scheduler variation, and run
benchmark coverage once:

```sh
export TSAN_OPTIONS="halt_on_error=1:history_size=7:second_deadlock_stack=1:external_symbolizer_path=$LLVM_PREFIX/bin/llvm-symbolizer"
ctest --test-dir build/mac-clang-tsan -C profile -R '^AZ::AzCore\.Tests\.(main|sandbox)::TEST_RUN$' --repeat until-fail:3 --output-on-failure
ctest --test-dir build/mac-clang-tsan -C profile -R '^AZ::AzCore\.Benchmarks\.benchmark::TEST_RUN$' --output-on-failure
```

Use
`TYSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:external_symbolizer_path=$LLVM_PREFIX/bin/llvm-symbolizer`
for TypeSanitizer. TySan is experimental; confirm every report against the source, especially
reports involving unions or aggregate initialization, before changing production code.

## Test coverage

The CTest entries exercise the main tests, sandbox tests, and benchmarks. A shortened full
benchmark pass can be run directly:

```sh
build/mac-clang-ubsan/bin/profile/AzTestRunner \
    build/mac-clang-ubsan/bin/profile/libAzCore.Tests.dylib AzRunBenchmarks \
    --benchmark_min_time=0.001 --benchmark_repetitions=1
```

Run each disabled test in isolation with a timeout, an exact GoogleTest filter, and
`--gtest_also_run_disabled_tests`:

```sh
build/mac-clang-ubsan/bin/profile/AzTestRunner \
    build/mac-clang-ubsan/bin/profile/libAzCore.Tests.dylib AzRunUnitTests \
    --gtest_filter='Suite.DISABLED_Test' --gtest_also_run_disabled_tests
```

Do not enable all disabled tests in one process: platform-inapplicable and intentional stress
tests need independent classification and timeouts. A clean dynamic pass means no confirmed
AzCore findings in the exercised paths; it cannot prove that unexecuted paths contain no UB.

## Static and initialization-stress passes

Clang 23's lifetime diagnostics can be run without promoting their findings to errors:

```sh
cmake -S . -B build/mac-clang-lifetime -G "Ninja Multi-Config" \
    -DCMAKE_C_COMPILER="$LLVM_PREFIX/bin/clang" \
    -DCMAKE_CXX_COMPILER="$LLVM_PREFIX/bin/clang++" \
    -DLY_UNITY_BUILD=OFF \
    -DCMAKE_CXX_FLAGS="-Wlifetime-safety -Wno-error=lifetime-safety"
cmake --build build/mac-clang-lifetime --config profile --target AzCore.Tests
```

Run the Static Analyzer on AzCore translation units with the stable lifetime and uninitialized
checks `core.uninitialized`, `cplusplus.InnerPointer`, `cplusplus.NewDelete`, and
`cplusplus.NewDeleteLeaks`. Keep analyzer output advisory and do not promote it to a compile
failure until source-reviewed.

Build two additional Profile trees with
`-ftrivial-auto-var-init=pattern` and `-ftrivial-auto-var-init=zero`. Compare main and sandbox
outcomes; a difference is evidence of initialization-sensitive behavior. These passes reduce,
but do not replace, the coverage gap left by Darwin's lack of MemorySanitizer.

## Unsanitized code-generation gate

Preserve fresh pre-fix and post-fix Profile and Release artifacts built with identical Clang,
flags, generator, and unity setting. Compare overall sections with `llvm-size`, symbol sizes
with `llvm-nm`, and normalized instructions with `llvm-objdump`. Inline and template changes
may require temporary out-of-line wrappers. Use `llvm-mca -mcpu=apple-m2` for meaningful hot
blocks.

Run relevant unsanitized benchmarks at least 15 times and compare CPU time with a two-sided
Mann-Whitney test. Rework a statistically significant (`p < 0.05`) median regression greater
than 5%. Smaller regressions require a documented correctness benefit and instruction delta.

## Clang 23 references

- [UndefinedBehaviorSanitizer](https://releases.llvm.org/23.1.0/tools/clang/docs/UndefinedBehaviorSanitizer.html)
- [AddressSanitizer](https://releases.llvm.org/23.1.0/tools/clang/docs/AddressSanitizer.html)
- [ThreadSanitizer](https://releases.llvm.org/23.1.0/tools/clang/docs/ThreadSanitizer.html)
- [TypeSanitizer](https://releases.llvm.org/23.1.0/tools/clang/docs/TypeSanitizer.html)
- [MemorySanitizer](https://releases.llvm.org/23.1.0/tools/clang/docs/MemorySanitizer.html)
