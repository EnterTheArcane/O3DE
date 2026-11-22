#!/usr/bin/env bash
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

set -euo pipefail

program=$1
shift
# Several runtime fixtures find engine.json by walking up from the executable.
if [[ -n "${O3DE_WINE_TEST_PROFILE_DIR:-}" && "$program" == "$O3DE_WINE_TEST_BUILD_BIN/"* ]]; then
    program="$O3DE_WINE_TEST_PROFILE_DIR/${program#"$O3DE_WINE_TEST_BUILD_BIN/"}"
fi

deadline=()
if [[ -n "${O3DE_WINE_TEST_TIMEOUT:-}" ]]; then
    deadline=(timeout --kill-after=10 "$O3DE_WINE_TEST_TIMEOUT")
fi

# Wine services can outlive the test and keep CTest's capture pipe open.
output=$(mktemp)
trap 'rm -f -- "$output"' EXIT
status=0
"${deadline[@]}" wine "$program" "$@" >"$output" 2>&1 || status=$?
cat -- "$output"
exit "$status"
