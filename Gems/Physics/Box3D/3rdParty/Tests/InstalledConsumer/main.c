/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <box3d/box3d.h>

int main(void)
{
    const b3Version version = b3GetVersion();
    return version.major == 0 && version.minor == 1 && version.revision == 0 ? 0 : 1;
}
