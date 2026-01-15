#!/usr/bin/env pwsh

# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

$ErrorActionPreference = "Stop"

function Write-ErrorAndExit([string]$Message, [int]$Code = 1) {
    Write-Host $Message
    exit $Code
}

function Get-ScriptDirectory {
    $path = $PSCommandPath
    if ([string]::IsNullOrEmpty($path)) { $path = $MyInvocation.MyCommand.Path }
    if ([string]::IsNullOrEmpty($path)) { Write-ErrorAndExit "ERROR: Unable to determine script path." }

    try {
        $item = Get-Item -LiteralPath $path -Force
        if ($item -and $item.LinkType) {
            try {
                $resolved = (Resolve-Path -LiteralPath $path).Path
                return (Split-Path -Parent $resolved)
            } catch {
                return (Split-Path -Parent $item.FullName)
            }
        }
        return (Split-Path -Parent $item.FullName)
    } catch {
        return (Split-Path -Parent $path)
    }
}

function Assert-CMakeAvailable {
    if (Get-Command cmake -ErrorAction SilentlyContinue) { return }

    if ([string]::IsNullOrEmpty($env:LY_CMAKE_PATH)) {
        Write-ErrorAndExit "ERROR: CMake was not found on the PATH and LY_CMAKE_PATH is not defined.`nPlease ensure CMake is on the PATH or set LY_CMAKE_PATH."
    }

    $env:PATH = ($env:LY_CMAKE_PATH + [IO.Path]::PathSeparator + $env:PATH)

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-ErrorAndExit "ERROR: CMake was not found on the PATH or at the known location: $($env:LY_CMAKE_PATH)`nPlease add it to the path, set LY_CMAKE_PATH to be the directory containing it, or place it at the above location."
    }
}

function Get-PlatformInfo {
    if ($IsWindows) { return @{ Pal = "Windows"; Arch = "" } }
    if ($IsMacOS)   { return @{ Pal = "Mac";     Arch = "" } }

    $arch = ""
    try { $arch = (& uname -m) 2>$null } catch { $arch = "" }
    return @{ Pal = "Linux"; Arch = $arch }
}

function Test-ExportProjectArgument {
    param([string[]]$Arguments)
    foreach ($arg in $Arguments) {
        if ($arg -eq "export-project") { return $true }
    }
    return $false
}

function Get-UserHome {
    $homeDir = $env:USERPROFILE
    if ([string]::IsNullOrEmpty($homeDir)) { $homeDir = $HOME }
    if ([string]::IsNullOrEmpty($homeDir)) { $homeDir = [Environment]::GetFolderPath("UserProfile") }
    return $homeDir
}

$cmdDir = Get-ScriptDirectory
$engineRoot = (Resolve-Path -LiteralPath (Join-Path $cmdDir "..")).Path

Assert-CMakeAvailable

# Optional: mirror the ROS2 export-project escape hatch behavior.
# If someone runs: ./get_python.ps1 export-project ...
# we should use system python3, but still allow setup to proceed normally otherwise.
if (-not [string]::IsNullOrEmpty($env:ROS_DISTRO) -and (Test-ExportProjectArgument -Arguments $args)) {
    $py3 = Get-Command python3 -ErrorAction SilentlyContinue
    if ($py3) {
        try { & $py3.Source -m pip install resolvelib | Out-Null } catch { }
        & $py3.Source @args
        exit $LASTEXITCODE
    } else {
        Write-Host "Warning. Detected ROS but cannot locate python3 for ROS, this may cause issues with O3DE."
    }
}

# Default LY_3RDPARTY_PATH if needed
if ([string]::IsNullOrEmpty($env:LY_3RDPARTY_PATH)) {
    $env:LY_3RDPARTY_PATH = Join-Path (Get-UserHome) ".o3de/3rdParty"
}

# Forensic logging
& cmake --version

# Quick check: is O3DE python already installed and usable?
$pythonLauncher = Join-Path $cmdDir "python.ps1"
if (-not (Test-Path -LiteralPath $pythonLauncher)) {
    Write-ErrorAndExit "ERROR: Missing python.ps1 next to get_python.ps1 at: $pythonLauncher"
}

$pythonReady = $false
try {
    & pwsh -NoProfile -File $pythonLauncher --version *> $null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "get_python.ps1: Python is already installed:"
        & pwsh -NoProfile -File $pythonLauncher --version
        $pythonReady = $true
    }
} catch {
    $pythonReady = $false
}

# If python not ready, fetch/setup via cmake package system
if (-not $pythonReady) {
    $getPythonCmake = Join-Path $cmdDir "get_python.cmake"
    if (-not (Test-Path -LiteralPath $getPythonCmake)) {
        Write-ErrorAndExit "ERROR: Missing get_python.cmake at expected path: $getPythonCmake"
    }

    $platform = Get-PlatformInfo

    # Keep parity with .bat: PAL_PLATFORM_NAME, LY_3RDPARTY_PATH, LY_ROOT_FOLDER
    # Also pass ARCH when available (non-Windows) since other scripts use it with hashing.
    $cmakeArgs = @(
        "-DPAL_PLATFORM_NAME:string=$($platform.Pal)",
        "-D", "LY_3RDPARTY_PATH:string=$($env:LY_3RDPARTY_PATH)",
        "-D", "LY_ROOT_FOLDER=$engineRoot",
        "-P", $getPythonCmake
    )

    if (-not [string]::IsNullOrEmpty($platform.Arch)) {
        $cmakeArgs = @(
            "-DPAL_PLATFORM_NAME:string=$($platform.Pal)",
            "-D", "LY_3RDPARTY_PATH:string=$($env:LY_3RDPARTY_PATH)",
            "-D", "LY_ROOT_FOLDER=$engineRoot",
            "-D", "LY_ARCHITECTURE:string=$($platform.Arch)",
            "-P", $getPythonCmake
        )
    }

    try {
        & cmake @cmakeArgs
        if ($LASTEXITCODE -ne 0) { throw "cmake returned exit code $LASTEXITCODE" }
    } catch {
        Write-ErrorAndExit "ERROR: Unable to fetch python using cmake.`n - Is LY_PACKAGE_SERVER_URLS set?`n - Do you have permission to access the packages?"
    }
}

# Install python packages into the O3DE venv
Write-Host "calling PIP to install requirements..."
try {
    & pwsh -NoProfile -File $pythonLauncher -m pip install -r (Join-Path $cmdDir "requirements.txt") --disable-pip-version-check --no-warn-script-location
    if ($LASTEXITCODE -ne 0) { throw "pip returned exit code $LASTEXITCODE" }
} catch {
    Write-ErrorAndExit "Failed to install the packages listed in $cmdDir/requirements.txt. Check the log above!"
}

Write-Host "calling PIP to O3DE"
try {
    & pwsh -NoProfile -File $pythonLauncher -m pip install -e (Join-Path $engineRoot "scripts/o3de") --disable-pip-version-check --no-warn-script-location --no-deps
    if ($LASTEXITCODE -ne 0) { throw "pip returned exit code $LASTEXITCODE" }
} catch {
    Write-ErrorAndExit "Failed to install $cmdDir/../scripts/o3de into python. Check the log above!"
}

exit 0
