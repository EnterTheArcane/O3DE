/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Per-API Slang prelude for the Null RHI, injected by the shader builder into every Slang
// compile for this RHI — the counterpart of AzslcHeader.azsli in the legacy pipeline.

public static const bool AtomApiIsDx12 = false;
public static const bool AtomApiIsVulkan = false;
