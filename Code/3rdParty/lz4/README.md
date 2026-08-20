# lz4 source provenance

- Source archive: https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
- Version: 1.10.0
- Archive SHA-256: `537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b`
- Imported: the `lib` C sources and private header under `Source`, with public headers under `Include`, excluding LZ4's bundled xxHash implementation
- Excluded: bundled xxHash, command-line programs, tests, examples, documentation, and upstream build files
- Local modifications: `Patches/use-external-xxhash.patch` makes `lz4frame.c` use O3DE's vendored xxHash 0.8.3 target
