"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import pytest

from ly_test_tools.o3de.editor_test import EditorSingleTest, EditorTestSuite


@pytest.mark.SUITE_main
@pytest.mark.parametrize("launcher_platform", ["windows_editor"])
@pytest.mark.parametrize("project", ["AutomatedTesting"])
class TestAutomation(EditorTestSuite):
    global_extra_cmdline_args = ["-BatchMode", "-autotest_mode", "-rhi=Null", "-NullRenderer"]

    class test_Jolt_AdvancedComponents(EditorSingleTest):
        from .tests import Jolt_AdvancedComponents as test_module

    class test_Jolt_ComponentSmoke(EditorSingleTest):
        from .tests import Jolt_ComponentSmoke as test_module

    class test_Jolt_FeatureComponents(EditorSingleTest):
        from .tests import Jolt_FeatureComponents as test_module

    class test_Jolt_SavedComponents(EditorSingleTest):
        from .tests import Jolt_SavedComponents as test_module

    class test_Jolt_WorldQueriesAndSnapshots(EditorSingleTest):
        from .tests import Jolt_WorldQueriesAndSnapshots as test_module
