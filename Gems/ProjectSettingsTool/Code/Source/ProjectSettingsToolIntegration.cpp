/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>

#include "ProjectSettingsToolIntegration.h"
#include "ProjectSettingsToolWindow.h"

ProjectSettingsToolIntegration::ProjectSettingsToolIntegration()
{
    AzToolsFramework::ViewPaneOptions options;
    options.showInMenu = false;
    AzToolsFramework::RegisterViewPane<ProjectSettingsTool::ProjectSettingsToolWindow>(
        ProjectSettingsTool::ViewPaneName, ProjectSettingsTool::ViewPaneName, options);
}

ProjectSettingsToolIntegration::~ProjectSettingsToolIntegration()
{
    AzToolsFramework::UnregisterViewPane(ProjectSettingsTool::ViewPaneName);
}
