#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$MainDir = Split-Path -Parent $PSScriptRoot

docker run --rm -it o3de:ubuntu
