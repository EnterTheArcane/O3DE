#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$MainDir = Split-Path -Parent $PSScriptRoot

docker buildx build `
    --builder o3de `
    --progress plain `
    --tag o3de:ubuntu `
    --file $MainDir/Toolchain/Ubuntu/Dockerfile `
    --load `
    .
