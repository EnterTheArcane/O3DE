/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <android/api-level.h>

static_assert(__ANDROID_API__ == EXPECTED_API);

extern "C" int ndk_probe()
{
    return __ANDROID_API__;
}
