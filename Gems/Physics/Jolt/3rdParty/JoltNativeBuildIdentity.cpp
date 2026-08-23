/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <cstdint>

extern "C" std::uint64_t O3DEJoltNativeBuildFingerprint()
{
    return JOLT_NATIVE_BUILD_FINGERPRINT;
}
