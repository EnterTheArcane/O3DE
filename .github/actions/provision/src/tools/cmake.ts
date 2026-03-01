/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import * as io from "@actions/io";
import * as tc from "@actions/tool-cache";
import * as path from "path";
import * as fs from "fs";
import { platform } from "@actions/core";
import type { Tool } from "./tool.js";

/** Resolve platform-specific download URL and binary subdirectory. */
function resolveDownload(version: string): { url: string; binSubdir: string } {
    if (platform.isWindows) {
        return {
            url: `https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}-windows-x86_64.zip`,
            binSubdir: `cmake-${version}-windows-x86_64/bin`,
        };
    }
    if (platform.isMacOS) {
        return {
            url: `https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}-macos-universal.tar.gz`,
            binSubdir: `cmake-${version}-macos-universal/CMake.app/Contents/bin`,
        };
    }
    // Linux
    const suffix = platform.arch === "arm64" ? "linux-aarch64" : "linux-x86_64";
    return {
        url: `https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}-${suffix}.tar.gz`,
        binSubdir: `cmake-${version}-${suffix}/bin`,
    };
}

async function install(version: string): Promise<void> {
    const { url, binSubdir } = resolveDownload(version);

    // Check tool-cache first (self-hosted runner reuse)
    let toolDir = tc.find("cmake", version);
    if (!toolDir) {
        core.info(`Downloading CMake ${version}...`);
        const archive = await tc.downloadTool(url);
        const extracted = platform.isWindows
            ? await tc.extractZip(archive)
            : await tc.extractTar(archive);
        toolDir = await tc.cacheDir(extracted, "cmake", version);
    } else {
        core.info(`CMake ${version} found in tool cache`);
    }

    const binDir = path.join(toolDir, binSubdir);
    core.addPath(binDir);

    const cmakePath = await io.which("cmake", true);
    core.info(`cmake: ${cmakePath}`);
}

export const cmake: Tool = {
    name: "CMake",
    platforms: [],
    install,
};
