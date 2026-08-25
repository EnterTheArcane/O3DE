"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Captures engine profiler metadata and physics-update timings for an authored Jolt workload.
"""

import math as python_math
import os
import statistics
from pathlib import Path

try:
    from .Jolt_ScenarioRecorder import record_scenario
    from .Jolt_ScenarioUtilities import enter_game_mode
    from .Jolt_ScenarioUtilities import exit_game_mode
except ImportError:
    from Jolt_ScenarioRecorder import record_scenario
    from Jolt_ScenarioUtilities import enter_game_mode
    from Jolt_ScenarioUtilities import exit_game_mode


class Tests:
    level_opened = ("Opened the Jolt performance level", "Failed to open the Jolt performance level")
    scene_created = ("Created the Jolt performance scene", "Failed to create the Jolt performance scene")
    scene_topology = ("Validated the Jolt performance scene", "The Jolt performance scene was malformed")
    scene_warmed = ("Warmed the Jolt performance scene", "Failed to warm the Jolt performance scene")
    metadata_captured = ("Captured Jolt benchmark metadata", "Failed to capture Jolt benchmark metadata")
    simulation_healthy = ("Completed without Jolt capacity errors", "Jolt reported simulation capacity errors")
    workload_active = ("Kept the Jolt workload active", "The Jolt workload became idle")
    updates_captured = ("Captured Jolt physics-update data", "Failed to capture Jolt physics-update data")


@record_scenario("Jolt_PerformanceCapture")
def Jolt_PerformanceCapture(recorder):
    import json

    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general
    import azlmbr.math as math
    import azlmbr.paths

    from Atom.atom_utils.benchmark_utils import BenchmarkHelper
    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("", "Base")
    recorder.result(Tests.level_opened, general.get_current_level_name() == "Base")
    general.set_cvar_integer("vsync_interval", 0)
    general.set_cvar_integer("sys_MaxFPS", -1)

    ground = EditorEntity.create_editor_entity("Jolt Performance Ground")
    ground.add_component("Jolt Collider")
    ground.add_component("Jolt Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", ground.id, math.Vector3(0.0, 0.0, -10.0))
    components.TransformBus(bus.Event, "SetLocalUniformScale", ground.id, 20.0)

    body_count = 256
    bodies = []
    body_names = []
    row_width = 8
    for body_index in range(body_count):
        body_name = f"Jolt Performance Body {body_index}"
        body = EditorEntity.create_editor_entity(body_name)
        body.add_component("Jolt Collider")
        body.add_component("Jolt Rigid Body")
        layer = body_index // (row_width * row_width)
        row = (body_index // row_width) % row_width
        column = body_index % row_width
        components.TransformBus(
            bus.Event,
            "SetWorldTranslation",
            body.id,
            math.Vector3(float(column) - 3.5, float(row) - 3.5, 0.55 + 1.02 * float(layer)),
        )
        bodies.append(body)
        body_names.append(body_name)

    recorder.result(
        Tests.scene_created,
        ground.has_component("Jolt Static Rigid Body")
        and len(bodies) == body_count
        and all(body.has_component("Jolt Rigid Body") for body in bodies),
    )

    first_body_z = components.TransformBus(bus.Event, "GetWorldZ", bodies[0].id)
    last_body_z = components.TransformBus(bus.Event, "GetWorldZ", bodies[-1].id)
    scene_positions_valid = (
        abs(first_body_z - 0.55) < 1.0e-3
        and abs(last_body_z - 3.61) < 1.0e-3
    )

    recorder.result(Tests.scene_topology, scene_positions_valid)

    entered_game_mode = enter_game_mode(recorder, 20.0)
    try:
        if not entered_game_mode:
            return

        game_body_ids = [general.find_game_entity(body_name) for body_name in body_names]
        recorder.check(
            "resolved every benchmark runtime body",
            len(game_body_ids) == body_count and all(body_id.IsValid() for body_id in game_body_ids),
        )
        if not game_body_ids or not all(body_id.IsValid() for body_id in game_body_ids):
            return

        world_handle = jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", game_body_ids[0])
        warmed = helper.wait_for_condition(
            lambda: all(
                jolt.JoltRigidBodyRequestBus(bus.Event, "IsSimulationEnabled", body_id)
                for body_id in game_body_ids
            ),
            20.0,
        )
        if warmed:
            general.idle_wait_frames(120)
        recorder.result(Tests.scene_warmed, warmed)

        benchmark = BenchmarkHelper("Jolt_FallingBodies")
        benchmark_result_directory = os.environ.get("JOLT_BENCHMARK_RESULT_DIRECTORY")
        if benchmark_result_directory:
            benchmark_output_directory = Path(benchmark_result_directory) / benchmark.benchmark_name
            benchmark_output_directory.mkdir(parents=True, exist_ok=True)
            benchmark.output_path = benchmark_output_directory.as_posix()
        recorder.result(Tests.metadata_captured, benchmark.capture_benchmark_metadata())

        update_times = []
        active_body_counts = []
        simulation_errors = []
        statistics_snapshot = jolt.WorldStatistics()
        for frame in range(1, 31):
            for body_index, body_id in enumerate(game_body_ids):
                x_velocity = -0.5
                if body_index & 1:
                    x_velocity = 0.5
                y_velocity = -0.25
                if body_index & 2:
                    y_velocity = 0.25
                velocity = math.Vector3(x_velocity, y_velocity, 0.2)
                jolt.JoltRigidBodyRequestBus(
                    bus.Event,
                    "SetVelocities",
                    body_id,
                    velocity,
                    math.Vector3(0.0, 0.0, 0.1),
                )
                jolt.JoltRigidBodyRequestBus(bus.Event, "ActivateBody", body_id)

            general.idle_wait_frames(1)
            statistics_read = jolt.JoltWorldDiagnosticsRequestBus(
                bus.Broadcast,
                "GetWorldStatistics",
                world_handle,
                statistics_snapshot,
            )
            if statistics_read:
                update_time = int(statistics_snapshot.lastUpdateNanoseconds)
                update_times.append(update_time)
                active_body_counts.append(statistics_snapshot.activeDynamicBodyCount)
                simulation_errors.append(statistics_snapshot.lastUpdateErrors)
                output_path = azlmbr.paths.resolve_path(
                    f"{benchmark.output_path}/physics_update{frame}_time.json"
                )
                with open(output_path, "w", encoding="utf-8") as update_file:
                    json.dump(
                        {
                            "schemaVersion": 1,
                            "workload": benchmark.benchmark_name,
                            "sampleIndex": frame,
                            "updateTimeNanoseconds": update_time,
                        },
                        update_file,
                        separators=(",", ":"),
                        sort_keys=True,
                    )
            else:
                update_times.append(0)
                active_body_counts.append(0)
                simulation_errors.append(None)

        general.log(f"Jolt active body samples: {active_body_counts}")
        general.log(f"Jolt simulation error samples: {simulation_errors}")
        recorder.result(Tests.workload_active, min(active_body_counts) >= int(body_count * 0.95))
        recorder.result(
            Tests.simulation_healthy,
            all(error is not None and not error for error in simulation_errors),
        )
        valid_update_times = all(update_time > 0 for update_time in update_times)
        update_details = {"count": len(update_times)}
        if valid_update_times:
            sorted_update_times = sorted(update_times)
            percentile_index = python_math.ceil(0.95 * len(sorted_update_times)) - 1
            update_details.update(
                {
                    "medianNanoseconds": statistics.median(update_times),
                    "p95Nanoseconds": sorted_update_times[percentile_index],
                }
            )
        recorder.result(
            Tests.updates_captured,
            len(update_times) == 30 and valid_update_times,
            update_details,
        )
    finally:
        if entered_game_mode:
            exit_game_mode(recorder, 20.0)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_PerformanceCapture)
