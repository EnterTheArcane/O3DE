/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#ifdef JOLT_TESTS_WITH_TOOLS
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#endif

#ifdef JOLT_TESTS_WITH_TOOLS
AZ_TOOLS_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);
#else
AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);
#endif
