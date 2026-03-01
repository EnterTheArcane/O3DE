/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import { DefaultArtifactClient } from "@actions/artifact";
import * as fs from "fs";
import * as path from "path";
import * as tar from "tar";

interface State {
    name: string;
    path: string;
    save: boolean;
}

function readState(): State | undefined {
    const state: State = {
        name: core.getState("name"),
        path: core.getState("path"),
        save: core.getState("save") === "true",
    };

    if (!state.name) {
        core.info("Main step did not run, skipping save");
        return undefined;
    }
    if (!state.save) {
        core.info("Artifact save is disabled for this build type, skipping");
        return undefined;
    }
    if (!state.path) {
        core.warning("Missing cache path from state");
        return undefined;
    }

    return state;
}

function validateCache(dir: string): boolean {
    if (!fs.existsSync(dir)) {
        core.warning(`Cache path does not exist: ${dir}`);
        return false;
    }
    if (fs.readdirSync(dir).length === 0) {
        core.info("Cache directory is empty, nothing to save");
        return false;
    }
    return true;
}

async function createArchive(dir: string, archiveFile: string): Promise<boolean> {
    const cacheParent = path.dirname(dir);
    const cacheDirName = path.basename(dir);

    core.info(`Compressing cache: ${dir}`);
    try {
        await tar.create(
            { file: archiveFile, cwd: cacheParent, gzip: true, portable: true },
            [cacheDirName],
        );
    } catch (e: unknown) {
        const msg = e instanceof Error ? e.message : String(e);
        core.warning(`Archive compression issue: ${msg}`);
    }

    if (!fs.existsSync(archiveFile)) {
        core.warning("Archive was not created, skipping upload");
        return false;
    }
    
    return true;
}

async function uploadArchive(name: string, archiveFile: string, cacheParent: string): Promise<void> {
    const artifact = new DefaultArtifactClient();

    // Delete existing artifact from this run to allow overwrite
    try {
        await artifact.deleteArtifact(name);
        core.info(`Deleted existing artifact '${name}'`);
    } catch {
        // Artifact doesn't exist yet
    }

    core.info(`Uploading artifact '${name}'...`);
    const { id, size } = await artifact.uploadArtifact(
        name,
        [archiveFile],
        cacheParent,
        { skipArchive: true },
    );

    const sizeMB = ((size ?? 0) / (1024 * 1024)).toFixed(1);
    core.info(`Artifact '${name}' uploaded (id: ${id}, size: ${sizeMB} MB)`);
}

async function run(): Promise<void> {
    try {
        const state = readState();
        if (!state) {
            return;
        }

        if (!validateCache(state.path)) {
            return;
        }

        const cacheParent = path.dirname(state.path);
        const archiveFile = path.join(cacheParent, state.name);

        if (!await createArchive(state.path, archiveFile)) {
            return;
        }

        try {
            await uploadArchive(state.name, archiveFile, cacheParent);
        } finally {
            // Cleanup archive file regardless of upload success
            try { fs.unlinkSync(archiveFile); } catch { /* ignore */ }
        }
    } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        core.warning(`Cache save failed: ${msg}`);
    }
}

run();
