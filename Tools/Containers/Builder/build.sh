#!/usr/bin/env bash
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

set -euo pipefail

usage() {
    echo "Usage: $0 {clang-cl|msvc} {probe|configure|build|targets|test} [arguments...]" >&2
    exit 2
}

[[ $# -ge 2 ]] || usage
export O3DE_WINDOWS_COMPILER=$1
action=$2
shift 2

case "$O3DE_WINDOWS_COMPILER" in
    clang-cl)
        export CMAKE_WINDOWS_KITS_10_DIR="/opt/xwin/Windows Kits/10"
        ;;
    msvc)
        export PATH="/opt/msvc/bin/x64:$PATH"
        export CMAKE_WINDOWS_KITS_10_DIR=/opt/msvc/kits/10
        ;;
    *) usage ;;
esac

case "$action" in
    probe|configure|build|targets|test) ;;
    *) usage ;;
esac

builder_dir=$(cd -- "$(dirname -- "$0")" && pwd)
work_dir=${O3DE_WORK_DIR:-/work}
source_dir=${O3DE_SOURCE_DIR:-"$work_dir/source"}
build_dir="$work_dir/build/$O3DE_WINDOWS_COMPILER"
compile_jobs=${O3DE_COMPILE_JOBS:-16}
link_jobs=${O3DE_LINK_JOBS:-2}
export WINEPREFIX="$work_dir/wine/$O3DE_WINDOWS_COMPILER"
if [[ "$action" == test ]]; then
    unset WINEARCH
    export WINEPREFIX="$work_dir/wine/ctest/$O3DE_WINDOWS_COMPILER"
fi
export CMAKE_DRIVER_STATE_DIR="$work_dir/logs/cmake-driver"
mkdir -p "$(dirname -- "$WINEPREFIX")"

if [[ "$O3DE_WINDOWS_COMPILER" == msvc || "$action" == probe || "$action" == test ]]; then
    if [[ ! -f "$WINEPREFIX/system.reg" ]]; then
        wineboot --init
        wineserver -w
    fi
    # Wine returns 2 when this prefix already has a server.
    wineserver -p || [[ $? -eq 2 ]]
fi

run_cmake() {
    local operation=$1
    shift
    if [[ -n "${CMAKE_DRIVER:-}" ]]; then
        "$CMAKE_DRIVER" --toolchain current "$operation" "$@"
    else
        case "$operation" in
            configure) cmake "$@" ;;
            build) cmake --build "$@" ;;
            targets) cmake --build "$@" --target help ;;
            test) ctest "$@" ;;
        esac
    fi
}

configure() {
    run_cmake configure -S "$1" -B "$2" -G 'Ninja Multi-Config' \
        "-DCMAKE_TOOLCHAIN_FILE=$builder_dir/windows-toolchain.cmake" \
        -DCMAKE_CONFIGURATION_TYPES=profile "${@:3}"
}

case "$action" in
    probe)
        probe_dir="$work_dir/probe/$O3DE_WINDOWS_COMPILER"
        configure "$builder_dir/Probe" "$probe_dir" "$@"
        run_cmake build "$probe_dir" --config profile --parallel "$compile_jobs"
        test -s "$probe_dir/profile/WindowsProbe.pdb"
        llvm-readobj --file-headers --coff-imports "$probe_dir/profile/WindowsProbe.exe"
        wine "$probe_dir/profile/WindowsProbe.exe"
        ;;
    configure)
        configure "$source_dir" "$build_dir" -DLY_PROJECTS=AutomatedTesting \
            "-DLY_3RDPARTY_PATH=$work_dir/3rdParty" -DLY_UNITY_BUILD=ON \
            "-DLY_PARALLEL_COMPILE_JOBS=$compile_jobs" "-DLY_PARALLEL_LINK_JOBS=$link_jobs" "$@"
        ;;
    build)
        mkdir -p "$work_dir/logs"
        TIMEFORMAT='Build wall time %3R seconds'
        time run_cmake build "$build_dir" --config profile --parallel "$compile_jobs" "$@"
        du -sh "$build_dir" "$work_dir/3rdParty"
        if [[ -r /sys/fs/cgroup/memory.peak ]]; then
            printf 'Container peak memory %s bytes\n' "$(cat /sys/fs/cgroup/memory.peak)"
        fi
        ;;
    targets)
        run_cmake targets "$build_dir" --config profile "$@"
        ;;
    test)
        export O3DE_WINE_TEST_BUILD_BIN="$build_dir/bin/profile"
        export O3DE_WINE_TEST_PROFILE_DIR="$source_dir/build/windows-cross-$O3DE_WINDOWS_COMPILER-runtime"
        mkdir -p -- "$source_dir/build"
        if [[ ! -L "$O3DE_WINE_TEST_PROFILE_DIR" ]]; then
            ln -sT -- "$O3DE_WINE_TEST_BUILD_BIN" "$O3DE_WINE_TEST_PROFILE_DIR"
        fi
        if [[ "$(readlink -- "$O3DE_WINE_TEST_PROFILE_DIR")" != "$O3DE_WINE_TEST_BUILD_BIN" ]]; then
            printf 'Unexpected test runtime path %s\n' "$O3DE_WINE_TEST_PROFILE_DIR" >&2
            exit 1
        fi

        # Tests need the matching Microsoft C++ runtime and UCRT inside their Wine prefix.
        for dll in /opt/msvc/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT/*.dll \
                   /opt/msvc/kits/10/Redist/*/ucrt/DLLs/x64/ucrtbase.dll; do
            [[ -f "$dll" ]] || continue
            install -m644 -- "$dll" "$WINEPREFIX/drive_c/windows/system32/"
            dll_name=$(basename "$dll" .dll)
            WINEDLLOVERRIDES="$dll_name=n,b;${WINEDLLOVERRIDES:-}"
        done
        export WINEDLLOVERRIDES="winedbg.exe=d;${WINEDLLOVERRIDES:-}"
        # Start Wine services before winepath can inherit CTest's output handles.
        wine cmd /c exit </dev/null >/dev/null 2>&1
        run_cmake test --test-dir "$build_dir" -C profile --output-on-failure "$@"
        ;;
esac
