#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import json
import math
import statistics
import sys
from pathlib import Path

import compare_provider_benchmarks


CAPABILITY_BENCHMARK = "Jolt/Diagnostic/AcquireRuntimeConfigurationCapability/real_time"
REPRESENTATIVE_OPERATION_BENCHMARKS = {
    "Jolt/Diagnostic/RaycastEmptyWorld/128/real_time": 128,
    "Jolt/Query/OverlapSphereGrid/1024/1/1/real_time": 1,
    "Jolt/Query/RaycastGrid/1024/128/1/real_time": 128,
}
ROLLBACK_BENCHMARK_PREFIXES = (
    "Jolt/Rollback/RecaptureFilteredState/",
    "Jolt/Rollback/RestoreFilteredStateTransactional/",
    "Jolt/Rollback/RestoreFilteredStateValidated/",
)


def load_raw_samples(
    report: dict,
    name: str,
    repetitions: int,
) -> tuple[list[float], list[dict]]:
    samples = []
    results = []
    repetition_indices = set()
    for result in report.get("benchmarks", []):
        if result.get("name") != name or result.get("run_type", "iteration") != "iteration":
            continue
        repetition_index = result.get("repetition_index")
        if not isinstance(repetition_index, int) or repetition_index in repetition_indices:
            raise ValueError(f"{name} has a missing or duplicate repetition index.")
        repetition_indices.add(repetition_index)
        time_unit = result.get("time_unit")
        if time_unit not in compare_provider_benchmarks.TIME_UNIT_TO_MICROSECONDS:
            raise ValueError(f"{name} has an unsupported time unit {time_unit!r}.")
        real_time = float(result.get("real_time", math.nan))
        if not math.isfinite(real_time) or real_time <= 0.0:
            raise ValueError(f"{name} has an invalid real time {real_time!r}.")
        samples.append(real_time * compare_provider_benchmarks.TIME_UNIT_TO_MICROSECONDS[time_unit])
        results.append(result)
    if len(samples) != repetitions or repetition_indices != set(range(repetitions)):
        raise ValueError(f"{name} does not contain exactly {repetitions} indexed raw repetitions.")
    return samples, results


def validate_report(
    report: dict,
    repetitions: int,
    maximum_capability_nanoseconds: float,
    maximum_capability_operation_ratio: float,
    maximum_cv: float,
) -> list[str]:
    errors = []
    context = report.get("context", {})
    if context.get("library_build_type") != "release":
        errors.append("The Jolt benchmark report is not from a Release build.")
    qualification = report.get("qualification", {})
    if qualification.get("provider") != "Jolt":
        errors.append("The benchmark qualification metadata does not identify Jolt.")
    if qualification.get("repetitions") != repetitions or qualification.get("raw_samples") is not True:
        errors.append("The benchmark qualification metadata does not describe the required raw repetitions.")

    try:
        capability_samples, capability_results = load_raw_samples(report, CAPABILITY_BENCHMARK, repetitions)
        for result in capability_results:
            if round(float(result.get("QualityValid", 0))) != 1:
                raise ValueError("Capability acquisition failed its correctness counter.")
        capability_nanoseconds = statistics.median(capability_samples) * 1_000.0
        capability_cv = compare_provider_benchmarks.coefficient_of_variation(capability_samples)
        if capability_cv > maximum_cv:
            errors.append(
                f"Capability acquisition CV {capability_cv:.3%} exceeds {maximum_cv:.3%}."
            )

        representative_operation_nanoseconds = []
        for name, operation_count in REPRESENTATIVE_OPERATION_BENCHMARKS.items():
            samples, _ = load_raw_samples(report, name, repetitions)
            representative_operation_nanoseconds.append(statistics.median(samples) * 1_000.0 / operation_count)
        cheapest_operation_nanoseconds = min(representative_operation_nanoseconds)
        capability_operation_ratio = capability_nanoseconds / cheapest_operation_nanoseconds
        if (
            capability_nanoseconds > maximum_capability_nanoseconds
            and capability_operation_ratio > maximum_capability_operation_ratio
        ):
            errors.append(
                f"Capability acquisition is {capability_nanoseconds:.3f} ns and "
                f"{capability_operation_ratio:.3%} of the cheapest representative operation."
            )
    except ValueError as error:
        errors.append(str(error))

    rollback_names = {
        result.get("name")
        for result in report.get("benchmarks", [])
        if any(result.get("name", "").startswith(prefix) for prefix in ROLLBACK_BENCHMARK_PREFIXES)
        and result.get("run_type", "iteration") == "iteration"
    }
    if len(rollback_names) != 6:
        errors.append(f"The report contains {len(rollback_names)} rollback workloads; expected 6.")
    for name in sorted(rollback_names):
        try:
            samples, results = load_raw_samples(report, name, repetitions)
            if compare_provider_benchmarks.coefficient_of_variation(samples) > maximum_cv:
                errors.append(f"{name} exceeds the {maximum_cv:.3%} CV gate.")
            for result in results:
                for counter in (
                    "NativeAllocationCount",
                    "NativeAllocatedGrowthBytes",
                    "NativeFreeCount",
                    "NativeReallocationCount",
                    "SnapshotFailures",
                    "TempAllocatorCapacityGrowthBytes",
                    "WrapperRetainedGrowthBytes",
                ):
                    if round(float(result.get(counter, -1))) != 0:
                        raise ValueError(f"{name} reports nonzero {counter}.")
                if round(float(result.get("QualityValid", 0))) != 1:
                    raise ValueError(f"{name} failed its correctness gate.")
        except ValueError as error:
            errors.append(str(error))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate absolute Jolt Release benchmark gates.")
    parser.add_argument("report", type=Path)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--maximum-capability-nanoseconds", type=float, default=3.0)
    parser.add_argument("--maximum-capability-operation-ratio", type=float, default=0.02)
    parser.add_argument("--maximum-cv", type=float, default=0.05)
    arguments = parser.parse_args()

    with arguments.report.open(encoding="utf-8") as report_file:
        report = json.load(report_file)
    errors = validate_report(
        report,
        arguments.repetitions,
        arguments.maximum_capability_nanoseconds,
        arguments.maximum_capability_operation_ratio,
        arguments.maximum_cv,
    )
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
