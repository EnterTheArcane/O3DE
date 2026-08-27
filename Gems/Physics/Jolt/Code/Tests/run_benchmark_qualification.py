#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Capture and qualify process-isolated Jolt provider benchmarks."""

import argparse
import ctypes
import hashlib
import json
import math
import os
import platform
import re
import statistics
import subprocess
import sys
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Sequence

import compare_provider_benchmarks
import prepare_benchmark_artifact
import validate_jolt_benchmarks


ABSOLUTE_BENCHMARK_SUFFIXES = (
    "Diagnostic/AcquireRuntimeConfigurationCapability/real_time",
    "Diagnostic/RaycastEmptyWorld/128/real_time",
    "Query/OverlapSphereGrid/1024/1/1/real_time",
    "Query/RaycastGrid/1024/128/1/real_time",
    "Rollback/RecaptureFilteredState/128/0/real_time",
    "Rollback/RecaptureFilteredState/1024/0/real_time",
    "Rollback/RestoreFilteredStateTransactional/128/1/real_time",
    "Rollback/RestoreFilteredStateTransactional/1024/1/real_time",
    "Rollback/RestoreFilteredStateValidated/128/2/real_time",
    "Rollback/RestoreFilteredStateValidated/1024/2/real_time",
)
ABSOLUTE_FILTER_SUFFIX = f"({'|'.join(ABSOLUTE_BENCHMARK_SUFFIXES)})"
FULL_REPETITION_COUNT = 30
REVIEW_REPETITION_COUNT = 3
FULL_MINIMUM_TIME_SECONDS = 2.0
REVIEW_MINIMUM_TIME_SECONDS = 0.05
FULL_MAXIMUM_CV = 0.05
FULL_MAXIMUM_CAPABILITY_NANOSECONDS = 3.0
FULL_MAXIMUM_CAPABILITY_OPERATION_RATIO = 0.02
FULL_MATCHED_CAPTURE_ATTEMPTS = 2
FULL_ABSOLUTE_CAPTURE_ATTEMPTS = 2


@dataclass(frozen=True)
class Provider:
    name: str
    module_name: str
    runtime_dependency_name: str = ""


@dataclass(frozen=True)
class Workload:
    label: str
    suffix: str
    worker_count: int
    minimum_time_seconds: float = 0.0


PROVIDERS = (
    Provider("Jolt", "Jolt.Tests.Gem.dll", "Jolt.API.dll"),
    Provider("Box3D", "Box3D.Tests.Gem.dll", "Box3D.API.dll"),
    Provider("PhysX", "PhysX5.Tests.Gem.dll"),
)


def matched_workloads() -> tuple[Workload, ...]:
    return tuple(
        Workload(
            label=workload["label"],
            suffix=workload["suffix"],
            worker_count=int(workload["exact"]["Workers"]),
            minimum_time_seconds=float(workload.get("minimum_time_seconds", 0.0)),
        )
        for workload in (*compare_provider_benchmarks.WORKLOADS, *compare_provider_benchmarks.TAIL_WORKLOADS)
    )


def review_workloads() -> tuple[Workload, ...]:
    workloads = matched_workloads()
    return workloads[0], workloads[8], workloads[12]


def describe_workload_filter(provider: str, workloads: Sequence[Workload]) -> str:
    return f"{provider}/({'|'.join(workload.suffix for workload in workloads)})"


def build_minimum_time_policy(
    workloads: Sequence[Workload],
    default_seconds: float,
) -> dict:
    overrides = {
        workload.suffix: workload.minimum_time_seconds
        for workload in workloads
        if workload.minimum_time_seconds > default_seconds
    }
    return {
        "default_seconds": default_seconds,
        "overrides_seconds": overrides,
    }


def select_compact_processors(processors: Sequence[int], count: int) -> tuple[int, ...]:
    if count <= 0:
        raise ValueError("The requested processor count must be positive.")
    if len(processors) < count:
        raise ValueError(f"Only {len(processors)} physical processors are available; {count} are required.")

    return tuple(processors[-count:])


def _windows_physical_processors() -> tuple[int, ...]:
    from ctypes import wintypes

    relation_processor_core = 0
    error_insufficient_buffer = 122
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    get_information = kernel32.GetLogicalProcessorInformationEx
    get_information.argtypes = [wintypes.DWORD, ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD)]
    get_information.restype = wintypes.BOOL
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    kernel32.GetProcessAffinityMask.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(ctypes.c_size_t),
    ]
    kernel32.GetProcessAffinityMask.restype = wintypes.BOOL
    required_size = wintypes.DWORD(0)
    if get_information(relation_processor_core, None, ctypes.byref(required_size)):
        raise OSError("GetLogicalProcessorInformationEx unexpectedly accepted an empty buffer.")
    if ctypes.get_last_error() != error_insufficient_buffer or required_size.value == 0:
        raise ctypes.WinError(ctypes.get_last_error())

    buffer = ctypes.create_string_buffer(required_size.value)
    if not get_information(relation_processor_core, buffer, ctypes.byref(required_size)):
        raise ctypes.WinError(ctypes.get_last_error())

    process = kernel32.GetCurrentProcess()
    process_mask = ctypes.c_size_t()
    system_mask = ctypes.c_size_t()
    if not kernel32.GetProcessAffinityMask(process, ctypes.byref(process_mask), ctypes.byref(system_mask)):
        raise ctypes.WinError(ctypes.get_last_error())

    processors = []
    offset = 0
    while offset < required_size.value:
        relationship = ctypes.c_uint32.from_buffer_copy(buffer.raw, offset).value
        record_size = ctypes.c_uint32.from_buffer_copy(buffer.raw, offset + 4).value
        if record_size < 40 or offset + record_size > required_size.value:
            raise ValueError("Windows returned a malformed processor topology record.")
        if relationship == relation_processor_core:
            efficiency_class = buffer.raw[offset + 9]
            group_count = ctypes.c_uint16.from_buffer_copy(buffer.raw, offset + 30).value
            for group_index in range(group_count):
                group_offset = offset + 32 + group_index * 16
                group_mask = ctypes.c_size_t.from_buffer_copy(buffer.raw, group_offset).value
                group_number = ctypes.c_uint16.from_buffer_copy(
                    buffer.raw,
                    group_offset + ctypes.sizeof(ctypes.c_size_t),
                ).value
                if group_number != 0:
                    continue
                available_mask = group_mask & process_mask.value
                if available_mask:
                    processor = (available_mask & -available_mask).bit_length() - 1
                    processors.append((efficiency_class, processor))
        offset += record_size

    if not processors:
        raise ValueError("No physical processors are available in Windows processor group zero.")
    processors.sort()
    return tuple(processor for _, processor in processors)


def _posix_physical_processors() -> tuple[int, ...]:
    allowed_processors = sorted(os.sched_getaffinity(0))
    topology_root = Path("/sys/devices/system/cpu")
    physical_processors = {}
    for processor in allowed_processors:
        topology_directory = topology_root / f"cpu{processor}" / "topology"
        try:
            package_id = int((topology_directory / "physical_package_id").read_text(encoding="utf-8"))
            core_id = int((topology_directory / "core_id").read_text(encoding="utf-8"))
        except (FileNotFoundError, ValueError):
            return tuple(allowed_processors)
        physical_processors.setdefault((package_id, core_id), processor)
    return tuple(physical_processors.values())


def physical_processors() -> tuple[int, ...]:
    if platform.system() == "Windows":
        return _windows_physical_processors()
    if hasattr(os, "sched_getaffinity"):
        return _posix_physical_processors()
    raise ValueError(f"Processor affinity is unsupported on {platform.system()}.")


def build_affinity_policy() -> dict[int, tuple[int, ...]]:
    available_processors = physical_processors()
    return {
        worker_count: select_compact_processors(available_processors, worker_count)
        for worker_count in (1, 4, 8)
    }


def describe_affinity_policy(policy: dict[int, tuple[int, ...]]) -> str:
    selections = []
    for worker_count in sorted(policy):
        processor_list = ",".join(str(processor) for processor in policy[worker_count])
        selections.append(f"{worker_count}={processor_list}")
    priority_policy = "default priority"
    if platform.system() == "Windows":
        priority_policy = "high priority"
    return "automatic compact high-performance physical-core lanes: " + "; ".join(selections) + f"; inherited affinity; {priority_policy}"


@contextmanager
def constrained_process(processors: Sequence[int]) -> Iterator[None]:
    if platform.system() == "Windows":
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.GetCurrentProcess.restype = wintypes.HANDLE
        kernel32.GetProcessAffinityMask.argtypes = [
            wintypes.HANDLE,
            ctypes.POINTER(ctypes.c_size_t),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        kernel32.GetProcessAffinityMask.restype = wintypes.BOOL
        kernel32.GetPriorityClass.argtypes = [wintypes.HANDLE]
        kernel32.GetPriorityClass.restype = wintypes.DWORD
        kernel32.SetPriorityClass.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel32.SetPriorityClass.restype = wintypes.BOOL
        kernel32.SetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.c_size_t]
        kernel32.SetProcessAffinityMask.restype = wintypes.BOOL
        process = kernel32.GetCurrentProcess()
        process_mask = ctypes.c_size_t()
        system_mask = ctypes.c_size_t()
        if not kernel32.GetProcessAffinityMask(process, ctypes.byref(process_mask), ctypes.byref(system_mask)):
            raise ctypes.WinError(ctypes.get_last_error())
        previous_priority = kernel32.GetPriorityClass(process)
        if previous_priority == 0:
            raise ctypes.WinError(ctypes.get_last_error())

        requested_mask = 0
        for processor in processors:
            requested_mask |= 1 << processor
        if not kernel32.SetProcessAffinityMask(process, ctypes.c_size_t(requested_mask)):
            raise ctypes.WinError(ctypes.get_last_error())
        high_priority_class = 0x00000080
        if not kernel32.SetPriorityClass(process, high_priority_class):
            kernel32.SetProcessAffinityMask(process, process_mask)
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            yield
        finally:
            kernel32.SetPriorityClass(process, previous_priority)
            kernel32.SetProcessAffinityMask(process, process_mask)
        return

    if hasattr(os, "sched_getaffinity") and hasattr(os, "sched_setaffinity"):
        previous_affinity = os.sched_getaffinity(0)
        os.sched_setaffinity(0, processors)
        try:
            yield
        finally:
            os.sched_setaffinity(0, previous_affinity)
        return

    raise ValueError(f"Processor affinity is unsupported on {platform.system()}.")


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "-", value).strip("-").lower()


def run_benchmark_process(
    runner: Path,
    module: Path,
    provider: str,
    filter_suffix: str,
    minimum_time: float,
    output_path: Path,
    processors: Sequence[int],
    timeout_seconds: int,
) -> None:
    benchmark_filter = f"^{provider}/{filter_suffix}$"
    command = (
        str(runner),
        str(module),
        "AzRunBenchmarks",
        f"--benchmark_filter={benchmark_filter}",
        f"--benchmark_min_time={minimum_time}",
        "--benchmark_repetitions=1",
        "--benchmark_report_aggregates_only=false",
        "--benchmark_display_aggregates_only=true",
        f"--benchmark_out={output_path}",
        "--benchmark_out_format=json",
    )
    log_path = output_path.with_suffix(".log")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with constrained_process(processors):
        with log_path.open("w", encoding="utf-8", errors="replace") as log:
            completed = subprocess.run(
                command,
                cwd=runner.parent,
                check=False,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=timeout_seconds,
            )
    if completed.returncode != 0:
        raise RuntimeError(f"{provider} benchmark failed with exit code {completed.returncode}: {log_path}")
    if not output_path.is_file() or output_path.stat().st_size == 0:
        raise RuntimeError(f"{provider} benchmark produced no report: {output_path}")
    try:
        report = json.loads(output_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as error:
        raise RuntimeError(f"{provider} benchmark produced an unreadable report: {output_path}") from error
    iteration_results = [
        result
        for result in report.get("benchmarks", [])
        if result.get("run_type", "iteration") == "iteration"
    ]
    if not iteration_results:
        raise RuntimeError(f"{provider} benchmark filter matched no workloads: {benchmark_filter}")
    for result in iteration_results:
        if round(float(result.get("AffinityConstrained", 0))) != 1:
            raise RuntimeError(
                f"{provider} benchmark did not preserve its inherited CPU affinity: {output_path}"
            )
        affinity_processor_count = round(float(result.get("AffinityProcessors", 0)))
        if affinity_processor_count != len(processors):
            raise RuntimeError(
                f"{provider} benchmark reported {affinity_processor_count} affinity processors; "
                f"expected {len(processors)}: {output_path}"
            )


def write_response_file(path: Path, arguments: Sequence[str]) -> None:
    path.write_text("\n".join(arguments) + "\n", encoding="utf-8", newline="\n")


def prepare_artifact(
    scripts_directory: Path,
    engine_root: Path,
    runner: Path,
    provider: Provider,
    module: Path,
    runtime_dependency: Path | None,
    raw_reports: Sequence[Path],
    warmup_reports: Sequence[Path],
    output_report: Path,
    compiler_id: str,
    compiler_version: str,
    affinity_description: str,
    benchmark_filter: str,
    minimum_time: float,
    minimum_time_policy: dict,
    repetitions: int,
) -> None:
    response_arguments = []
    for raw_report in raw_reports[1:]:
        response_arguments.extend(("--additional-raw-report", str(raw_report)))
    for warmup_report in warmup_reports:
        response_arguments.extend(("--warmup-report", str(warmup_report)))
    response_file = output_report.with_suffix(".rsp")
    write_response_file(response_file, response_arguments)

    command = [
        sys.executable,
        str(scripts_directory / "prepare_benchmark_artifact.py"),
        str(raw_reports[0]),
        str(output_report),
        str(module),
        f"@{response_file}",
        "--runner",
        str(runner),
        "--source-root",
        str(engine_root),
        "--provider",
        provider.name,
        "--compiler-id",
        compiler_id,
        "--compiler-version",
        compiler_version,
        "--build-configuration",
        "Release",
        "--cpu-affinity-policy",
        affinity_description,
        "--benchmark-filter",
        benchmark_filter,
        "--minimum-time",
        str(minimum_time),
        "--minimum-time-policy",
        json.dumps(minimum_time_policy, sort_keys=True, separators=(",", ":")),
        "--repetitions",
        str(repetitions),
        "--reindex-repetitions",
    ]
    if runtime_dependency:
        command.extend(("--runtime-dependency", str(runtime_dependency)))
    subprocess.run(command, cwd=engine_root, check=True)


def validate_smoke_report(
    path: Path,
    repetitions: int,
    expected_names: Sequence[str],
) -> None:
    report = json.loads(path.read_text(encoding="utf-8"))
    observed_repetitions = {}
    for result in report.get("benchmarks", []):
        if result.get("run_type", "iteration") != "iteration":
            continue
        name = result.get("name", "")
        repetition_index = result.get("repetition_index")
        observed_repetitions.setdefault(name, set()).add(repetition_index)
        real_time = float(result.get("real_time", math.nan))
        if not math.isfinite(real_time) or real_time <= 0.0:
            raise ValueError(f"{name} has an invalid smoke measurement.")
        if "QualityValid" in result and round(float(result["QualityValid"])) != 1:
            raise ValueError(f"{name} failed its benchmark correctness counter.")
    if not observed_repetitions:
        raise ValueError("The benchmark smoke report contains no measurements.")
    if set(observed_repetitions) != set(expected_names):
        missing_names = sorted(set(expected_names) - set(observed_repetitions))
        unexpected_names = sorted(set(observed_repetitions) - set(expected_names))
        raise ValueError(
            f"The benchmark smoke report has missing names {missing_names} "
            f"and unexpected names {unexpected_names}."
        )
    expected_indices = set(range(repetitions))
    for name, repetition_indices in observed_repetitions.items():
        if repetition_indices != expected_indices:
            raise ValueError(f"{name} does not contain {repetitions} process-isolated repetitions.")


def validate_reusable_report(
    report_path: Path,
    engine_root: Path,
    runner: Path,
    provider: Provider,
    module: Path,
    runtime_dependency: Path | None,
    compiler_id: str,
    compiler_version: str,
    affinity_description: str,
    benchmark_filter: str,
    minimum_time: float,
    minimum_time_policy: dict,
    repetitions: int,
    expected_names: Sequence[str],
) -> None:
    report = compare_provider_benchmarks.load_report(report_path)
    metadata = report.get("qualification", {})
    source_revision = prepare_benchmark_artifact.run_git(
        engine_root,
        "rev-parse",
        "HEAD",
    ).decode("utf-8").strip()
    source_state, _ = prepare_benchmark_artifact.get_source_state(engine_root)
    expected_metadata = {
        "benchmark_filter": benchmark_filter,
        "binary_sha256": prepare_benchmark_artifact.sha256_file(module),
        "build_configuration": "Release",
        "compiler_id": compiler_id,
        "compiler_version": compiler_version,
        "cpu_affinity_policy": affinity_description,
        "minimum_time": minimum_time,
        "minimum_time_policy": minimum_time_policy,
        "provider": provider.name,
        "raw_samples": True,
        "reindexed_repetitions": True,
        "repetitions": repetitions,
        "runner_sha256": prepare_benchmark_artifact.sha256_file(runner),
        "source_revision": source_revision,
        "source_state_sha256": hashlib.sha256(source_state).hexdigest(),
        "workload_signature": compare_provider_benchmarks.workload_signature(),
    }
    for field, expected_value in expected_metadata.items():
        if metadata.get(field) != expected_value:
            raise ValueError(
                f"The reusable {provider.name} benchmark report has stale {field}: "
                f"expected {expected_value!r}, found {metadata.get(field)!r}."
            )

    expected_dependencies = []
    if runtime_dependency:
        expected_dependencies.append(
            {
                "path": str(runtime_dependency.resolve()),
                "sha256": prepare_benchmark_artifact.sha256_file(runtime_dependency),
            }
        )
    reported_dependencies = [
        {
            "path": dependency.get("path"),
            "sha256": dependency.get("sha256"),
        }
        for dependency in metadata.get("runtime_dependencies", [])
        if isinstance(dependency, dict)
    ]
    if reported_dependencies != expected_dependencies:
        raise ValueError(f"The reusable {provider.name} benchmark runtime dependencies are stale.")

    expected_raw_report_count = len(expected_names) * repetitions
    if len(metadata.get("raw_report_sha256", [])) != expected_raw_report_count:
        raise ValueError(
            f"The reusable {provider.name} benchmark report has an incomplete raw sample set."
        )
    if len(metadata.get("warmup_report_sha256", [])) != len(expected_names):
        raise ValueError(
            f"The reusable {provider.name} benchmark report has an incomplete warmup set."
        )
    validate_smoke_report(report_path, repetitions, expected_names)


def validate_matched_report_quality(
    report_path: Path,
    provider: Provider,
    workloads: Sequence[Workload],
    repetitions: int,
    maximum_cv: float | None,
) -> None:
    report = compare_provider_benchmarks.load_report(report_path)
    workload_specifications = {
        workload["suffix"]: workload
        for workload in (
            *compare_provider_benchmarks.WORKLOADS,
            *compare_provider_benchmarks.TAIL_WORKLOADS,
        )
    }
    tail_workloads = {
        workload["suffix"]
        for workload in compare_provider_benchmarks.TAIL_WORKLOADS
    }
    for workload in workloads:
        specification = workload_specifications.get(workload.suffix)
        if not specification:
            raise ValueError(f"The reusable benchmark workload {workload.suffix!r} is unknown.")

        name = f"{provider.name}/{workload.suffix}"
        if workload.suffix in tail_workloads:
            _, samples, _ = compare_provider_benchmarks.load_tail_samples(
                report,
                provider.name,
                name,
                specification,
                repetitions,
            )
        else:
            samples, _ = compare_provider_benchmarks.load_samples(
                report,
                provider.name,
                name,
                specification,
                repetitions,
            )
        if maximum_cv is not None:
            cv = compare_provider_benchmarks.coefficient_of_variation(samples)
            if cv > maximum_cv:
                raise ValueError(f"{name} CV {cv:.3%} exceeds {maximum_cv:.3%}.")


def validate_absolute_report_quality(
    report_path: Path,
    repetitions: int,
) -> None:
    report = compare_provider_benchmarks.load_report(report_path)
    errors = validate_jolt_benchmarks.validate_report(
        report,
        repetitions,
        FULL_MAXIMUM_CAPABILITY_NANOSECONDS,
        FULL_MAXIMUM_CAPABILITY_OPERATION_RATIO,
        FULL_MAXIMUM_CV,
    )
    if errors:
        raise ValueError(" ".join(errors))


def resolve_provider_files(binary_directory: Path, provider: Provider) -> tuple[Path, Path | None]:
    module = binary_directory / provider.module_name
    if platform.system() != "Windows":
        module = binary_directory / f"lib{provider.module_name.removesuffix('.dll')}.so"
    if not module.is_file():
        raise FileNotFoundError(module)
    runtime_dependency = None
    if provider.runtime_dependency_name:
        runtime_dependency = binary_directory / provider.runtime_dependency_name
        if platform.system() != "Windows":
            runtime_dependency = binary_directory / f"lib{provider.runtime_dependency_name.removesuffix('.dll')}.so"
        if not runtime_dependency.is_file():
            raise FileNotFoundError(runtime_dependency)
    return module, runtime_dependency


def capture_matched_provider(
    provider: Provider,
    workloads: Sequence[Workload],
    repetitions: int,
    minimum_time: float,
    runner: Path,
    binary_directory: Path,
    output_directory: Path,
    affinity_policy: dict[int, tuple[int, ...]],
    timeout_seconds: int,
    maximum_cv: float | None = None,
    maximum_attempts: int = 1,
) -> tuple[list[Path], list[Path]]:
    if maximum_attempts <= 0:
        raise ValueError("The maximum capture attempt count must be positive.")

    module, _ = resolve_provider_files(binary_directory, provider)
    raw_reports = []
    warmup_reports = []
    for workload in workloads:
        workload_minimum_time = max(minimum_time, workload.minimum_time_seconds)
        workload_name = safe_name(workload.label)
        warmup_path = output_directory / provider.name / f"{workload_name}-warmup.json"
        workload_reports = [
            output_directory / provider.name / f"{workload_name}-{repetition:02d}.json"
            for repetition in range(repetitions)
        ]
        for attempt in range(maximum_attempts):
            run_benchmark_process(
                runner,
                module,
                provider.name,
                workload.suffix,
                workload_minimum_time,
                warmup_path,
                affinity_policy[workload.worker_count],
                timeout_seconds,
            )
            for raw_path in workload_reports:
                run_benchmark_process(
                    runner,
                    module,
                    provider.name,
                    workload.suffix,
                    workload_minimum_time,
                    raw_path,
                    affinity_policy[workload.worker_count],
                    timeout_seconds,
                )

            rejection_reason = ""
            if maximum_cv is not None:
                rejection_reason = validate_absolute_capture_batch(
                    workload_reports,
                    warmup_path,
                    maximum_cv,
                )
            if not rejection_reason:
                break
            if attempt + 1 == maximum_attempts:
                raise RuntimeError(
                    f"{provider.name}/{workload.suffix} remained invalid after "
                    f"{maximum_attempts} complete capture attempts: {rejection_reason}"
                )
            rejected_directory = next_rejected_capture_directory(
                output_directory / provider.name / "rejected-matched",
                workload_name,
            )
            archive_capture_batch(
                (warmup_path, *workload_reports),
                rejected_directory,
                rejection_reason,
            )
            print(
                f"Retrying {provider.name}/{workload.suffix} as a complete batch: "
                f"{rejection_reason}",
                file=sys.stderr,
            )

        warmup_reports.append(warmup_path)
        raw_reports.extend(workload_reports)
    return raw_reports, warmup_reports


def capture_absolute_jolt(
    provider: Provider,
    repetitions: int,
    minimum_time: float,
    runner: Path,
    binary_directory: Path,
    output_directory: Path,
    affinity_policy: dict[int, tuple[int, ...]],
    timeout_seconds: int,
    maximum_cv: float | None = None,
    maximum_attempts: int = 1,
) -> tuple[list[Path], list[Path]]:
    if maximum_attempts <= 0:
        raise ValueError("The maximum capture attempt count must be positive.")

    module, _ = resolve_provider_files(binary_directory, provider)
    raw_reports = []
    warmup_reports = []
    for benchmark_suffix in ABSOLUTE_BENCHMARK_SUFFIXES:
        benchmark_name = safe_name(benchmark_suffix)
        warmup_path = output_directory / provider.name / f"absolute-{benchmark_name}-warmup.json"
        workload_reports = [
            output_directory / provider.name / f"absolute-{benchmark_name}-{repetition:02d}.json"
            for repetition in range(repetitions)
        ]
        for attempt in range(maximum_attempts):
            run_benchmark_process(
                runner,
                module,
                provider.name,
                benchmark_suffix,
                minimum_time,
                warmup_path,
                affinity_policy[1],
                timeout_seconds,
            )
            for raw_path in workload_reports:
                run_benchmark_process(
                    runner,
                    module,
                    provider.name,
                    benchmark_suffix,
                    minimum_time,
                    raw_path,
                    affinity_policy[1],
                    timeout_seconds,
                )

            rejection_reason = ""
            if maximum_cv is not None:
                rejection_reason = validate_absolute_capture_batch(
                    workload_reports,
                    warmup_path,
                    maximum_cv,
                )
            if not rejection_reason:
                break
            if attempt + 1 == maximum_attempts:
                raise RuntimeError(
                    f"{provider.name}/{benchmark_suffix} remained invalid after "
                    f"{maximum_attempts} complete capture attempts: {rejection_reason}"
                )
            rejected_directory = next_rejected_capture_directory(
                output_directory / provider.name / "rejected-absolute",
                benchmark_name,
            )
            archive_capture_batch(
                (warmup_path, *workload_reports),
                rejected_directory,
                rejection_reason,
            )
            print(
                f"Retrying {provider.name}/{benchmark_suffix} as a complete batch: "
                f"{rejection_reason}",
                file=sys.stderr,
            )

        warmup_reports.append(warmup_path)
        raw_reports.extend(workload_reports)
    return raw_reports, warmup_reports


def validate_absolute_capture_batch(
    raw_reports: Sequence[Path],
    warmup_report: Path,
    maximum_cv: float,
) -> str:
    contexts = []
    frequencies = []
    samples = []
    for report_path in (warmup_report, *raw_reports):
        report = json.loads(report_path.read_text(encoding="utf-8"))
        context = dict(report.get("context", {}))
        context.pop("date", None)
        context.pop("load_avg", None)
        frequency = float(context.pop("mhz_per_cpu", 0.0))
        if frequency <= 0.0:
            return f"{report_path.name} has no valid CPU frequency"
        contexts.append(context)
        frequencies.append(frequency)

        if report_path == warmup_report:
            continue
        results = [
            result
            for result in report.get("benchmarks", [])
            if result.get("run_type", "iteration") == "iteration"
        ]
        if len(results) != 1:
            return f"{report_path.name} does not contain exactly one measured workload"
        result = results[0]
        time_unit = result.get("time_unit")
        if time_unit not in compare_provider_benchmarks.TIME_UNIT_TO_MICROSECONDS:
            return f"{report_path.name} has unsupported time unit {time_unit!r}"
        real_time = float(result.get("real_time", math.nan))
        if not math.isfinite(real_time) or real_time <= 0.0:
            return f"{report_path.name} has invalid real time {real_time!r}"
        samples.append(real_time * compare_provider_benchmarks.TIME_UNIT_TO_MICROSECONDS[time_unit])

    if any(context != contexts[0] for context in contexts[1:]):
        return "capture reports have different benchmark contexts"
    median_frequency = statistics.median(frequencies)
    for report_path, frequency in zip((warmup_report, *raw_reports), frequencies, strict=True):
        frequency_ratio = frequency / median_frequency
        if frequency_ratio < 0.99 or frequency_ratio > 1.01:
            return f"{report_path.name} has incompatible CPU frequency {frequency:.0f} MHz"
    cv = compare_provider_benchmarks.coefficient_of_variation(samples)
    if cv > maximum_cv:
        return f"CV {cv:.3%} exceeds {maximum_cv:.3%}"
    return ""


def archive_capture_batch(
    reports: Sequence[Path],
    rejected_directory: Path,
    rejection_reason: str,
) -> None:
    rejected_directory.mkdir(parents=True, exist_ok=False)
    (rejected_directory / "reason.txt").write_text(
        rejection_reason + "\n",
        encoding="utf-8",
        newline="\n",
    )
    for report_path in reports:
        report_path.replace(rejected_directory / report_path.name)
        log_path = report_path.with_suffix(".log")
        if log_path.is_file():
            log_path.replace(rejected_directory / log_path.name)


def next_rejected_capture_directory(
    output_directory: Path,
    benchmark_name: str,
) -> Path:
    attempt = 1
    while True:
        rejected_directory = output_directory / f"{benchmark_name}-attempt-{attempt:02d}"
        if not rejected_directory.exists():
            return rejected_directory
        attempt += 1


def archive_rejected_checkpoint(
    report_path: Path,
    output_directory: Path,
    rejection_reason: str,
) -> None:
    rejected_directory = next_rejected_capture_directory(
        output_directory,
        safe_name(report_path.stem),
    )
    rejected_directory.mkdir(parents=True, exist_ok=False)
    (rejected_directory / "reason.txt").write_text(
        rejection_reason + "\n",
        encoding="utf-8",
        newline="\n",
    )
    report_path.replace(rejected_directory / report_path.name)
    response_path = report_path.with_suffix(".rsp")
    if response_path.is_file():
        response_path.replace(rejected_directory / response_path.name)


def parse_arguments(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("review", "full"))
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--binary-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--compiler-id", required=True)
    parser.add_argument("--compiler-version", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=1_800)
    parser.add_argument("--resume", action="store_true")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    if arguments is None:
        arguments = sys.argv[1:]
    options = parse_arguments(arguments)
    engine_root = options.engine_root.resolve()
    binary_directory = options.binary_directory.resolve()
    output_directory = options.output_directory.resolve()
    build_root = (engine_root / "build").resolve()
    try:
        output_directory.relative_to(build_root)
    except ValueError:
        print(f"Benchmark output must remain beneath {build_root}.", file=sys.stderr)
        return 1
    if output_directory.exists() and any(output_directory.iterdir()) and not options.resume:
        print(f"Benchmark output directory is not empty: {output_directory}", file=sys.stderr)
        return 1
    output_directory.mkdir(parents=True, exist_ok=True)

    runner = binary_directory / "AzTestRunner.exe"
    if platform.system() != "Windows":
        runner = binary_directory / "AzTestRunner"
    if not runner.is_file():
        print(f"Benchmark runner does not exist: {runner}", file=sys.stderr)
        return 1

    scripts_directory = Path(__file__).resolve().parent
    affinity_policy = build_affinity_policy()
    affinity_description = describe_affinity_policy(affinity_policy)
    repetitions = REVIEW_REPETITION_COUNT
    minimum_time = REVIEW_MINIMUM_TIME_SECONDS
    workloads = review_workloads()
    providers = PROVIDERS[:1]
    if options.mode == "full":
        repetitions = FULL_REPETITION_COUNT
        minimum_time = FULL_MINIMUM_TIME_SECONDS
        workloads = matched_workloads()
        providers = PROVIDERS

    matched_minimum_time_policy = build_minimum_time_policy(workloads, minimum_time)

    qualified_reports = {}
    for provider in providers:
        module, runtime_dependency = resolve_provider_files(binary_directory, provider)
        qualified_report = output_directory / f"{provider.name}-matched-qualified.json"
        expected_names = tuple(f"{provider.name}/{workload.suffix}" for workload in workloads)
        maximum_cv = None
        maximum_attempts = 1
        if options.mode == "full" and provider.name == "Jolt":
            maximum_cv = FULL_MAXIMUM_CV
            maximum_attempts = FULL_MATCHED_CAPTURE_ATTEMPTS
        if options.resume and qualified_report.is_file():
            try:
                validate_reusable_report(
                    qualified_report,
                    engine_root,
                    runner,
                    provider,
                    module,
                    runtime_dependency,
                    options.compiler_id,
                    options.compiler_version,
                    affinity_description,
                    describe_workload_filter(provider.name, workloads),
                    minimum_time,
                    matched_minimum_time_policy,
                    repetitions,
                    expected_names,
                )
                validate_matched_report_quality(
                    qualified_report,
                    provider,
                    workloads,
                    repetitions,
                    maximum_cv,
                )
            except (OSError, ValueError) as error:
                archive_rejected_checkpoint(
                    qualified_report,
                    output_directory / "rejected-checkpoints",
                    str(error),
                )
                print(
                    f"Recapturing rejected matched {provider.name} benchmark evidence: {error}",
                    file=sys.stderr,
                )
            else:
                print(f"Reusing qualified matched {provider.name} benchmark evidence.", file=sys.stderr)
                qualified_reports[provider.name] = qualified_report
                continue

        raw_reports, warmup_reports = capture_matched_provider(
            provider,
            workloads,
            repetitions,
            minimum_time,
            runner,
            binary_directory,
            output_directory / "raw-matched",
            affinity_policy,
            options.timeout_seconds,
            maximum_cv,
            maximum_attempts,
        )
        prepare_artifact(
            scripts_directory,
            engine_root,
            runner,
            provider,
            module,
            runtime_dependency,
            raw_reports,
            warmup_reports,
            qualified_report,
            options.compiler_id,
            options.compiler_version,
            affinity_description,
            describe_workload_filter(provider.name, workloads),
            minimum_time,
            matched_minimum_time_policy,
            repetitions,
        )
        qualified_reports[provider.name] = qualified_report

    if options.mode == "full":
        subprocess.run(
            (
                sys.executable,
                str(scripts_directory / "compare_provider_benchmarks.py"),
                str(qualified_reports["Jolt"]),
                str(qualified_reports["Box3D"]),
                str(qualified_reports["PhysX"]),
                "--gate-provider",
                "PhysX",
                "--repetitions",
                str(repetitions),
                "--maximum-median-ratio",
                "1.0",
                "--maximum-bootstrap-ratio",
                "1.05",
                "--maximum-repetition-p95-ratio",
                "1.10",
                "--maximum-frame-tail-ratio",
                "1.10",
                "--maximum-cv",
                str(FULL_MAXIMUM_CV),
            ),
            cwd=engine_root,
            check=True,
        )

    jolt = PROVIDERS[0]
    jolt_module, jolt_runtime = resolve_provider_files(binary_directory, jolt)
    absolute_report = output_directory / "Jolt-absolute-qualified.json"
    absolute_names = tuple(f"Jolt/{suffix}" for suffix in ABSOLUTE_BENCHMARK_SUFFIXES)
    absolute_minimum_time_policy = build_minimum_time_policy((), minimum_time)
    if options.resume and absolute_report.is_file():
        try:
            validate_reusable_report(
                absolute_report,
                engine_root,
                runner,
                jolt,
                jolt_module,
                jolt_runtime,
                options.compiler_id,
                options.compiler_version,
                affinity_description,
                f"Jolt/{ABSOLUTE_FILTER_SUFFIX}",
                minimum_time,
                absolute_minimum_time_policy,
                repetitions,
                absolute_names,
            )
            if options.mode == "full":
                validate_absolute_report_quality(absolute_report, repetitions)
        except (OSError, ValueError) as error:
            archive_rejected_checkpoint(
                absolute_report,
                output_directory / "rejected-checkpoints",
                str(error),
            )
            print(
                f"Recapturing rejected absolute Jolt benchmark evidence: {error}",
                file=sys.stderr,
            )
        else:
            print("Reusing qualified absolute Jolt benchmark evidence.", file=sys.stderr)
            if options.mode == "review":
                validate_smoke_report(
                    qualified_reports["Jolt"],
                    repetitions,
                    tuple(f"Jolt/{workload.suffix}" for workload in workloads),
                )
                validate_smoke_report(
                    absolute_report,
                    repetitions,
                    absolute_names,
                )
            return 0

    if not absolute_report.is_file():
        absolute_raw, absolute_warmup = capture_absolute_jolt(
            jolt,
            repetitions,
            minimum_time,
            runner,
            binary_directory,
            output_directory / "raw-absolute",
            affinity_policy,
            options.timeout_seconds,
            FULL_MAXIMUM_CV if options.mode == "full" else None,
            FULL_ABSOLUTE_CAPTURE_ATTEMPTS if options.mode == "full" else 1,
        )
        prepare_artifact(
            scripts_directory,
            engine_root,
            runner,
            jolt,
            jolt_module,
            jolt_runtime,
            absolute_raw,
            absolute_warmup,
            absolute_report,
            options.compiler_id,
            options.compiler_version,
            affinity_description,
            f"Jolt/{ABSOLUTE_FILTER_SUFFIX}",
            minimum_time,
            absolute_minimum_time_policy,
            repetitions,
        )

    if options.mode == "review":
        validate_smoke_report(
            qualified_reports["Jolt"],
            repetitions,
            tuple(f"Jolt/{workload.suffix}" for workload in workloads),
        )
        validate_smoke_report(
            absolute_report,
            repetitions,
            absolute_names,
        )
        return 0
    subprocess.run(
        (
            sys.executable,
            str(scripts_directory / "validate_jolt_benchmarks.py"),
            str(absolute_report),
            "--repetitions",
            str(repetitions),
            "--maximum-capability-nanoseconds",
            str(FULL_MAXIMUM_CAPABILITY_NANOSECONDS),
            "--maximum-capability-operation-ratio",
            str(FULL_MAXIMUM_CAPABILITY_OPERATION_RATIO),
            "--maximum-cv",
            str(FULL_MAXIMUM_CV),
        ),
        cwd=engine_root,
        check=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
