"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Verifies the checked-in Jolt level exports and instantiates authored provider assets.
"""


class Tests:
    level_opened = ("Opened the saved Jolt level", "Failed to open the saved Jolt level")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    body_operational = ("Simulated the saved rigid body", "The saved rigid body was not operational")
    scene_operational = ("Instantiated the saved scene asset", "The saved scene asset was not operational")
    skeleton_operational = ("Loaded the saved skeleton asset", "The saved skeleton asset was not operational")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Jolt_SavedComponents():
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general

    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("", "JoltSavedComponents")
    Report.result(Tests.level_opened, general.get_current_level_name() == "JoltSavedComponents")

    helper.enter_game_mode(Tests.enter_game_mode)
    body_id = general.find_game_entity("Jolt Saved Falling Body")
    scene_id = general.find_game_entity("Jolt Saved Scene")
    skeleton_id = general.find_game_entity("Jolt Saved Skeleton")

    Report.result(
        Tests.body_operational,
        jolt.JoltRigidBodyRequestBus(bus.Event, "IsSimulationEnabled", body_id)
        and helper.wait_for_condition(
            lambda: components.TransformBus(bus.Event, "GetWorldZ", body_id) < 4.5,
            3.0,
        ),
    )
    Report.result(
        Tests.scene_operational,
        jolt.JoltSceneRequestBus(bus.Event, "IsReady", scene_id)
        and len(jolt.JoltSceneRequestBus(bus.Event, "CopyBodies", scene_id)) == 1,
    )
    Report.result(
        Tests.skeleton_operational,
        helper.wait_for_condition(
            lambda: jolt.JoltSkeletonComponentRequestBus(bus.Event, "IsReady", skeleton_id),
            3.0,
        )
        and len(jolt.JoltSkeletonComponentRequestBus(bus.Event, "CopyAnimationNames", skeleton_id)) == 1,
    )
    helper.exit_game_mode(Tests.exit_game_mode)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_SavedComponents)
