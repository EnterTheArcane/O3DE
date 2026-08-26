#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import unittest

import validate_jolt_benchmarks


def make_result(name: str, repetition_index: int, real_time: float, time_unit: str = "ns") -> dict:
    return {
        "name": name,
        "run_type": "iteration",
        "repetition_index": repetition_index,
        "real_time": real_time,
        "time_unit": time_unit,
        "QualityValid": 1,
    }


def make_report() -> dict:
    benchmarks = []
    for repetition_index in range(30):
        benchmarks.append(
            make_result(
                validate_jolt_benchmarks.CAPABILITY_BENCHMARK,
                repetition_index,
                1.0,
            )
        )
        for name, operation_count in validate_jolt_benchmarks.REPRESENTATIVE_OPERATION_BENCHMARKS.items():
            benchmarks.append(make_result(name, repetition_index, 20.0 * operation_count))
        for prefix in validate_jolt_benchmarks.ROLLBACK_BENCHMARK_PREFIXES:
            for body_count in (128, 1_024):
                result = make_result(f"{prefix}{body_count}/0/real_time", repetition_index, 10.0, "us")
                result.update(
                    {
                        "NativeAllocatedBytes": 0,
                        "NativeAllocatedGrowthBytes": 0,
                        "NativeAllocationCount": 0,
                        "NativeFreeCount": 0,
                        "NativeReallocationCount": 0,
                        "SnapshotFailures": 0,
                        "TempAllocatorCapacityGrowthBytes": 0,
                        "WrapperRetainedGrowthBytes": 0,
                    }
                )
                benchmarks.append(result)
    return {
        "context": {"library_build_type": "release"},
        "qualification": {
            "provider": "Jolt",
            "raw_samples": True,
            "repetitions": 30,
            "runtime_dependencies": [
                {
                    "mtime_ns": 1,
                    "path": "Jolt.API.dll",
                    "sha256": "jolt-api-hash",
                }
            ],
        },
        "benchmarks": benchmarks,
    }


class ValidateJoltBenchmarksTests(unittest.TestCase):
    def test_accepts_valid_absolute_report(self):
        errors = validate_jolt_benchmarks.validate_report(make_report(), 30, 3.0, 0.02, 0.05)
        self.assertEqual(errors, [])

    def test_rejects_slow_capability_acquisition(self):
        report = make_report()
        for result in report["benchmarks"]:
            if result["name"] == validate_jolt_benchmarks.CAPABILITY_BENCHMARK:
                result["real_time"] = 4.0
        errors = validate_jolt_benchmarks.validate_report(report, 30, 3.0, 0.02, 0.05)
        self.assertTrue(any("Capability acquisition is" in error for error in errors))

    def test_rejects_unstable_representative_operation(self):
        report = make_report()
        operation_name = next(iter(validate_jolt_benchmarks.REPRESENTATIVE_OPERATION_BENCHMARKS))
        result = next(
            result
            for result in report["benchmarks"]
            if result["name"] == operation_name
        )
        result["real_time"] *= 10.0

        errors = validate_jolt_benchmarks.validate_report(report, 30, 3.0, 0.02, 0.05)
        self.assertTrue(any(operation_name in error and "CV gate" in error for error in errors))

    def test_rejects_steady_state_allocation_growth(self):
        report = make_report()
        rollback_result = next(
            result
            for result in report["benchmarks"]
            if result["name"].startswith(validate_jolt_benchmarks.ROLLBACK_BENCHMARK_PREFIXES[0])
        )
        rollback_result["WrapperRetainedGrowthBytes"] = 64
        errors = validate_jolt_benchmarks.validate_report(report, 30, 3.0, 0.02, 0.05)
        self.assertTrue(any("WrapperRetainedGrowthBytes" in error for error in errors))

    def test_rejects_restore_reallocations(self):
        report = make_report()
        for result in report["benchmarks"]:
            if result["name"].startswith(validate_jolt_benchmarks.ROLLBACK_BENCHMARK_PREFIXES[1:]):
                result["NativeReallocationCount"] = 8
        errors = validate_jolt_benchmarks.validate_report(report, 30, 3.0, 0.02, 0.05)
        self.assertTrue(any("NativeReallocationCount" in error for error in errors))

    def test_rejects_native_retained_growth(self):
        report = make_report()
        rollback_result = next(
            result
            for result in report["benchmarks"]
            if result["name"].startswith(validate_jolt_benchmarks.ROLLBACK_BENCHMARK_PREFIXES[1])
        )
        rollback_result["NativeAllocatedGrowthBytes"] = 64
        errors = validate_jolt_benchmarks.validate_report(report, 30, 3.0, 0.02, 0.05)
        self.assertTrue(any("NativeAllocatedGrowthBytes" in error for error in errors))

    def test_rejects_missing_runtime_dependency_fingerprint(self):
        report = make_report()
        report["qualification"]["runtime_dependencies"] = []
        errors = validate_jolt_benchmarks.validate_report(report, 30, 3.0, 0.02, 0.05)
        self.assertTrue(any("runtime dependency fingerprint" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
