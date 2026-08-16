"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Runs focused Jolt constraint and path integration checks.
"""

try:
    from .Jolt_FocusedScenarios import run_constraints
except ImportError:
    from Jolt_FocusedScenarios import run_constraints


def Jolt_Constraints():
    run_constraints()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_Constraints)
