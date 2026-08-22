/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/PlatformDef.h>

#ifdef AZ_MONOLITHIC_BUILD
    #define JOLT_API
#elif defined(JOLT_API_EXPORTS)
    #define JOLT_API AZ_DLL_EXPORT
#else
    #define JOLT_API AZ_DLL_IMPORT
#endif
