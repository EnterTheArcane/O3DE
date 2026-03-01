/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

/**
 * Every provisionable tool implements this interface.
 * To add a new tool, create a file in src/tools/ that exports a Tool and
 * register it in src/tools/index.ts.
 */
export interface Tool {
    /** Human-readable tool name for log messages. */
    readonly name: string;

    /** Platforms this tool supports. Empty array means all platforms. */
    readonly platforms: NodeJS.Platform[];

    /**
     * Install the tool at the requested version.
     * @param version  Exact version string or "latest".
     */
    install(version: string): Promise<void>;
}
