/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#ifndef AZ_RELEASE_BUILD
#include <AzCore/Debug/Profiler.h>
#define JOLT_PROFILE_SCOPE(Budget, Name) AZ_PROFILE_SCOPE(Budget, Name)
#else
#define JOLT_PROFILE_SCOPE(Budget, Name)
#endif
