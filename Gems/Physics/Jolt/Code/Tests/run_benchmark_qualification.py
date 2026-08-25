#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Capture and qualify process-isolated Jolt provider benchmarks."""

import argparse
import ctypes
import json
import math
import os
import platform
import re
import subprocess
import sys
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Sequence

import compare_provider_benchmarks


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
        )
        for workload in (*compare_provider_benchmarks.WORKLOADS, *compare_provider_benchmarks.TAIL_WORKLOADS)
    )


def review_workloads() -> tuple[Workload, ...]:
    workloads = matched_workloads()
    return workloads[0], workloads[8], workloads[12]


def describe_workload_filter(provider: str, workloads: Sequence[Workload]) -> str:
    return f"{provider}/({'|'.join(workload.suffix for workload in workloads)})"


def select_spread_processors(processors: Sequence[int], count: int) -> tuple[int, ...]:
    if count <= 0:
        raise ValueError("The requested processor count must be positive.")
    if len(processors) < count:
        raise ValueError(f"Only {len(processors)} physical processors are available; {count} are required.")

    selected = tuple(processors[index * len(processors) // count] for index in range(count))
    if len(set(selected)) != count:
        raise ValueError("The physical processor selection contains duplicates.")
    return selected


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
                    processors.append((available_mask & -available_mask).bit_length() - 1)
        offset += record_size

    if not processors:
        raise ValueError("No physical processors are available in Windows processor group zero.")
    return tuple(processors)


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
        worker_count: select_spread_processors(available_processors, worker_count)
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
    return "automatic physical-core lanes: " + "; ".join(selections) + f"; inherited affinity; {priority_policy}"


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
    if not any(
        result.get("run_type", "iteration") == "iteration"
        for result in report.get("benchmarks", [])
    ):
        raise RuntimeError(f"{provider} benchmark filter matched no workloads: {benchmark_filter}")


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
) -> tuple[list[Path], list[Path]]:
    module, _ = resolve_provider_files(binary_directory, provider)
    raw_reports = []
    warmup_reports = []
    for workload in workloads:
        workload_name = safe_name(workload.label)
        warmup_path = output_directory / provider.name / f"{workload_name}-warmup.json"
        run_benchmark_process(
            runner,
            module,
            provider.name,
            workload.suffix,
            minimum_time,
            warmup_path,
            affinity_policy[workload.worker_count],
            timeout_seconds,
        )
        warmup_reports.append(warmup_path)
        for repetition in range(repetitions):
            raw_path = output_directory / provider.name / f"{workload_name}-{repetition:02d}.json"
            run_benchmark_process(
                runner,
                module,
                provider.name,
                workload.suffix,
                minimum_time,
                raw_path,
                affinity_policy[workload.worker_count],
                timeout_seconds,
            )
            raw_reports.append(raw_path)
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
) -> tuple[list[Path], list[Path]]:
    module, _ = resolve_provider_files(binary_directory, provider)
    warmup_path = output_directory / provider.name / "absolute-warmup.json"
    run_benchmark_process(
        runner,
        module,
        provider.name,
        ABSOLUTE_FILTER_SUFFIX,
        minimum_time,
        warmup_path,
        affinity_policy[1],
        timeout_seconds,
    )
    raw_reports = []
    for repetition in range(repetitions):
        raw_path = output_directory / provider.name / f"absolute-{repetition:02d}.json"
        run_benchmark_process(
            runner,
            module,
            provider.name,
            ABSOLUTE_FILTER_SUFFIX,
            minimum_time,
            raw_path,
            affinity_policy[1],
            timeout_seconds,
        )
        raw_reports.append(raw_path)
    return raw_reports, [warmup_path]


def parse_arguments(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("review", "full"))
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--binary-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--compiler-id", required=True)
    parser.add_argument("--compiler-version", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=1_800)
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
    if output_directory.exists() and any(output_directory.iterdir()):
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
    minimum_time = 0.05
    workloads = review_workloads()
    providers = PROVIDERS[:1]
    if options.mode == "full":
        repetitions = FULL_REPETITION_COUNT
        minimum_time = 0.5
        workloads = matched_workloads()
        providers = PROVIDERS

    qualified_reports = {}
    for provider in providers:
        module, runtime_dependency = resolve_provider_files(binary_directory, provider)
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
        )
        qualified_report = output_directory / f"{provider.name}-matched-qualified.json"
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
            repetitions,
        )
        qualified_reports[provider.name] = qualified_report

    jolt = PROVIDERS[0]
    jolt_module, jolt_runtime = resolve_provider_files(binary_directory, jolt)
    absolute_raw, absolute_warmup = capture_absolute_jolt(
        jolt,
        repetitions,
        minimum_time,
        runner,
        binary_directory,
        output_directory / "raw-absolute",
        affinity_policy,
        options.timeout_seconds,
    )
    absolute_report = output_directory / "Jolt-absolute-qualified.json"
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
            tuple(f"Jolt/{suffix}" for suffix in ABSOLUTE_BENCHMARK_SUFFIXES),
        )
        return 0

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
            "0.05",
        ),
        cwd=engine_root,
        check=True,
    )
    subprocess.run(
        (
            sys.executable,
            str(scripts_directory / "validate_jolt_benchmarks.py"),
            str(absolute_report),
            "--repetitions",
            str(repetitions),
            "--maximum-capability-nanoseconds",
            "3.0",
            "--maximum-capability-operation-ratio",
            "0.02",
            "--maximum-cv",
            "0.05",
        ),
        cwd=engine_root,
        check=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
