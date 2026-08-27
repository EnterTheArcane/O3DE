#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import hashlib
import json
import math
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

import compare_provider_benchmarks


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_git(source_root: Path, *arguments: str) -> bytes:
    result = subprocess.run(
        ("git", *arguments),
        cwd=source_root,
        check=True,
        capture_output=True,
    )
    return result.stdout


def get_source_state(source_root: Path) -> tuple[bytes, int]:
    source_state = bytearray(run_git(source_root, "diff", "--binary", "HEAD"))
    source_epoch_ns = int(run_git(source_root, "log", "-1", "--format=%ct", "HEAD")) * 1_000_000_000
    untracked_paths = run_git(
        source_root,
        "ls-files",
        "--others",
        "--exclude-standard",
        "-z",
    ).split(b"\0")
    for encoded_path in sorted(path for path in untracked_paths if path):
        relative_path = encoded_path.decode("utf-8", errors="surrogateescape")
        source_path = source_root / relative_path
        if not source_path.is_file():
            continue
        source_state.extend(b"\0untracked\0")
        source_state.extend(encoded_path)
        source_state.extend(b"\0")
        source_state.extend(source_path.read_bytes())
        source_epoch_ns = max(source_epoch_ns, source_path.stat().st_mtime_ns)

    changed_paths = run_git(source_root, "diff", "--name-only", "HEAD", "-z").split(b"\0")
    for encoded_path in changed_paths:
        if not encoded_path:
            continue
        relative_path = encoded_path.decode("utf-8", errors="surrogateescape")
        source_path = source_root / relative_path
        if source_path.is_file():
            source_epoch_ns = max(source_epoch_ns, source_path.stat().st_mtime_ns)
    return bytes(source_state), source_epoch_ns


def load_raw_reports(
    raw_reports: list[Path],
    reindex_repetitions: bool = False,
    expected_repetitions: int = 0,
    validate_only: bool = False,
) -> dict:
    reports = []
    for raw_report in raw_reports:
        try:
            with raw_report.open(encoding="utf-8") as report_file:
                report = json.load(report_file)
        except json.JSONDecodeError as error:
            raise ValueError(f"{raw_report} is not valid benchmark JSON: {error}.") from error
        if report.get("context", {}).get("library_build_type") != "release":
            raise ValueError(f"{raw_report} is not from a Release build.")
        if not report.get("benchmarks"):
            raise ValueError(f"{raw_report} contains no benchmark results.")
        reports.append(report)

    reference_context = dict(reports[0].get("context", {}))
    reference_context.pop("date", None)
    reference_context.pop("load_avg", None)
    reference_frequency = float(reference_context.pop("mhz_per_cpu", 0.0))
    if reference_frequency <= 0.0:
        raise ValueError(f"{raw_reports[0]} has no valid CPU frequency.")
    merged_benchmarks = []
    benchmark_keys = set()
    repetition_offsets = {}
    for raw_report, report in zip(raw_reports, reports, strict=True):
        context = dict(report.get("context", {}))
        context.pop("date", None)
        context.pop("load_avg", None)
        frequency = float(context.pop("mhz_per_cpu", 0.0))
        frequency_ratio = frequency / reference_frequency
        if frequency_ratio < 0.99 or frequency_ratio > 1.01:
            raise ValueError(f"{raw_report} was captured at an incompatible CPU frequency.")
        if context != reference_context:
            raise ValueError(f"{raw_report} was captured under a different benchmark context.")
        if validate_only:
            continue

        report_repetitions = {}
        if reindex_repetitions:
            for benchmark in report.get("benchmarks", []):
                if benchmark.get("run_type", "iteration") != "iteration":
                    continue
                run_name = benchmark.get("run_name", benchmark.get("name"))
                report_repetitions.setdefault(run_name, []).append(benchmark.get("repetition_index"))
            for run_name, repetition_indices in report_repetitions.items():
                if sorted(repetition_indices) != list(range(len(repetition_indices))):
                    raise ValueError(
                        f"{raw_report} has non-contiguous local repetitions for {run_name!r}."
                    )

        for source_benchmark in report.get("benchmarks", []):
            if reindex_repetitions and source_benchmark.get("run_type") == "aggregate":
                continue
            benchmark = dict(source_benchmark)
            run_name = benchmark.get("run_name", benchmark.get("name"))
            if reindex_repetitions:
                benchmark["repetition_index"] += repetition_offsets.get(run_name, 0)
                benchmark["repetitions"] = expected_repetitions
            benchmark_key = (
                benchmark.get("name"),
                benchmark.get("run_type", "iteration"),
                benchmark.get("repetition_index"),
                benchmark.get("aggregate_name"),
            )
            if benchmark_key in benchmark_keys:
                raise ValueError(f"{raw_report} contains a duplicate benchmark result {benchmark_key!r}.")
            benchmark_keys.add(benchmark_key)
            merged_benchmarks.append(benchmark)

        for run_name, repetition_indices in report_repetitions.items():
            repetition_offsets[run_name] = repetition_offsets.get(run_name, 0) + len(repetition_indices)

    if reindex_repetitions:
        for run_name, repetition_count in repetition_offsets.items():
            if repetition_count != expected_repetitions:
                raise ValueError(
                    f"Merged benchmark {run_name!r} has {repetition_count} repetitions; "
                    f"expected {expected_repetitions}."
                )

    merged_report = dict(reports[0])
    merged_report["benchmarks"] = merged_benchmarks
    merged_report["capture_contexts"] = [report.get("context", {}) for report in reports]
    return merged_report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Add reproducibility and stale-binary metadata to a raw benchmark report.",
        fromfile_prefix_chars="@",
    )
    parser.add_argument("raw_report", type=Path)
    parser.add_argument("output_report", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--provider", choices=compare_provider_benchmarks.PROVIDERS, required=True)
    parser.add_argument("--compiler-id", required=True)
    parser.add_argument("--compiler-version", required=True)
    parser.add_argument("--build-configuration", default="Release")
    parser.add_argument("--cpu-affinity-policy", required=True)
    parser.add_argument("--benchmark-filter", required=True)
    parser.add_argument("--minimum-time", type=float, required=True)
    parser.add_argument("--minimum-time-policy", required=True)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--additional-raw-report", action="append", type=Path, default=[])
    parser.add_argument("--runtime-dependency", action="append", type=Path, default=[])
    parser.add_argument("--reindex-repetitions", action="store_true")
    parser.add_argument("--warmup-report", action="append", type=Path, default=[])
    arguments = parser.parse_args()

    raw_reports = [arguments.raw_report.resolve()]
    raw_reports.extend(raw_report.resolve() for raw_report in arguments.additional_raw_report)
    warmup_reports = [warmup_report.resolve() for warmup_report in arguments.warmup_report]
    output_report = arguments.output_report.resolve()
    binary = arguments.binary.resolve()
    runner = arguments.runner.resolve()
    runtime_dependencies = sorted(
        (dependency.resolve() for dependency in arguments.runtime_dependency),
        key=lambda dependency: str(dependency).lower(),
    )
    source_root = arguments.source_root.resolve()
    if (
        not all(raw_report.is_file() for raw_report in raw_reports)
        or not all(warmup_report.is_file() for warmup_report in warmup_reports)
        or not binary.is_file()
        or not runner.is_file()
        or not all(dependency.is_file() for dependency in runtime_dependencies)
        or not source_root.is_dir()
    ):
        print(
            "The raw report, measured binary, runtime dependency, runner, or source root does not exist.",
            file=sys.stderr,
        )
        return 1
    measured_binaries = [binary, *runtime_dependencies]
    newest_binary_mtime_ns = max(
        runner.stat().st_mtime_ns,
        *(measured_binary.stat().st_mtime_ns for measured_binary in measured_binaries),
    )
    if any(report.stat().st_mtime_ns < newest_binary_mtime_ns for report in raw_reports + warmup_reports):
        print("The benchmark report predates its measured binary or runner.", file=sys.stderr)
        return 1
    if arguments.repetitions <= 0 or arguments.minimum_time <= 0.0:
        print("Repetitions and minimum time must be positive.", file=sys.stderr)
        return 1

    try:
        minimum_time_policy = json.loads(arguments.minimum_time_policy)
        raw_default_seconds = minimum_time_policy["default_seconds"]
        if isinstance(raw_default_seconds, bool):
            raise ValueError
        default_seconds = float(raw_default_seconds)
        overrides_seconds = minimum_time_policy["overrides_seconds"]
        if (
            not isinstance(minimum_time_policy, dict)
            or set(minimum_time_policy) != {"default_seconds", "overrides_seconds"}
            or not isinstance(overrides_seconds, dict)
            or default_seconds != arguments.minimum_time
            or not math.isfinite(default_seconds)
            or default_seconds <= 0.0
            or any(
                not isinstance(suffix, str)
                or not suffix
                or not isinstance(seconds, (float, int))
                or isinstance(seconds, bool)
                or not math.isfinite(seconds)
                or seconds <= default_seconds
                for suffix, seconds in overrides_seconds.items()
            )
        ):
            raise ValueError
    except (KeyError, TypeError, ValueError, json.JSONDecodeError):
        print("The minimum-time policy is invalid or does not match the default minimum time.", file=sys.stderr)
        return 1

    try:
        report = load_raw_reports(
            raw_reports,
            arguments.reindex_repetitions,
            arguments.repetitions,
        )
        if warmup_reports:
            warmup_report = load_raw_reports(warmup_reports, validate_only=True)
            measured_context = dict(report["context"])
            warmup_context = dict(warmup_report["context"])
            measured_context.pop("date", None)
            warmup_context.pop("date", None)
            measured_context.pop("load_avg", None)
            warmup_context.pop("load_avg", None)
            measured_frequency = float(measured_context.pop("mhz_per_cpu", 0.0))
            warmup_frequency = float(warmup_context.pop("mhz_per_cpu", 0.0))
            frequency_ratio = warmup_frequency / measured_frequency
            if frequency_ratio < 0.99 or frequency_ratio > 1.01 or warmup_context != measured_context:
                raise ValueError("Warmup reports were captured under a different benchmark context.")
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    source_revision = run_git(source_root, "rev-parse", "HEAD").decode("utf-8").strip()
    source_state, source_epoch_ns = get_source_state(source_root)
    if any(measured_binary.stat().st_mtime_ns < source_epoch_ns for measured_binary in measured_binaries):
        print("A measured binary predates the current source state.", file=sys.stderr)
        return 1
    report["qualification"] = {
        "benchmark_filter": arguments.benchmark_filter,
        "binary_path": str(binary),
        "binary_sha256": sha256_file(binary),
        "binary_mtime_ns": binary.stat().st_mtime_ns,
        "build_configuration": arguments.build_configuration,
        "compiler_id": arguments.compiler_id,
        "compiler_version": arguments.compiler_version,
        "cpu_affinity_policy": arguments.cpu_affinity_policy,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "minimum_time": arguments.minimum_time,
        "minimum_time_policy": minimum_time_policy,
        "provider": arguments.provider,
        "raw_samples": True,
        "raw_report_sha256": [sha256_file(raw_report) for raw_report in raw_reports],
        "reindexed_repetitions": arguments.reindex_repetitions,
        "repetitions": arguments.repetitions,
        "runtime_dependencies": [
            {
                "mtime_ns": dependency.stat().st_mtime_ns,
                "path": str(dependency),
                "sha256": sha256_file(dependency),
            }
            for dependency in runtime_dependencies
        ],
        "runner_path": str(runner),
        "runner_sha256": sha256_file(runner),
        "runner_mtime_ns": runner.stat().st_mtime_ns,
        "source_epoch_ns": source_epoch_ns,
        "source_revision": source_revision,
        "source_state_sha256": hashlib.sha256(source_state).hexdigest(),
        "workload_signature": compare_provider_benchmarks.workload_signature(),
        "warmup_report_sha256": [sha256_file(warmup_report) for warmup_report in warmup_reports],
    }

    output_report.parent.mkdir(parents=True, exist_ok=True)
    with output_report.open("w", encoding="utf-8", newline="\n") as output_file:
        json.dump(report, output_file, indent=2)
        output_file.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
