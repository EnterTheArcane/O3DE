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
import { run, sudo } from "../exec.js";
import type { Tool } from "./tool.js";

/**
 * Install ccache.
 *
 * - macOS/Windows: download prebuilt binary from GitHub Releases.
 * - Linux: use apt (no prebuilt binaries published for Linux).
 *
 * When version is "latest" on Linux, whatever the package manager provides is
 * used. On macOS/Windows, "latest" resolves via GitHub redirect.
 */
async function install(version: string): Promise<void> {
    if (platform.isLinux) {
        await installApt();
    } else if (platform.isMacOS) {
        await installFromGitHub(version);
    } else if (platform.isWindows) {
        await installFromGitHub(version);
    }

    const ccachePath = await io.which("ccache", true);
    core.info(`ccache: ${ccachePath}`);
}

async function installApt(): Promise<void> {
    core.info("Installing ccache via apt...");
    sudo("apt-get", ["install", "-y", "ccache"]);
}

async function installFromGitHub(version: string): Promise<void> {
    const { url, binPath } = resolveDownload(version);

    let toolDir = tc.find("ccache", version);
    if (!toolDir) {
        core.info(`Downloading ccache ${version}...`);
        const archive = await tc.downloadTool(url);
        const extracted = platform.isWindows
            ? await tc.extractZip(archive)
            : await tc.extractTar(archive);
        toolDir = await tc.cacheDir(extracted, "ccache", version);
    } else {
        core.info(`ccache ${version} found in tool cache`);
    }

    const binDir = path.join(toolDir, binPath);

    // Ensure the binary is executable (macOS tar extraction may not preserve)
    if (!platform.isWindows) {
        const binary = path.join(binDir, "ccache");
        if (fs.existsSync(binary)) {
            fs.chmodSync(binary, 0o755);
        }
    }

    core.addPath(binDir);
}

function resolveDownload(version: string): { url: string; binPath: string } {
    if (platform.isMacOS) {
        return {
            url: `https://github.com/ccache/ccache/releases/download/v${version}/ccache-${version}-darwin.tar.gz`,
            binPath: `ccache-${version}-darwin`,
        };
    }
    // Windows
    return {
        url: `https://github.com/ccache/ccache/releases/download/v${version}/ccache-${version}-windows-x86_64.zip`,
        binPath: `ccache-${version}-windows-x86_64`,
    };
}

export const ccache: Tool = {
    name: "ccache",
    platforms: [],
    install,
};
