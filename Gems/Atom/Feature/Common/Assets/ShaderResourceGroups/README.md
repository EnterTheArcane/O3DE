# Project Shader Resource Groups

Projects can customize `scenesrg.srgi` and `viewsrg.srgi` through their `ShaderLib` directory or configured include paths. Keep one selected definition of each group for a compiler environment.

The unconditional Terrain scene-SRG declaration lives in Common's `TerrainSceneSrg.azsli`, so Asset Processor watches it even when Terrain is disabled. Keep unconditional partial-SRG declarations in an active gem's watched assets. Their collector order defines the shared layout.

Quoted includes first search the including file's directory. The shader builder then supplies include directories in this order:

1. Configured `-I` arguments from `AddBuildArguments.preprocessor`.
2. The root AZSL file's directory, if it exists.
3. The project root.
4. Existing `ShaderLib` directories under Asset Processor scan folders, in scan-folder order.
5. The engine's `Gems` directory.
6. Additional `-I` arguments from `AddBuildArguments.azslc`.

Angled includes use the configured directories without the including-directory step. Relative configured paths resolve against the project. Configure arguments in `shader_build_options.settings`, with `/O3DE/Atom/Shaders/Build/ConfigPath` selecting its configuration directory.
