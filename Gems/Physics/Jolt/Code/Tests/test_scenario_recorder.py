"""Tests for retained AutomatedTesting scenario evidence."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[5]
MODULE_PATH = (
    ENGINE_ROOT
    / "AutomatedTesting"
    / "Gem"
    / "PythonTests"
    / "Physics"
    / "Jolt"
    / "tests"
    / "Jolt_ScenarioRecorder.py"
)
SPECIFICATION = importlib.util.spec_from_file_location("jolt_scenario_recorder", MODULE_PATH)
assert SPECIFICATION and SPECIFICATION.loader
jolt_scenario_recorder = importlib.util.module_from_spec(SPECIFICATION)
sys.modules[SPECIFICATION.name] = jolt_scenario_recorder
SPECIFICATION.loader.exec_module(jolt_scenario_recorder)


class ScenarioRecorderTests(unittest.TestCase):
    def test_zero_checks_cannot_pass(self) -> None:
        recorder = jolt_scenario_recorder.ScenarioRecorder("Jolt_ComponentSmoke")

        self.assertFalse(recorder.passed)
        payload = recorder._build_payload()
        self.assertFalse(payload["passed"])
        self.assertIn("expected at least", payload["contractErrors"][0])

    def test_duplicate_check_names_cannot_pass(self) -> None:
        recorder = jolt_scenario_recorder.ScenarioRecorder("Jolt_CpuHair")
        for _ in range(5):
            recorder.check("duplicate", True)

        self.assertFalse(recorder.passed)
        self.assertIn("duplicate check names", recorder._build_payload()["contractErrors"][0])

    def test_benchmark_scenarios_have_an_independent_evidence_policy(self) -> None:
        recorder = jolt_scenario_recorder.ScenarioRecorder("Jolt_PerformanceCapture")
        for check_index in range(10):
            recorder.check(f"benchmark check {check_index}", True)

        self.assertTrue(recorder.passed)

    def test_chunked_result_reassembles_to_the_verified_payload(self) -> None:
        recorder = jolt_scenario_recorder.ScenarioRecorder("Jolt_CpuHair")
        for check_index in range(5):
            recorder.check(f"check {check_index}", True, "x" * 400)

        lines = recorder._encode_result_lines()
        envelope = json.loads(lines[0][len("JOLT_SCENARIO_JSON_BEGIN(") : -1])
        chunks = [
            json.loads(line[len("JOLT_SCENARIO_JSON_CHUNK(") : -1])
            for line in lines[1:-1]
        ]
        encoded_payload = "".join(chunk["data"] for chunk in sorted(chunks, key=lambda value: value["index"]))
        encoded_bytes = encoded_payload.encode("utf-8")

        self.assertTrue(json.loads(encoded_payload)["passed"])
        self.assertEqual(envelope["byteCount"], len(encoded_bytes))
        self.assertEqual(envelope["chunkCount"], len(chunks))
        self.assertEqual(envelope["sha256"], hashlib.sha256(encoded_bytes).hexdigest())


if __name__ == "__main__":
    unittest.main()
