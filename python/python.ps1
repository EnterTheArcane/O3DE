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
    # Prefer $PSCommandPath when available; try to resolve symlinks best-effort.
    $path = $PSCommandPath
    if ([string]::IsNullOrEmpty($path)) {
        # Fallback (rare)
        $path = $MyInvocation.MyCommand.Path
    }
    if ([string]::IsNullOrEmpty($path)) {
        Write-ErrorAndExit "ERROR: Unable to determine script path."
    }

    try {
        $item = Get-Item -LiteralPath $path -Force
        if ($item -and $item.LinkType) {
            try {
                $resolved = (Resolve-Path -LiteralPath $path).Path
                return (Split-Path -Parent $resolved)
            }
            catch {
                return (Split-Path -Parent $item.FullName)
            }
        }
        return (Split-Path -Parent $item.FullName)
    }
    catch {
        return (Split-Path -Parent $path)
    }
}

function Assert-CMakeAvailable {
    if (Get-Command cmake -ErrorAction SilentlyContinue) {
        return
    }

    if ([string]::IsNullOrEmpty($env:LY_CMAKE_PATH)) {
        Write-ErrorAndExit "ERROR: Could not find cmake on the PATH and LY_CMAKE_PATH is not defined, cannot continue.`nPlease add cmake to your PATH, or define LY_CMAKE_PATH"
    }

    # Prepend LY_CMAKE_PATH to PATH
    $env:PATH = ($env:LY_CMAKE_PATH + [IO.Path]::PathSeparator + $env:PATH)

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-ErrorAndExit "ERROR: Could not find cmake on the PATH or at the known location: $($env:LY_CMAKE_PATH)`nPlease add cmake to the environment PATH or place it at the above known location."
    }
}

function Get-PalAndArch {
    if ($IsWindows) {
        return @{ Pal = "Windows"; Arch = "" }
    }
    if ($IsMacOS) {
        return @{ Pal = "Mac"; Arch = "" }
    }
    # Linux and others
    $arch = ""
    try {
        $arch = (& uname -m) 2>$null
    }
    catch {
        $arch = ""
    }
    return @{ Pal = "Linux"; Arch = $arch }
}

function Test-ContainExportProject([string[]]$ArgsIn) {
    foreach ($a in $ArgsIn) {
        if ($a -eq "export-project") { return $true }
    }
    return $false
}

$cmdDir = Get-ScriptDirectory
$engineRoot = (Resolve-Path -LiteralPath (Join-Path $cmdDir "..")).Path

Assert-CMakeAvailable

# Special Case: ROS2 export-project should use system python3
if (-not [string]::IsNullOrEmpty($env:ROS_DISTRO) -and (Test-ContainExportProject $args)) {
    $py3 = Get-Command python3 -ErrorAction SilentlyContinue
    if ($py3) {
        # Ensure resolvelib is present; ignore failures (matches "|| true")
        try {
            & $py3.Source -m pip install resolvelib | Out-Null
        }
        catch { }

        & $py3.Source @args
        exit $LASTEXITCODE
    }
    else {
        Write-Host "Warning. Detected ROS but cannot locate python3 for ROS, this may cause issues with O3DE."
        # Fall through to O3DE venv behavior
    }
}

# Calculate engine ID
$calcPath = Join-Path $engineRoot "cmake/CalculateEnginePathId.cmake"
if (-not (Test-Path -LiteralPath $calcPath)) {
    Write-ErrorAndExit "ERROR: Missing CalculateEnginePathId.cmake at expected path: $calcPath"
}

$engineId = ""
try {
    $engineId = (& cmake -P $calcPath $engineRoot) | Select-Object -First 1
}
catch {
    $engineId = ""
}

if ([string]::IsNullOrWhiteSpace($engineId)) {
    Write-ErrorAndExit "Unable to calculate engine ID"
}

# Determine user home (USERPROFILE on Windows, HOME on others)
$homeDir = $env:USERPROFILE
if ([string]::IsNullOrEmpty($homeDir)) { $homeDir = $HOME }
if ([string]::IsNullOrEmpty($homeDir)) { $homeDir = [Environment]::GetFolderPath("UserProfile") }

$pythonVenv = Join-Path $homeDir ".o3de/Python/venv/$engineId"

# Activation script location in python venv for PowerShell:
# - Windows: <venv>\Scripts\Activate.ps1
# - Unix:    <venv>/bin/Activate.ps1
$activateCandidates = @()
if ($IsWindows) {
    $activateCandidates += (Join-Path $pythonVenv "Scripts/Activate.ps1")
    $activateCandidates += (Join-Path $pythonVenv "Scripts/activate.ps1")
}
else {
    $activateCandidates += (Join-Path $pythonVenv "bin/Activate.ps1")
    $activateCandidates += (Join-Path $pythonVenv "bin/activate.ps1")
}

$activateScript = $activateCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

# Choose python executable
$pythonExe = $null
$pythonArgs = @($args)

# Support python.cmd behavior: leading "debug" selects python_d.exe (Windows)
if ($pythonArgs.Count -gt 0 -and $pythonArgs[0] -eq "debug") {
    $pythonArgs = $pythonArgs[1..($pythonArgs.Count - 1)]
    if ($IsWindows) {
        $candidate = Join-Path $pythonVenv "Scripts/python_d.exe"
        if (Test-Path -LiteralPath $candidate) { $pythonExe = $candidate }
    }
}

if (-not $pythonExe) {
    if ($IsWindows) {
        $candidate = Join-Path $pythonVenv "Scripts/python.exe"
        if (Test-Path -LiteralPath $candidate) { $pythonExe = $candidate }
    }
    else {
        # venv python is usually in bin/python on Unix
        $candidate = Join-Path $pythonVenv "bin/python"
        if (Test-Path -LiteralPath $candidate) { $pythonExe = $candidate }
    }
}

if (-not $pythonExe -or -not (Test-Path -LiteralPath $pythonExe)) {
    if ($IsWindows) {
        Write-ErrorAndExit "Python has not been setup completely for O3DE. Missing Python venv $pythonExe`nTry running $cmdDir\get_python.bat to setup Python for O3DE."
    }
    else {
        Write-ErrorAndExit "Python has not been downloaded/configured yet.`nTry running $cmdDir/get_python.sh first."
    }
}

# Validate venv hash matches current python package hash
$venvHashFile = Join-Path $pythonVenv ".hash"
if (-not (Test-Path -LiteralPath $venvHashFile)) {
    if ($IsWindows) {
        Write-ErrorAndExit "Python has not been setup completely for O3DE. Missing venv hash $venvHashFile`nTry running $cmdDir\get_python.bat to setup Python for O3DE."
    }
    else {
        Write-ErrorAndExit "Python has not been downloaded/configured yet.`nTry running $cmdDir/get_python.sh first."
    }
}

$venvPackageHash = (Get-Content -LiteralPath $venvHashFile -Raw).Trim()

$palArch = Get-PalAndArch
$getHashCmake = Join-Path $cmdDir "get_python_package_hash.cmake"
if (-not (Test-Path -LiteralPath $getHashCmake)) {
    Write-ErrorAndExit "ERROR: Missing get_python_package_hash.cmake at expected path: $getHashCmake"
}

$currentPackageHash = ""
try {
    if ([string]::IsNullOrEmpty($palArch.Arch)) {
        $currentPackageHash = (& cmake -P $getHashCmake $engineRoot $palArch.Pal) | Select-Object -First 1
    }
    else {
        $currentPackageHash = (& cmake -P $getHashCmake $engineRoot $palArch.Pal $palArch.Arch) | Select-Object -First 1
    }
}
catch {
    $currentPackageHash = ""
}

if ([string]::IsNullOrWhiteSpace($currentPackageHash)) {
    Write-ErrorAndExit "Unable to get current python package hash"
}

if ($venvPackageHash -ne $currentPackageHash.Trim()) {
    if ($IsWindows) {
        Write-ErrorAndExit "Python needs to be updated against the current version.`nTry running $cmdDir\get_python.bat to update Python for O3DE."
    }
    else {
        Write-ErrorAndExit "Python has been updated since the last time the python command was invoked.`nRun $cmdDir/get_python.sh to update."
    }
}

# Activate venv (if the PS activation script exists), run python, then deactivate
$oldLdLibraryPath = $env:LD_LIBRARY_PATH

try {
    if ($activateScript) {
        . $activateScript
    }
    
    $env:PYTHONNOUSERSITE = "1"
    $env:PYTHONPATH = ""

    if (-not $IsWindows) {
        $pythonLibPath = Join-Path $pythonVenv "lib"
        if (Test-Path -LiteralPath $pythonLibPath) {
            if ([string]::IsNullOrEmpty($env:LD_LIBRARY_PATH)) {
                $env:LD_LIBRARY_PATH = $pythonLibPath
            }
            else {
                $env:LD_LIBRARY_PATH = "$pythonLibPath`:$env:LD_LIBRARY_PATH"
            }
        }
    }

    & $pythonExe -B @pythonArgs
    $exitCode = $LASTEXITCODE
}
finally {
    # Attempt deactivate if activation defined it
    try {
        if (Get-Command deactivate -ErrorAction SilentlyContinue) {
            deactivate | Out-Null
        }
    }
    catch { }

    # Restore LD_LIBRARY_PATH
    $env:LD_LIBRARY_PATH = $oldLdLibraryPath
}

exit $exitCode
