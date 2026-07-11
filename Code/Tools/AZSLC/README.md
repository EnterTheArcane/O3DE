# Amazon Shading Language Compiler

AZSLC is O3DE's command-line compiler for the Amazon Shading Language (AZSL).
It transpiles AZSL shaders into High Level Shading Language Shader Model 6+ (HLSL) shaders.

## Build

AZSLC is registered as an O3DE host tool.
Configure O3DE normally, then build the `azslc` target with the build directory and configuration for your platform:

```powershell
cmake --build <build-directory> --target AZSLC --config Profile
```

The executable is written to O3DE's normal configuration-specific runtime output directory.
Building and configuring AZSLC does not require Java.

## Test

AZSLC tests are registered individually with CTest (through O3DE's standard `ly_add_test` wrapper) when O3DE host tests and `BUILD_TESTING` are enabled.
Each test is labelled `AZSLC.<Category>` and, like the rest of the engine, belongs to the `main` suite so it runs in CI:

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

Regeneration requires PowerShell 7 and a Java runtime available on `PATH`:

```powershell
pwsh Tools/AZSLC/Source/Grammar/Generate.ps1
```

The script uses the pinned JAR in `3rdParty/antlr4`, writes the C++ sources directly into `Source/Grammar`, and removes ANTLR's intermediate token and interpreter files.
Commit grammar and generated-source changes together.

The generated C++ must remain compatible with the pinned ANTLR C++ runtime in `3rdParty/antlr4/CMakeLists.txt`.
