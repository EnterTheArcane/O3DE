#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import compare_provider_benchmarks
import run_benchmark_qualification


class RunBenchmarkQualificationTests(unittest.TestCase):
    def test_full_workloads_follow_the_comparison_contract(self) -> None:
        workloads = run_benchmark_qualification.matched_workloads()
        expected_workloads = (
            *compare_provider_benchmarks.WORKLOADS,
            *compare_provider_benchmarks.TAIL_WORKLOADS,
        )

        self.assertEqual(len(workloads), len(expected_workloads))
        self.assertEqual(
            [workload.suffix for workload in workloads],
            [workload["suffix"] for workload in expected_workloads],
        )
        self.assertEqual(
            [workload.worker_count for workload in workloads],
            [workload["exact"]["Workers"] for workload in expected_workloads],
        )

    def test_review_workloads_cover_step_query_and_tail(self) -> None:
        workloads = run_benchmark_qualification.review_workloads()

        self.assertEqual(len(workloads), 3)
        self.assertTrue(workloads[0].suffix.startswith("Step/"))
        self.assertTrue(workloads[1].suffix.startswith("Query/"))
        self.assertTrue(workloads[2].suffix.startswith("Tail/"))

    def test_processor_selection_spreads_across_physical_cores(self) -> None:
        processors = tuple(range(16))

        self.assertEqual(run_benchmark_qualification.select_spread_processors(processors, 1), (0,))
        self.assertEqual(run_benchmark_qualification.select_spread_processors(processors, 4), (0, 4, 8, 12))
        self.assertEqual(
            run_benchmark_qualification.select_spread_processors(processors, 8),
            (0, 2, 4, 6, 8, 10, 12, 14),
        )

    def test_processor_selection_rejects_clamped_worker_evidence(self) -> None:
        with self.assertRaisesRegex(ValueError, "Only 4 physical processors"):
            run_benchmark_qualification.select_spread_processors((0, 2, 4, 6), 8)

    def test_smoke_report_requires_every_process_repetition(self) -> None:
        report = {
            "benchmarks": [
                {
                    "name": "Jolt/Smoke/real_time",
                    "run_type": "iteration",
                    "repetition_index": repetition,
                    "real_time": 10.0 + repetition,
                    "QualityValid": 1,
                }
                for repetition in range(3)
            ]
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            report_path = Path(temporary_directory) / "review.json"
            report_path.write_text(json.dumps(report), encoding="utf-8")

            run_benchmark_qualification.validate_smoke_report(
                report_path,
                3,
                ("Jolt/Smoke/real_time",),
            )

            report["benchmarks"].pop()
            report_path.write_text(json.dumps(report), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "does not contain 3 process-isolated repetitions"):
                run_benchmark_qualification.validate_smoke_report(
                    report_path,
                    3,
                    ("Jolt/Smoke/real_time",),
                )

            with self.assertRaisesRegex(ValueError, "missing names"):
                run_benchmark_qualification.validate_smoke_report(
                    report_path,
                    2,
                    ("Jolt/Smoke/real_time", "Jolt/Missing/real_time"),
                )

    def test_benchmark_process_rejects_an_empty_successful_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            runner = root / "AzTestRunner.exe"
            module = root / "Jolt.Tests.Gem.dll"
            output_path = root / "empty.json"

            def write_empty_report(*_args, **_kwargs):
                output_path.write_text(json.dumps({"benchmarks": []}), encoding="utf-8")
                return mock.Mock(returncode=0)

            with mock.patch.object(
                run_benchmark_qualification,
                "constrained_process",
                return_value=mock.MagicMock(),
            ), mock.patch.object(
                run_benchmark_qualification.subprocess,
                "run",
                side_effect=write_empty_report,
            ):
                with self.assertRaisesRegex(RuntimeError, "matched no workloads"):
                    run_benchmark_qualification.run_benchmark_process(
                        runner,
                        module,
                        "Jolt",
                        "Missing/real_time",
                        0.05,
                        output_path,
                        (0,),
                        30,
                    )


if __name__ == "__main__":
    unittest.main()
