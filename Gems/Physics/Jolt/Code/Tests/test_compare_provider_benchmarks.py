#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import sys
import unittest
from unittest.mock import patch

import compare_provider_benchmarks


def make_report(provider: str, time_microseconds: float) -> dict:
    benchmarks = []
    runtime_dependencies = []
    if provider != "PhysX":
        runtime_dependencies.append(
            {
                "mtime_ns": 1,
                "path": f"{provider}.API.dll",
                "sha256": f"{provider.lower()}-runtime-hash",
            }
        )
    workloads = (
        compare_provider_benchmarks.WORKLOADS
        + compare_provider_benchmarks.TAIL_WORKLOADS
    )
    for workload in workloads:
        for repetition_index in range(30):
            result = {
                "name": f"{provider}/{workload['suffix']}",
                "run_type": "iteration",
                "repetition_index": repetition_index,
                "iterations": workload.get("iterations", 100),
                "real_time": time_microseconds,
                "time_unit": "us",
                "AffinityConstrained": 1,
            }
            result.update(workload.get("exact", {}))
            result.update(workload.get("provider_exact", {}).get(provider, {}))
            result.update(workload.get("minimum", {}))
            result.update(workload.get("maximum", {}))
            worker_count = round(float(result.get("Workers", 0)))
            result["AffinityProcessors"] = worker_count
            result["BackgroundWorkers"] = max(0, worker_count - 1)
            result["CallerParticipates"] = 1
            if completion_counter := workload.get("completion_counter"):
                result[completion_counter] = (
                    result["iterations"] * workload["operations_per_iteration"]
                )
            if "TailSampleCount" in result:
                sample_nanoseconds = time_microseconds * 1_000.0
                for sample_index in range(compare_provider_benchmarks.TAIL_SAMPLE_COUNT):
                    result[f"Frame{sample_index}Ns"] = sample_nanoseconds
                result["TailMaximumNs"] = sample_nanoseconds
                result["TailP50Ns"] = sample_nanoseconds
                result["TailP95Ns"] = sample_nanoseconds
                result["TailP99Ns"] = sample_nanoseconds
            benchmarks.append(result)
    return {
        "context": {
            "host_name": "benchmark-host",
            "num_cpus": 16,
            "mhz_per_cpu": 4_500,
            "caches": [{"level": 3, "size": 32 * 1024 * 1024}],
            "library_build_type": "release",
        },
        "qualification": {
            "benchmark_filter": f"{provider}/matched",
            "binary_sha256": f"{provider.lower()}-binary-hash",
            "build_configuration": "Release",
            "compiler_id": "Clang",
            "compiler_version": "22.1.8",
            "cpu_affinity_policy": "physical-core-group-0",
            "evidence_kind": "qualified",
            "minimum_time": 0.05,
            "minimum_time_policy": {
                "default_seconds": 0.05,
                "overrides_seconds": {
                    "Query/RaycastClosestBatchGrid/1024/1024/4/real_time": 5.0,
                },
            },
            "provider": provider,
            "raw_samples": True,
            "repetitions": 30,
            "runtime_dependencies": runtime_dependencies,
            "runner_sha256": "runner-hash",
            "source_revision": "source-revision",
            "source_state_sha256": "source-state-hash",
            "workload_signature": compare_provider_benchmarks.workload_signature(),
        },
        "benchmarks": benchmarks,
    }


def run_comparison(reports: dict[str, dict]) -> int:
    arguments = [
        "compare_provider_benchmarks.py",
        "Jolt.json",
        "Box3D.json",
        "PhysX.json",
    ]
    report_sequence = [reports["Jolt"], reports["Box3D"], reports["PhysX"]]
    with patch.object(sys, "argv", arguments):
        with patch.object(compare_provider_benchmarks, "load_report", side_effect=report_sequence):
            return compare_provider_benchmarks.main()


def set_tail_window_time(
    reports: dict[str, dict],
    provider: str,
    repetition_index: int,
    time_microseconds: float,
) -> None:
    tail_name = f"{provider}/{compare_provider_benchmarks.TAIL_WORKLOADS[0]['suffix']}"
    result = next(
        result
        for result in reports[provider]["benchmarks"]
        if result["name"] == tail_name
        and result["repetition_index"] == repetition_index
    )
    sample_nanoseconds = time_microseconds * 1_000.0
    result["real_time"] = time_microseconds
    result["TailMaximumNs"] = sample_nanoseconds
    result["TailP50Ns"] = sample_nanoseconds
    result["TailP95Ns"] = sample_nanoseconds
    result["TailP99Ns"] = sample_nanoseconds
    for sample_index in range(compare_provider_benchmarks.TAIL_SAMPLE_COUNT):
        result[f"Frame{sample_index}Ns"] = sample_nanoseconds


def set_workload_time(
    reports: dict[str, dict],
    provider: str,
    repetition_index: int,
    time_microseconds: float,
) -> None:
    workload_name = f"{provider}/{compare_provider_benchmarks.WORKLOADS[0]['suffix']}"
    result = next(
        result
        for result in reports[provider]["benchmarks"]
        if result["name"] == workload_name
        and result["repetition_index"] == repetition_index
    )
    result["real_time"] = time_microseconds


class CompareProviderBenchmarksTests(unittest.TestCase):
    def test_normalizes_equivalent_numeric_compiler_versions(self):
        self.assertEqual(
            compare_provider_benchmarks.normalize_compiler_version("19.51.36256.0"),
            "19.51.36256",
        )
        self.assertEqual(
            compare_provider_benchmarks.normalize_compiler_version("22.1.8"),
            "22.1.8",
        )
        self.assertEqual(
            compare_provider_benchmarks.normalize_compiler_version("22.1.8-rc1"),
            "22.1.8-rc1",
        )

    def test_accepts_valid_faster_jolt_report(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        self.assertEqual(run_comparison(reports), 0)

    def test_rejects_non_release_report(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["context"]["library_build_type"] = "profile"
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_invalid_quality_counter(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["benchmarks"][0]["QualityValid"] = 0
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_unconstrained_cpu_topology(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["benchmarks"][0]["AffinityConstrained"] = 0
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_wrong_affinity_processor_count(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["PhysX"]["benchmarks"][0]["AffinityProcessors"] = 4
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_wrong_execution_topology(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Box3D"]["benchmarks"][0]["BackgroundWorkers"] = 1
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_incomplete_timed_operations(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        ray_workload_index = next(
            index
            for index, workload in enumerate(compare_provider_benchmarks.WORKLOADS)
            if workload["label"] == "Raycast 1024/128"
        )
        result_index = ray_workload_index * 30
        reports["Jolt"]["benchmarks"][result_index]["SuccessfulQueries"] -= 1
        self.assertEqual(run_comparison(reports), 1)

    def test_accepts_documented_correctness_differentiated_ratio(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 90.0),
            "PhysX": make_report("PhysX", 90.0),
        }
        raycast_suffix = next(
            workload["suffix"]
            for workload in compare_provider_benchmarks.WORKLOADS
            if workload["label"] == "Raycast 1024/128"
        )
        raycast_name = f"Jolt/{raycast_suffix}"
        for result in reports["Jolt"]["benchmarks"]:
            if result["name"] == raycast_name:
                result["real_time"] = 110.0

        self.assertEqual(run_comparison(reports), 0)

    def test_rejects_jolt_performance_regression(self):
        reports = {
            "Jolt": make_report("Jolt", 110.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        self.assertEqual(run_comparison(reports), 1)

    def test_accepts_variable_reference_with_conservative_repetition_margin(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        for repetition_index in range(10, 30):
            set_workload_time(reports, "PhysX", repetition_index, 150.0)

        self.assertEqual(run_comparison(reports), 0)

    def test_rejects_variable_reference_without_conservative_repetition_margin(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 150.0),
        }
        for repetition_index in range(2):
            set_workload_time(reports, "PhysX", repetition_index, 70.0)

        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_raw_frame_tail_regression(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        tail_name = (
            f"Jolt/{compare_provider_benchmarks.TAIL_WORKLOADS[0]['suffix']}"
        )
        tail_results = [
            result
            for result in reports["Jolt"]["benchmarks"]
            if result["name"] == tail_name
        ]
        for tail_result in tail_results[-2:]:
            tail_result["real_time"] = 200.0
            tail_result["TailMaximumNs"] = 200_000.0
            tail_result["TailP50Ns"] = 200_000.0
            tail_result["TailP95Ns"] = 200_000.0
            tail_result["TailP99Ns"] = 200_000.0
            for sample_index in range(compare_provider_benchmarks.TAIL_SAMPLE_COUNT):
                tail_result[f"Frame{sample_index}Ns"] = 200_000.0
        self.assertEqual(run_comparison(reports), 1)

    def test_accepts_variable_reference_with_conservative_tail_margin(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        for repetition_index in range(10, 30):
            set_tail_window_time(reports, "PhysX", repetition_index, 150.0)

        self.assertEqual(run_comparison(reports), 0)

    def test_rejects_variable_reference_without_conservative_tail_margin(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 150.0),
        }
        for repetition_index in range(2):
            set_tail_window_time(reports, "PhysX", repetition_index, 70.0)

        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_missing_raw_frame_tail_sample(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        tail_name = f"Jolt/{compare_provider_benchmarks.TAIL_WORKLOADS[0]['suffix']}"
        tail_result = next(
            result
            for result in reports["Jolt"]["benchmarks"]
            if result["name"] == tail_name
        )
        del tail_result[f"Frame{compare_provider_benchmarks.TAIL_SAMPLE_COUNT - 1}Ns"]
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_duplicate_repetition(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["benchmarks"][1]["repetition_index"] = 0
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_invalid_sample(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["benchmarks"][0]["real_time"] = float("nan")
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_stale_workload_signature(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["qualification"]["workload_signature"] = "stale"
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_mismatched_minimum_time_policy(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Box3D"]["qualification"]["minimum_time_policy"]["overrides_seconds"] = {}
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_mismatched_source_revision(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["PhysX"]["qualification"]["source_revision"] = "other-revision"
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_mixed_qualified_and_diagnostic_evidence(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Box3D"]["qualification"]["evidence_kind"] = "diagnostic"

        with self.assertRaisesRegex(ValueError, "differs for evidence_kind"):
            compare_provider_benchmarks.validate_context(reports)

    def test_accepts_equivalent_numeric_compiler_versions(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Box3D"]["qualification"]["compiler_version"] = "22.1.8.0"

        compare_provider_benchmarks.validate_context(reports)

    def test_rejects_different_compiler_versions(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Box3D"]["qualification"]["compiler_version"] = "22.1.9"

        with self.assertRaisesRegex(ValueError, "differs for compiler_version"):
            compare_provider_benchmarks.validate_context(reports)

    def test_rejects_mismatched_runner(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Box3D"]["qualification"]["runner_sha256"] = "different-runner"
        self.assertEqual(run_comparison(reports), 1)

    def test_rejects_missing_runtime_dependency_fingerprint(self):
        reports = {
            "Jolt": make_report("Jolt", 90.0),
            "Box3D": make_report("Box3D", 95.0),
            "PhysX": make_report("PhysX", 100.0),
        }
        reports["Jolt"]["qualification"]["runtime_dependencies"] = []
        self.assertEqual(run_comparison(reports), 1)


if __name__ == "__main__":
    unittest.main()
