# zstd source provenance

- Source archive: https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz
- Version: 1.5.7
- Archive SHA-256: `eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3`
- Imported: common, compression, decompression, dictionary-builder, legacy, and deprecated compatibility library sources and headers
- Excluded: programs, tests, examples, contrib code, upstream build files, and the bundled xxHash implementation
- Local modifications: the public `zbuff.h` copy uses its flattened installed include path; optional assembly is disabled so every O3DE architecture uses the portable implementation; `Patches/use-external-xxhash.patch` makes zstd use O3DE's vendored xxHash library
