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
    def test_full_capture_uses_stable_measurement_windows(self) -> None:
        self.assertEqual(run_benchmark_qualification.FULL_REPETITION_COUNT, 30)
        self.assertGreaterEqual(run_benchmark_qualification.FULL_MINIMUM_TIME_SECONDS, 2.0)
        self.assertGreaterEqual(compare_provider_benchmarks.TAIL_SAMPLE_COUNT, 8192)

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

    def test_processor_selection_uses_nested_compact_physical_cores(self) -> None:
        processors = tuple(range(16))

        self.assertEqual(run_benchmark_qualification.select_compact_processors(processors, 1), (15,))
        self.assertEqual(run_benchmark_qualification.select_compact_processors(processors, 4), (12, 13, 14, 15))
        self.assertEqual(
            run_benchmark_qualification.select_compact_processors(processors, 8),
            (8, 9, 10, 11, 12, 13, 14, 15),
        )

    def test_processor_selection_rejects_clamped_worker_evidence(self) -> None:
        with self.assertRaisesRegex(ValueError, "Only 4 physical processors"):
            run_benchmark_qualification.select_compact_processors((0, 2, 4, 6), 8)

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

    def test_absolute_capture_warms_and_isolates_each_workload(self) -> None:
        provider = run_benchmark_qualification.Provider("Jolt", "Jolt.Tests.Gem.dll")
        runner = Path("AzTestRunner.exe")
        binary_directory = Path("bin")
        output_directory = Path("results")
        affinity_policy = {1: (15,)}

        with mock.patch.object(
            run_benchmark_qualification,
            "resolve_provider_files",
            return_value=(binary_directory / provider.module_name, None),
        ), mock.patch.object(
            run_benchmark_qualification,
            "run_benchmark_process",
        ) as run_process:
            raw_reports, warmup_reports = run_benchmark_qualification.capture_absolute_jolt(
                provider,
                2,
                0.05,
                runner,
                binary_directory,
                output_directory,
                affinity_policy,
                30,
            )

        workload_count = len(run_benchmark_qualification.ABSOLUTE_BENCHMARK_SUFFIXES)
        self.assertEqual(len(raw_reports), workload_count * 2)
        self.assertEqual(len(warmup_reports), workload_count)
        self.assertEqual(run_process.call_count, workload_count * 3)
        for benchmark_suffix in run_benchmark_qualification.ABSOLUTE_BENCHMARK_SUFFIXES:
            matching_calls = [
                call
                for call in run_process.call_args_list
                if call.args[3] == benchmark_suffix
            ]
            self.assertEqual(len(matching_calls), 3)

    def test_absolute_capture_retries_an_entire_invalid_batch(self) -> None:
        provider = run_benchmark_qualification.Provider("Jolt", "Jolt.Tests.Gem.dll")
        validation_results = iter(("CV 10.000% exceeds 5.000%", ""))

        def validate_first_batch_only(*_args, **_kwargs):
            return next(validation_results, "")

        with mock.patch.object(
            run_benchmark_qualification,
            "resolve_provider_files",
            return_value=(Path("bin") / provider.module_name, None),
        ), mock.patch.object(
            run_benchmark_qualification,
            "run_benchmark_process",
        ) as run_process, mock.patch.object(
            run_benchmark_qualification,
            "validate_absolute_capture_batch",
            side_effect=validate_first_batch_only,
        ), mock.patch.object(
            run_benchmark_qualification,
            "archive_capture_batch",
        ) as archive_batch:
            raw_reports, warmup_reports = run_benchmark_qualification.capture_absolute_jolt(
                provider,
                2,
                0.05,
                Path("AzTestRunner.exe"),
                Path("bin"),
                Path("results"),
                {1: (15,)},
                30,
                0.05,
                2,
            )

        workload_count = len(run_benchmark_qualification.ABSOLUTE_BENCHMARK_SUFFIXES)
        self.assertEqual(len(raw_reports), workload_count * 2)
        self.assertEqual(len(warmup_reports), workload_count)
        self.assertEqual(run_process.call_count, workload_count * 3 + 3)
        archive_batch.assert_called_once()

    def test_absolute_capture_batch_rejects_frequency_and_timing_outliers(self) -> None:
        def write_report(path: Path, real_time: float, frequency: float) -> None:
            path.write_text(
                json.dumps(
                    {
                        "context": {
                            "date": "2026-08-26T00:00:00",
                            "library_build_type": "release",
                            "mhz_per_cpu": frequency,
                        },
                        "benchmarks": [
                            {
                                "name": "Jolt/Absolute/real_time",
                                "real_time": real_time,
                                "run_type": "iteration",
                                "time_unit": "us",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            warmup_report = root / "warmup.json"
            raw_reports = [root / f"sample-{index}.json" for index in range(3)]
            write_report(warmup_report, 10.0, 4_500.0)
            for raw_report in raw_reports:
                write_report(raw_report, 10.0, 4_500.0)

            self.assertEqual(
                run_benchmark_qualification.validate_absolute_capture_batch(
                    raw_reports,
                    warmup_report,
                    0.05,
                ),
                "",
            )

            write_report(raw_reports[0], 20.0, 4_500.0)
            self.assertIn(
                "CV",
                run_benchmark_qualification.validate_absolute_capture_batch(
                    raw_reports,
                    warmup_report,
                    0.05,
                ),
            )

            write_report(raw_reports[0], 10.0, 4_600.0)
            self.assertIn(
                "incompatible CPU frequency",
                run_benchmark_qualification.validate_absolute_capture_batch(
                    raw_reports,
                    warmup_report,
                    0.05,
                ),
            )


if __name__ == "__main__":
    unittest.main()
