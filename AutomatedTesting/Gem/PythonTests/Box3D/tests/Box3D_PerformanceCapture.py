"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Captures engine profiler metadata and CPU frame data for an authored Box3D workload.
"""


class Tests:
    level_opened = ("Opened the Box3D performance level", "Failed to open the Box3D performance level")
    scene_created = ("Created the Box3D performance scene", "Failed to create the Box3D performance scene")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    scene_warmed = ("Warmed the Box3D performance scene", "Failed to warm the Box3D performance scene")
    metadata_captured = ("Captured Box3D benchmark metadata", "Failed to capture Box3D benchmark metadata")
    workload_active = ("Kept the Box3D benchmark workload active", "Box3D benchmark workload became idle")
    frames_captured = ("Captured Box3D CPU frame data", "Failed to capture Box3D CPU frame data")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Box3D_PerformanceCapture():
    import json

    import azlmbr.box3d as box3d
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.legacy.general as general
    import azlmbr.math as math
    import azlmbr.paths

    from Atom.atom_utils.benchmark_utils import BenchmarkHelper
    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("Box3D", "ComponentSmoke")
    Report.result(Tests.level_opened, general.get_current_level_name() == "ComponentSmoke")
    general.set_cvar_integer("vsync_interval", 0)
    general.set_cvar_integer("sys_MaxFPS", -1)

    ground = EditorEntity.create_editor_entity("Box3D Performance Ground")
    ground.add_component("Box3D Collider")
    ground.add_component("Box3D Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", ground.id, math.Vector3(0.0, 0.0, -10.0))
    components.TransformBus(bus.Event, "SetLocalUniformScale", ground.id, 20.0)

    body_count = 256
    bodies = []
    row_width = 8
    for body_index in range(body_count):
        body = EditorEntity.create_editor_entity(f"Box3D Performance Body {body_index}")
        body.add_component("Box3D Collider")
        body.add_component("Box3D Rigid Body")
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

    Report.result(
        Tests.scene_created,
        ground.has_component("Box3D Static Rigid Body")
        and len(bodies) == body_count
        and all(body.has_component("Box3D Rigid Body") for body in bodies),
    )

    helper.enter_game_mode(Tests.enter_game_mode)
    world_handle = box3d.Box3DWorldRequestBus(bus.Broadcast, "GetDefaultWorldHandle")
    warmed = helper.wait_for_condition(
        lambda: box3d.Box3DDiagnosticsRequestBus(
            bus.Broadcast,
            "GetWorldStatistics",
            world_handle,
            box3d.StatisticsFlags_Counters,
        ).simulationTick
        >= 120,
        20.0,
    )
    Report.result(Tests.scene_warmed, warmed)

    benchmark = BenchmarkHelper("Box3D_FallingBodies")
    Report.result(Tests.metadata_captured, benchmark.capture_benchmark_metadata())
    captured_frames = []
    active_body_counts = []
    for frame in range(1, 31):
        for body_index, body in enumerate(bodies):
            velocity = math.Vector3(
                0.5 if body_index & 1 else -0.5,
                0.25 if body_index & 2 else -0.25,
                0.2,
            )
            box3d.Box3DRigidBodyRequestBus(bus.Event, "SetLinearVelocity", body.id, velocity)
            box3d.Box3DRigidBodyRequestBus(bus.Event, "SetAwake", body.id, True)

        capture_succeeded = benchmark.capture_cpu_frame_time(frame)
        output_path = azlmbr.paths.resolve_path(f"{benchmark.output_path}/cpu_frame{frame}_time.json")
        with open(output_path, encoding="utf-8") as frame_file:
            frame_time = json.load(frame_file)["ClassData"]["frameTime"]
        captured_frames.append(capture_succeeded and frame_time > 0.0)
        statistics = box3d.Box3DDiagnosticsRequestBus(
            bus.Broadcast,
            "GetWorldStatistics",
            world_handle,
            box3d.StatisticsFlags_Counters,
        )
        active_body_counts.append(statistics.counters.awakeBodyCount)
    general.log(f"Box3D awake body samples: {active_body_counts}")
    Report.result(Tests.workload_active, min(active_body_counts) >= int(body_count * 0.95))
    Report.result(Tests.frames_captured, all(captured_frames))
    helper.exit_game_mode(Tests.exit_game_mode)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Box3D_PerformanceCapture)
