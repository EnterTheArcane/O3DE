"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Collects independent Jolt scenario checks and emits one machine-readable result.
"""

import json
import traceback


class ScenarioRecorder:
    schema_version = 1

    def __init__(self, scenario_name):
        self._scenario_name = scenario_name
        self._checks = []

    @property
    def passed(self):
        return all(check["passed"] for check in self._checks)

    def check(self, name, condition, details=""):
        error = ""
        try:
            if callable(condition):
                condition = condition()
            passed = bool(condition)
        except Exception:
            passed = False
            error = traceback.format_exc()

        self._checks.append(
            {
                "name": name,
                "passed": passed,
                "details": str(details),
                "exception": error,
            }
        )
        return passed

    def capture(self, name, operation, predicate=bool, fallback=None):
        try:
            value = operation()
            passed = bool(predicate(value))
            error = ""
        except Exception:
            value = fallback
            passed = False
            error = traceback.format_exc()

        self._checks.append(
            {
                "name": name,
                "passed": passed,
                "details": "",
                "exception": error,
            }
        )
        return value

    def finish(self):
        import azlmbr.legacy.general as general

        from editor_python_test_tools.utils import Report

        payload = {
            "schemaVersion": self.schema_version,
            "scenario": self._scenario_name,
            "passed": self.passed,
            "checkCount": len(self._checks),
            "failedCheckCount": sum(not check["passed"] for check in self._checks),
            "checks": self._checks,
        }
        encoded_payload = json.dumps(payload, separators=(",", ":"), sort_keys=True)
        output = f"JOLT_SCENARIO_JSON({encoded_payload})"
        print(output)
        general.test_output(output)

        Report.result(
            (
                f"{self._scenario_name} passed every independent check",
                f"{self._scenario_name} failed one or more independent checks",
            ),
            self.passed,
        )
