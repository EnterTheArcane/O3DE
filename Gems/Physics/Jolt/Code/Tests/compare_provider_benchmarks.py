#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import argparse
import json
import math
import random
import statistics
import sys
from pathlib import Path


PROVIDERS = ("Jolt", "Box3D", "PhysX")
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
        "suffix": "Step/FallingBoxes/128/1/iterations:600/real_time",
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
        "suffix": "Step/FallingBoxes/128/4/iterations:600/real_time",
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
        "suffix": "Step/FallingBoxes/128/8/iterations:600/real_time",
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
        "suffix": "Step/FallingBoxes/1024/1/iterations:600/real_time",
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
        "suffix": "Step/FallingBoxes/1024/4/iterations:600/real_time",
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
        "suffix": "Step/FallingBoxes/1024/8/iterations:600/real_time",
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
        "exact": {"DynamicBodies": 128, "Notifications": 0, "Workers": 1},
    },
    {
        "label": "Lifecycle 1024",
        "suffix": "Lifecycle/CreateDestroyBodies/1024/1/real_time",
        "exact": {"DynamicBodies": 1024, "Notifications": 0, "Workers": 1},
    },
    {
        "label": "Raycast 1024/128",
        "suffix": "Query/RaycastGrid/1024/128/1/real_time",
        "exact": {"Obstacles": 1024, "QualityValid": 1, "Workers": 1},
    },
    {
        "label": "Batch raycast 1024/128",
        "suffix": "Query/RaycastClosestBatchGrid/1024/128/4/real_time",
        "exact": {"Obstacles": 1024, "QualityValid": 1, "WarmupMs": 100, "Workers": 4},
    },
    {
        "label": "Batch raycast 1024/1024",
        "suffix": "Query/RaycastClosestBatchGrid/1024/1024/4/real_time",
        "exact": {"Obstacles": 1024, "QualityValid": 1, "WarmupMs": 100, "Workers": 4},
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
    },
)


def load_report(path: Path) -> dict:
    with path.open(encoding="utf-8") as report_file:
        return json.load(report_file)


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


def load_samples(
    report: dict,
    provider: str,
    name: str,
    workload: dict,
    minimum_repetitions: int,
) -> tuple[list[float], int]:
    samples = []
    affinity_processor_counts = set()
    for result in report.get("benchmarks", []):
        if result.get("name") != name or result.get("run_type", "iteration") != "iteration":
            continue

        exact_counters = dict(workload.get("exact", {}))
        exact_counters.update(workload.get("provider_exact", {}).get(provider, {}))
        if round(float(result.get("AffinityConstrained", 0))) != 1:
            raise ValueError(
                f"{name} was not constrained to the benchmark CPU topology."
            )
        actual_affinity_processors = round(float(result.get("AffinityProcessors", 0)))
        if actual_affinity_processors <= 0:
            raise ValueError(f"{name} has no constrained benchmark processors.")
        affinity_processor_counts.add(actual_affinity_processors)
        for counter, expected in exact_counters.items():
            if counter not in result or round(float(result[counter])) != expected:
                raise ValueError(
                    f"{name} has invalid {counter}: expected {expected}, "
                    f"found {result.get(counter, 'missing')}"
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

        time_unit = result.get("time_unit")
        if time_unit not in TIME_UNIT_TO_MICROSECONDS:
            raise ValueError(f"{name} has unsupported time unit {time_unit!r}.")
        samples.append(float(result["real_time"]) * TIME_UNIT_TO_MICROSECONDS[time_unit])

    if len(samples) < minimum_repetitions:
        raise ValueError(
            f"{name} has {len(samples)} raw repetitions; expected at least {minimum_repetitions}."
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
    parser.add_argument("--maximum-repetition-tail-ratio", type=float, default=1.10)
    parser.add_argument("--maximum-cv", type=float, default=0.05)
    parser.add_argument("--minimum-repetitions", type=int, default=30)
    arguments = parser.parse_args()

    reports = {
        "Jolt": load_report(arguments.jolt_results),
        "Box3D": load_report(arguments.box3d_results),
        "PhysX": load_report(arguments.physx_results),
    }
    try:
        validate_context(reports)
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
                    arguments.minimum_repetitions,
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
            if cv > arguments.maximum_cv:
                print(
                    f"  {workload['label']} {provider} CV {cv:.3%} exceeds "
                    f"{arguments.maximum_cv:.3%}.",
                    file=sys.stderr,
                )
                failed = True

        reference_samples = provider_samples[arguments.gate_provider]
        median_ratio = medians["Jolt"] / medians[arguments.gate_provider]
        tail_ratio = percentile(provider_samples["Jolt"], 0.95) / percentile(reference_samples, 0.95)
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
        if tail_ratio > arguments.maximum_repetition_tail_ratio:
            print(
                f"  {workload['label']} repetition p95 ratio {tail_ratio:.3f} exceeds "
                f"{arguments.maximum_repetition_tail_ratio:.3f} against {arguments.gate_provider}.",
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

    if failed:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
