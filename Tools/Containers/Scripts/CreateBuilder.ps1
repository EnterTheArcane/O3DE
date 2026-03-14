#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$MainDir = Split-Path -Parent $PSScriptRoot

docker buildx create `
    --name o3de `
    --use `
    --driver docker-container `
    --buildkitd-config $MainDir/buildkitd.toml
