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

    class test_Jolt_Characters(EditorSingleTest):
        from .tests import Jolt_Characters as test_module

    class test_Jolt_Constraints(EditorSingleTest):
        from .tests import Jolt_Constraints as test_module

    class test_Jolt_Diagnostics(EditorSingleTest):
        timeout = 600
        from .tests import Jolt_Diagnostics as test_module

    class test_Jolt_EventsAndFilters(EditorSingleTest):
        from .tests import Jolt_EventsAndFilters as test_module

    class test_Jolt_Hair(EditorSingleTest):
        from .tests import Jolt_Hair as test_module

    class test_Jolt_Queries(EditorSingleTest):
        from .tests import Jolt_Queries as test_module

    class test_Jolt_RagdollsAndSkeletons(EditorSingleTest):
        from .tests import Jolt_RagdollsAndSkeletons as test_module

    class test_Jolt_RollbackAndDeterminism(EditorSingleTest):
        from .tests import Jolt_RollbackAndDeterminism as test_module

    class test_Jolt_SavedFeatureGallery(EditorSingleTest):
        from .tests import Jolt_SavedFeatureGallery as test_module

    class test_Jolt_ScenesAndAssets(EditorSingleTest):
        from .tests import Jolt_ScenesAndAssets as test_module

    class test_Jolt_ShapesAndCooking(EditorSingleTest):
        from .tests import Jolt_ShapesAndCooking as test_module

    class test_Jolt_SoftBodies(EditorSingleTest):
        from .tests import Jolt_SoftBodies as test_module

    class test_Jolt_StressAndSoak(EditorSingleTest):
        timeout = 3600
        from .tests import Jolt_StressAndSoak as test_module

    class test_Jolt_Vehicles(EditorSingleTest):
        from .tests import Jolt_Vehicles as test_module
