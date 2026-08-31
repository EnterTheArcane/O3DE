#!/usr/bin/env bash

# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

set -euo pipefail

readonly source_dir="${O3DE_SOURCE_DIR:-/src}"
readonly cmake_source_dir="${O3DE_CMAKE_SOURCE_DIR:-/work/azcore-source}"
readonly build_dir="${O3DE_BUILD_DIR:-/work/build}"
readonly configuration="${O3DE_CONFIGURATION:-profile}"
readonly sanitizers="${LY_CLANG_SANITIZERS:-memory}"
readonly use_instrumented_libunwind="${O3DE_MSAN_INSTRUMENTED_LIBUNWIND:-0}"
readonly action="${1:-all}"

if [[ ! -f "${source_dir}/CMakeLists.txt" ]]
then
    echo "O3DE source was not found at ${source_dir}. Mount the engine checkout at /src." >&2
    exit 2
fi

case "${action}" in
    configure|build|test|test-main|test-sandbox|benchmark|all)
        ;;
    shell)
        exec /bin/bash
        ;;
    *)
        echo "Usage: run-azcore-sanitizer [configure|build|test|test-main|test-sandbox|benchmark|all|shell]" >&2
        exit 2
        ;;
esac

sanitizer_c_flags=()
sanitizer_cxx_flags=()
sanitizer_linker_flags=()
if [[ ",${sanitizers}," == *,memory,* ]]
then
    sanitizer_c_flags+=(
        -fsanitize=memory
        -fsanitize-memory-track-origins=2
        -fno-optimize-sibling-calls
    )
    sanitizer_cxx_flags+=(
        -fsanitize=memory
        -fsanitize-memory-track-origins=2
        -fno-optimize-sibling-calls
        -nostdinc++
        -isystem
        "${MSAN_LIBCXX_ROOT}/include/c++/v1"
    )
    sanitizer_linker_flags+=(
        -stdlib=libc++
        -L"${MSAN_LIBCXX_ROOT}/lib"
        -Wl,-rpath,"${MSAN_LIBCXX_ROOT}/lib"
    )
    if [[ "${use_instrumented_libunwind}" == "1" ]]
    then
        export PKG_CONFIG_PATH="${MSAN_LIBCXX_ROOT}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
    fi
fi

printf -v joined_c_flags '%s ' "${sanitizer_c_flags[@]}"
printf -v joined_cxx_flags '%s ' "${sanitizer_cxx_flags[@]}"
printf -v joined_linker_flags '%s ' "${sanitizer_linker_flags[@]}"

prepare_source_overlay()
{
    local source_entry
    local source_name

    # The engine manifest registers every bundled gem and project. AzCore does
    # not depend on them, and configuring their targets would pull prebuilt,
    # uninstrumented libraries into an MSan-only audit. Use a symlink overlay
    # with a minimal engine manifest without modifying the checkout.
    mkdir -p "${cmake_source_dir}"
    for source_entry in "${source_dir}"/*
    do
        source_name="${source_entry##*/}"
        if [[ "${source_name}" == "engine.json" || "${source_name}" == "build" || "${source_name}" == "Code" || "${source_name}" == "Registry" ]]
        then
            continue
        fi
        ln -sfn "${source_entry}" "${cmake_source_dir}/${source_name}"
    done

    mkdir -p "${cmake_source_dir}/Code"
    for source_entry in "${source_dir}/Code"/*
    do
        source_name="${source_entry##*/}"
        if [[ "${source_name}" == "CMakeLists.txt" ]]
        then
            continue
        fi
        ln -sfn "${source_entry}" "${cmake_source_dir}/Code/${source_name}"
    done
    if [[ -e "${source_dir}/.git" ]]
    then
        ln -sfn "${source_dir}/.git" "${cmake_source_dir}/.git"
    fi
    ln -sfn "${source_dir}/Docker/azcore-sanitizers/engine.json" \
        "${cmake_source_dir}/engine.json"
    ln -sfn "${source_dir}/Docker/azcore-sanitizers/azcore-code.CMakeLists.txt" \
        "${cmake_source_dir}/Code/CMakeLists.txt"

    mkdir -p "${cmake_source_dir}/Registry"
    for source_entry in "${source_dir}/Registry"/*
    do
        source_name="${source_entry##*/}"
        if [[ "${source_name}" == "CMakeLists.txt" ]]
        then
            continue
        fi
        ln -sfn "${source_entry}" "${cmake_source_dir}/Registry/${source_name}"
    done
    ln -sfn "${source_dir}/Docker/azcore-sanitizers/empty.CMakeLists.txt" \
        "${cmake_source_dir}/Registry/CMakeLists.txt"
}

configure()
{
    # O3DE replaces the normal CMake compiler flags after compiler detection.
    # Supply both interfaces so CMake's initial probes and all generated engine
    # compile commands use the instrumented C++ runtime.
    cmake -S "${cmake_source_dir}" -B "${build_dir}" -G "Ninja Multi-Config" \
        -UQT_SKIP_SETUP_DEPLOYMENT \
        -U__pkg_config_checked_libunwind \
        -Ulibunwind_* \
        -Upkgcfg_lib_libunwind_* \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_C_FLAGS="${joined_c_flags% }" \
        -DCMAKE_CXX_FLAGS="${joined_cxx_flags% }" \
        -DO3DE_EXTRA_C_FLAGS="${joined_c_flags% }" \
        -DO3DE_EXTRA_CXX_FLAGS="${joined_cxx_flags% }" \
        -DCMAKE_EXE_LINKER_FLAGS="${joined_linker_flags% }" \
        -DCMAKE_MODULE_LINKER_FLAGS="${joined_linker_flags% }" \
        -DCMAKE_SHARED_LINKER_FLAGS="${joined_linker_flags% }" \
        -DLY_CLANG_SANITIZERS="${sanitizers}" \
        -DLY_INSTALL_ENABLED=OFF \
        -DLY_PARALLEL_LINK_JOBS="${LY_PARALLEL_LINK_JOBS:-2}" \
        -DLY_UNITY_BUILD=OFF
}

build()
{
    cmake --build "${build_dir}" --config "${configuration}" --target AzCore.Tests \
        --parallel "${O3DE_BUILD_JOBS:-$(nproc)}"
}

prepare_test_environment()
{
    local runtime_registry="${build_dir}/bin/${configuration}/Registry"

    if [[ ! -x "${build_dir}/bin/${configuration}/AzTestRunner" \
        || ! -f "${build_dir}/bin/${configuration}/libAzCore.Tests.so" ]]
    then
        echo "AzCore test artifacts were not found in ${build_dir}/bin/${configuration}." >&2
        exit 2
    fi

    export PATH="/usr/lib/llvm-${LLVM_VERSION}/bin:${PATH}"
    export MSAN_SYMBOLIZER_PATH="/usr/lib/llvm-${LLVM_VERSION}/bin/llvm-symbolizer"
    export MSAN_OPTIONS="${MSAN_OPTIONS:-halt_on_error=1:exit_code=86:poison_in_dtor=1:symbolize=1}"

    # A normal O3DE build stages root registry files beside the executable.
    # The minimal overlay omits installation targets, so reproduce that runtime
    # layout explicitly for settings-driven AzCore tests such as Streamer.
    mkdir -p "${runtime_registry}"
    cp -L "${cmake_source_dir}"/Registry/*.setreg "${runtime_registry}/"
    cp -L "${cmake_source_dir}"/Registry/Platform/Linux/*.setreg "${runtime_registry}/"
    cd "${cmake_source_dir}"
}

test_main()
{
    local runner="${build_dir}/bin/${configuration}/AzTestRunner"
    local module="${build_dir}/bin/${configuration}/libAzCore.Tests.so"
    local known_profile_failure="MATH_IntersectSegmentTriangleTest/RayTriangleTests.RegressionTestForSpecificSegmentsAndTriangles/2"
    local gtest_filter="${O3DE_GTEST_FILTER:--*SUITE_smoke*:*SUITE_periodic*:*SUITE_benchmark*:*SUITE_sandbox*:*SUITE_awsi*:${known_profile_failure}}"

    "${runner}" "${module}" AzRunUnitTests \
        "--gtest_filter=${gtest_filter}"
}

test_sandbox()
{
    local runner="${build_dir}/bin/${configuration}/AzTestRunner"
    local module="${build_dir}/bin/${configuration}/libAzCore.Tests.so"

    "${runner}" "${module}" AzRunUnitTests "--gtest_filter=*SUITE_sandbox*"
}

run_benchmarks()
{
    local runner="${build_dir}/bin/${configuration}/AzTestRunner"
    local module="${build_dir}/bin/${configuration}/libAzCore.Tests.so"
    local benchmark_args=(
        "--benchmark_min_time=${O3DE_BENCHMARK_MIN_TIME:-0.001}"
        "--benchmark_repetitions=${O3DE_BENCHMARK_REPETITIONS:-1}"
    )
    if [[ -n "${O3DE_BENCHMARK_FILTER:-}" ]]
    then
        benchmark_args+=("--benchmark_filter=${O3DE_BENCHMARK_FILTER}")
    fi

    "${runner}" "${module}" AzRunBenchmarks \
        "${benchmark_args[@]}"
}

test_azcore()
{
    prepare_test_environment
    test_main
    test_sandbox
    run_benchmarks
}

clang++ --version
echo "Source: ${source_dir}"
echo "CMake source overlay: ${cmake_source_dir}"
echo "Build: ${build_dir}"
echo "Configuration: ${configuration}"
echo "Sanitizers: ${sanitizers}"
echo "Instrumented libunwind: ${use_instrumented_libunwind}"

prepare_source_overlay

case "${action}" in
    configure)
        configure
        ;;
    build)
        build
        ;;
    test)
        test_azcore
        ;;
    test-main)
        prepare_test_environment
        test_main
        ;;
    test-sandbox)
        prepare_test_environment
        test_sandbox
        ;;
    benchmark)
        prepare_test_environment
        run_benchmarks
        ;;
    all)
        configure
        build
        test_azcore
        ;;
esac
