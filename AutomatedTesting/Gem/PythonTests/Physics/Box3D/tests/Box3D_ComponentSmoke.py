"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Verifies that provider-owned Box3D components simulate while PhysX remains enabled in the same project.
"""


class Tests:
    level_opened = ("Opened the Box3D component level", "Failed to open the Box3D component level")
    box_body_created = ("Created a Box3D rigid body", "Failed to create a Box3D rigid body")
    physx_body_created = ("Created a PhysX rigid body", "Failed to create a PhysX rigid body")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    simulation_enabled = ("Box3D simulation is enabled", "Box3D simulation did not enable")
    body_fell = ("Box3D body moved under gravity", "Box3D body did not move under gravity")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Box3D_ComponentSmoke():
    import azlmbr.box3d as box3d
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("Physics/Box3D", "ComponentSmoke")
    Report.result(Tests.level_opened, general.get_current_level_name() == "ComponentSmoke")

    falling_body = EditorEntity.create_editor_entity("Box3D Falling Body")
    falling_body.add_component("Box3D Collider")
    falling_body.add_component("Box3D Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", falling_body.id, math.Vector3(0.0, 0.0, 5.0))
    Report.result(
        Tests.box_body_created,
        falling_body.has_component("Box3D Collider") and falling_body.has_component("Box3D Rigid Body"),
    )

    physx_body = EditorEntity.create_editor_entity("PhysX Coexistence Body")
    physx_body.add_component("PhysX Primitive Collider")
    physx_body.add_component("PhysX Static Rigid Body")
    Report.result(
        Tests.physx_body_created,
        physx_body.has_component("PhysX Primitive Collider") and physx_body.has_component("PhysX Static Rigid Body"),
    )

    helper.enter_game_mode(Tests.enter_game_mode)
    game_entity_id = general.find_game_entity("Box3D Falling Body")
    Report.result(
        Tests.simulation_enabled,
        box3d.Box3DRigidBodyRequestBus(bus.Event, "IsSimulationEnabled", game_entity_id),
    )
    Report.result(
        Tests.body_fell,
        helper.wait_for_condition(
            lambda: components.TransformBus(bus.Event, "GetWorldZ", game_entity_id) < 4.5,
            3.0,
        ),
    )
    helper.exit_game_mode(Tests.exit_game_mode)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Box3D_ComponentSmoke)
