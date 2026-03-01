/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import { execSync } from "child_process";
import * as core from "@actions/core";
import * as fs from "fs";
import * as path from "path";
import { run } from "../exec.js";
import type { Tool } from "./tool.js";

const PROGRAM_FILES_X86 = process.env["ProgramFiles(x86)"] ?? "";
const PROGRAM_FILES = [PROGRAM_FILES_X86, process.env["ProgramFiles"] ?? ""];

const EDITIONS = ["Enterprise", "Professional", "Community", "BuildTools"];
const YEARS = ["2022", "2019", "2017"];

const VS_YEAR_VERSION: Record<string, string> = {
    "2022": "17.0",
    "2019": "16.0",
    "2017": "15.0",
    "2015": "14.0",
};

function yearToVersion(year: string): string | undefined {
    return VS_YEAR_VERSION[year];
}

function versionToYear(version: string): string | undefined {
    for (const [year, ver] of Object.entries(VS_YEAR_VERSION)) {
        if (ver === version) return year;
    }
    return undefined;
}

const VSWHERE_PATH = `${PROGRAM_FILES_X86}\\Microsoft Visual Studio\\Installer`;

/** Locate vcvarsall.bat using vswhere, then standard paths. */
function findVcvarsall(vsversion?: string): string {
    // Add vswhere to PATH for discovery
    process.env.PATH = `${process.env.PATH}${path.delimiter}${VSWHERE_PATH}`;

    // Try vswhere first
    let versionPattern = "-latest";
    if (vsversion) {
        const verNum = yearToVersion(vsversion) ?? vsversion;
        const major = verNum.split(".")[0];
        versionPattern = `-version "${verNum},${major}.9"`;
    }

    try {
        const installPath = execSync(
            `vswhere -products * ${versionPattern} -prerelease -property installationPath`,
            { encoding: "utf8" },
        ).trim();
        const vcvars = `${installPath}\\VC\\Auxiliary\\Build\\vcvarsall.bat`;
        if (fs.existsSync(vcvars)) {
            core.info(`Found with vswhere: ${vcvars}`);
            return vcvars;
        }
    } catch {
        core.info("vswhere lookup failed, trying standard locations...");
    }

    // Standard locations
    const years = vsversion ? [versionToYear(vsversion) ?? vsversion] : YEARS;
    for (const progFiles of PROGRAM_FILES) {
        for (const year of years) {
            for (const edition of EDITIONS) {
                const vcvars = `${progFiles}\\Microsoft Visual Studio\\${year}\\${edition}\\VC\\Auxiliary\\Build\\vcvarsall.bat`;
                if (fs.existsSync(vcvars)) {
                    core.info(`Found standard location: ${vcvars}`);
                    return vcvars;
                }
            }
        }
    }

    throw new Error("Could not find vcvarsall.bat — is Visual Studio installed?");
}

const PATH_LIKE = new Set(["PATH", "INCLUDE", "LIB", "LIBPATH"]);

function deduplicatePath(value: string): string {
    const seen = new Set<string>();
    return value
        .split(";")
        .filter((p) => {
            if (seen.has(p)) return false;
            seen.add(p);
            return true;
        })
        .join(";");
}

/**
 * Install an MSVC toolset version and configure the developer environment.
 *
 * The version string is the VC toolset version (e.g. "14.29").
 * This:
 *   1. Installs the specific toolset component via vs_installer if needed
 *   2. Runs vcvarsall.bat with the toolset
 *   3. Exports the resulting environment variables into the workflow
 */
async function install(version: string): Promise<void> {
    await installToolset(version);
    setupDevEnvironment(version);
}

/** Install a specific MSVC toolset component via the VS installer. */
async function installToolset(version: string): Promise<void> {
    const component = `Microsoft.VisualStudio.Component.VC.${version}.x86.x64`;
    const installerPaths = [
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vs_installer.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vs_installer.exe",
    ];

    let installerPath: string | undefined;
    for (const p of installerPaths) {
        if (fs.existsSync(p)) {
            installerPath = p;
            break;
        }
    }

    if (!installerPath) {
        core.warning("VS Installer not found, assuming toolset is already installed");
        return;
    }

    // Find VS installation path
    let installPath: string;
    try {
        installPath = execSync(
            "vswhere -products * -latest -prerelease -property installationPath",
            { encoding: "utf8" },
        ).trim();
    } catch {
        throw new Error("Could not find Visual Studio installation via vswhere");
    }

    core.info(`Installing MSVC toolset component: ${component}`);
    run(installerPath, [
        "modify",
        "--installPath", installPath,
        "--add", component,
        "--quiet",
        "--norestart",
        "--wait",
    ]);
}

/**
 * Run vcvarsall.bat and export the resulting environment changes.
 * Uses the `set && cls && vcvars && cls && set` technique to diff environment.
 */
function setupDevEnvironment(toolsetVersion: string): void {
    const vcvarsall = findVcvarsall();
    const args = `x64 -vcvars_ver=${toolsetVersion}`;
    const cmd = `set && cls && "${vcvarsall}" ${args} && cls && set`;

    core.info(`Running: vcvarsall.bat ${args}`);
    const output = execSync(cmd, { shell: "cmd", encoding: "utf8" });
    const parts = output.split("\f");

    if (parts.length < 3) {
        throw new Error("Unexpected vcvarsall output format");
    }

    // Parse old and new environment
    const parseEnv = (lines: string[]): Map<string, string> => {
        const env = new Map<string, string>();
        for (const line of lines) {
            const eq = line.indexOf("=");
            if (eq > 0) {
                env.set(line.substring(0, eq), line.substring(eq + 1));
            }
        }
        return env;
    };

    const oldEnv = parseEnv(parts[0].split("\r\n"));
    const vcvarsOutput = parts[1];
    const newEnv = parseEnv(parts[2].split("\r\n"));

    // Check for errors in vcvars output
    for (const line of vcvarsOutput.split("\r\n")) {
        if (line.match(/^\[ERROR.*\]/) && !line.includes("correct usage")) {
            throw new Error(`vcvarsall error: ${line}`);
        }
    }

    // Export changed/new variables
    // For PATH-like variables, only add new entries via core.addPath() rather
    // than replacing the entire value, so we don't clobber GITHUB_PATH additions
    // from earlier tools (cmake, ccache) provisioned in the same step.
    let exported = 0;
    for (const [name, newValue] of newEnv) {
        const oldValue = oldEnv.get(name);
        if (newValue !== oldValue) {
            if (name.toUpperCase() === "PATH") {
                const oldPaths = new Set(
                    (oldValue ?? "").split(";").map((p) => p.toLowerCase()),
                );
                const newEntries = newValue
                    .split(";")
                    .filter((p) => p && !oldPaths.has(p.toLowerCase()));
                for (const entry of newEntries) {
                    core.addPath(entry);
                }
                exported += newEntries.length;
            } else {
                const value = PATH_LIKE.has(name.toUpperCase())
                    ? deduplicatePath(newValue)
                    : newValue;
                core.exportVariable(name, value);
                exported++;
            }
        }
    }

    core.info(`Exported ${exported} environment variables from MSVC developer prompt`);
}

export const msvc: Tool = {
    name: "MSVC",
    platforms: ["win32"],
    install,
};
