# syntax=docker/dockerfile:1
# escape=`
# check=error=true

#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

ARG BASE_IMAGE=mcr.microsoft.com/windows/servercore:ltsc2025
FROM ${BASE_IMAGE} AS base

SHELL ["powershell", "-NoLogo", "-NoProfile", "-Command", "$ErrorActionPreference = "Stop"; $ProgressPreference = "SilentlyContinue";"]

# == BUILDER == #
FROM base AS builder

RUN <<EOSH
Invoke-WebRequest "https://aka.ms/vs/stable/vs_buildtools.exe" -OutFile "vs_buildtools.exe";
$p = Start-Process -FilePath "vs_buildtools.exe" -ArgumentList @(
    "--quiet","--wait","--norestart","--nocache",
    "--installPath","C:\BuildTools",
    "--add","Component.Microsoft.Windows.DriverKit",
    "--add","Microsoft.Component.MSBuild",
    "--add","Microsoft.VisualStudio.Component.VC.ATL",
    "--add","Microsoft.VisualStudio.Component.VC.ATL.ARM64",
    "--add","Microsoft.VisualStudio.Component.VC.MFC",
    "--add","Microsoft.VisualStudio.Component.VC.MFC.ARM64",
    "--add","Microsoft.VisualStudio.Component.VC.Redist.14.Latest",
    "--add","Microsoft.VisualStudio.Component.VC.Tools.ARM64",
    "--add","Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
    "--add","Microsoft.VisualStudio.Component.Windows11SDK",
    "--add","Microsoft.VisualStudio.Component.Windows11Sdk.WindowsPerformanceToolkit",
    "--add","Microsoft.VisualStudio.Workload.NativeDesktop"
) -Wait -PassThru;
if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { exit $p.ExitCode };
Remove-Item "vs_buildtools.exe" -Force
EOSH

# == TOOLCHAIN == #
FROM base AS toolchain

COPY --from=builder ["C:\\BuildTools", "C:\\BuildTools"]
