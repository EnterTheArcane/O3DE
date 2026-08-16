"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Runs focused deterministic CPU Hair integration checks.
"""

try:
    from .Jolt_FocusedScenarios import run_hair
except ImportError:
    from Jolt_FocusedScenarios import run_hair


def Jolt_Hair():
    run_hair()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_Hair)
