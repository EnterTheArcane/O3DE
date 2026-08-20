# mcpp source provenance

- Upstream: https://sourceforge.net/projects/mcpp/files/mcpp/V.2.7.2/mcpp-2.7.2.tar.gz/download
- Version: 2.7.2
- Archive SHA-256: `3b9b4421888519876c4fc68ade324a3bbd81ceeb7092ecdbbc2055099fcb8864`
- Imported: the `libmcpp` sources, internal headers, and public library headers
- Excluded: command-line drivers, replacement-compiler tools, tests, documentation, and autotools files
- Local modifications:
  - `Patches/compiler-configuration.patch` carries the compiler defaults required by the library build.
  - `Patches/library-api.patch` carries O3DE's C linkage, const argument, import macro, and output-enum API changes.
  - `Patches/include-reporting.patch` adds the include-report callback.
  - `Patches/line-number-preservation.patch` preserves source-to-preprocessed line mapping.
  - `Patches/path-handling.patch` preserves Windows separators and absolute source paths.
  - `Patches/whitespace-cleanup.patch` removes upstream trailing whitespace from imported files.
  - `Source/config.h` replaces configure-time host probing for O3DE platforms.
