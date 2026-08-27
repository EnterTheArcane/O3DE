/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzCore/Settings/CommandLine.h>
#include <AzCore/Settings/SettingsRegistry.h>
#include <AzCore/Settings/SettingsRegistryImpl.h>
#include <AzCore/Settings/SettingsRegistryMergeUtils.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/algorithm.h>

#include "BuilderManagerTests.h"
#include <AzCore/std/smart_ptr/make_shared.h>
#include <native/connection/connectionManager.h>

namespace UnitTests
{
    class BuilderManagerTest : public ::UnitTest::LeakDetectionFixture
    {

    };

    TEST_F(BuilderManagerTest, GetBuilder_ReservesFirstBuilderForCreateJobs)
    {
        ConnectionManager cm{nullptr};
        TestBuilderManager bm(&cm);

        // We start off with 1 builder pre-created
        ASSERT_EQ(bm.GetBuilderCreationCount(), 1);

        // Save off the uuid of the CreateJobs builder for later
        auto createJobsBuilderUuid = bm.GetBuilder(AssetProcessor::BuilderPurpose::CreateJobs)->GetUuid();

        constexpr int NumberOfBuilders = 15; // Start up several builders, more builders increases the chances of exposing a bug
        AZStd::vector<AssetProcessor::BuilderRef> builders;

        for (int i = 0; i < NumberOfBuilders; ++i)
        {
            builders.push_back(bm.GetBuilder(AssetProcessor::BuilderPurpose::ProcessJob));
        }

        // There should now be NumberOfBuilders + 1 builders, because the first one is reserved for CreateJobs
        ASSERT_EQ(bm.GetBuilderCreationCount(), NumberOfBuilders + 1);

        // Now if we request a CreateJob builder, we should get the same builder again
        ASSERT_EQ(bm.GetBuilder(AssetProcessor::BuilderPurpose::CreateJobs)->GetUuid(), createJobsBuilderUuid);

        // And the number of builders should remain the same
        ASSERT_EQ(bm.GetBuilderCreationCount(), NumberOfBuilders + 1);

        // Release the builders and check that we still get the same builder for CreateJobs
        builders = {};

        ASSERT_EQ(bm.GetBuilder(AssetProcessor::BuilderPurpose::CreateJobs)->GetUuid(), createJobsBuilderUuid);
        ASSERT_EQ(bm.GetBuilderCreationCount(), NumberOfBuilders + 1);
    }

    TEST_F(BuilderManagerTest, BuildParams_PropagatesRegistryOverridesToChildBuilders)
    {
        auto* originalSettingsRegistry = AZ::SettingsRegistry::Get();
        if (originalSettingsRegistry)
        {
            AZ::SettingsRegistry::Unregister(originalSettingsRegistry);
        }

        AZ::SettingsRegistryImpl settingsRegistry;
        settingsRegistry.Set(AZ::SettingsRegistryMergeUtils::FilePathKey_EngineRootFolder, ".");
        settingsRegistry.Set(AZ::SettingsRegistryMergeUtils::FilePathKey_ProjectPath, ".");
        AZ::SettingsRegistryInterface::FixedValueString projectNameKey{
            AZ::SettingsRegistryMergeUtils::ProjectSettingsRootKey };
        projectNameKey += "/project_name";
        settingsRegistry.Set(projectNameKey, "AssetProcessorTests");
        AZ::SettingsRegistry::Register(&settingsRegistry);

        AZ::CommandLine commandLine;
        commandLine.Parse(
            {
                "--regset=/Jolt/Setting=enabled",
                "--regset-file=JoltQualificationRegistry.setreg",
                "--regremove=/O3DE/Gems/Box3D",
            });
        AZ::SettingsRegistryMergeUtils::StoreCommandLineToRegistry(settingsRegistry, commandLine);

        AssetUtilities::QuitListener quitListener;
        TestBuilder builder(quitListener, AZ::Uuid::CreateRandom(), 1);
        const AZStd::vector<AZStd::string> params = builder.BuildParamsForTesting();

        AZ::SettingsRegistry::Unregister(&settingsRegistry);
        if (originalSettingsRegistry)
        {
            AZ::SettingsRegistry::Register(originalSettingsRegistry);
        }

        EXPECT_NE(AZStd::find(params.begin(), params.end(), R"(--regset="/Jolt/Setting=enabled")"), params.end());
        EXPECT_NE(
            AZStd::find(params.begin(), params.end(), R"(--regset-file="JoltQualificationRegistry.setreg")"),
            params.end());
        EXPECT_NE(AZStd::find(params.begin(), params.end(), R"(--regremove="/O3DE/Gems/Box3D")"), params.end());
    }

    AZ::Outcome<void, AZStd::string> TestBuilder::Start(AssetProcessor::BuilderPurpose /*purpose*/)
    {
        return AZ::Success();
    }

    TestBuilderManager::TestBuilderManager(ConnectionManager* connectionManager): BuilderManager(connectionManager)
    {
        TestBuilderManager::AddNewBuilder(AssetProcessor::BuilderPurpose::CreateJobs);
    }

    int TestBuilderManager::GetBuilderCreationCount() const
    {
        return m_connectionCounter;
    }

    AZStd::shared_ptr<AssetProcessor::Builder> TestBuilderManager::AddNewBuilder(AssetProcessor::BuilderPurpose purpose)
    {
        auto uuid = AZ::Uuid::CreateRandom();
        auto builder = AZStd::make_shared<TestBuilder>(m_quitListener, uuid, ++m_connectionCounter);

        m_builderList.AddBuilder(builder, purpose);

        return builder;
    }
}
