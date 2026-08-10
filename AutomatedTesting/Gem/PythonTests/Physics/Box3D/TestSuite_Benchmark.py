"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import pytest

from ly_test_tools.o3de.editor_test import EditorSingleTest, EditorTestSuite


@pytest.mark.SUITE_benchmark
@pytest.mark.parametrize("launcher_platform", ["windows_editor"])
@pytest.mark.parametrize("project", ["AutomatedTesting"])
class TestAutomation(EditorTestSuite):
    use_null_renderer = False
    global_extra_cmdline_args = ["-BatchMode", "-autotest_mode", "-rhi=dx12"]

    class test_Box3D_PerformanceCapture(EditorSingleTest):
        timeout = 600
        from .tests import Box3D_PerformanceCapture as test_module
