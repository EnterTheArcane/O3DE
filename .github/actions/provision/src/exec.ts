/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import { execFileSync } from "child_process";
import * as core from "@actions/core";
import { platform } from "@actions/core";

/** Run a command, streaming output to Actions log. Throws on non-zero exit. */
export function run(command: string, args: string[] = []): void {
    core.info(`> ${command} ${args.join(" ")}`);
    execFileSync(command, args, { stdio: "inherit" });
}

/** Run a command with sudo on Linux (when not already root), passthrough on other platforms. */
export function sudo(command: string, args: string[] = []): void {
    if (platform.isLinux && process.getuid?.() !== 0) {
        run("sudo", [command, ...args]);
    } else {
        run(command, args);
    }
}
