/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

import type { Tool } from "./tool.js";
import { cmake } from "./cmake.js";
import { ninja } from "./ninja.js";
import { ccache } from "./ccache.js";
import { clang } from "./clang.js";
import { msvc } from "./msvc.js";

/**
 * Registry mapping action input names to tool implementations.
 * To add a new tool: create the module in src/tools/, then add an entry here.
 * Order determines installation order.
 */
export const tools: ReadonlyMap<string, Tool> = new Map([
    ["cmake", cmake],
    ["ninja", ninja],
    ["ccache", ccache],
    ["clang", clang],
    ["msvc", msvc],
]);
