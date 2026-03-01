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

/**
 * Ninja ≥1.12 uses platform-triple naming; older versions use simple names.
 * We support the modern convention and fall back to legacy if download fails.
 */
function resolveDownload(version: string): { url: string; legacy: boolean } {
    if (platform.isWindows) {
        return {
            url: `https://github.com/ninja-build/ninja/releases/download/v${version}/ninja-x86_64-pc-windows-msvc.zip`,
            legacy: false,
        };
    }
    if (platform.isMacOS) {
        return {
            url: `https://github.com/ninja-build/ninja/releases/download/v${version}/ninja-universal-apple-darwin.tar.gz`,
            legacy: false,
        };
    }
    return {
        url: `https://github.com/ninja-build/ninja/releases/download/v${version}/ninja-x86_64-linux-gnu.tar.gz`,
        legacy: false,
    };
}

function resolveLegacyDownload(version: string): string {
    if (platform.isWindows) {
        return `https://github.com/ninja-build/ninja/releases/download/v${version}/ninja-win.zip`;
    }
    if (platform.isMacOS) {
        return `https://github.com/ninja-build/ninja/releases/download/v${version}/ninja-mac.zip`;
    }
    return `https://github.com/ninja-build/ninja/releases/download/v${version}/ninja-linux.zip`;
}

/**
 * Ninja's newer archives sometimes name the binary after the platform triple
 * instead of just "ninja". Rename it if needed.
 */
function fixExecutableName(dir: string): void {
    const expected = platform.isWindows ? "ninja.exe" : "ninja";
    if (fs.existsSync(path.join(dir, expected))) return;

    for (const entry of fs.readdirSync(dir)) {
        if (entry.startsWith("ninja") && entry !== expected) {
            const src = path.join(dir, entry);
            const dest = path.join(dir, expected);
            fs.renameSync(src, dest);
            if (!platform.isWindows) {
                fs.chmodSync(dest, 0o755);
            }
            core.info(`Renamed ${entry} → ${expected}`);
            return;
        }
    }
}

async function download(url: string): Promise<string> {
    const archive = await tc.downloadTool(url);
    return platform.isWindows
        ? await tc.extractZip(archive)
        : await tc.extractTar(archive);
}

async function install(version: string): Promise<void> {
    let toolDir = tc.find("ninja", version);
    if (!toolDir) {
        const { url } = resolveDownload(version);
        let extracted: string;
        try {
            core.info(`Downloading Ninja ${version}...`);
            extracted = await download(url);
        } catch {
            core.info("Modern archive not found, trying legacy naming...");
            const legacyUrl = resolveLegacyDownload(version);
            extracted = await download(legacyUrl);
        }
        fixExecutableName(extracted);
        toolDir = await tc.cacheDir(extracted, "ninja", version);
    } else {
        core.info(`Ninja ${version} found in tool cache`);
    }

    core.addPath(toolDir);

    const ninjaPath = await io.which("ninja", true);
    core.info(`ninja: ${ninjaPath}`);
}

export const ninja: Tool = {
    name: "ninja",
    platforms: [],
    install,
};
