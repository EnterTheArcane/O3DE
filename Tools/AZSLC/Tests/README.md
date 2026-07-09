# AZSLC Test Suite

## License

Because of the nature of the tests, not all test shader code *can* have inlined comments.
Please mind that **all** shader source code found in this directory and its sub-folders
is covered by the following license:

> Copyright (c) Contributors to the Open 3D Engine Project.
> For complete copyright and license terms please see the LICENSE
> at the root of this distribution.
>
> SPDX-License-Identifier: Apache-2.0 OR MIT

## Overview

Tests are run via **CTest** and are automatically registered when the project is configured with `BUILD_TESTING=ON` (the default). A Python-based runner (
`ctest-runner.py`) adapts the existing test infrastructure to CTest.

The test suite uses **convention over configuration**: you do not need to maintain a list of tests. Tests are discovered automatically from filesystem structure
and naming conventions.

## Running Tests

```bash
# Run all tests (from repository root)
ctest --test-dir Build

# Run in parallel
ctest --test-dir Build -j8

# Run a single category
ctest --test-dir Build -L Advanced
ctest --test-dir Build -L Emission
ctest --test-dir Build -L Samples
ctest --test-dir Build -L Semantic
ctest --test-dir Build -L Syntax

# Run a specific test by name
ctest --test-dir Build -R "Syntax/empty"

# Show output for failures
ctest --test-dir Build --output-on-failure
```

## Test Categories

### Advanced (`Tests/Advanced/`)

Complex, multi-step test scripts written in Python. Each `.py` file must define a `doTests(compiler, silent)` function and report results via module-level
`result` (passed) and `resultFailed` (failed) counters.

These tests may invoke the compiler multiple times, chain with DXC, or perform other sophisticated validation.

### Emission (`Tests/Emission/`)

Tests that verify the **emitted HLSL output** matches expected patterns. Each `.azsl` file is compiled, and the output is compared against corresponding `.txt`
pattern files.

- `Emission/*.azsl` - Compiled and verified against pattern files.
- `Emission/AsError/*.azsl` - Expected to fail, with error code verification.

### Samples (`Tests/Samples/`)

Full compilation tests - the compiler runs without restriction flags and emits HLSL output.

- `Samples/*.azsl` - Expected to compile successfully.
- `Samples/AsError/*.azsl` - Expected to fail compilation.

### Semantic (`Tests/Semantic/`)

Tests that semantic analysis (type checking, scope resolution, etc.) works correctly. The compiler is invoked with the `--semantic` flag.

- `Semantic/*.azsl` - Expected to pass semantic analysis.
- `Semantic/AsError/*.azsl` - Expected to fail semantic analysis. These files must still have **valid syntax** -- the runner verifies this automatically.

Semantic error tests can include a `#EC <number>` annotation in the source to assert that a specific error code is reported.

### Syntax (`Tests/Syntax/`)

Tests that the AZSL grammar parser accepts or rejects input correctly. The compiler is invoked with the `--syntax` flag (parse only, no semantic analysis).

- `Syntax/*.azsl` - Expected to parse successfully.
- `Syntax/AsErrors/*.azsl` - Expected to fail parsing (invalid syntax).

## Naming Conventions

| Convention                                      | Meaning                                                                 |
|-------------------------------------------------|-------------------------------------------------------------------------|
| Located in an `AsError` or `AsErrors` directory | Test expects the compiler to **reject** the input                       |
| Filename starts with `wip-`                     | Work-in-progress; failures are treated as **skipped** instead of failed |
| `.azsl` extension                               | Shader source file to compile                                           |
| `.py` extension (Advanced only)                 | Python test script                                                      |

## Adding New Tests

### Simple Tests (Syntax, Semantic, Samples)

1. Create a new `.azsl` file in the appropriate directory.
2. Re-run CMake configure (`cmake -B Build -S .`) so the new file is discovered. Tests are registered by globbing at configure time.
3. Run `ctest --test-dir Build` to verify.

To add a test that should **fail**, place it in the corresponding `AsError`/`AsErrors` subdirectory.

### Emission Tests

1. Create a `.azsl` file in `Tests/Emission/`.
2. Create one or more `.txt` pattern files alongside it that describe the expected output.
3. Re-run CMake configure and test.

### Advanced Tests

1. Create a `.py` file anywhere under `Tests/Advanced/`.
2. Implement a `do_tests(compiler, silent)` function.
3. Set module-level `result` and `result_failed` counters.
4. Re-run CMake configure and test.
