#!/usr/bin/env pwsh

# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

$ErrorActionPreference = "Stop"

Set-Location -Path $PSScriptRoot

$isRoot = (id -u) -eq 0

if ($isRoot) {
    & ./install-ubuntu.sh
} else {
    sudo -E ./install-ubuntu.sh
}
