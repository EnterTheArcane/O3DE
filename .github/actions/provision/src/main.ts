/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import * as core from "@actions/core";
import { platform } from "@actions/core";
import { tools } from "./tools/index.js";

async function run(): Promise<void> {
    const errors: string[] = [];

    for (const [input, tool] of tools) {
        const version = core.getInput(input);
        if (!version) continue;

        if (tool.platforms.length > 0 && !tool.platforms.includes(platform.platform)) {
            core.info(`Skipping ${tool.name}: not supported on ${platform.platform}`);
            continue;
        }

        core.startGroup(`Provision ${tool.name} ${version}`);
        try {
            await tool.install(version);
            core.info(`${tool.name} ${version} provisioned successfully`);
        } catch (e: unknown) {
            const msg = e instanceof Error ? e.message : String(e);
            core.error(`Failed to provision ${tool.name}: ${msg}`);
            errors.push(tool.name);
        } finally {
            core.endGroup();
        }
    }

    if (errors.length > 0) {
        core.setFailed(`Failed to provision: ${errors.join(", ")}`);
    }
}

run();
