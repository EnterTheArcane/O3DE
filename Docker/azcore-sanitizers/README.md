# AzCore Linux sanitizer container

This image supplies an Ubuntu 24.04 LTS arm64 or x86-64 environment with LLVM/Clang 23
and O3DE's Linux build dependencies. It is intentionally separate from the general O3DE
installer image in the parent directory. Docker's `--platform` selects the target
architecture; use a distinct image tag, build directory, and third-party cache for each one.

Ubuntu 24.04 is used because O3DE carries a Noble build-node package list and Linux
arm64 build coverage. LLVM's apt repository also publishes explicitly versioned Clang 23,
compiler-rt, and libc++ archives for Noble arm64 and x86-64. Ubuntu 26.04 is newer, but its
apt.llvm.org versioned archive does not provide the same pinned Clang 23 path, so
using it would make this historical-toolchain audit depend on the repository's moving
default packages. The image builds LLVM 23.1.0's libc++, libc++abi, and
libunwind from source with the upstream `MemoryWithOrigins` configuration and installs
them in `/opt/msan`; MSan builds use that instrumented standard library instead of
Ubuntu's uninstrumented libstdc++. It also reproduces O3DE's patched Lua 5.4.4 recipe
under MSan because AzCore statically links Lua. An instrumented libunwind and private
pkg-config file are available for advisory runtime work.

Build the image from the engine root. This example selects x86-64; replace both `amd64`
occurrences with `arm64` for a native Apple Silicon image:

```sh
docker build --platform linux/amd64 \
    -f Docker/azcore-sanitizers/Dockerfile \
    -t o3de-azcore-clang23-sanitizers-amd64 \
    Docker/azcore-sanitizers
```

Use separate build and Linux third-party cache directories. Do not reuse macOS CMake
artifacts or third-party packages:

```sh
mkdir -p build/azcore_linux_amd64_msan build/azcore_linux_amd64_thirdparty
docker run --rm --platform linux/amd64 \
    --mount type=bind,src="$PWD",dst=/src \
    --mount type=bind,src="$PWD/build/azcore_linux_amd64_msan",dst=/work/build \
    --mount type=bind,src="$PWD/build/azcore_linux_amd64_thirdparty",dst=/root/.o3de \
    o3de-azcore-clang23-sanitizers-amd64 all
```

The image can also run the Linux UBSan and ASan/LSan gates. Mount a fresh build directory
for each selector and add one of these options to `docker run`:

```sh
--env LY_CLANG_SANITIZERS=undefined,local-bounds,vptr
--env LY_CLANG_SANITIZERS=address,undefined,local-bounds,vptr
```

For a strict final gate, set `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:symbolize=1`
and set `ASAN_OPTIONS` to include
`detect_leaks=1:detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1:detect_odr_violation=2:halt_on_error=1`.
Address-enabled builds already compile with use-after-scope instrumentation. The test runner
asks LSan to scan while the test module is still loaded, before unloading it, so reports retain
complete symbols and loader teardown does not hide module-owned leaks.

The complete allocator benchmark set deliberately reaches multi-gigabyte working sets; give
Docker Desktop at least 10 GiB of memory or run benchmark filters in separate processes. The
unit-test-only `test-main` and `test-sandbox` actions require substantially less memory.

The entry point accepts `configure`, `build`, `test`, `test-main`, `test-sandbox`,
`benchmark`, `all`, or `shell`. The split test actions make it possible to retry or
shard the long main pass without rerunning sandbox tests and benchmarks. Relevant
environment overrides include:

- `LY_CLANG_SANITIZERS` (default `memory`)
- `O3DE_CONFIGURATION` (default `profile`)
- `O3DE_CMAKE_SOURCE_DIR` (default `/work/azcore-source`)
- `O3DE_GTEST_FILTER` (overrides the default main-suite GoogleTest filter)
- `O3DE_BENCHMARK_FILTER`, `O3DE_BENCHMARK_MIN_TIME`, and
  `O3DE_BENCHMARK_REPETITIONS`
- `MSAN_LIBCXX_ROOT` (image default `/opt/msan`)
- `O3DE_MSAN_INSTRUMENTED_LIBUNWIND=1` (experimental; default `0`)
- `O3DE_BUILD_JOBS` and `LY_PARALLEL_LINK_JOBS`
- `MSAN_OPTIONS`

Set `O3DE_CONFIGURATION=debug` and mount a different build directory for the Debug
configuration. Although the generator is multi-config, separate directories keep the audit
artifacts and logs unambiguous:

```sh
mkdir -p build/azcore_linux_amd64_msan_debug
docker run --rm --platform linux/amd64 \
    --mount type=bind,src="$PWD",dst=/src \
    --mount type=bind,src="$PWD/build/azcore_linux_amd64_msan_debug",dst=/work/build \
    --mount type=bind,src="$PWD/build/azcore_linux_amd64_thirdparty",dst=/root/.o3de \
    --env O3DE_CONFIGURATION=debug \
    o3de-azcore-clang23-sanitizers-amd64 all
```

On an Apple Silicon host, Docker Desktop can run the x86-64 image through Rosetta. UBSan,
ASan/LSan, and MSan work in that configuration. TSan does not: Rosetta is mapped at
`0x800000000000`, which collides with compiler-rt's fixed x86-64 TSan shadow layout. Disabling
Docker's seccomp profile does not change that mapping. Run the x86-64 TSan gate on native
x86-64 Linux hardware or a VM/emulator whose address layout is compatible with TSan.

For the final gate, run the main suite in isolated GoogleTest shards if needed, then replay
any shard-order failures unsharded. Enumerate disabled tests with `--gtest_list_tests` and run
each exact name in its own process with `--gtest_also_run_disabled_tests` and an external
timeout. Do not enable all disabled tests in a shared process: several intentionally exercise
hangs, assertions, or platform-specific behavior.

MemorySanitizer origin tracking level 2 and use-after-destruction poisoning are enabled
by default. Linux host tools are omitted from an MSan configuration because they depend on
prebuilt GUI libraries that cannot be fully instrumented; the AzCore production library,
unit tests, sandbox tests, and benchmarks remain enabled. A complete MSan audit should
instrument source-built C++ dependencies as well; reports whose origin crosses an
uninstrumented third-party binary require source review.

Profile's `_FORTIFY_SOURCE` definition is disabled only for MemorySanitizer builds.
Glibc lowers fortified calls such as `strcat` to `__strcat_chk`, while compiler-rt 23
intercepts `strcat` but not the fortified entry point and therefore cannot update the
destination's MSan shadow state. Unsanitized Profile and Release builds retain fortification.

The default pass uses Ubuntu's libunwind. AzCore brackets procedure-name lookup so MSan
does not validate libc calls made with the uninstrumented library's internal storage; the
buffers AzCore passes across that boundary remain initialized and checked. LLVM 23.1.0's
MSan-instrumented libunwind can be selected with
`O3DE_MSAN_INSTRUMENTED_LIBUNWIND=1`, but on glibc it currently reports poisoned
`_dl_find_object` output in `AddressSpace.hpp` because glibc writes that structure outside
MSan instrumentation. Keep this mode advisory until that system-library boundary is
instrumented or annotated upstream.

The runner creates a disposable symlink overlay at `/work/azcore-source` with the minimal
manifest and `Code/CMakeLists.txt` in this directory. The latter adds only AzCore, AzTest,
and AzTestRunner. This prevents unrelated engine frameworks, gems, and the AutomatedTesting
project from being configured while leaving the mounted checkout unchanged. The engine's
registry payload is retained because AzCore tests use settings such as the test Streamer
stack; only the registry installation CMake file is replaced. The runner stages that
payload beside the test executable before execution, matching a normal O3DE runtime
layout. Because CMake records this overlay path, invoke `configure` before `build` when
using a fresh container or use `all`.
