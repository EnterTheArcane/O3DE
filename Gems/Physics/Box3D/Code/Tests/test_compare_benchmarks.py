#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import json
import sys

import compare_benchmarks


def make_result(name: str, time_microseconds: float, **counters: int) -> dict:
    return {
        "name": name,
        "run_type": "iteration",
        "real_time": time_microseconds,
        "time_unit": "us",
        **counters,
    }


def make_report(provider: str, step_time: float, raycast_time: float) -> dict:
    benchmarks = []
    for body_count in (128, 1024):
        for _ in range(3):
            benchmarks.append(
                make_result(
                    f"{provider}/Step/FallingBoxes/{body_count}/4/real_time",
                    step_time,
                    Ccd=0,
                    DynamicBodies=body_count,
                    Notifications=0,
                    QualityValid=1,
                    Sleep=0,
                    WarmupStable=1,
                    Workers=4,
                )
            )
    for _ in range(3):
        for body_count in (128, 1024):
            benchmarks.append(
                make_result(
                    f"{provider}/Lifecycle/CreateDestroyBodies/{body_count}/1/real_time",
                    step_time,
                    DynamicBodies=body_count,
                    Notifications=0,
                    Workers=1,
                )
            )
        benchmarks.append(
            make_result(
                f"{provider}/Query/RaycastGrid/1024/128/1/real_time",
                raycast_time,
                Obstacles=1024,
                Workers=1,
            )
        )
        for ray_count in (128, 1024):
            benchmarks.append(
                make_result(
                    f"{provider}/Query/RaycastClosestBatchGrid/1024/{ray_count}/4/real_time",
                    raycast_time,
                    Obstacles=1024,
                    WarmupMs=100,
                    Workers=4,
                )
            )
        benchmarks.append(
            make_result(
                f"{provider}/Query/OverlapSphereGrid/1024/1/1/real_time",
                raycast_time,
                ActualHits=25,
                ExpectedHits=25,
                Obstacles=1024,
                QualityValid=1,
                Workers=1,
            )
        )
    return {"context": {"cpu_model": "Synthetic CPU", "num_cpus": 8}, "benchmarks": benchmarks}


def run_comparison(monkeypatch, tmp_path, box3d_report: dict, physx_report: dict) -> int:
    box3d_path = tmp_path / "box3d.json"
    physx_path = tmp_path / "physx.json"
    box3d_path.write_text(json.dumps(box3d_report), encoding="utf-8")
    physx_path.write_text(json.dumps(physx_report), encoding="utf-8")
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "compare_benchmarks.py",
            str(box3d_path),
            str(physx_path),
            "--minimum-repetitions=3",
        ],
    )
    return compare_benchmarks.main()


def test_equivalent_faster_workloads_pass(monkeypatch, tmp_path):
    assert run_comparison(
        monkeypatch,
        tmp_path,
        make_report("Box3D", 90.0, 45.0),
        make_report("PhysX", 100.0, 50.0),
    ) == 0


def test_mismatched_worker_counter_fails(monkeypatch, tmp_path, capsys):
    box3d_report = make_report("Box3D", 90.0, 45.0)
    box3d_report["benchmarks"][0]["Workers"] = 8

    assert run_comparison(monkeypatch, tmp_path, box3d_report, make_report("PhysX", 100.0, 50.0)) == 1
    assert "invalid Workers" in capsys.readouterr().err


def test_mismatched_simulation_policy_fails(monkeypatch, tmp_path, capsys):
    physx_report = make_report("PhysX", 100.0, 50.0)
    physx_report["benchmarks"][0]["Ccd"] = 1

    assert run_comparison(monkeypatch, tmp_path, make_report("Box3D", 90.0, 45.0), physx_report) == 1
    assert "invalid Ccd" in capsys.readouterr().err


def test_failed_quality_gate_fails(monkeypatch, tmp_path, capsys):
    box3d_report = make_report("Box3D", 90.0, 45.0)
    box3d_report["benchmarks"][0]["QualityValid"] = 0

    assert run_comparison(monkeypatch, tmp_path, box3d_report, make_report("PhysX", 100.0, 50.0)) == 1
    assert "invalid QualityValid" in capsys.readouterr().err


def test_unstable_warmup_fails(monkeypatch, tmp_path, capsys):
    physx_report = make_report("PhysX", 100.0, 50.0)
    physx_report["benchmarks"][0]["WarmupStable"] = 0

    assert run_comparison(monkeypatch, tmp_path, make_report("Box3D", 90.0, 45.0), physx_report) == 1
    assert "invalid WarmupStable" in capsys.readouterr().err


def test_mismatched_machine_context_fails(monkeypatch, tmp_path, capsys):
    physx_report = make_report("PhysX", 100.0, 50.0)
    physx_report["context"]["num_cpus"] = 16

    assert run_comparison(monkeypatch, tmp_path, make_report("Box3D", 90.0, 45.0), physx_report) == 1
    assert "Benchmark context differs" in capsys.readouterr().err


def test_bootstrap_upper_bound_rejects_a_regression(monkeypatch, tmp_path):
    assert run_comparison(
        monkeypatch,
        tmp_path,
        make_report("Box3D", 110.0, 55.0),
        make_report("PhysX", 100.0, 50.0),
    ) == 1
