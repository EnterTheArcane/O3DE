/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import * as io from "@actions/io";
import { sudo } from "../exec.js";
import type { Tool } from "./tool.js";

/**
 * Install Clang + LLD on Linux via apt.
 *
 * The version input is the major version number (e.g. "18", "14").
 * "latest" installs the unversioned packages (clang, lld).
 */
async function install(version: string): Promise<void> {
    const isLatest = version === "latest";
    const clangPkg = isLatest ? "clang" : `clang-${version}`;
    const lldPkg = isLatest ? "lld" : `lld-${version}`;

    core.info(`Installing ${clangPkg} and ${lldPkg} via apt...`);
    sudo("apt-get", ["install", "-y", clangPkg, lldPkg]);

    // Verify installation
    const suffix = isLatest ? "" : `-${version}`;
    const clangBin = await io.which(`clang${suffix}`, true);
    core.info(`clang: ${clangBin}`);

    // Export CC/CXX so CMake picks up the right compiler
    core.exportVariable("CC", `clang${suffix}`);
    core.exportVariable("CXX", `clang++${suffix}`);
    core.info(`Set CC=clang${suffix}, CXX=clang++${suffix}`);
}

export const clang: Tool = {
    name: "Clang",
    platforms: ["linux"],
    install,
};
