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


TIME_UNIT_TO_MICROSECONDS = {
    "ns": 0.001,
    "us": 1.0,
    "ms": 1_000.0,
    "s": 1_000_000.0,
}

T_CRITICAL_95 = {
    1: 12.706,
    2: 4.303,
    3: 3.182,
    4: 2.776,
    5: 2.571,
    6: 2.447,
    7: 2.365,
    8: 2.306,
    9: 2.262,
    10: 2.228,
    11: 2.201,
    12: 2.179,
    13: 2.160,
    14: 2.145,
    15: 2.131,
    16: 2.120,
    17: 2.110,
    18: 2.101,
    19: 2.093,
    20: 2.086,
    21: 2.080,
    22: 2.074,
    23: 2.069,
    24: 2.064,
    25: 2.060,
    26: 2.056,
    27: 2.052,
    28: 2.048,
    29: 2.045,
    30: 2.042,
}


def load_report(path: Path) -> dict:
    with path.open(encoding="utf-8") as benchmark_file:
        return json.load(benchmark_file)


def load_samples(report: dict, name: str, expected_counters: dict[str, int]) -> list[float]:
    samples = []
    for result in report["benchmarks"]:
        if result["name"] != name or result.get("run_type", "iteration") != "iteration":
            continue
        for counter, expected_value in expected_counters.items():
            if counter not in result or round(float(result[counter])) != expected_value:
                raise ValueError(
                    f"{name} has invalid {counter}: expected {expected_value}, found {result.get(counter, 'missing')}"
                )
        samples.append(result["real_time"] * TIME_UNIT_TO_MICROSECONDS[result["time_unit"]])
    return samples


def percentile(samples: list[float], probability: float) -> float:
    ordered_samples = sorted(samples)
    index = max(0, math.ceil(probability * len(ordered_samples)) - 1)
    return ordered_samples[index]


def mean_confidence_interval(samples: list[float]) -> tuple[float, float, float]:
    mean = statistics.mean(samples)
    if len(samples) < 2:
        return mean, mean, mean

    degrees_of_freedom = len(samples) - 1
    critical = T_CRITICAL_95.get(degrees_of_freedom, 1.960)
    margin = critical * statistics.stdev(samples) / math.sqrt(len(samples))
    return mean, mean - margin, mean + margin


def measurement_pairs(box3d_samples: list[float], physx_samples: list[float]) -> dict[str, tuple[float, float]]:
    return {
        "median": (statistics.median(box3d_samples), statistics.median(physx_samples)),
        "rep_p95": (percentile(box3d_samples, 0.95), percentile(physx_samples, 0.95)),
        "rep_p99": (percentile(box3d_samples, 0.99), percentile(physx_samples, 0.99)),
    }


def coefficient_of_variation(samples: list[float]) -> float:
    mean = statistics.mean(samples)
    return statistics.stdev(samples) / mean if len(samples) > 1 and mean > 0.0 else 0.0


def bootstrap_median_ratio_interval(
    box3d_samples: list[float], physx_samples: list[float], sample_count: int = 5000
) -> tuple[float, float]:
    generator = random.Random(0xB03D)
    ratios = []
    for _ in range(sample_count):
        box3d_resample = generator.choices(box3d_samples, k=len(box3d_samples))
        physx_resample = generator.choices(physx_samples, k=len(physx_samples))
        ratios.append(statistics.median(box3d_resample) / statistics.median(physx_resample))
    return percentile(ratios, 0.025), percentile(ratios, 0.975)


def validate_context(box3d_report: dict, physx_report: dict) -> None:
    box3d_context = box3d_report.get("context", {})
    physx_context = physx_report.get("context", {})
    for field in ("cpu_model", "num_cpus"):
        if box3d_context.get(field) != physx_context.get(field):
            raise ValueError(
                f"Benchmark context differs for {field}: "
                f"Box3D={box3d_context.get(field)!r}, PhysX={physx_context.get(field)!r}"
            )


def print_throughput_confidence_interval(
    label: str, items_per_iteration: int, box3d_samples: list[float], physx_samples: list[float]
) -> None:
    box3d_throughput = [items_per_iteration * 1_000_000.0 / sample for sample in box3d_samples]
    physx_throughput = [items_per_iteration * 1_000_000.0 / sample for sample in physx_samples]
    box3d_mean, box3d_lower, box3d_upper = mean_confidence_interval(box3d_throughput)
    physx_mean, physx_lower, physx_upper = mean_confidence_interval(physx_throughput)
    print(
        f"{label} throughput items/s (95% t-CI): "
        f"Box3D {box3d_mean:,.0f} [{box3d_lower:,.0f}, {box3d_upper:,.0f}], "
        f"PhysX {physx_mean:,.0f} [{physx_lower:,.0f}, {physx_upper:,.0f}]"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare equivalent Box3D and PhysX step benchmarks.")
    parser.add_argument("box3d_results", type=Path)
    parser.add_argument("physx_results", type=Path)
    parser.add_argument("--worker-count", type=int, default=4)
    parser.add_argument("--maximum-ratio", type=float, default=1.0)
    parser.add_argument("--maximum-bootstrap-ratio", type=float, default=1.05)
    parser.add_argument("--maximum-repetition-tail-ratio", type=float, default=1.10)
    parser.add_argument("--maximum-cv", type=float, default=0.05)
    parser.add_argument("--minimum-repetitions", type=int, default=30)
    parser.add_argument("--body-count", type=int, action="append")
    arguments = parser.parse_args()

    box3d_report = load_report(arguments.box3d_results)
    physx_report = load_report(arguments.physx_results)
    try:
        validate_context(box3d_report, physx_report)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    failed = False
    print("Bodies  Metric  Box3D (us)  PhysX (us)  Ratio")
    for body_count in arguments.body_count or [128, 1024]:
        box3d_name = f"Box3D/Step/FallingSpheres/{body_count}/{arguments.worker_count}/real_time"
        physx_name = f"PhysX/Step/FallingSpheres/{body_count}/{arguments.worker_count}/real_time"
        expected_counters = {
            "Ccd": 0,
            "DynamicBodies": body_count,
            "Notifications": 0,
            "QualityValid": 1,
            "Sleep": 0,
            "WarmupStable": 1,
            "Workers": arguments.worker_count,
        }
        try:
            box3d_samples = load_samples(box3d_report, box3d_name, expected_counters)
            physx_samples = load_samples(physx_report, physx_name, expected_counters)
        except ValueError as error:
            print(error, file=sys.stderr)
            failed = True
            continue
        if len(box3d_samples) < arguments.minimum_repetitions or len(physx_samples) < arguments.minimum_repetitions:
            print(
                f"Expected at least {arguments.minimum_repetitions} repetitions for {body_count} bodies; "
                f"found {len(box3d_samples)} Box3D and {len(physx_samples)} PhysX samples.",
                file=sys.stderr,
            )
            failed = True
            continue

        measurements = measurement_pairs(box3d_samples, physx_samples)
        for metric, (box3d_time, physx_time) in measurements.items():
            ratio = box3d_time / physx_time
            print(f"{body_count:>6}  {metric:<6}  {box3d_time:>10.2f}  {physx_time:>10.2f}  {ratio:>5.3f}")
            maximum_ratio = arguments.maximum_ratio if metric == "median" else arguments.maximum_repetition_tail_ratio
            failed = failed or ratio > maximum_ratio

        box3d_cv = coefficient_of_variation(box3d_samples)
        physx_cv = coefficient_of_variation(physx_samples)
        ratio_lower, ratio_upper = bootstrap_median_ratio_interval(box3d_samples, physx_samples)
        print(
            f"       CV Box3D={box3d_cv:.3%} PhysX={physx_cv:.3%}; "
            f"median ratio bootstrap 95% CI [{ratio_lower:.3f}, {ratio_upper:.3f}]"
        )
        failed = failed or box3d_cv > arguments.maximum_cv or physx_cv > arguments.maximum_cv
        failed = failed or ratio_upper > arguments.maximum_bootstrap_ratio

        print_throughput_confidence_interval(f"{body_count:>6} ", body_count, box3d_samples, physx_samples)

    print("\nLifecycle                       Metric  Box3D (us)  PhysX (us)  Ratio")
    for body_count in arguments.body_count or [128, 1024]:
        suffix = f"Lifecycle/CreateDestroyBodies/{body_count}/1/real_time"
        expected_counters = {
            "DynamicBodies": body_count,
            "Notifications": 0,
            "Workers": 1,
        }
        try:
            box3d_samples = load_samples(box3d_report, f"Box3D/{suffix}", expected_counters)
            physx_samples = load_samples(physx_report, f"PhysX/{suffix}", expected_counters)
        except ValueError as error:
            print(error, file=sys.stderr)
            failed = True
            continue
        if len(box3d_samples) < arguments.minimum_repetitions or len(physx_samples) < arguments.minimum_repetitions:
            print(
                f"Expected at least {arguments.minimum_repetitions} repetitions for lifecycle {body_count}; "
                f"found {len(box3d_samples)} Box3D and {len(physx_samples)} PhysX samples.",
                file=sys.stderr,
            )
            failed = True
            continue

        label = f"CreateDestroyBodies {body_count}"
        for metric, (box3d_time, physx_time) in measurement_pairs(box3d_samples, physx_samples).items():
            ratio = box3d_time / physx_time
            print(f"{label:<31} {metric:<6}  {box3d_time:>10.2f}  {physx_time:>10.2f}  {ratio:>5.3f}")
            maximum_ratio = arguments.maximum_ratio if metric == "median" else arguments.maximum_repetition_tail_ratio
            failed = failed or ratio > maximum_ratio
        box3d_cv = coefficient_of_variation(box3d_samples)
        physx_cv = coefficient_of_variation(physx_samples)
        ratio_lower, ratio_upper = bootstrap_median_ratio_interval(box3d_samples, physx_samples)
        print(
            f"{label} CV Box3D={box3d_cv:.3%} PhysX={physx_cv:.3%}; "
            f"median ratio bootstrap 95% CI [{ratio_lower:.3f}, {ratio_upper:.3f}]"
        )
        failed = failed or box3d_cv > arguments.maximum_cv or physx_cv > arguments.maximum_cv
        failed = failed or ratio_upper > arguments.maximum_bootstrap_ratio
        print_throughput_confidence_interval(label, body_count, box3d_samples, physx_samples)

    obstacle_count = 1024
    print("\nWorkload                        Metric  Box3D (us)  PhysX (us)  Ratio")
    query_workloads = (
        ("RaycastGrid", 128, 1, {}, 128),
        ("RaycastClosestBatchGrid", 128, arguments.worker_count, {"WarmupMs": 100}, 128),
        ("RaycastClosestBatchGrid", 1024, arguments.worker_count, {"WarmupMs": 100}, 1024),
        ("OverlapSphereGrid", 1, 1, {"ActualHits": 25, "ExpectedHits": 25, "QualityValid": 1}, 1),
    )
    for workload, query_count, worker_count, workload_counters, items_per_iteration in query_workloads:
        box3d_name = f"Box3D/Query/{workload}/{obstacle_count}/{query_count}/{worker_count}/real_time"
        physx_name = f"PhysX/Query/{workload}/{obstacle_count}/{query_count}/{worker_count}/real_time"
        expected_counters = {"Obstacles": obstacle_count, "Workers": worker_count, **workload_counters}
        try:
            box3d_samples = load_samples(box3d_report, box3d_name, expected_counters)
            physx_samples = load_samples(physx_report, physx_name, expected_counters)
        except ValueError as error:
            print(error, file=sys.stderr)
            failed = True
            continue

        if len(box3d_samples) < arguments.minimum_repetitions or len(physx_samples) < arguments.minimum_repetitions:
            print(
                f"Expected at least {arguments.minimum_repetitions} repetitions for {workload}; "
                f"found {len(box3d_samples)} Box3D and {len(physx_samples)} PhysX samples.",
                file=sys.stderr,
            )
            failed = True
            continue

        label = f"{workload} {obstacle_count}/{query_count}"
        measurements = measurement_pairs(box3d_samples, physx_samples)
        for metric, (box3d_time, physx_time) in measurements.items():
            ratio = box3d_time / physx_time
            print(f"{label:<31} {metric:<6}  {box3d_time:>10.2f}  {physx_time:>10.2f}  {ratio:>5.3f}")
            maximum_ratio = arguments.maximum_ratio if metric == "median" else arguments.maximum_repetition_tail_ratio
            failed = failed or ratio > maximum_ratio
        box3d_cv = coefficient_of_variation(box3d_samples)
        physx_cv = coefficient_of_variation(physx_samples)
        ratio_lower, ratio_upper = bootstrap_median_ratio_interval(box3d_samples, physx_samples)
        print(
            f"{workload} CV Box3D={box3d_cv:.3%} PhysX={physx_cv:.3%}; "
            f"median ratio bootstrap 95% CI [{ratio_lower:.3f}, {ratio_upper:.3f}]"
        )
        failed = failed or box3d_cv > arguments.maximum_cv or physx_cv > arguments.maximum_cv
        failed = failed or ratio_upper > arguments.maximum_bootstrap_ratio
        print_throughput_confidence_interval(label, items_per_iteration, box3d_samples, physx_samples)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
