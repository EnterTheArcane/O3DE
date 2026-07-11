#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$grammarDirectory = $PSScriptRoot
$antlrJar = Join-Path $grammarDirectory "antlr4.jar"
$lexerGrammar = Join-Path $grammarDirectory "azslLexer.g4"
$parserGrammar = Join-Path $grammarDirectory "azslParser.g4"

$java = Get-Command java -CommandType Application -ErrorAction SilentlyContinue
if (-not $java)
{
    throw "Java was not found on PATH. Install a Java runtime before regenerating the grammar."
}

function Invoke-Antlr
{
    param(
        [Parameter(Mandatory)]
        [string] $Grammar,

        [Parameter(ValueFromRemainingArguments)]
        [string[]] $AdditionalArguments = @()
    )

    $antlrArguments = @(
        "-jar"
        $antlrJar
        "-Dlanguage=Cpp"
        "-o"
        $grammarDirectory
        "-listener"
        "-visitor"
    ) + $AdditionalArguments + @($Grammar)

    & $java.Source @antlrArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "ANTLR failed while generating sources from '$Grammar' (exit code $LASTEXITCODE)."
    }
}

Push-Location $grammarDirectory
try
{
    Invoke-Antlr -Grammar $lexerGrammar
    Invoke-Antlr -Grammar $parserGrammar -lib $grammarDirectory
    Get-ChildItem -LiteralPath $grammarDirectory -File
        | Where-Object Extension -In ".interp", ".tokens"
        | Remove-Item -Force
}
finally
{
    Pop-Location
}

Write-Host "ANTLR C++ sources regenerated in: $grammarDirectory"
