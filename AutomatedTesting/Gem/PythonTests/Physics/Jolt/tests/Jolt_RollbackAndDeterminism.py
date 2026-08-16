"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Runs focused Jolt rollback and deterministic replay checks.
"""

try:
    from .Jolt_FocusedScenarios import run_rollback_and_determinism
except ImportError:
    from Jolt_FocusedScenarios import run_rollback_and_determinism


def Jolt_RollbackAndDeterminism():
    run_rollback_and_determinism()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_RollbackAndDeterminism)
