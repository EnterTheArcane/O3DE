"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Verifies that Jolt simulates and remains scriptable while the Box3D and PhysX Gems are loaded.
"""


class Tests:
    level_opened = ("Opened the Jolt component level", "Failed to open the Jolt component level")
    components_created = ("Created Jolt, Box3D, and PhysX bodies", "Failed to create all provider bodies")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    simulation_enabled = ("Jolt simulation is enabled", "Jolt simulation did not enable")
    generic_body_access = ("Queried the shared Jolt body bus", "The shared Jolt body bus was unavailable")
    handles_valid = ("Resolved valid Jolt handles", "Failed to resolve valid Jolt handles")
    body_moved = ("Received Jolt body movement", "Did not receive Jolt body movement")
    body_fell = ("Jolt body moved under gravity", "Jolt body did not move under gravity")
    raycast_found_body = ("Raycast found Jolt geometry", "Raycast did not find Jolt geometry")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Jolt_ComponentSmoke():
    import time

    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("", "Base")
    Report.result(Tests.level_opened, general.get_current_level_name() == "Base")

    floor = EditorEntity.create_editor_entity("Jolt Floor")
    floor.add_component("Jolt Collider")
    floor.add_component("Jolt Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", floor.id, math.Vector3(0.0, 0.0, -5.0))
    components.TransformBus(bus.Event, "SetLocalUniformScale", floor.id, 10.0)

    falling_body = EditorEntity.create_editor_entity("Jolt Falling Body")
    falling_body.add_component("Jolt Collider")
    falling_body.add_component("Jolt Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", falling_body.id, math.Vector3(0.0, 0.0, 5.0))

    box_body = EditorEntity.create_editor_entity("Box3D Coexistence Body")
    box_body.add_component("Box3D Collider")
    box_body.add_component("Box3D Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", box_body.id, math.Vector3(20.0, 0.0, 0.0))

    physx_body = EditorEntity.create_editor_entity("PhysX Coexistence Body")
    physx_body.add_component("PhysX Primitive Collider")
    physx_body.add_component("PhysX Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", physx_body.id, math.Vector3(40.0, 0.0, 0.0))

    Report.result(
        Tests.components_created,
        floor.has_component("Jolt Static Rigid Body")
        and falling_body.has_component("Jolt Rigid Body")
        and box_body.has_component("Box3D Static Rigid Body")
        and physx_body.has_component("PhysX Static Rigid Body"),
    )

    Report.info("Entering game mode")
    general.enter_game_mode()
    enter_deadline = time.monotonic() + 3.0
    while not general.is_in_game_mode() and time.monotonic() < enter_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.enter_game_mode, general.is_in_game_mode())
    falling_body_id = general.find_game_entity("Jolt Falling Body")
    movement_received = {"value": False}

    def on_body_moved(args):
        movement_received["value"] = args[0].bodyHandle.IsValid()

    notification_handler = jolt.JoltBodyNotificationBusHandler()
    notification_handler.connect(falling_body_id)
    notification_handler.add_callback("OnBodyMoved", on_body_moved)

    gravity_frozen = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetGravityFactor",
        falling_body_id,
        0.0,
    )
    reset_position = jolt.WorldPosition()
    reset_position.x = 0.0
    reset_position.y = 0.0
    reset_position.z = 3.0
    position_reset = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetPosition",
        falling_body_id,
        reset_position,
        True,
    )
    velocities_reset = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetVelocities",
        falling_body_id,
        math.Vector3(0.0, 0.0, 0.0),
        math.Vector3(0.0, 0.0, 0.0),
    )
    general.idle_wait(0.05)

    simulation_enabled = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "IsSimulationEnabled",
        falling_body_id,
    )
    Report.result(Tests.simulation_enabled, simulation_enabled)

    generic_simulation_enabled = jolt.JoltBodyRequestBus(
        bus.Event,
        "IsSimulationEnabled",
        falling_body_id,
    )
    generic_world_handle = jolt.JoltBodyRequestBus(
        bus.Event,
        "GetWorldHandle",
        falling_body_id,
    )
    generic_body_handle = jolt.JoltBodyRequestBus(
        bus.Event,
        "GetBodyHandle",
        falling_body_id,
    )
    generic_user_data_set = jolt.JoltBodyRequestBus(
        bus.Event,
        "SetUserData",
        falling_body_id,
        0x12345678,
    )
    generic_user_data = jolt.JoltBodyRequestBus(
        bus.Event,
        "GetUserData",
        falling_body_id,
    )
    generic_center_of_mass = jolt.JoltBodyRequestBus(
        bus.Event,
        "GetCenterOfMassTransform",
        falling_body_id,
    )
    Report.info(
        "Generic body state: "
        f"enabled={generic_simulation_enabled}, "
        f"world={generic_world_handle.IsValid()}, body={generic_body_handle.IsValid()}, "
        f"height={generic_center_of_mass.position.z}"
    )
    Report.result(
        Tests.generic_body_access,
        generic_simulation_enabled
        and gravity_frozen
        and position_reset
        and velocities_reset
        and generic_world_handle.IsValid()
        and generic_body_handle.IsValid()
        and generic_user_data_set
        and generic_user_data == 0x12345678
        and generic_center_of_mass.position.z > 0.0,
    )

    world_handle = jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", falling_body_id)
    body_handle = jolt.JoltRigidBodyRequestBus(bus.Event, "GetBodyHandle", falling_body_id)
    shape_handle = jolt.JoltColliderRequestBus(bus.Event, "GetRootShapeHandle", falling_body_id)
    Report.result(
        Tests.handles_valid,
        world_handle.IsValid() and body_handle.IsValid() and shape_handle.IsValid(),
    )

    raycast = jolt.RaycastRequest()
    raycast_start = jolt.WorldPosition()
    raycast_start.x = 0.0
    raycast_start.y = 0.0
    raycast_start.z = 8.0
    raycast.start = raycast_start
    raycast.displacement = math.Vector3(0.0, 0.0, -12.0)
    raycast_result = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastClosest",
        world_handle,
        raycast,
    )
    Report.result(
        Tests.raycast_found_body,
        raycast_result.found and raycast_result.hit.bodyHandle.IsValid(),
    )

    gravity_enabled = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetGravityFactor",
        falling_body_id,
        1.0,
    )

    fall_deadline = time.monotonic() + 3.0
    body_fell = components.TransformBus(bus.Event, "GetWorldZ", falling_body_id) < 2.5
    while not body_fell and time.monotonic() < fall_deadline:
        general.idle_wait(0.01)
        body_fell = components.TransformBus(bus.Event, "GetWorldZ", falling_body_id) < 2.5
    Report.result(Tests.body_fell, gravity_enabled and body_fell)
    Report.result(Tests.body_moved, movement_received["value"])
    Report.info(
        "Component smoke result: "
        f"moved={movement_received['value']}, raycast={raycast_result.found}"
    )

    notification_handler.disconnect()
    Report.info("Exiting game mode")
    general.exit_game_mode()
    exit_deadline = time.monotonic() + 3.0
    while general.is_in_game_mode() and time.monotonic() < exit_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.exit_game_mode, not general.is_in_game_mode())


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_ComponentSmoke)
