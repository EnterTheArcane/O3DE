//
// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//

import { readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const [, , sdk, bits] = process.argv;
const modulePath = pathToFileURL(resolve(sdk, "bin/python3.15.mjs"));
const { default: python } = await import(modulePath);

function copyLibrary(FS, source, destination) {
  FS.mkdirTree(destination);
  for (const entry of readdirSync(source, { withFileTypes: true })) {
    const from = join(source, entry.name);
    const to = `${destination}/${entry.name}`;
    if (entry.isDirectory() && entry.name !== "__pycache__") {
      copyLibrary(FS, from, to);
    } else if (entry.isFile()) {
      FS.writeFile(to, readFileSync(from));
    }
  }
}

await python({
  thisProgram: "/python/bin/python3.15",
  preRun({ FS }) {
    copyLibrary(FS, join(sdk, "lib/python3.15"), "/python/lib/python3.15");
  },
  arguments: [
    "-IB",
    "-c",
    `import bz2, compression.zstd, hashlib, sys, zlib
assert sys.version_info[:3] == (3, 15, 2)
assert (sys.maxsize.bit_length() + 1) == ${bits}
assert sys.base_prefix == "/python"
print("CPython wasm${bits} passed")`,
  ],
});
