"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Collects independent Jolt scenario checks and emits one machine-readable result.
"""

import hashlib
import json
import traceback


class ScenarioRecorder:
    schema_version = 1
    _chunk_size = 1000
    _maximum_observed_value_length = 512

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
                "details": self._summarize_value(value),
                "exception": error,
            }
        )
        return value

    @classmethod
    def _summarize_value(cls, value):
        summary = repr(value)
        if len(summary) <= cls._maximum_observed_value_length:
            return summary

        omitted_character_count = len(summary) - cls._maximum_observed_value_length
        return (
            summary[: cls._maximum_observed_value_length]
            + f"... <{omitted_character_count} characters omitted>"
        )

    def _encode_result_lines(self):
        payload = {
            "schemaVersion": self.schema_version,
            "scenario": self._scenario_name,
            "passed": self.passed,
            "checkCount": len(self._checks),
            "failedCheckCount": sum(not check["passed"] for check in self._checks),
            "checks": self._checks,
        }
        encoded_payload = json.dumps(payload, separators=(",", ":"), sort_keys=True)
        chunks = [
            encoded_payload[offset : offset + self._chunk_size]
            for offset in range(0, len(encoded_payload), self._chunk_size)
        ]
        encoded_bytes = encoded_payload.encode("utf-8")
        envelope = {
            "schemaVersion": self.schema_version,
            "scenario": self._scenario_name,
            "byteCount": len(encoded_bytes),
            "chunkCount": len(chunks),
            "sha256": hashlib.sha256(encoded_bytes).hexdigest(),
        }
        lines = [
            "JOLT_SCENARIO_JSON_BEGIN("
            + json.dumps(envelope, separators=(",", ":"), sort_keys=True)
            + ")"
        ]
        for chunk_index, chunk in enumerate(chunks):
            chunk_payload = {
                "index": chunk_index,
                "data": chunk,
            }
            lines.append(
                "JOLT_SCENARIO_JSON_CHUNK("
                + json.dumps(chunk_payload, separators=(",", ":"), sort_keys=True)
                + ")"
            )
        lines.append(
            "JOLT_SCENARIO_JSON_END("
            + json.dumps(
                {
                    "scenario": self._scenario_name,
                    "sha256": envelope["sha256"],
                },
                separators=(",", ":"),
                sort_keys=True,
            )
            + ")"
        )
        return lines

    def finish(self):
        import azlmbr.legacy.general as general

        from editor_python_test_tools.utils import Report

        for output_line in self._encode_result_lines():
            general.test_output(output_line)

        Report.result(
            (
                f"{self._scenario_name} passed every independent check",
                f"{self._scenario_name} failed one or more independent checks",
            ),
            self.passed,
        )
