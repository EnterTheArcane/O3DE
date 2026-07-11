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

Tests are run via **CTest** and are registered when the project is configured with `BUILD_TESTING=ON` (the default). A Python-based runner
(`ctest-runner.py`) adapts the AZSLC test infrastructure to CTest.

Every test file is listed explicitly in [`azslc_test_files.cmake`](azslc_test_files.cmake); [`CMakeLists.txt`](CMakeLists.txt) then registers each one through
O3DE's standard `ly_add_test` wrapper (via a local `ly_add_azslc_test` helper). This gives every AZSLC test the `SUITE_main` label and the `TEST_SUITE_main`
build hookup that O3DE's CI relies on, so these tests run in CI alongside the rest of the engine. **Adding a test therefore means creating the file *and*
listing it in `azslc_test_files.cmake`** (see [Adding New Tests](#adding-new-tests)).

A test's name mirrors its path minus extension (e.g. `Semantic/Error/GlobalVariables`), and each test carries an `AZSLC.<Category>` label — `AZSLC.Syntax`,
`AZSLC.Semantic`, `AZSLC.Emission`, `AZSLC.Samples`, `AZSLC.Advanced`, plus `AZSLC.WIP` for work-in-progress tests.

## Running Tests

Use the build directory for your platform (e.g. `build/<preset>`) as `<build-directory>`:

```bash
# Run all AZSLC tests
ctest --test-dir <build-directory> -L "AZSLC\."

# Run in parallel
ctest --test-dir <build-directory> -L "AZSLC\." -j8

# Run a single category
ctest --test-dir <build-directory> -L AZSLC.Advanced
ctest --test-dir <build-directory> -L AZSLC.Emission
ctest --test-dir <build-directory> -L AZSLC.Samples
ctest --test-dir <build-directory> -L AZSLC.Semantic
ctest --test-dir <build-directory> -L AZSLC.Syntax

# Run a specific test by name (regex match against the test name)
ctest --test-dir <build-directory> -R "Syntax/Empty"

# Show output for failures
ctest --test-dir <build-directory> --output-on-failure
```

## Test Categories

### Advanced (`Tests/Advanced/`)

Complex, multi-step test scripts written in Python. Each `.py` file must define a `do_tests(compiler, silent)` function and report results via module-level
`result` (passed) and `result_failed` (failed) counters.

These tests may invoke the compiler multiple times, chain with DXC, or perform other sophisticated validation.

### Emission (`Tests/Emission/`)

Tests that verify the **emitted HLSL output** matches expected patterns. Each `.azsl` file is compiled, and the output is compared against corresponding `.txt`
pattern files.

- `Emission/*.azsl` - Compiled and verified against pattern files.
- `Emission/Error/*.azsl` - Expected to fail, with error code verification.

### Samples (`Tests/Samples/`)

Full compilation tests - the compiler runs without restriction flags and emits HLSL output.

- `Samples/*.azsl` - Expected to compile successfully.
- `Samples/Error/*.azsl` - Expected to fail compilation.

### Semantic (`Tests/Semantic/`)

Tests that semantic analysis (type checking, scope resolution, etc.) works correctly. The compiler is invoked with the `--semantic` flag.

- `Semantic/*.azsl` - Expected to pass semantic analysis.
- `Semantic/Error/*.azsl` - Expected to fail semantic analysis. These files must still have **valid syntax** -- the runner verifies this automatically.

Semantic error tests can include a `#EC <number>` annotation in the source to assert that a specific error code is reported.

### Syntax (`Tests/Syntax/`)

Tests that the AZSL grammar parser accepts or rejects input correctly. The compiler is invoked with the `--syntax` flag (parse only, no semantic analysis).

- `Syntax/*.azsl` - Expected to parse successfully.
- `Syntax/Error/*.azsl` - Expected to fail parsing (invalid syntax).

## Naming Conventions

| Convention                                      | Meaning                                                                 |
|-------------------------------------------------|-------------------------------------------------------------------------|
| Located in an `Error` directory | Test expects the compiler to **reject** the input                       |
| Filename starts with `Wip`                     | Work-in-progress; failures are treated as **skipped** instead of failed |
| `.azsl` extension                               | Shader source file to compile                                           |
| `.py` extension (Advanced only)                 | Python test script                                                      |

Test files use **PascalCase** names, matching the rest of the engine (e.g. `AttributeSpecifiers.azsl`, `WipTypealiasUndeclared.azsl`).

## Adding New Tests

Tests are **not** auto-discovered from the filesystem — each file must be listed in
[`azslc_test_files.cmake`](azslc_test_files.cmake), so every "add a test" flow includes editing that file.

### Simple Tests (Syntax, Semantic, Samples)

1. Create a new PascalCase `.azsl` file in the appropriate directory. To add a test that should **fail**, place it in that category's `Error/` subdirectory.
2. Add its path to the matching list in `azslc_test_files.cmake` — e.g. `AZSLC_SYNTAX_TEST_FILES`, or `AZSLC_SYNTAX_ERROR_TEST_FILES` for a failing test.
3. Re-run CMake configure so the test is registered, then run `ctest --test-dir <build-directory>` to verify.

### Emission Tests

1. Create a PascalCase `.azsl` file in `Tests/Emission/` (or `Tests/Emission/Error/` for a failure test).
2. Create one or more `.txt` pattern files **with the same stem** alongside it that describe the expected output.
3. Add the `.azsl` path to `AZSLC_EMISSION_TEST_FILES` (or `AZSLC_EMISSION_ERROR_TEST_FILES`) in `azslc_test_files.cmake`.
4. Re-run CMake configure and test.

### Advanced Tests

1. Create a PascalCase `.py` file under `Tests/Advanced/`.
2. Implement a `do_tests(compiler, silent)` function.
3. Set module-level `result` and `result_failed` counters.
4. Add its path to `AZSLC_ADVANCED_TEST_FILES` in `azslc_test_files.cmake`.
5. Re-run CMake configure and test.
