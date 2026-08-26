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
import random
import statistics
import sys
from pathlib import Path


PROVIDERS = ("Jolt", "Box3D", "PhysX")
WORKLOAD_SCHEMA_VERSION = 6
TAIL_SAMPLE_COUNT = 8192
TIME_UNIT_TO_MICROSECONDS = {
    "ns": 0.001,
    "us": 1.0,
    "ms": 1_000.0,
    "s": 1_000_000.0,
}
JOLT_MATCHED_STEP_POLICY = {
    "PenetrationSlopMillimeters": 1,
    "PositionSteps": 4,
    "VelocitySteps": 2,
}

WORKLOADS = (
    {
        "label": "Step 128/1",
        "suffix": "Step/SettledBoxes/128/1/iterations:600/real_time",
        "exact": {
            "AngularDamping": 1,
            "Ccd": 0,
            "DynamicBodies": 128,
            "Friction": 0,
            "Layers": 1,
            "LinearDamping": 1,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "GridSpacingMillimeters": 1100,
            "RowWidth": 16,
            "Restitution": 0,
            "ValidationFrames": 600,
            "WarmupCompleted": 1,
            "Workers": 1,
        },
        "minimum": {"MinimumHeight": 0.45, "WarmupTicks": 600},
        "maximum": {"MaximumDisplacement": 0.02},
        "provider_exact": {
            "Jolt": {
                **JOLT_MATCHED_STEP_POLICY,
                "LargeIslandSplitter": 0,
            },
        },
        "iterations": 600,
    },
    {
        "label": "Step 128/4",
        "suffix": "Step/SettledBoxes/128/4/iterations:600/real_time",
        "exact": {
            "AngularDamping": 1,
            "Ccd": 0,
            "DynamicBodies": 128,
            "Friction": 0,
            "Layers": 1,
            "LinearDamping": 1,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "GridSpacingMillimeters": 1100,
            "RowWidth": 16,
            "Restitution": 0,
            "ValidationFrames": 600,
            "WarmupCompleted": 1,
            "Workers": 4,
        },
        "minimum": {"MinimumHeight": 0.45, "WarmupTicks": 600},
        "maximum": {"MaximumDisplacement": 0.02},
        "provider_exact": {
            "Jolt": {
                **JOLT_MATCHED_STEP_POLICY,
                "LargeIslandSplitter": 1,
            },
        },
        "iterations": 600,
    },
    {
        "label": "Step 128/8",
        "suffix": "Step/SettledBoxes/128/8/iterations:600/real_time",
        "exact": {
            "AngularDamping": 1,
            "Ccd": 0,
            "DynamicBodies": 128,
            "Friction": 0,
            "Layers": 1,
            "LinearDamping": 1,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "GridSpacingMillimeters": 1100,
            "RowWidth": 16,
            "Restitution": 0,
            "ValidationFrames": 600,
            "WarmupCompleted": 1,
            "Workers": 8,
        },
        "minimum": {"MinimumHeight": 0.45, "WarmupTicks": 600},
        "maximum": {"MaximumDisplacement": 0.02},
        "provider_exact": {
            "Jolt": {
                **JOLT_MATCHED_STEP_POLICY,
                "LargeIslandSplitter": 1,
            },
        },
        "iterations": 600,
    },
    {
        "label": "Step 1024/1",
        "suffix": "Step/SettledBoxes/1024/1/iterations:600/real_time",
        "exact": {
            "AngularDamping": 1,
            "Ccd": 0,
            "DynamicBodies": 1024,
            "Friction": 0,
            "Layers": 1,
            "LinearDamping": 1,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "GridSpacingMillimeters": 1100,
            "RowWidth": 32,
            "Restitution": 0,
            "ValidationFrames": 600,
            "WarmupCompleted": 1,
            "Workers": 1,
        },
        "minimum": {"MinimumHeight": 0.45, "WarmupTicks": 600},
        "maximum": {"MaximumDisplacement": 0.02},
        "provider_exact": {
            "Jolt": {
                **JOLT_MATCHED_STEP_POLICY,
                "LargeIslandSplitter": 0,
            },
        },
        "iterations": 600,
    },
    {
        "label": "Step 1024/4",
        "suffix": "Step/SettledBoxes/1024/4/iterations:600/real_time",
        "exact": {
            "AngularDamping": 1,
            "Ccd": 0,
            "DynamicBodies": 1024,
            "Friction": 0,
            "Layers": 1,
            "LinearDamping": 1,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "GridSpacingMillimeters": 1100,
            "RowWidth": 32,
            "Restitution": 0,
            "ValidationFrames": 600,
            "WarmupCompleted": 1,
            "Workers": 4,
        },
        "minimum": {"MinimumHeight": 0.45, "WarmupTicks": 600},
        "maximum": {"MaximumDisplacement": 0.02},
        "provider_exact": {
            "Jolt": {
                **JOLT_MATCHED_STEP_POLICY,
                "LargeIslandSplitter": 1,
            },
        },
        "iterations": 600,
    },
    {
        "label": "Step 1024/8",
        "suffix": "Step/SettledBoxes/1024/8/iterations:600/real_time",
        "exact": {
            "AngularDamping": 1,
            "Ccd": 0,
            "DynamicBodies": 1024,
            "Friction": 0,
            "Layers": 1,
            "LinearDamping": 1,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "GridSpacingMillimeters": 1100,
            "RowWidth": 32,
            "Restitution": 0,
            "ValidationFrames": 600,
            "WarmupCompleted": 1,
            "Workers": 8,
        },
        "minimum": {"MinimumHeight": 0.45, "WarmupTicks": 600},
        "maximum": {"MaximumDisplacement": 0.02},
        "provider_exact": {
            "Jolt": {
                **JOLT_MATCHED_STEP_POLICY,
                "LargeIslandSplitter": 1,
            },
        },
        "iterations": 600,
    },
    {
        "label": "Lifecycle 128",
        "suffix": "Lifecycle/CreateDestroyBodies/128/1/real_time",
        "exact": {
            "DynamicBodies": 128,
            "InstrumentationEnabled": 0,
            "Notifications": 0,
            "Workers": 1,
        },
    },
    {
        "label": "Lifecycle 1024",
        "suffix": "Lifecycle/CreateDestroyBodies/1024/1/real_time",
        "exact": {
            "DynamicBodies": 1024,
            "InstrumentationEnabled": 0,
            "Notifications": 0,
            "Workers": 1,
        },
    },
    {
        "label": "Raycast 1024/128",
        "suffix": "Query/RaycastGrid/1024/128/1/real_time",
        "exact": {"Obstacles": 1024, "QualityValid": 1, "Workers": 1},
        "completion_counter": "SuccessfulQueries",
        "operations_per_iteration": 128,
        "ratio_gate": False,
        "ratio_gate_reason": (
            "Jolt preserves canonical tie ordering, deterministic floating-point state, "
            "and double-precision world positions."
        ),
    },
    {
        "label": "Batch raycast 1024/128",
        "suffix": "Query/RaycastClosestBatchGrid/1024/128/4/real_time",
        "exact": {
            "Obstacles": 1024,
            "QualityValid": 1,
            "WarmupCompleted": 1,
            "WarmupMs": 100,
            "Workers": 4,
        },
        "completion_counter": "CompleteOperations",
        "operations_per_iteration": 1,
    },
    {
        "label": "Batch raycast 1024/1024",
        "suffix": "Query/RaycastClosestBatchGrid/1024/1024/4/real_time",
        "exact": {
            "Obstacles": 1024,
            "QualityValid": 1,
            "WarmupCompleted": 1,
            "WarmupMs": 100,
            "Workers": 4,
        },
        "completion_counter": "CompleteOperations",
        "operations_per_iteration": 1,
    },
    {
        "label": "Sphere overlap 1024/1",
        "suffix": "Query/OverlapSphereGrid/1024/1/1/real_time",
        "exact": {
            "ActualHits": 25,
            "ExpectedHits": 25,
            "Obstacles": 1024,
            "QualityValid": 1,
            "Workers": 1,
        },
        "completion_counter": "CompleteOperations",
        "operations_per_iteration": 1,
    },
)


def make_tail_workload(workload: dict) -> dict:
    exact = dict(workload["exact"])
    exact["TailSampleCount"] = TAIL_SAMPLE_COUNT
    exact["ValidationFrames"] = TAIL_SAMPLE_COUNT
    return {
        "label": f"{workload['label']} frame tail",
        "suffix": workload["suffix"]
            .replace("Step/SettledBoxes", "Tail/Step/SettledBoxes")
            .replace("iterations:600/real_time", "iterations:1/manual_time"),
        "exact": exact,
        "minimum": dict(workload.get("minimum", {})),
        "maximum": dict(workload.get("maximum", {})),
        "provider_exact": dict(workload.get("provider_exact", {})),
        "iterations": 1,
    }


TAIL_WORKLOADS = tuple(make_tail_workload(workload) for workload in WORKLOADS[:6])


def workload_signature() -> str:
    encoded_workloads = json.dumps(
        {
            "schema_version": WORKLOAD_SCHEMA_VERSION,
            "tail_workloads": TAIL_WORKLOADS,
            "workloads": WORKLOADS,
        },
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(encoded_workloads).hexdigest()


def load_report(path: Path) -> dict:
    with path.open(encoding="utf-8") as report_file:
        return json.load(report_file)


def validate_runtime_dependencies(metadata: dict, provider: str) -> None:
    runtime_dependencies = metadata.get("runtime_dependencies")
    if not isinstance(runtime_dependencies, list):
        raise ValueError(f"{provider} benchmark report is missing runtime dependency fingerprints.")
    if provider in ("Jolt", "Box3D") and not runtime_dependencies:
        raise ValueError(f"{provider} benchmark report has no runtime dependency fingerprint.")
    for dependency in runtime_dependencies:
        if (
            not isinstance(dependency, dict)
            or not dependency.get("path")
            or not dependency.get("sha256")
            or not isinstance(dependency.get("mtime_ns"), int)
            or dependency["mtime_ns"] <= 0
        ):
            raise ValueError(f"{provider} benchmark report has an invalid runtime dependency fingerprint.")


def validate_context(reports: dict[str, dict]) -> None:
    reference = reports["Jolt"].get("context", {})
    if reference.get("library_build_type") != "release":
        raise ValueError("Jolt benchmark report was not produced by a Release build.")

    for provider, report in reports.items():
        context = report.get("context", {})
        if context.get("library_build_type") != "release":
            raise ValueError(f"{provider} benchmark report was not produced by a Release build.")
        for field in ("host_name", "num_cpus", "caches"):
            if context.get(field) != reference.get(field):
                raise ValueError(
                    f"Benchmark context differs for {field}: "
                    f"Jolt={reference.get(field)!r}, {provider}={context.get(field)!r}"
                )

        reference_frequency = float(reference.get("mhz_per_cpu", 0.0))
        provider_frequency = float(context.get("mhz_per_cpu", 0.0))
        if reference_frequency <= 0.0 or provider_frequency <= 0.0:
            raise ValueError(f"{provider} benchmark report has no valid CPU frequency.")
        frequency_ratio = provider_frequency / reference_frequency
        if frequency_ratio < 0.99 or frequency_ratio > 1.01:
            raise ValueError(
                f"Benchmark CPU frequency differs by more than one percent: "
                f"Jolt={reference_frequency}, {provider}={provider_frequency}"
            )

    required_metadata = (
        "binary_sha256",
        "benchmark_filter",
        "build_configuration",
        "compiler_id",
        "compiler_version",
        "cpu_affinity_policy",
        "minimum_time",
        "provider",
        "raw_samples",
        "repetitions",
        "runner_sha256",
        "source_revision",
        "source_state_sha256",
        "workload_signature",
    )
    reference_metadata = reports["Jolt"].get("qualification", {})
    for provider, report in reports.items():
        metadata = report.get("qualification", {})
        for field in required_metadata:
            if not metadata.get(field):
                raise ValueError(f"{provider} benchmark report is missing qualification field {field}.")
        validate_runtime_dependencies(metadata, provider)
        if metadata["build_configuration"].lower() != "release":
            raise ValueError(f"{provider} qualification metadata is not for a Release build.")
        if metadata["workload_signature"] != workload_signature():
            raise ValueError(f"{provider} benchmark workload signature is stale or invalid.")
        for field in (
            "build_configuration",
            "compiler_id",
            "compiler_version",
            "cpu_affinity_policy",
            "minimum_time",
            "raw_samples",
            "repetitions",
            "runner_sha256",
            "source_revision",
            "source_state_sha256",
            "workload_signature",
        ):
            if metadata[field] != reference_metadata.get(field):
                raise ValueError(
                    f"Benchmark qualification differs for {field}: "
                    f"Jolt={reference_metadata.get(field)!r}, {provider}={metadata[field]!r}"
                )

    binary_hashes = [
        report["qualification"]["binary_sha256"]
        for report in reports.values()
    ]
    if len(set(binary_hashes)) != len(binary_hashes):
        raise ValueError("Provider benchmark reports contain duplicate binary fingerprints.")


def load_samples(
    report: dict,
    provider: str,
    name: str,
    workload: dict,
    expected_repetitions: int,
) -> tuple[list[float], int]:
    samples = []
    affinity_processor_counts = set()
    repetition_indices = set()
    for result in report.get("benchmarks", []):
        if result.get("name") != name or result.get("run_type", "iteration") != "iteration":
            continue

        repetition_index = result.get("repetition_index")
        if (
            not isinstance(repetition_index, int)
            or repetition_index < 0
            or repetition_index >= expected_repetitions
        ):
            raise ValueError(f"{name} has invalid repetition index {repetition_index!r}.")
        if repetition_index in repetition_indices:
            raise ValueError(f"{name} has duplicate repetition index {repetition_index}.")
        repetition_indices.add(repetition_index)

        exact_counters = dict(workload.get("exact", {}))
        exact_counters.update(workload.get("provider_exact", {}).get(provider, {}))
        if round(float(result.get("AffinityConstrained", 0))) != 1:
            raise ValueError(
                f"{name} was not constrained to the benchmark CPU topology."
            )
        for counter, expected in exact_counters.items():
            if counter not in result or round(float(result[counter])) != expected:
                raise ValueError(
                    f"{name} has invalid {counter}: expected {expected}, "
                    f"found {result.get(counter, 'missing')}"
                )
        worker_count = round(float(result.get("Workers", 0)))
        actual_affinity_processors = round(float(result.get("AffinityProcessors", 0)))
        if actual_affinity_processors != worker_count:
            raise ValueError(
                f"{name} has invalid AffinityProcessors: expected {worker_count}, "
                f"found {actual_affinity_processors}."
            )
        affinity_processor_counts.add(actual_affinity_processors)
        expected_background_workers = max(0, worker_count - 1)
        expected_caller_participation = 1
        if round(float(result.get("BackgroundWorkers", -1))) != expected_background_workers:
            raise ValueError(
                f"{name} has invalid BackgroundWorkers: expected "
                f"{expected_background_workers}, found {result.get('BackgroundWorkers', 'missing')}"
            )
        if round(float(result.get("CallerParticipates", -1))) != expected_caller_participation:
            raise ValueError(
                f"{name} has invalid CallerParticipates: expected "
                f"{expected_caller_participation}, found {result.get('CallerParticipates', 'missing')}"
            )
        for counter, minimum in workload.get("minimum", {}).items():
            if counter not in result or float(result[counter]) < minimum:
                raise ValueError(
                    f"{name} has invalid {counter}: expected at least {minimum}, "
                    f"found {result.get(counter, 'missing')}"
                )
        for counter, maximum in workload.get("maximum", {}).items():
            if counter not in result or float(result[counter]) > maximum:
                raise ValueError(
                    f"{name} has invalid {counter}: expected at most {maximum}, "
                    f"found {result.get(counter, 'missing')}"
                )
        if "iterations" in workload and int(result.get("iterations", 0)) != workload["iterations"]:
            raise ValueError(
                f"{name} has invalid iterations: expected {workload['iterations']}, "
                f"found {result.get('iterations', 'missing')}"
            )
        if completion_counter := workload.get("completion_counter"):
            expected_completion_count = (
                int(result.get("iterations", 0)) * workload["operations_per_iteration"]
            )
            if round(float(result.get(completion_counter, -1))) != expected_completion_count:
                raise ValueError(
                    f"{name} has invalid {completion_counter}: expected "
                    f"{expected_completion_count}, found {result.get(completion_counter, 'missing')}"
                )

        time_unit = result.get("time_unit")
        if time_unit not in TIME_UNIT_TO_MICROSECONDS:
            raise ValueError(f"{name} has unsupported time unit {time_unit!r}.")
        real_time = float(result.get("real_time", math.nan))
        if not math.isfinite(real_time) or real_time <= 0.0:
            raise ValueError(f"{name} has invalid real time {real_time!r}.")
        samples.append(real_time * TIME_UNIT_TO_MICROSECONDS[time_unit])

    if len(samples) != expected_repetitions:
        raise ValueError(
            f"{name} has {len(samples)} raw repetitions; expected exactly {expected_repetitions}."
        )
    if len(affinity_processor_counts) != 1:
        raise ValueError(
            f"{name} used inconsistent benchmark processor counts: "
            f"{sorted(affinity_processor_counts)}"
        )
    return samples, affinity_processor_counts.pop()


def percentile(samples: list[float], probability: float) -> float:
    ordered = sorted(samples)
    index = max(0, math.ceil(probability * len(ordered)) - 1)
    return ordered[index]


def load_tail_samples(
    report: dict,
    provider: str,
    name: str,
    workload: dict,
    expected_repetitions: int,
) -> tuple[list[float], list[float], int]:
    reported_p95_samples, affinity_processor_count = load_samples(
        report,
        provider,
        name,
        workload,
        expected_repetitions,
    )
    raw_frame_samples = []
    p95_by_repetition = {}
    for result in report.get("benchmarks", []):
        if result.get("name") != name or result.get("run_type", "iteration") != "iteration":
            continue

        repetition_index = result["repetition_index"]
        repetition_samples = []
        for sample_index in range(TAIL_SAMPLE_COUNT):
            counter = f"Frame{sample_index}Ns"
            sample_nanoseconds = float(result.get(counter, math.nan))
            if not math.isfinite(sample_nanoseconds) or sample_nanoseconds <= 0.0:
                raise ValueError(
                    f"{name} repetition {repetition_index} has invalid raw frame "
                    f"sample {counter}: {result.get(counter, 'missing')}"
                )
            repetition_samples.append(sample_nanoseconds * 0.001)

        expected_p95 = percentile(repetition_samples, 0.95)
        reported_tail_p95 = float(result.get("TailP95Ns", math.nan)) * 0.001
        if not math.isclose(reported_tail_p95, expected_p95, rel_tol=1.0e-9):
            raise ValueError(
                f"{name} repetition {repetition_index} reported TailP95Ns "
                f"{reported_tail_p95} us but raw samples produce {expected_p95} us."
            )

        time_unit = result["time_unit"]
        reported_real_time = float(result["real_time"]) * TIME_UNIT_TO_MICROSECONDS[time_unit]
        if not math.isclose(reported_real_time, expected_p95, rel_tol=1.0e-6):
            raise ValueError(
                f"{name} repetition {repetition_index} reported real time "
                f"{reported_real_time} us but raw samples produce p95 {expected_p95} us."
            )

        raw_frame_samples.extend(repetition_samples)
        p95_by_repetition[repetition_index] = expected_p95

    if len(raw_frame_samples) != expected_repetitions * TAIL_SAMPLE_COUNT:
        raise ValueError(
            f"{name} has {len(raw_frame_samples)} raw frame samples; expected "
            f"{expected_repetitions * TAIL_SAMPLE_COUNT}."
        )

    ordered_p95_samples = [
        p95_by_repetition[repetition_index]
        for repetition_index in range(expected_repetitions)
    ]
    for reported, reconstructed in zip(reported_p95_samples, ordered_p95_samples):
        if not math.isclose(reported, reconstructed, rel_tol=1.0e-6):
            raise ValueError(
                f"{name} reported p95 batch time {reported} us but raw samples "
                f"produce {reconstructed} us."
            )

    return raw_frame_samples, ordered_p95_samples, affinity_processor_count


def coefficient_of_variation(samples: list[float]) -> float:
    mean = statistics.mean(samples)
    if len(samples) < 2 or mean <= 0.0:
        return 0.0
    return statistics.stdev(samples) / mean


def bootstrap_median_ratio_interval(
    candidate: list[float],
    reference: list[float],
    seed: int,
    sample_count: int = 5_000,
) -> tuple[float, float]:
    generator = random.Random(seed)
    ratios = []
    for _ in range(sample_count):
        candidate_sample = generator.choices(candidate, k=len(candidate))
        reference_sample = generator.choices(reference, k=len(reference))
        ratios.append(statistics.median(candidate_sample) / statistics.median(reference_sample))
    return percentile(ratios, 0.025), percentile(ratios, 0.975)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare matched Jolt, Box3D, and PhysX Release benchmark reports."
    )
    parser.add_argument("jolt_results", type=Path)
    parser.add_argument("box3d_results", type=Path)
    parser.add_argument("physx_results", type=Path)
    parser.add_argument("--gate-provider", choices=("Box3D", "PhysX"), default="PhysX")
    parser.add_argument("--maximum-median-ratio", type=float, default=1.0)
    parser.add_argument("--maximum-bootstrap-ratio", type=float, default=1.05)
    parser.add_argument("--maximum-repetition-p95-ratio", type=float, default=1.10)
    parser.add_argument("--maximum-frame-tail-ratio", type=float, default=1.10)
    parser.add_argument("--maximum-cv", type=float, default=0.05)
    parser.add_argument("--repetitions", type=int, default=30)
    arguments = parser.parse_args()

    reports = {
        "Jolt": load_report(arguments.jolt_results),
        "Box3D": load_report(arguments.box3d_results),
        "PhysX": load_report(arguments.physx_results),
    }
    try:
        validate_context(reports)
        for provider, report in reports.items():
            metadata = report["qualification"]
            if metadata["provider"] != provider:
                raise ValueError(
                    f"{provider} benchmark metadata identifies provider {metadata['provider']!r}."
                )
            if metadata["repetitions"] != arguments.repetitions:
                raise ValueError(
                    f"{provider} benchmark metadata has {metadata['repetitions']} repetitions; "
                    f"expected {arguments.repetitions}."
                )
            if metadata["raw_samples"] is not True:
                raise ValueError(f"{provider} benchmark report does not contain raw samples.")
            if float(metadata["minimum_time"]) <= 0.0:
                raise ValueError(f"{provider} benchmark report has an invalid minimum time.")
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    failed = False
    print("Workload                      Jolt (us)  Box3D (us)  PhysX (us)  J/B    J/P")
    for workload_index, workload in enumerate(WORKLOADS):
        provider_samples = {}
        provider_affinity_processor_counts = {}
        try:
            for provider in PROVIDERS:
                name = f"{provider}/{workload['suffix']}"
                provider_samples[provider], provider_affinity_processor_counts[provider] = load_samples(
                    reports[provider],
                    provider,
                    name,
                    workload,
                    arguments.repetitions,
                )
            if len(set(provider_affinity_processor_counts.values())) != 1:
                raise ValueError(
                    f"{workload['label']} used different benchmark processor counts: "
                    f"{provider_affinity_processor_counts}"
                )
        except ValueError as error:
            print(error, file=sys.stderr)
            failed = True
            continue

        medians = {
            provider: statistics.median(provider_samples[provider])
            for provider in PROVIDERS
        }
        jolt_to_box3d = medians["Jolt"] / medians["Box3D"]
        jolt_to_physx = medians["Jolt"] / medians["PhysX"]
        print(
            f"{workload['label']:<29} {medians['Jolt']:>9.2f}  "
            f"{medians['Box3D']:>10.2f}  {medians['PhysX']:>9.2f}  "
            f"{jolt_to_box3d:>5.3f}  {jolt_to_physx:>5.3f}"
        )

        for provider in PROVIDERS:
            cv = coefficient_of_variation(provider_samples[provider])
            if cv <= arguments.maximum_cv:
                continue
            if provider == "Jolt":
                print(
                    f"  {workload['label']} {provider} CV {cv:.3%} exceeds "
                    f"{arguments.maximum_cv:.3%}.",
                    file=sys.stderr,
                )
                failed = True
                continue
            print(
                f"  Diagnostic: {workload['label']} {provider} CV {cv:.3%} exceeds "
                f"{arguments.maximum_cv:.3%}."
            )

        if not workload.get("ratio_gate", True):
            print(f"  Contextual comparison: {workload['ratio_gate_reason']}")
            continue

        reference_samples = provider_samples[arguments.gate_provider]
        median_ratio = medians["Jolt"] / medians[arguments.gate_provider]
        repetition_p95_ratio = (
            percentile(provider_samples["Jolt"], 0.95)
            / percentile(reference_samples, 0.95)
        )
        conservative_repetition_ratio = (
            percentile(provider_samples["Jolt"], 0.95)
            / percentile(reference_samples, 0.05)
        )
        _, bootstrap_upper = bootstrap_median_ratio_interval(
            provider_samples["Jolt"],
            reference_samples,
            0x4A_4F_4C_54 + workload_index,
        )
        if median_ratio > arguments.maximum_median_ratio:
            print(
                f"  {workload['label']} median ratio {median_ratio:.3f} exceeds "
                f"{arguments.maximum_median_ratio:.3f} against {arguments.gate_provider}.",
                file=sys.stderr,
            )
            failed = True
        if repetition_p95_ratio > arguments.maximum_repetition_p95_ratio:
            print(
                f"  {workload['label']} repetition p95 ratio {repetition_p95_ratio:.3f} exceeds "
                f"{arguments.maximum_repetition_p95_ratio:.3f} against {arguments.gate_provider}.",
                file=sys.stderr,
            )
            failed = True
        if conservative_repetition_ratio > arguments.maximum_repetition_p95_ratio:
            print(
                f"  {workload['label']} conservative repetition ratio "
                f"{conservative_repetition_ratio:.3f} exceeds "
                f"{arguments.maximum_repetition_p95_ratio:.3f} against "
                f"{arguments.gate_provider}.",
                file=sys.stderr,
            )
            failed = True
        if bootstrap_upper > arguments.maximum_bootstrap_ratio:
            print(
                f"  {workload['label']} bootstrap ratio upper bound {bootstrap_upper:.3f} exceeds "
                f"{arguments.maximum_bootstrap_ratio:.3f} against {arguments.gate_provider}.",
                file=sys.stderr,
            )
            failed = True

    print("\nRaw-frame tail workload        Jolt p95  Box3D p95  PhysX p95  J/B    J/P")
    for workload in TAIL_WORKLOADS:
        provider_p95_samples = {}
        provider_raw_frame_samples = {}
        provider_affinity_processor_counts = {}
        try:
            for provider in PROVIDERS:
                name = f"{provider}/{workload['suffix']}"
                (
                    provider_raw_frame_samples[provider],
                    provider_p95_samples[provider],
                    provider_affinity_processor_counts[provider],
                ) = load_tail_samples(
                    reports[provider],
                    provider,
                    name,
                    workload,
                    arguments.repetitions,
                )
            if len(set(provider_affinity_processor_counts.values())) != 1:
                raise ValueError(
                    f"{workload['label']} used different benchmark processor counts: "
                    f"{provider_affinity_processor_counts}"
                )
        except ValueError as error:
            print(error, file=sys.stderr)
            failed = True
            continue

        p95 = {
            provider: percentile(provider_raw_frame_samples[provider], 0.95)
            for provider in PROVIDERS
        }
        jolt_to_box3d = p95["Jolt"] / p95["Box3D"]
        jolt_to_physx = p95["Jolt"] / p95["PhysX"]
        print(
            f"{workload['label']:<29} {p95['Jolt']:>9.2f}  "
            f"{p95['Box3D']:>10.2f}  {p95['PhysX']:>9.2f}  "
            f"{jolt_to_box3d:>5.3f}  {jolt_to_physx:>5.3f}"
        )

        for provider in PROVIDERS:
            cv = coefficient_of_variation(provider_p95_samples[provider])
            if cv <= arguments.maximum_cv:
                continue
            if provider == "Jolt":
                print(
                    f"  {workload['label']} {provider} p95-window CV {cv:.3%} exceeds "
                    f"{arguments.maximum_cv:.3%}.",
                    file=sys.stderr,
                )
                failed = True
                continue
            print(
                f"  Diagnostic: {workload['label']} {provider} p95-window CV {cv:.3%} "
                f"exceeds {arguments.maximum_cv:.3%}."
            )

        frame_tail_ratio = p95["Jolt"] / p95[arguments.gate_provider]
        if frame_tail_ratio > arguments.maximum_frame_tail_ratio:
            print(
                f"  {workload['label']} frame p95 ratio {frame_tail_ratio:.3f} exceeds "
                f"{arguments.maximum_frame_tail_ratio:.3f} against {arguments.gate_provider}.",
                file=sys.stderr,
            )
            failed = True

        candidate_window_p95 = percentile(provider_p95_samples["Jolt"], 0.95)
        reference_window_p05 = percentile(provider_p95_samples[arguments.gate_provider], 0.05)
        conservative_window_ratio = candidate_window_p95 / reference_window_p05
        if conservative_window_ratio > arguments.maximum_frame_tail_ratio:
            print(
                f"  {workload['label']} conservative p95-window ratio "
                f"{conservative_window_ratio:.3f} exceeds "
                f"{arguments.maximum_frame_tail_ratio:.3f} against "
                f"{arguments.gate_provider}.",
                file=sys.stderr,
            )
            failed = True

    if failed:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
