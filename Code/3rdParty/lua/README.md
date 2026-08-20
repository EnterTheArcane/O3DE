# Lua source provenance

- Upstream: https://www.lua.org/ftp/lua-5.4.4.tar.gz
- Version: 5.4.4
- Archive SHA-256: `164c7849653b80ae67bec4b7473b884bf5cc8d2dca05653475ec2ed27b9ebf61`
- Imported: the Lua core and standard-library sources and headers
- Excluded: the `lua` interpreter, `luac` compiler, documentation, and tests
- Local modifications:
  - `Patches/ios-disable-os-execute.patch` disables process execution on iOS.
  - `Patches/mobile-disable-file-operations.patch` disables unsupported rename and temporary-file operations on iOS and Android.
