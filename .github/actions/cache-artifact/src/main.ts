/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import * as github from "@actions/github";
import { DefaultArtifactClient } from "@actions/artifact";
import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";
import * as tar from "tar";

async function run(): Promise<void> {
    try {
        const name = core.getInput("name", { required: true });
        const searchPrevious = core.getInput("search-previous") === "true";
        const save = core.getInput("save") !== "false";
        const token = core.getInput("token");

        let cachePath = core.getInput("path") || "Cache";
        if (!path.isAbsolute(cachePath)) {
            cachePath = path.join(process.env.GITHUB_WORKSPACE!, cachePath);
        }
        cachePath = path.resolve(cachePath);

        // Persist state for the post step
        core.saveState("name", name);
        core.saveState("cachePath", cachePath);
        core.saveState("save", save.toString());
        core.saveState("token", token);

        fs.mkdirSync(cachePath, { recursive: true });

        if (!token) {
            core.warning("No token provided, cannot search for artifacts");
            return;
        }

        const artifact = new DefaultArtifactClient();
        const octokit = github.getOctokit(token);
        const { owner, repo } = github.context.repo;
        const cacheParent = path.dirname(cachePath);
        const archiveFile = path.join(cacheParent, name);

        const runAttempt = parseInt(process.env.GITHUB_RUN_ATTEMPT || "1", 10);

        if (runAttempt > 1) {
            core.info(`Rerun attempt #${runAttempt}`);
        }

        let restored = false;

        // Try 1: Check current workflow run for artifact.
        // Handles job chaining (Profile -> Asset -> Test) and reruns
        // where a previous attempt already uploaded.
        try {
            const { artifact: found } = await artifact.getArtifact(name);
            if (found) {
                core.info(`Found artifact '${name}' in current run (id: ${found.id})`);
                await artifact.downloadArtifact(found.id, { path: cacheParent });
                restored = true;
            }
        } catch {
            core.info(`No artifact '${name}' in current run`);
        }

        // Try 2: Search previous workflow runs on the target branch.
        if (!restored && searchPrevious) {
            const targetBranch =
                github.context.eventName === "pull_request"
                    ? github.context.payload.pull_request!.base.ref
                    : process.env.GITHUB_REF_NAME || github.context.ref.replace("refs/heads/", "");

            core.info(`Searching previous runs on branch '${targetBranch}' for artifact '${name}'...`);

            try {
                // List recent workflow runs on the target branch
                const { data: runs } = await octokit.rest.actions.listWorkflowRunsForRepo({
                    owner,
                    repo,
                    branch: targetBranch,
                    per_page: 10,
                    status: "completed",
                });

                for (const run of runs.workflow_runs) {
                    try {
                        const { artifact: found } = await artifact.getArtifact(name, {
                            findBy: {
                                token,
                                workflowRunId: run.id,
                                repositoryOwner: owner,
                                repositoryName: repo,
                            },
                        });

                        if (found) {
                            core.info(`Found artifact from run #${run.id} on '${targetBranch}', downloading...`);
                            await artifact.downloadArtifact(found.id, {
                                path: cacheParent,
                                findBy: {
                                    token,
                                    workflowRunId: run.id,
                                    repositoryOwner: owner,
                                    repositoryName: repo,
                                },
                            });
                            restored = true;
                            break;
                        }
                    } catch {
                        // Artifact not found in this run, continue
                    }
                }

                if (!restored) {
                    core.info("No matching artifact found in previous runs");
                }
            } catch (e: unknown) {
                const msg = e instanceof Error ? e.message : String(e);
                core.warning(`Error searching previous runs: ${msg}`);
            }
        }

        // Extract archive if we downloaded something
        if (restored && fs.existsSync(archiveFile)) {
            core.info("Extracting cache artifact...");
            await tar.extract({ file: archiveFile, cwd: cacheParent, gzip: true });
            fs.unlinkSync(archiveFile);

            // Reset ccache stats if available
            try {
                execSync("ccache --zero-stats --cleanup", { stdio: "inherit" });
                core.info("ccache stats reset");
            } catch {
                // ccache may not be installed yet
            }

            core.info("Cache artifact restored successfully");
        } else if (restored) {
            core.warning(`Artifact downloaded but '${name}' not found in expected location`);
        } else {
            core.info("No cache artifact to restore");
        }
    } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        core.warning(`Cache restore failed: ${msg}`);
    }
}

run();
