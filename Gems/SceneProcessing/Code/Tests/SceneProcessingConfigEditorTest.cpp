/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#include <SceneSettings/SceneSettingsEditorSystemComponent.h>

AZ_TOOLS_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace UnitTest
{
    TEST(SceneSettingsReflectionTests, AssetImporterScriptingBusAndEventAreReflected)
    {
        AZ::BehaviorContext behaviorContext;
        SceneSettingsEditorSystemComponent::Reflect(&behaviorContext);

        const auto busIterator = behaviorContext.m_ebuses.find("SceneSettingsAssetImporterForScriptRequestBus");
        ASSERT_NE(busIterator, behaviorContext.m_ebuses.end());

        const AZ::BehaviorEBus* behaviorBus = busIterator->second;
        EXPECT_NE(behaviorBus->m_events.find("EditImportSettings"), behaviorBus->m_events.end());
    }
} // namespace UnitTest
