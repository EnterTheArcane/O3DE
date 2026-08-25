"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Captures engine profiler metadata and CPU frame data for an authored Jolt workload.
"""


class Tests:
    level_opened = ("Opened the Jolt performance level", "Failed to open the Jolt performance level")
    scene_created = ("Created the Jolt performance scene", "Failed to create the Jolt performance scene")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    scene_topology = ("Validated the Jolt performance scene", "The Jolt performance scene was malformed")
    scene_warmed = ("Warmed the Jolt performance scene", "Failed to warm the Jolt performance scene")
    metadata_captured = ("Captured Jolt benchmark metadata", "Failed to capture Jolt benchmark metadata")
    simulation_healthy = ("Completed without Jolt capacity errors", "Jolt reported simulation capacity errors")
    workload_active = ("Kept the Jolt workload active", "The Jolt workload became idle")
    frames_captured = ("Captured Jolt CPU frame data", "Failed to capture Jolt CPU frame data")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Jolt_PerformanceCapture():
    import json

    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general
    import azlmbr.math as math
    import azlmbr.paths

    from Atom.atom_utils.benchmark_utils import BenchmarkHelper
    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("", "Base")
    Report.result(Tests.level_opened, general.get_current_level_name() == "Base")
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

    Report.result(
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

    helper.enter_game_mode(Tests.enter_game_mode)
    game_body_ids = [general.find_game_entity(body_name) for body_name in body_names]
    Report.result(
        Tests.scene_topology,
        scene_positions_valid and all(body_id.IsValid() for body_id in game_body_ids),
    )

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
    Report.result(Tests.scene_warmed, warmed)

    benchmark = BenchmarkHelper("Jolt_FallingBodies")
    Report.result(Tests.metadata_captured, benchmark.capture_benchmark_metadata())
    captured_frames = []
    active_body_counts = []
    simulation_errors = []
    statistics = jolt.WorldStatistics()
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

        capture_succeeded = benchmark.capture_cpu_frame_time(frame)
        output_path = azlmbr.paths.resolve_path(f"{benchmark.output_path}/cpu_frame{frame}_time.json")
        with open(output_path, encoding="utf-8") as frame_file:
            frame_time = json.load(frame_file)["ClassData"]["frameTime"]
        captured_frames.append(capture_succeeded and frame_time > 0.0)
        active_body_counts.append(
            sum(
                jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", body_id).isActive
                for body_id in game_body_ids
            )
        )
        statistics_read = jolt.JoltWorldDiagnosticsRequestBus(
            bus.Broadcast,
            "GetWorldStatistics",
            world_handle,
            statistics,
        )
        if statistics_read:
            simulation_errors.append(statistics.lastUpdateErrors)
        else:
            simulation_errors.append(None)

    general.log(f"Jolt active body samples: {active_body_counts}")
    general.log(f"Jolt simulation error samples: {simulation_errors}")
    Report.result(Tests.workload_active, min(active_body_counts) >= int(body_count * 0.95))
    Report.result(
        Tests.simulation_healthy,
        all(error is not None and not error for error in simulation_errors),
    )
    Report.result(Tests.frames_captured, all(captured_frames))
    helper.exit_game_mode(Tests.exit_game_mode)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_PerformanceCapture)
