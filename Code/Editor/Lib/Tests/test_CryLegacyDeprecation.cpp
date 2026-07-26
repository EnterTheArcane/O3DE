/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "EditorDefs.h"
#include <AzTest/AzTest.h>

namespace EditorUtilsTest
{
    class LegacryDeprecationHelper : public ::testing::Test
    {
    public:
        LegacryDeprecationHelper()
        {
        }
    };

    // CryCommon->AzCore migration: the Matrix33/34/44 and Vec/Color equivalence tests were
    // removed. The Cry Matrix33/34/44 and Quat types are gone (migrated to AZ::Matrix3x3/3x4/4x4
    // and AZ::Quaternion), and ColorF/Vec2/3/4 are now direct aliases of the AZ types. The
    // ColorB test below still exercises a genuine remaining Cry type (uint8 color, no AZ analog).
    TEST_F(LegacryDeprecationHelper, TestLegacyColor_ToU32)
    {
        AZ_PUSH_DISABLE_WARNING(4996, "-Wdeprecated-declarations");

        ColorB legacyColor(50, 100, 150, 200);
        AZ::Color newColor(50, 100, 150, 200);

        ASSERT_EQ(legacyColor.pack_abgr8888(), newColor.ToU32());

        AZ_POP_DISABLE_WARNING;
    }
} // namespace EditorUtilsTest
