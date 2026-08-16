"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Runs focused Jolt event and filtering integration checks.
"""

try:
    from .Jolt_FocusedScenarios import run_events_and_filters
except ImportError:
    from Jolt_FocusedScenarios import run_events_and_filters


def Jolt_EventsAndFilters():
    run_events_and_filters()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_EventsAndFilters)
