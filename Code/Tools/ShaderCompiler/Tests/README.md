# AZSLC Tests

These legacy Python tests run directly against an engine-built `azslc`.

The runner requires only Python 3 and its standard library. Symbol dumps (`--dumpsym`) and predefined-type listings (`--listpredefined`) use JSON.

Build `azslc`, change to this directory, and pass the executable to `testapp.py`:

```
cmake --build <build-directory> --target Azslc --config Profile
cd <engine-root>/Code/Tools/ShaderCompiler/Tests
python testapp.py --silent \
    --compiler <build-directory>/bin/profile/azslc \
    --path Syntax Semantic Advanced Samples
```

On Windows, use the corresponding `azslc.exe` path. A successful complete run may include tests reported as `TODO`, but it must report no `FAIL` results.

Use `--path` to focus the run on a category or individual test:

```
python testapp.py --compiler <path-to-azslc> --path Syntax
python testapp.py --compiler <path-to-azslc> --path Semantic/AsError/deportedmethod-undeclared.azsl
python testapp.py --compiler <path-to-azslc> --path Advanced/emission-full.py
```

Paths are interpreted relative to this `Tests` directory. Omit `--silent` to show compiler output while diagnosing a failure.

The registered compiler tests also run the C++ ownership/fuzz suite and `native_preprocessing_tests.py`. Legacy inputs use `.preprocessed` and explicit compiler options, including temporary diagnostic fixtures. Test subprocess output is UTF-8 on every platform.

Malformed preprocessing arguments also check diagnostic text after CLI11 unwinds, including missing values, unknown switches and the `--dependencies`/`-E` conflict. MSVC builds use `_HAS_EXCEPTIONS=1` consistently across AZSL, ANTLR and JsonCpp. The C++ suite checks that exceptions thrown by both runtimes reach the compiler's `std::exception` handlers with intact messages.

Run performance measurements separately from correctness tests:

```text
python benchmark_preprocessing.py <ShaderCompiler.PreprocessingBenchmark> --compiler <azslc> --output measurements.json
```

The driver warms each case once, then records five samples and their medians for ordinary declarations, long ASCII/Unicode lines and macro expansion. It reports process time, separate preprocessing/adapter times, ordinary C++ allocation counts and process peak resident memory. Dependency mode timings include process launch and JSON serialization. Use `--counts` and `--iterations` to adjust workloads. Compare results on the same idle machine and toolchain. Peak memory includes linked libraries and the entire measurement process.

The 34 retained empty-expansion and placemarker stringification inputs in `PreprocessorTests.cpp` originated in the no-MCPP audit and its hardening follow-up. Their checked results were verified against Clang 22.1.7. Some nested stringification spacing differs from MCPP 2.7.2_az.2, including `W(I(a E)+b)`. These cases follow Clang. The GCC preprocessor internals' Token Spacing chapter describes the empty-expansion and export-boundary cases. No external preprocessor is required by default tests, and no upstream implementation text is imported.
