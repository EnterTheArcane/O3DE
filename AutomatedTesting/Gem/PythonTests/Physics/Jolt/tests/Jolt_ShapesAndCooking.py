"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Runs focused Jolt shape and cooking integration checks.
"""

try:
    from .Jolt_FocusedScenarios import run_shapes_and_cooking
except ImportError:
    from Jolt_FocusedScenarios import run_shapes_and_cooking


def Jolt_ShapesAndCooking():
    run_shapes_and_cooking()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_ShapesAndCooking)
