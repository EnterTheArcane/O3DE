#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import hashlib
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
        self.assertEqual(
            [workload.minimum_time_seconds for workload in workloads],
            [float(workload.get("minimum_time_seconds", 0.0)) for workload in expected_workloads],
        )

    def test_batch_raycast_uses_the_measured_stable_window_for_every_provider(self) -> None:
        workload = next(
            workload
            for workload in run_benchmark_qualification.matched_workloads()
            if workload.suffix == "Query/RaycastClosestBatchGrid/1024/1024/4/real_time"
        )
        with mock.patch.object(
            run_benchmark_qualification,
            "resolve_provider_files",
            side_effect=lambda _binary_directory, provider: (
                Path("bin") / provider.module_name,
                None,
            ),
        ), mock.patch.object(
            run_benchmark_qualification,
            "run_benchmark_process",
        ) as run_process:
            for provider in run_benchmark_qualification.PROVIDERS:
                run_benchmark_qualification.capture_matched_provider(
                    provider,
                    (workload,),
                    2,
                    2.0,
                    Path("AzTestRunner.exe"),
                    Path("bin"),
                    Path("results"),
                    {workload.worker_count: (12, 13, 14, 15)},
                    30,
                )

        self.assertEqual(run_process.call_count, 9)
        self.assertTrue(all(call.args[4] == 5.0 for call in run_process.call_args_list))

    def test_full_empty_world_diagnostic_uses_the_measured_stable_window(self) -> None:
        workloads = run_benchmark_qualification.absolute_workloads(True)
        empty_world = next(
            workload
            for workload in workloads
            if workload.suffix == "Diagnostic/RaycastEmptyWorld/128/real_time"
        )
        self.assertEqual(
            empty_world.minimum_time_seconds,
            run_benchmark_qualification.FULL_EMPTY_WORLD_MINIMUM_TIME_SECONDS,
        )
        review_workloads = run_benchmark_qualification.absolute_workloads(False)
        self.assertTrue(all(workload.minimum_time_seconds == 0.0 for workload in review_workloads))
        self.assertEqual(
            run_benchmark_qualification.build_minimum_time_policy(workloads, 2.0),
            {
                "default_seconds": 2.0,
                "overrides_seconds": {
                    empty_world.suffix: run_benchmark_qualification.FULL_EMPTY_WORLD_MINIMUM_TIME_SECONDS,
                },
            },
        )

        with mock.patch.object(
            run_benchmark_qualification,
            "resolve_provider_files",
            return_value=(Path("bin/Jolt.Tests.Gem.dll"), None),
        ), mock.patch.object(
            run_benchmark_qualification,
            "run_benchmark_process",
        ) as run_process:
            run_benchmark_qualification.capture_absolute_jolt(
                run_benchmark_qualification.PROVIDERS[0],
                (empty_world,),
                2,
                2.0,
                Path("AzTestRunner.exe"),
                Path("bin"),
                Path("results"),
                {1: (15,)},
                30,
            )

        self.assertEqual(run_process.call_count, 3)
        self.assertTrue(
            all(
                call.args[4] == run_benchmark_qualification.FULL_EMPTY_WORLD_MINIMUM_TIME_SECONDS
                for call in run_process.call_args_list
            )
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

    def test_reusable_report_requires_current_source_and_binary_fingerprints(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            runner = root / "AzTestRunner"
            module = root / "libJolt.Tests.Gem.so"
            runtime_dependency = root / "libJolt.API.so"
            report_path = root / "Jolt-matched-qualified.json"
            runner.write_bytes(b"runner")
            module.write_bytes(b"module")
            runtime_dependency.write_bytes(b"runtime")
            source_state = b"source-state"
            benchmark_name = "Jolt/Example/real_time"
            report_path.write_text(
                json.dumps(
                    {
                        "benchmarks": [
                            {
                                "name": benchmark_name,
                                "repetition_index": 0,
                                "real_time": 1.0,
                                "run_type": "iteration",
                                "QualityValid": 1,
                            }
                        ],
                        "qualification": {
                            "benchmark_filter": "Jolt/Example",
                            "binary_sha256": run_benchmark_qualification.prepare_benchmark_artifact.sha256_file(
                                module
                            ),
                            "build_configuration": "Release",
                            "compiler_id": "Clang",
                            "compiler_version": "20.1.8.0",
                            "cpu_affinity_policy": "test-affinity",
                            "minimum_time": 2.0,
                            "minimum_time_policy": {
                                "default_seconds": 2.0,
                                "overrides_seconds": {},
                            },
                            "provider": "Jolt",
                            "raw_report_sha256": ["raw"],
                            "raw_samples": True,
                            "reindexed_repetitions": True,
                            "repetitions": 1,
                            "runner_sha256": run_benchmark_qualification.prepare_benchmark_artifact.sha256_file(
                                runner
                            ),
                            "runtime_dependencies": [
                                {
                                    "path": str(runtime_dependency.resolve()),
                                    "sha256": run_benchmark_qualification.prepare_benchmark_artifact.sha256_file(
                                        runtime_dependency
                                    ),
                                }
                            ],
                            "source_revision": "revision",
                            "source_state_sha256": hashlib.sha256(source_state).hexdigest(),
                            "warmup_report_sha256": ["warmup"],
                            "workload_signature": compare_provider_benchmarks.workload_signature(),
                        },
                    }
                ),
                encoding="utf-8",
            )

            with mock.patch.object(
                run_benchmark_qualification.prepare_benchmark_artifact,
                "run_git",
                return_value=b"revision\n",
            ), mock.patch.object(
                run_benchmark_qualification.prepare_benchmark_artifact,
                "get_source_state",
                return_value=(source_state, 0),
            ) as source_state_mock:
                run_benchmark_qualification.validate_reusable_report(
                    report_path,
                    root,
                    runner,
                    run_benchmark_qualification.PROVIDERS[0],
                    module,
                    runtime_dependency,
                    "Clang",
                    "20.1.8",
                    "test-affinity",
                    "Jolt/Example",
                    2.0,
                    {"default_seconds": 2.0, "overrides_seconds": {}},
                    1,
                    (benchmark_name,),
                )

                module.write_bytes(b"different-module")
                with self.assertRaisesRegex(ValueError, "stale binary_sha256"):
                    run_benchmark_qualification.validate_reusable_report(
                        report_path,
                        root,
                        runner,
                        run_benchmark_qualification.PROVIDERS[0],
                        module,
                        runtime_dependency,
                        "Clang",
                        "20.1.8",
                        "test-affinity",
                        "Jolt/Example",
                        2.0,
                        {"default_seconds": 2.0, "overrides_seconds": {}},
                        1,
                        (benchmark_name,),
                    )

                module.write_bytes(b"module")
                source_state_mock.return_value = (b"different-source-state", 0)
                with self.assertRaisesRegex(ValueError, "stale source_state_sha256"):
                    run_benchmark_qualification.validate_reusable_report(
                        report_path,
                        root,
                        runner,
                        run_benchmark_qualification.PROVIDERS[0],
                        module,
                        runtime_dependency,
                        "Clang",
                        "20.1.8",
                        "test-affinity",
                        "Jolt/Example",
                        2.0,
                        {"default_seconds": 2.0, "overrides_seconds": {}},
                        1,
                        (benchmark_name,),
                    )

    def test_rejected_capture_directory_preserves_prior_attempts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            rejected_root = Path(temporary_directory)
            (rejected_root / "example-attempt-01").mkdir()
            (rejected_root / "example-attempt-02").mkdir()

            self.assertEqual(
                run_benchmark_qualification.next_rejected_capture_directory(
                    rejected_root,
                    "example",
                ),
                rejected_root / "example-attempt-03",
            )

    def test_rejected_checkpoint_preserves_report_response_and_reason(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            report_path = root / "Jolt-matched-qualified.json"
            response_path = report_path.with_suffix(".rsp")
            rejected_root = root / "rejected"
            report_path.write_text("report", encoding="utf-8")
            response_path.write_text("response", encoding="utf-8")

            run_benchmark_qualification.archive_rejected_checkpoint(
                report_path,
                rejected_root,
                "stale evidence",
            )

            rejected_directories = tuple(rejected_root.iterdir())
            self.assertEqual(len(rejected_directories), 1)
            rejected_directory = rejected_directories[0]
            self.assertEqual(
                (rejected_directory / report_path.name).read_text(encoding="utf-8"),
                "report",
            )
            self.assertEqual(
                (rejected_directory / response_path.name).read_text(encoding="utf-8"),
                "response",
            )
            self.assertEqual(
                (rejected_directory / "reason.txt").read_text(encoding="utf-8"),
                "stale evidence\n",
            )

    def test_matched_checkpoint_rejects_jolt_variability(self) -> None:
        workload = run_benchmark_qualification.review_workloads()[0]
        with mock.patch.object(
            run_benchmark_qualification.compare_provider_benchmarks,
            "load_report",
            return_value={},
        ), mock.patch.object(
            run_benchmark_qualification.compare_provider_benchmarks,
            "load_samples",
            return_value=([1.0, 1.0, 2.0], 1),
        ):
            with self.assertRaisesRegex(ValueError, "CV .* exceeds"):
                run_benchmark_qualification.validate_matched_report_quality(
                    Path("report.json"),
                    run_benchmark_qualification.PROVIDERS[0],
                    (workload,),
                    3,
                    0.05,
                )

    def test_absolute_checkpoint_surfaces_quality_failures(self) -> None:
        with mock.patch.object(
            run_benchmark_qualification.compare_provider_benchmarks,
            "load_report",
            return_value={},
        ), mock.patch.object(
            run_benchmark_qualification.validate_jolt_benchmarks,
            "validate_report",
            return_value=["invalid capability evidence", "invalid allocation evidence"],
        ):
            with self.assertRaisesRegex(
                ValueError,
                "invalid capability evidence invalid allocation evidence",
            ):
                run_benchmark_qualification.validate_absolute_report_quality(
                    Path("report.json"),
                    30,
                )

    def test_matched_capture_retries_an_entire_invalid_workload_batch(self) -> None:
        provider = run_benchmark_qualification.Provider("Jolt", "Jolt.Tests.Gem.dll")
        workload = run_benchmark_qualification.review_workloads()[0]
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
            raw_reports, warmup_reports = run_benchmark_qualification.capture_matched_provider(
                provider,
                (workload,),
                2,
                0.05,
                Path("AzTestRunner.exe"),
                Path("bin"),
                Path("results"),
                {workload.worker_count: (15,)},
                30,
                0.05,
                2,
            )

        self.assertEqual(len(raw_reports), 2)
        self.assertEqual(len(warmup_reports), 1)
        self.assertEqual(run_process.call_count, 6)
        archive_batch.assert_called_once()

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

    def test_benchmark_process_rejects_invalid_inherited_affinity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            runner = root / "AzTestRunner.exe"
            module = root / "Jolt.Tests.Gem.dll"
            output_path = root / "invalid-affinity.json"
            benchmark_result = {
                "name": "Jolt/Step/real_time",
                "AffinityConstrained": 0,
                "AffinityProcessors": 0,
            }

            def write_invalid_report(*_args, **_kwargs):
                output_path.write_text(
                    json.dumps({"benchmarks": [benchmark_result]}),
                    encoding="utf-8",
                )
                return mock.Mock(returncode=0)

            with mock.patch.object(
                run_benchmark_qualification,
                "constrained_process",
                return_value=mock.MagicMock(),
            ), mock.patch.object(
                run_benchmark_qualification.subprocess,
                "run",
                side_effect=write_invalid_report,
            ):
                with self.assertRaisesRegex(RuntimeError, "did not preserve its inherited CPU affinity"):
                    run_benchmark_qualification.run_benchmark_process(
                        runner,
                        module,
                        "Jolt",
                        "Step/real_time",
                        0.05,
                        output_path,
                        (0,),
                        30,
                    )

                benchmark_result["AffinityConstrained"] = 1
                benchmark_result["AffinityProcessors"] = 4
                with self.assertRaisesRegex(RuntimeError, "reported 4 affinity processors; expected 1"):
                    run_benchmark_qualification.run_benchmark_process(
                        runner,
                        module,
                        "Jolt",
                        "Step/real_time",
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
                run_benchmark_qualification.absolute_workloads(False),
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
                run_benchmark_qualification.absolute_workloads(False),
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
        def write_report(
            path: Path,
            real_time: float,
            frequency: float,
            load_average: float,
        ) -> None:
            path.write_text(
                json.dumps(
                    {
                        "context": {
                            "date": "2026-08-26T00:00:00",
                            "library_build_type": "release",
                            "load_avg": [load_average, load_average, load_average],
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
            write_report(warmup_report, 10.0, 4_500.0, 1.0)
            for index, raw_report in enumerate(raw_reports):
                write_report(raw_report, 10.0, 4_500.0, 2.0 + index)

            self.assertEqual(
                run_benchmark_qualification.validate_absolute_capture_batch(
                    raw_reports,
                    warmup_report,
                    0.05,
                ),
                "",
            )

            write_report(raw_reports[0], 20.0, 4_500.0, 2.0)
            self.assertIn(
                "CV",
                run_benchmark_qualification.validate_absolute_capture_batch(
                    raw_reports,
                    warmup_report,
                    0.05,
                ),
            )

            write_report(raw_reports[0], 10.0, 4_600.0, 2.0)
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
