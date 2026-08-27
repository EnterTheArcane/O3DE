#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import prepare_benchmark_artifact


class PrepareBenchmarkArtifactTests(unittest.TestCase):
    def create_repository(self, root: Path) -> tuple[Path, Path, Path, Path, Path]:
        subprocess.run(("git", "init", "--quiet"), cwd=root, check=True)
        subprocess.run(("git", "config", "user.email", "benchmark@example.com"), cwd=root, check=True)
        subprocess.run(("git", "config", "user.name", "Benchmark Test"), cwd=root, check=True)
        (root / ".gitignore").write_text(
            "\n".join(
                (
                    "/AzTestRunner.exe",
                    "/Jolt.API.dll",
                    "/Jolt.Tests.Gem.dll",
                    "/prepare-arguments.rsp",
                    "/qualified.json",
                    "/raw-additional.json",
                    "/raw.json",
                    "/warmup.json",
                    "",
                )
            ),
            encoding="utf-8",
        )
        source = root / "source.cpp"
        source.write_text("int benchmark_source = 1;\n", encoding="utf-8")
        subprocess.run(("git", "add", ".gitignore", "source.cpp"), cwd=root, check=True)
        subprocess.run(("git", "commit", "--quiet", "-m", "Create source"), cwd=root, check=True)

        binary = root / "Jolt.Tests.Gem.dll"
        runner = root / "AzTestRunner.exe"
        raw_report = root / "raw.json"
        output_report = root / "qualified.json"
        binary.write_bytes(b"jolt-binary")
        runner.write_bytes(b"benchmark-runner")
        raw_report.write_text(
            json.dumps(
                {
                    "context": {"library_build_type": "release", "mhz_per_cpu": 4_500},
                    "benchmarks": [{"name": "Jolt/Dummy"}],
                }
            ),
            encoding="utf-8",
        )
        return source, binary, runner, raw_report, output_report

    def run_prepare(
        self,
        root: Path,
        binary: Path,
        runner: Path,
        raw_report: Path,
        output_report: Path,
        additional_raw_reports: tuple[Path, ...] = (),
        warmup_reports: tuple[Path, ...] = (),
        runtime_dependencies: tuple[Path, ...] = (),
        reindex_repetitions: bool = False,
        repetitions: int = 30,
        use_response_file: bool = False,
    ) -> int:
        arguments = [
            "prepare_benchmark_artifact.py",
            str(raw_report),
            str(output_report),
            str(binary),
            "--runner",
            str(runner),
            "--source-root",
            str(root),
            "--provider",
            "Jolt",
            "--compiler-id",
            "Clang",
            "--compiler-version",
            "22.1.8",
            "--cpu-affinity-policy",
            "eight-physical-cores",
            "--benchmark-filter",
            "Jolt/Matched",
            "--minimum-time",
            "0.05",
            "--repetitions",
            str(repetitions),
        ]
        for additional_raw_report in additional_raw_reports:
            arguments.extend(("--additional-raw-report", str(additional_raw_report)))
        for warmup_report in warmup_reports:
            arguments.extend(("--warmup-report", str(warmup_report)))
        for runtime_dependency in runtime_dependencies:
            arguments.extend(("--runtime-dependency", str(runtime_dependency)))
        if reindex_repetitions:
            arguments.append("--reindex-repetitions")
        if use_response_file:
            response_file = root / "prepare-arguments.rsp"
            response_file.write_text("\n".join(arguments[1:]) + "\n", encoding="utf-8")
            arguments = [arguments[0], f"@{response_file}"]
        with patch.object(sys, "argv", arguments):
            return prepare_benchmark_artifact.main()

    def test_records_binary_runner_and_complete_source_state(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            untracked = root / "new_source.cpp"
            untracked.write_text("int new_source = 2;\n", encoding="utf-8")
            runtime_dependency = root / "Jolt.API.dll"
            runtime_dependency.write_bytes(b"jolt-runtime")
            newest_source_time = max(source.stat().st_mtime_ns, untracked.stat().st_mtime_ns)
            output_time = newest_source_time + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runtime_dependency, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    runtime_dependencies=(runtime_dependency,),
                ),
                0,
            )
            report = json.loads(output_report.read_text(encoding="utf-8"))
            qualification = report["qualification"]
            self.assertEqual(qualification["binary_sha256"], prepare_benchmark_artifact.sha256_file(binary))
            self.assertEqual(qualification["runner_sha256"], prepare_benchmark_artifact.sha256_file(runner))
            self.assertEqual(
                qualification["runtime_dependencies"],
                [
                    {
                        "mtime_ns": runtime_dependency.stat().st_mtime_ns,
                        "path": str(runtime_dependency.resolve()),
                        "sha256": prepare_benchmark_artifact.sha256_file(runtime_dependency),
                    }
                ],
            )
            self.assertEqual(
                qualification["raw_report_sha256"],
                [prepare_benchmark_artifact.sha256_file(raw_report)],
            )
            self.assertTrue(qualification["source_state_sha256"])
            self.assertGreaterEqual(qualification["source_epoch_ns"], untracked.stat().st_mtime_ns)

    def test_accepts_response_file_arguments(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    use_response_file=True,
                ),
                0,
            )
            self.assertTrue(output_report.is_file())

    def test_rejects_binary_older_than_dirty_source(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            stale_time = min(binary.stat().st_mtime_ns, runner.stat().st_mtime_ns)
            newer_source_time = stale_time + 1_000_000_000
            source.write_text("int benchmark_source = 2;\n", encoding="utf-8")
            os.utime(source, ns=(newer_source_time, newer_source_time))
            os.utime(raw_report, ns=(newer_source_time + 1, newer_source_time + 1))

            self.assertEqual(self.run_prepare(root, binary, runner, raw_report, output_report), 1)
            self.assertFalse(output_report.exists())

    def test_rejects_runtime_dependency_older_than_dirty_source(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            runtime_dependency = root / "Jolt.API.dll"
            runtime_dependency.write_bytes(b"stale-jolt-runtime")
            stale_time = runtime_dependency.stat().st_mtime_ns
            source_time = stale_time + 1_000_000_000
            source.write_text("int benchmark_source = 2;\n", encoding="utf-8")
            os.utime(source, ns=(source_time, source_time))
            os.utime(binary, ns=(source_time + 1, source_time + 1))
            os.utime(raw_report, ns=(source_time + 2, source_time + 2))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    runtime_dependencies=(runtime_dependency,),
                ),
                1,
            )
            self.assertFalse(output_report.exists())

    def test_merges_disjoint_reports_from_the_same_context(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            context = {
                "library_build_type": "release",
                "host_name": "benchmark-host",
                "mhz_per_cpu": 4_500,
            }
            raw_report.write_text(
                json.dumps({"context": context, "benchmarks": [{"name": "Jolt/One"}]}),
                encoding="utf-8",
            )
            additional_report = root / "raw-additional.json"
            additional_report.write_text(
                json.dumps({"context": context, "benchmarks": [{"name": "Jolt/Four"}]}),
                encoding="utf-8",
            )
            newest_source_time = source.stat().st_mtime_ns
            output_time = newest_source_time + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))
            os.utime(additional_report, ns=(output_time + 2, output_time + 2))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    (additional_report,),
                ),
                0,
            )
            report = json.loads(output_report.read_text(encoding="utf-8"))
            self.assertEqual(
                [benchmark["name"] for benchmark in report["benchmarks"]],
                ["Jolt/One", "Jolt/Four"],
            )
            self.assertEqual(len(report["qualification"]["raw_report_sha256"]), 2)
            self.assertEqual(len(report["capture_contexts"]), 2)

    def test_preserves_but_does_not_compare_observational_context(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            raw_report.write_text(
                json.dumps(
                    {
                        "context": {
                            "date": "2026-08-26T00:00:00",
                            "library_build_type": "release",
                            "load_avg": [1.0, 2.0, 3.0],
                            "mhz_per_cpu": 4_500,
                        },
                        "benchmarks": [{"name": "Jolt/One"}],
                    }
                ),
                encoding="utf-8",
            )
            additional_report = root / "raw-additional.json"
            additional_report.write_text(
                json.dumps(
                    {
                        "context": {
                            "date": "2026-08-26T00:00:01",
                            "library_build_type": "release",
                            "load_avg": [4.0, 5.0, 6.0],
                            "mhz_per_cpu": 4_500,
                        },
                        "benchmarks": [{"name": "Jolt/Two"}],
                    }
                ),
                encoding="utf-8",
            )
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))
            os.utime(additional_report, ns=(output_time + 2, output_time + 2))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    (additional_report,),
                ),
                0,
            )
            report = json.loads(output_report.read_text(encoding="utf-8"))
            self.assertEqual(report["capture_contexts"][0]["load_avg"], [1.0, 2.0, 3.0])
            self.assertEqual(report["capture_contexts"][1]["load_avg"], [4.0, 5.0, 6.0])

            changed_report = json.loads(additional_report.read_text(encoding="utf-8"))
            changed_report["context"]["host_name"] = "different-benchmark-host"
            additional_report.write_text(json.dumps(changed_report), encoding="utf-8")
            os.utime(additional_report, ns=(output_time + 3, output_time + 3))
            output_report.unlink()

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    (additional_report,),
                ),
                1,
            )
            self.assertFalse(output_report.exists())

    def test_rejects_duplicate_results_across_reports(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            report = {
                "context": {"library_build_type": "release", "mhz_per_cpu": 4_500},
                "benchmarks": [{"name": "Jolt/Duplicate"}],
            }
            raw_report.write_text(json.dumps(report), encoding="utf-8")
            additional_report = root / "raw-additional.json"
            additional_report.write_text(json.dumps(report), encoding="utf-8")
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))
            os.utime(additional_report, ns=(output_time + 2, output_time + 2))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    (additional_report,),
                ),
                1,
            )
            self.assertFalse(output_report.exists())

    def test_reindexes_repetition_batches_without_preserving_batch_aggregates(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            context = {"library_build_type": "release", "mhz_per_cpu": 4_500}

            def create_benchmark(repetition_index: int) -> dict:
                return {
                    "name": "Jolt/Repeated",
                    "run_name": "Jolt/Repeated",
                    "run_type": "iteration",
                    "repetitions": 2,
                    "repetition_index": repetition_index,
                }

            benchmarks = [create_benchmark(0), create_benchmark(1)]
            benchmarks.append(
                {
                    "name": "Jolt/Repeated_mean",
                    "run_name": "Jolt/Repeated",
                    "run_type": "aggregate",
                    "aggregate_name": "mean",
                }
            )
            raw_report.write_text(
                json.dumps({"context": context, "benchmarks": benchmarks}),
                encoding="utf-8",
            )
            additional_report = root / "raw-additional.json"
            additional_report.write_text(
                json.dumps({"context": context, "benchmarks": benchmarks}),
                encoding="utf-8",
            )
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))
            os.utime(additional_report, ns=(output_time + 2, output_time + 2))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    (additional_report,),
                    (),
                    reindex_repetitions=True,
                    repetitions=4,
                ),
                0,
            )
            report = json.loads(output_report.read_text(encoding="utf-8"))
            self.assertEqual(
                [benchmark["repetition_index"] for benchmark in report["benchmarks"]],
                [0, 1, 2, 3],
            )
            self.assertTrue(all(benchmark["repetitions"] == 4 for benchmark in report["benchmarks"]))
            self.assertTrue(report["qualification"]["reindexed_repetitions"])

    def test_rejects_non_contiguous_local_repetition_batch(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            raw_report.write_text(
                json.dumps(
                    {
                        "context": {"library_build_type": "release", "mhz_per_cpu": 4_500},
                        "benchmarks": [
                            {
                                "name": "Jolt/Repeated",
                                "run_name": "Jolt/Repeated",
                                "run_type": "iteration",
                                "repetitions": 2,
                                "repetition_index": 1,
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    (),
                    (),
                    reindex_repetitions=True,
                    repetitions=1,
                ),
                1,
            )
            self.assertFalse(output_report.exists())

    def test_records_separate_warmup_reports_without_merging_their_results(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            context = {"library_build_type": "release", "mhz_per_cpu": 4_500}
            raw_report.write_text(
                json.dumps({"context": context, "benchmarks": [{"name": "Jolt/Measured"}]}),
                encoding="utf-8",
            )
            warmup_report = root / "warmup.json"
            warmup_report.write_text(
                json.dumps({"context": context, "benchmarks": [{"name": "Jolt/Warmup"}]}),
                encoding="utf-8",
            )
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))
            os.utime(warmup_report, ns=(output_time + 2, output_time + 2))

            self.assertEqual(
                self.run_prepare(
                    root,
                    binary,
                    runner,
                    raw_report,
                    output_report,
                    warmup_reports=(warmup_report,),
                ),
                0,
            )
            report = json.loads(output_report.read_text(encoding="utf-8"))
            self.assertEqual([benchmark["name"] for benchmark in report["benchmarks"]], ["Jolt/Measured"])
            self.assertEqual(
                report["qualification"]["warmup_report_sha256"],
                [prepare_benchmark_artifact.sha256_file(warmup_report)],
            )

    def test_rejects_empty_benchmark_report(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            raw_report.write_text(
                json.dumps(
                    {
                        "context": {"library_build_type": "release", "mhz_per_cpu": 4_500},
                        "benchmarks": [],
                    }
                ),
                encoding="utf-8",
            )
            output_time = source.stat().st_mtime_ns + 1_000_000_000
            os.utime(binary, ns=(output_time, output_time))
            os.utime(runner, ns=(output_time, output_time))
            os.utime(raw_report, ns=(output_time + 1, output_time + 1))

            self.assertEqual(self.run_prepare(root, binary, runner, raw_report, output_report), 1)
            self.assertFalse(output_report.exists())

    def test_accepts_runner_older_than_provider_source(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source, binary, runner, raw_report, output_report = self.create_repository(root)
            source_time = runner.stat().st_mtime_ns + 1_000_000_000
            source.write_text("int benchmark_source = 2;\n", encoding="utf-8")
            os.utime(source, ns=(source_time, source_time))
            os.utime(binary, ns=(source_time + 1, source_time + 1))
            os.utime(raw_report, ns=(source_time + 2, source_time + 2))

            self.assertEqual(self.run_prepare(root, binary, runner, raw_report, output_report), 0)

    def test_rejects_report_older_than_runner(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _, binary, runner, raw_report, output_report = self.create_repository(root)
            report_time = raw_report.stat().st_mtime_ns
            runner_time = report_time + 1_000_000_000
            os.utime(runner, ns=(runner_time, runner_time))

            self.assertEqual(self.run_prepare(root, binary, runner, raw_report, output_report), 1)
            self.assertFalse(output_report.exists())


if __name__ == "__main__":
    unittest.main()
