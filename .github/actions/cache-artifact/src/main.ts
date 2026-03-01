/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import * as github from "@actions/github";
import { DefaultArtifactClient, type Artifact } from "@actions/artifact";
import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";
import * as tar from "tar";

interface Inputs {
    name: string;
    path: string;
    searchPrevious: boolean;
    save: boolean;
    token: string;
}

function readInputs(): Inputs {
    const inputs: Inputs = {
        name: core.getInput("name", { required: true }),
        path: core.getInput("path") || "Cache",
        searchPrevious: core.getBooleanInput("search-previous"),
        save: core.getBooleanInput("save"),
        token: core.getInput("token"),
    };

    if (!path.isAbsolute(inputs.path)) {
        inputs.path = path.join(process.env.GITHUB_WORKSPACE!, inputs.path);
    }
    inputs.path = core.toPlatformPath(inputs.path);

    return inputs;
}

function persistState(inputs: Inputs): void {
    core.saveState("name", inputs.name);
    core.saveState("path", inputs.path);
    core.saveState("save", inputs.save.toString());
    core.saveState("token", inputs.token);
}

/**
 * Search for the named artifact in the current workflow run, then optionally
 * in previous completed runs on the target branch. Returns the artifact and
 * the `findBy` options needed to download it, or `undefined` if not found.
 */
async function findArtifact(
    inputs: Inputs,
    artifact: DefaultArtifactClient,
): Promise<{ found: Artifact; downloadOpts: Record<string, unknown> } | undefined> {
    // Try 1: Current workflow run (handles job chaining and reruns)
    try {
        const { artifact: found } = await artifact.getArtifact(inputs.name);
        if (found) {
            core.info(`Found artifact '${inputs.name}' in current run (id: ${found.id})`);
            return { found, downloadOpts: {} };
        }
    } catch {
        core.info(`No artifact '${inputs.name}' in current run`);
    }

    // Try 2: Previous workflow runs on the target branch
    if (!inputs.searchPrevious) {
        return undefined;
    }

    const { owner, repo } = github.context.repo;
    const octokit = github.getOctokit(inputs.token);
    const targetBranch =
        github.context.eventName === "pull_request"
            ? github.context.payload.pull_request!.base.ref
            : process.env.GITHUB_REF_NAME || github.context.ref.replace("refs/heads/", "");

    core.info(`Searching previous runs on branch '${targetBranch}' for artifact '${inputs.name}'...`);

    try {
        const { data: runs } = await octokit.rest.actions.listWorkflowRunsForRepo({
            owner,
            repo,
            branch: targetBranch,
            per_page: 10,
            status: "completed",
        });

        for (const run of runs.workflow_runs) {
            const findBy = {
                token: inputs.token,
                workflowRunId: run.id,
                repositoryOwner: owner,
                repositoryName: repo,
            };

            try {
                const { artifact: found } = await artifact.getArtifact(inputs.name, { findBy });
                if (found) {
                    core.info(`Found artifact from run #${run.id} on '${targetBranch}', downloading...`);
                    return { found, downloadOpts: { findBy } };
                }
            } catch {
                // Artifact not found in this run, continue
            }
        }

        core.info("No matching artifact found in previous runs");
    } catch (e: unknown) {
        const msg = e instanceof Error ? e.message : String(e);
        core.warning(`Error searching previous runs: ${msg}`);
    }

    return undefined;
}

async function extractArchive(archiveFile: string, cacheParent: string): Promise<void> {
    core.info("Extracting cache artifact...");
    await tar.extract({ file: archiveFile, cwd: cacheParent, gzip: true });
    fs.unlinkSync(archiveFile);
    core.info("Cache artifact restored successfully");
}

async function run(): Promise<void> {
    try {
        const inputs = readInputs();
        persistState(inputs);

        const cacheParent = path.dirname(inputs.path);
        const archiveFile = path.join(cacheParent, inputs.name);

        fs.mkdirSync(inputs.path, { recursive: true });

        if (!inputs.token) {
            core.warning("No token provided, cannot search for artifacts");
            return;
        }

        const runAttempt = parseInt(process.env.GITHUB_RUN_ATTEMPT || "1", 10);
        if (runAttempt > 1) {
            core.info(`Rerun attempt #${runAttempt}`);
        }

        const artifact = new DefaultArtifactClient();
        const result = await findArtifact(inputs, artifact);

        if (!result) {
            core.info("No cache artifact to restore");
            core.setOutput("cache-hit", false);
            return;
        }

        await artifact.downloadArtifact(result.found.id, {
            path: cacheParent,
            ...result.downloadOpts,
        });

        if (fs.existsSync(archiveFile)) {
            await extractArchive(archiveFile, cacheParent);
            core.setOutput("cache-hit", true);
        } else {
            core.warning(`Artifact downloaded but '${inputs.name}' not found in expected location`);
            core.setOutput("cache-hit", false);
        }
    } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        core.warning(`Cache restore failed: ${msg}`);
        core.setOutput("cache-hit", false);
    }
}

run();
