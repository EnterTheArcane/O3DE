#!/usr/bin/env bash
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

set -euo pipefail

: "${O3DE_WINE_PYTHON:?Set O3DE_WINE_PYTHON to python.exe in the Windows test environment}"

args=()
for arg in "$@"; do
    if [[ "$arg" == "${O3DE_WINE_TEST_BUILD_BIN:-}" && -n "${O3DE_WINE_TEST_PROFILE_DIR:-}" ]]; then
        arg=$O3DE_WINE_TEST_PROFILE_DIR
    fi
    case "$arg" in
        /*) arg=$(winepath -w "$arg") ;;
        --*=/*) arg="${arg%%=*}=$(winepath -w "${arg#*=}")" ;;
    esac
    args+=("$arg")
done

exec bash "$(dirname -- "$0")/wine-test.sh" "$O3DE_WINE_PYTHON" "${args[@]}"
