# Preprocessed-input compatibility fixture

`main.preprocessed` preserves the historical input for the `RespectEmitLine` tests. The test harness explicitly selects `--preprocessed` for this fixture.

Native include, macro, diagnostic and source-location behavior is tested by `ShaderCompiler.Tests`. Use `azslc -E source.azsl -o source.preprocessed` to export a new diagnostic fixture.
