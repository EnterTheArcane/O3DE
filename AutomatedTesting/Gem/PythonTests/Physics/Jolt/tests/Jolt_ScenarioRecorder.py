"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Collects independent Jolt scenario checks and emits one machine-readable result.
"""

import hashlib
import json
import os
import traceback
from collections import Counter
from functools import wraps
from pathlib import Path


SCENARIO_MINIMUM_CHECK_COUNTS = {
    "Jolt_AdvancedComponents": 13,
    "Jolt_Characters": 12,
    "Jolt_ComponentSmoke": 12,
    "Jolt_Constraints": 20,
    "Jolt_CpuHair": 5,
    "Jolt_Diagnostics": 10,
    "Jolt_EventsAndFilters": 12,
    "Jolt_FeatureComponents": 7,
    "Jolt_Hair": 6,
    "Jolt_Queries": 10,
    "Jolt_RagdollsAndSkeletons": 10,
    "Jolt_RollbackAndDeterminism": 10,
    "Jolt_SavedComponents": 14,
    "Jolt_SavedFeatureGallery": 189,
    "Jolt_ScenesAndAssets": 10,
    "Jolt_ShapesAndCooking": 41,
    "Jolt_SoftBodies": 8,
    "Jolt_StressAndSoak": 50,
    "Jolt_Vehicles": 15,
    "Jolt_WorldQueriesAndSnapshots": 16,
}

BENCHMARK_MINIMUM_CHECK_COUNTS = {
    "Jolt_PerformanceCapture": 10,
}


class ScenarioRecorder:
    schema_version = 1
    _chunk_size = 1000
    _maximum_observed_value_length = 512

    def __init__(self, scenario_name):
        self._scenario_name = scenario_name
        self._checks = []
        if scenario_name in SCENARIO_MINIMUM_CHECK_COUNTS:
            self._minimum_check_count = SCENARIO_MINIMUM_CHECK_COUNTS[scenario_name]
        else:
            self._minimum_check_count = BENCHMARK_MINIMUM_CHECK_COUNTS[scenario_name]

    @property
    def passed(self):
        check_names = [check["name"] for check in self._checks]
        return (
            len(self._checks) >= self._minimum_check_count
            and len(set(check_names)) == len(check_names)
            and all(check["passed"] for check in self._checks)
        )

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

    def result(self, messages, condition, details=""):
        if not details and len(messages) > 1:
            details = messages[1]
        return self.check(messages[0], condition, details)

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

    def _build_payload(self):
        check_names = [check["name"] for check in self._checks]
        contract_errors = []
        if len(self._checks) < self._minimum_check_count:
            contract_errors.append(
                f"expected at least {self._minimum_check_count} checks, observed {len(self._checks)}"
            )
        duplicate_names = sorted(name for name, count in Counter(check_names).items() if count > 1)
        if duplicate_names:
            contract_errors.append(f"duplicate check names: {', '.join(duplicate_names)}")

        return {
            "schemaVersion": self.schema_version,
            "scenario": self._scenario_name,
            "passed": self.passed,
            "minimumCheckCount": self._minimum_check_count,
            "checkCount": len(self._checks),
            "failedCheckCount": sum(not check["passed"] for check in self._checks),
            "contractErrors": contract_errors,
            "checks": self._checks,
        }

    @staticmethod
    def _encode_payload(payload):
        return json.dumps(payload, separators=(",", ":"), sort_keys=True)

    def _build_envelope(self, encoded_payload):
        encoded_bytes = encoded_payload.encode("utf-8")
        chunk_count = max(
            1,
            (len(encoded_payload) + self._chunk_size - 1) // self._chunk_size,
        )
        return {
            "schemaVersion": self.schema_version,
            "scenario": self._scenario_name,
            "byteCount": len(encoded_bytes),
            "chunkCount": chunk_count,
            "sha256": hashlib.sha256(encoded_bytes).hexdigest(),
        }

    def _encode_result_lines(self, payload=None):
        if payload is None:
            payload = self._build_payload()

        encoded_payload = self._encode_payload(payload)
        chunks = [
            encoded_payload[offset : offset + self._chunk_size]
            for offset in range(0, len(encoded_payload), self._chunk_size)
        ]
        envelope = self._build_envelope(encoded_payload)
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

    def _write_result(self, payload):
        result_directory = os.environ.get("JOLT_SCENARIO_RESULT_DIRECTORY")
        if not result_directory:
            return

        destination_directory = Path(result_directory)
        destination_directory.mkdir(parents=True, exist_ok=True)
        encoded_payload = self._encode_payload(payload)
        document = {
            "envelope": self._build_envelope(encoded_payload),
            "result": payload,
        }
        destination = destination_directory / f"{self._scenario_name}.json"
        temporary = destination.with_suffix(f".json.{os.getpid()}.tmp")
        temporary.write_text(
            json.dumps(document, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, destination)

    def finish(self):
        import azlmbr.legacy.general as general

        from editor_python_test_tools.utils import Report

        payload = self._build_payload()
        self._write_result(payload)
        for output_line in self._encode_result_lines(payload):
            general.test_output(output_line)

        Report.result(
            (
                f"{self._scenario_name} passed every independent check",
                f"{self._scenario_name} failed one or more independent checks",
            ),
            self.passed,
        )


def record_scenario(scenario_name):
    def decorate(operation):
        @wraps(operation)
        def run():
            recorder = ScenarioRecorder(scenario_name)
            try:
                operation(recorder)
            except Exception as exception:
                recorder.check("scenario completed without an unhandled exception", False, exception)
            finally:
                recorder.finish()

        return run

    return decorate
