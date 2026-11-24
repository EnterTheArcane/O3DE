// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

const path = require("node:path");
const [, , executable, libraries] = process.argv;

require(executable)({
    locateFile(name, prefix) {
        if (name.startsWith("libQt6")) {
            return path.join(libraries, name);
        }
        return prefix + name;
    },
});
