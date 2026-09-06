# AZSLC

AZSLC is the front-end compiler for Atom's shader build pipeline. It transpiles Amazon Shading Language (AZSL) source into HLSL and generates reflection data for resource bindings, shader options, and other shader metadata.

The host-tools build provides two internal targets:

- `AZ::ShaderCompiler` is the static compiler implementation.
- `AZ::Azslc` is the command-line executable and produces `azslc`.

## Building

Configure a build directory, then build the command-line target:

```
cmake --build <build-directory> --target Azslc --config Profile
```

## Grammar

The generated ANTLR C++ sources are checked in. With Java and PowerShell available, regenerate them from the engine root:

```
pwsh Code/Tools/ShaderCompiler/Source/Grammar/Generate.ps1
```

The script downloads ANTLR when it is not cached locally before invoking it.
Commit grammar and generated-source changes together.

## Preprocessing and testing

AZSLC preprocesses original AZSL files natively. Pass `-I` include directories, ordered `-D`/`-U` macro operations, and repeatable `--include` forced headers directly to the compiler. Use `-E` to stream an explicit preprocessing export and `--preprocessed` for legacy preprocessed input. See [the preprocessing design](Docs/Preprocessing.md) for the dialect, source ownership, diagnostics and prior art.

Build and run the registered frontend checks:

```sh
cmake --build <build-directory> --target Azslc ShaderCompiler.Tests --config profile
ctest --test-dir <build-directory> -C profile -R 'ShaderCompiler\.' --output-on-failure
```

The legacy suite runs its fixtures in a temporary directory and reports a failing process status for regressions. New TODO classifications are not accepted implicitly.
