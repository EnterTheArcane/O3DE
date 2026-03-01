/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import { DefaultArtifactClient } from "@actions/artifact";
import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

async function run(): Promise<void> {
    try {
        const name = core.getState("name");
        const cachePath = core.getState("cachePath");
        const save = core.getState("save") === "true";

        if (!name) {
            core.info("Main step did not run, skipping save");
            return;
        }

        if (!save) {
            core.info("Artifact save is disabled for this build type, skipping");
            return;
        }

        if (!cachePath) {
            core.warning("Missing cache path from state");
            return;
        }

        if (!fs.existsSync(cachePath)) {
            core.warning(`Cache path does not exist: ${cachePath}`);
            return;
        }

        const contents = fs.readdirSync(cachePath);
        if (contents.length === 0) {
            core.info("Cache directory is empty, nothing to save");
            return;
        }

        const cacheParent = path.dirname(cachePath);
        const cacheDirName = path.basename(cachePath);
        const tarFile = path.join(cacheParent, "cache.tar");

        // Create tar archive of the cache directory
        core.info(`Compressing cache: ${cachePath}`);
        try {
            execSync(
                `tar --format=posix -cpf "${tarFile}" -C "${cacheParent}" "${cacheDirName}"`,
                { stdio: "inherit" },
            );
        } catch (e: unknown) {
            const msg = e instanceof Error ? e.message : String(e);
            core.warning(`Tar compression issue: ${msg}`);
            if (!fs.existsSync(tarFile)) {
                core.warning("Tar file was not created, skipping upload");
                return;
            }
        }

        const artifact = new DefaultArtifactClient();

        // Delete existing artifact from this run to allow overwrite
        try {
            await artifact.deleteArtifact(name);
            core.info(`Deleted existing artifact '${name}'`);
        } catch {
            // Artifact doesn't exist yet
        }

        // Upload the tar as an artifact
        core.info(`Uploading artifact '${name}'...`);
        const { id, size } = await artifact.uploadArtifact(
            name,
            [tarFile],
            cacheParent,
            { skipArchive: true },
        );

        const sizeMB = ((size ?? 0) / (1024 * 1024)).toFixed(1);
        core.info(`Artifact '${name}' uploaded (id: ${id}, size: ${sizeMB} MB)`);

        // Cleanup tar file
        try {
            fs.unlinkSync(tarFile);
        } catch {
            // Ignore cleanup errors
        }
    } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        core.warning(`Cache save failed: ${msg}`);
    }
}

run();
