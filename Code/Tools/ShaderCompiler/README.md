# ShaderCompiler

ShaderCompiler is O3DE's AZSL compiler.

The host-tools build provides two internal CMake targets:

- `AZ::ShaderCompiler` is the static compiler implementation.
- `AZ::Azslc` is the command-line executable and produces `azslc`.

ShaderCompiler does not yet expose a supported C++ API. Its headers under `Source`
are implementation details.

## Build

Configure O3DE normally, then build the command-line target:

```console
cmake --build <build-directory> --target Azslc --config Profile
```

The executable is written to `<build-directory>/bin/profile/azslc` (or
`azslc.exe` on Windows). Building Atom Shader Builder also builds and stages this
executable under `Builders/AZSLc`.

## Regenerate the grammar

The generated ANTLR C++ sources are checked in. With Java and PowerShell available,
regenerate them from the engine root:

```console
pwsh Code/Tools/ShaderCompiler/Source/Grammar/Generate.ps1
```

The script downloads ANTLR 4.13.2 when it is not cached locally and verifies the
pinned SHA-256 before invoking it. The first run therefore requires network access;
the downloaded JAR is ignored by Git.

Commit grammar and generated-source changes together.
