"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Verifies the checked-in Jolt level exports and instantiates authored provider assets.
"""

try:
    from .Jolt_ScenarioRecorder import record_scenario
except ImportError:
    from Jolt_ScenarioRecorder import record_scenario


class Tests:
    level_opened = ("Opened the saved Jolt level", "Failed to open the saved Jolt level")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    body_operational = ("Simulated the saved rigid body", "The saved rigid body was not operational")
    scene_operational = ("Instantiated the saved scene asset", "The saved scene asset was not operational")
    skeleton_operational = ("Loaded the saved skeleton asset", "The saved skeleton asset was not operational")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


@record_scenario("Jolt_SavedComponents")
def Jolt_SavedComponents(recorder):
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general

    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import TestHelper as helper

    try:
        from .Jolt_ScenarioUtilities import enter_game_mode
        from .Jolt_ScenarioUtilities import exit_game_mode
        from .Jolt_ScenarioUtilities import wait_for_condition
    except ImportError:
        from Jolt_ScenarioUtilities import enter_game_mode
        from Jolt_ScenarioUtilities import exit_game_mode
        from Jolt_ScenarioUtilities import wait_for_condition

    helper.init_idle()
    helper.open_level("", "JoltSavedComponents")
    recorder.result(Tests.level_opened, general.get_current_level_name() == "JoltSavedComponents")

    authored_body = EditorEntity.find_editor_entity("Jolt Saved Falling Body")
    authored_scene = EditorEntity.find_editor_entity("Jolt Saved Scene")
    authored_skeleton = EditorEntity.find_editor_entity("Jolt Saved Skeleton")
    recorder.check(
        "saved rigid body authored",
        authored_body.exists()
        and authored_body.has_component("Jolt Collider")
        and authored_body.has_component("Jolt Rigid Body"),
    )
    recorder.check(
        "saved scene authored",
        authored_scene.exists() and authored_scene.has_component("Jolt Scene"),
    )
    recorder.check(
        "saved skeleton authored",
        authored_skeleton.exists() and authored_skeleton.has_component("Jolt Skeleton"),
    )

    entered_game_mode = enter_game_mode(recorder)
    try:
        if not entered_game_mode:
            return

        body_id = general.find_game_entity("Jolt Saved Falling Body")
        scene_id = general.find_game_entity("Jolt Saved Scene")
        skeleton_id = general.find_game_entity("Jolt Saved Skeleton")
        recorder.check(
            "saved runtime entities exported",
            body_id.IsValid() and scene_id.IsValid() and skeleton_id.IsValid(),
        )

        initial_body_z = components.TransformBus(bus.Event, "GetWorldZ", body_id)
        recorder.check(
            "saved body simulation enabled",
            jolt.JoltRigidBodyRequestBus(bus.Event, "IsSimulationEnabled", body_id),
        )
        recorder.result(
            Tests.body_operational,
            wait_for_condition(
                lambda: components.TransformBus(bus.Event, "GetWorldZ", body_id) < initial_body_z - 0.25,
                3.0,
            ),
            f"initial z={initial_body_z}",
        )

        recorder.capture(
            "saved scene became ready",
            lambda: wait_for_condition(
                lambda: jolt.JoltSceneRequestBus(bus.Event, "IsReady", scene_id),
                3.0,
            ),
        )
        scene_bodies = recorder.capture(
            "saved scene instantiated its exact body inventory",
            lambda: jolt.JoltSceneRequestBus(bus.Event, "CopyBodies", scene_id),
            lambda bodies: len(bodies) == 1 and bodies[0].IsValid(),
            [],
        )
        recorder.result(Tests.scene_operational, len(scene_bodies) == 1)

        recorder.capture(
            "saved skeleton became ready",
            lambda: wait_for_condition(
                lambda: jolt.JoltSkeletonComponentRequestBus(bus.Event, "IsReady", skeleton_id),
                3.0,
            ),
        )
        animation_names = recorder.capture(
            "saved skeleton preserved its exact animation inventory",
            lambda: jolt.JoltSkeletonComponentRequestBus(
                bus.Event,
                "CopyAnimationNames",
                skeleton_id,
            ),
            lambda names: len(names) == 1 and str(names[0]) == "Walk",
            [],
        )
        recorder.capture(
            "saved skeleton animation resolved",
            lambda: jolt.JoltSkeletonComponentRequestBus(
                bus.Event,
                "FindAnimation",
                skeleton_id,
                animation_names[0],
            ),
            lambda handle: handle.IsValid(),
        )
        recorder.result(Tests.skeleton_operational, len(animation_names) == 1)
    finally:
        if entered_game_mode:
            exit_game_mode(recorder)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_SavedComponents)
