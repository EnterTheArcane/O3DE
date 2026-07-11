# Amazon Shading Language Compiler

AZSLC is O3DE's command-line compiler for the Amazon Shading Language (AZSL).
It transpiles AZSL shaders into High Level Shading Language Shader Model 6+ (HLSL) shaders.

## Build

AZSLC is registered as an O3DE host tool.
Configure O3DE normally, then build the `azslc` target with the build directory and configuration for your platform:

```powershell
cmake --build <build-directory> --target AZSLC --config Profile
```

## Test

AZSLC tests are registered individually with CTest when O3DE host tests and `BUILD_TESTING` are enabled.

```powershell
# All AZSLC tests
ctest --test-dir <build-directory> -C Profile -L "AZSLC\."

# A single category
ctest --test-dir <build-directory> -C Profile -L AZSLC.Syntax
ctest --test-dir <build-directory> -C Profile -L AZSLC.Semantic
ctest --test-dir <build-directory> -C Profile -L AZSLC.Emission
ctest --test-dir <build-directory> -C Profile -L AZSLC.Samples
ctest --test-dir <build-directory> -C Profile -L AZSLC.Advanced
```

Run CTest with `--output-on-failure` for compiler and test-runner diagnostics.
See [`Tests/README.md`](Tests/README.md) for how the suite is organized and how to add tests.

## Regenerate the ANTLR sources

The generated ANTLR C++ lexer and parser are checked in under `Source/Grammar`.
They should only be regenerated when `azslLexer.g4` or `azslParser.g4` changes.

Regeneration requires PowerShell and a Java runtime available on `PATH`:

```powershell
pwsh Source/Grammar/Generate.ps1
```
