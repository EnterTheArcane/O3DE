"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Exercises the complete Jolt editor component surface and core runtime query and mutation buses.
"""


class Tests:
    level_opened = ("Opened the Jolt feature level", "Failed to open the Jolt feature level")
    components_created = ("Created the Jolt feature components", "Failed to create the Jolt feature components")
    components_serialized = ("Serialized Jolt component configurations", "Failed to serialize Jolt configurations")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    core_components_enabled = ("Enabled core Jolt runtime components", "Core Jolt components did not enable")
    characters_operational = ("Exercised both Jolt character controllers", "A Jolt character controller failed")
    vehicles_operational = ("Exercised all Jolt vehicle controllers", "A Jolt vehicle controller failed")
    body_mutated = ("Mutated a Jolt body through its script bus", "Failed to mutate the Jolt body")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Jolt_FeatureComponents():
    import time

    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper
    from editor_python_test_tools.utils import Tracer

    helper.init_idle()
    helper.open_level("", "Base")
    Report.result(Tests.level_opened, general.get_current_level_name() == "Base")

    with Tracer() as serialization_tracer:
        rigid_body = EditorEntity.create_editor_entity("Jolt Feature Rigid Body")
        rigid_body.add_component("Jolt Collider")
        rigid_body.add_component("Jolt Rigid Body")
        components.TransformBus(bus.Event, "SetWorldTranslation", rigid_body.id, math.Vector3(0.0, 0.0, 4.0))

        static_body = EditorEntity.create_editor_entity("Jolt Feature Static Body")
        static_body.add_component("Jolt Collider")
        static_body.add_component("Jolt Static Rigid Body")
        components.TransformBus(bus.Event, "SetWorldTranslation", static_body.id, math.Vector3(0.0, 0.0, -0.5))
        components.TransformBus(bus.Event, "SetLocalUniformScale", static_body.id, 8.0)

        character = EditorEntity.create_editor_entity("Jolt Feature Character")
        character.add_component("Jolt Collider")
        character.add_component("Jolt Character Controller")
        components.TransformBus(bus.Event, "SetWorldTranslation", character.id, math.Vector3(10.0, 0.0, 2.0))

        virtual_character = EditorEntity.create_editor_entity("Jolt Feature Virtual Character")
        virtual_character.add_component("Jolt Collider")
        virtual_character.add_component("Jolt Virtual Character Controller")
        components.TransformBus(
            bus.Event,
            "SetWorldTranslation",
            virtual_character.id,
            math.Vector3(15.0, 0.0, 2.0),
        )

        constraint_body = EditorEntity.create_editor_entity("Jolt Constraint Body")
        constraint_body.add_component("Jolt Collider")
        constraint_body.add_component("Jolt Rigid Body")
        constraint_body.add_component("Jolt Constraint")
        components.TransformBus(
            bus.Event,
            "SetWorldTranslation",
            constraint_body.id,
            math.Vector3(20.0, 0.0, 3.0),
        )

        vehicle_components = (
            "Jolt Motorcycle",
            "Jolt Tracked Vehicle",
            "Jolt Wheeled Vehicle",
        )
        vehicle_entities = []
        for component_name in vehicle_components:
            entity = EditorEntity.create_editor_entity(component_name)
            entity.add_component("Jolt Collider")
            entity.add_component("Jolt Rigid Body")
            entity.add_component(component_name)
            vehicle_entities.append((component_name, entity))

        Report.result(
            Tests.components_created,
            rigid_body.has_component("Jolt Rigid Body")
            and static_body.has_component("Jolt Static Rigid Body")
            and character.has_component("Jolt Character Controller")
            and virtual_character.has_component("Jolt Virtual Character Controller")
            and constraint_body.has_component("Jolt Constraint")
            and all(entity.has_component(name) for name, entity in vehicle_entities),
        )

    reflection_errors = [
        entry
        for entry in serialization_tracer.errors
        if entry.window == "Reflection" or "Jolt" in entry.message
    ]
    serialization_warnings = [
        entry
        for entry in serialization_tracer.warnings
        if "Jolt" in entry.message or "AZStd::variant" in entry.message
    ]
    Report.result(
        Tests.components_serialized,
        not reflection_errors and not serialization_warnings,
    )

    Report.info("Entering game mode")
    general.enter_game_mode()
    enter_deadline = time.monotonic() + 3.0
    while not general.is_in_game_mode() and time.monotonic() < enter_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.enter_game_mode, general.is_in_game_mode())
    general.idle_wait(0.05)
    rigid_body_id = general.find_game_entity("Jolt Feature Rigid Body")
    static_body_id = general.find_game_entity("Jolt Feature Static Body")
    character_id = general.find_game_entity("Jolt Feature Character")
    virtual_character_id = general.find_game_entity("Jolt Feature Virtual Character")
    motorcycle_id = general.find_game_entity("Jolt Motorcycle")
    tracked_vehicle_id = general.find_game_entity("Jolt Tracked Vehicle")
    wheeled_vehicle_id = general.find_game_entity("Jolt Wheeled Vehicle")

    rigid_enabled = jolt.JoltRigidBodyRequestBus(bus.Event, "IsSimulationEnabled", rigid_body_id)
    static_enabled = jolt.JoltStaticRigidBodyRequestBus(
        bus.Event,
        "IsSimulationEnabled",
        static_body_id,
    )
    character_enabled = jolt.JoltCharacterRequestBus(
        bus.Event,
        "IsSimulationEnabled",
        character_id,
    )
    virtual_character_enabled = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "IsSimulationEnabled",
        virtual_character_id,
    )
    Report.result(
        Tests.core_components_enabled,
        rigid_enabled and static_enabled and character_enabled and virtual_character_enabled,
    )

    character_state = jolt.JoltCharacterRequestBus(bus.Event, "GetState", character_id)
    character_configuration = jolt.JoltCharacterRequestBus(
        bus.Event,
        "GetRuntimeConfiguration",
        character_id,
    )
    character_updated = jolt.JoltCharacterRequestBus(
        bus.Event,
        "UpdateRuntimeConfiguration",
        character_id,
        character_configuration,
    )
    character_velocity_set = jolt.JoltCharacterRequestBus(
        bus.Event,
        "SetVelocity",
        character_id,
        math.Vector3(0.25, 0.0, 0.0),
    )
    character_impulse_added = jolt.JoltCharacterRequestBus(
        bus.Event,
        "AddImpulse",
        character_id,
        math.Vector3(0.0, 0.0, 0.1),
    )
    character_user_data_set = jolt.JoltBodyRequestBus(
        bus.Event,
        "SetUserData",
        character_id,
        0x12345678,
    )
    character_user_data = jolt.JoltBodyRequestBus(
        bus.Event,
        "GetUserData",
        character_id,
    )

    virtual_moved = {"value": False}

    def on_virtual_character_moved(args):
        virtual_moved["value"] = args[0].characterHandle.IsValid()

    virtual_notification_handler = jolt.JoltVirtualCharacterNotificationBusHandler()
    virtual_notification_handler.connect(virtual_character_id)
    virtual_notification_handler.add_callback("OnCharacterMoved", on_virtual_character_moved)
    virtual_tracking_started = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "BeginContactTracking",
        virtual_character_id,
    )
    virtual_velocity_set = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "SetVelocity",
        virtual_character_id,
        math.Vector3(0.25, 0.0, 0.0),
    )
    virtual_configuration = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "GetRuntimeConfiguration",
        virtual_character_id,
    )
    virtual_updated = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "UpdateRuntimeConfiguration",
        virtual_character_id,
        virtual_configuration,
    )
    virtual_contacts_refreshed = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "RefreshContacts",
        virtual_character_id,
    )
    virtual_contacts = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "GetContacts",
        virtual_character_id,
    )
    virtual_state = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "GetState",
        virtual_character_id,
    )
    virtual_user_data_set = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "SetUserData",
        virtual_character_id,
        0x87654321,
    )
    virtual_user_data = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "GetUserData",
        virtual_character_id,
    )
    virtual_tracking_ended = jolt.JoltVirtualCharacterRequestBus(
        bus.Event,
        "EndContactTracking",
        virtual_character_id,
    )
    virtual_move_deadline = time.monotonic() + 3.0
    while not virtual_moved["value"] and time.monotonic() < virtual_move_deadline:
        general.idle_wait(0.01)
    virtual_move_received = virtual_moved["value"]
    Report.result(
        Tests.characters_operational,
        character_state.shapeHandle.IsValid()
        and character_updated
        and character_velocity_set
        and character_impulse_added
        and character_user_data_set
        and character_user_data == 0x12345678
        and virtual_tracking_started
        and virtual_velocity_set
        and virtual_updated
        and virtual_contacts_refreshed
        and virtual_contacts is not None
        and virtual_state.shapeHandle.IsValid()
        and virtual_user_data_set
        and virtual_user_data == 0x87654321
        and virtual_tracking_ended
        and virtual_move_received,
    )
    virtual_notification_handler.disconnect()

    wheeled_input = jolt.WheeledVehicleInput()
    wheeled_input.forward = 0.25
    wheeled_enabled = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "EnableSimulation",
        wheeled_vehicle_id,
    )
    wheeled_state = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "GetState",
        wheeled_vehicle_id,
    )
    wheeled_wheels = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "CopyWheelStates",
        wheeled_vehicle_id,
    )
    wheeled_anti_roll_bars = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "CopyAntiRollBars",
        wheeled_vehicle_id,
    )
    wheeled_differentials = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "CopyDifferentials",
        wheeled_vehicle_id,
    )
    wheeled_input_set = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "SetInput",
        wheeled_vehicle_id,
        wheeled_input,
    )
    wheel_motion = jolt.WheelMotion()
    wheel_motion.angularVelocity = 0.25
    wheel_motion_set = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "SetWheelMotion",
        wheeled_vehicle_id,
        0,
        wheel_motion,
    )
    powertrain_control = jolt.VehiclePowertrainControl()
    powertrain_control.currentGear = 1
    powertrain_control.engineRpm = 1500.0
    powertrain_set = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "SetPowertrainControl",
        wheeled_vehicle_id,
        powertrain_control,
    )
    engine_configuration = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "GetEngineConfiguration",
        wheeled_vehicle_id,
    )
    engine_configuration.maximumTorque = 550.0
    engine_updated = jolt.JoltWheeledVehicleRequestBus(
        bus.Event,
        "UpdateEngineConfiguration",
        wheeled_vehicle_id,
        engine_configuration,
    )

    motorcycle_input = jolt.WheeledVehicleInput()
    motorcycle_input.forward = 0.2
    motorcycle_enabled = jolt.JoltMotorcycleRequestBus(
        bus.Event,
        "EnableSimulation",
        motorcycle_id,
    )
    motorcycle_state = jolt.JoltMotorcycleRequestBus(
        bus.Event,
        "GetState",
        motorcycle_id,
    )
    motorcycle_wheels = jolt.JoltMotorcycleRequestBus(
        bus.Event,
        "CopyWheelStates",
        motorcycle_id,
    )
    motorcycle_input_set = jolt.JoltMotorcycleRequestBus(
        bus.Event,
        "SetInput",
        motorcycle_id,
        motorcycle_input,
    )
    motorcycle_controller = jolt.MotorcycleControllerUpdateConfiguration()
    motorcycle_controller.leanSmoothingFactor = 0.75
    motorcycle_controller_updated = jolt.JoltMotorcycleRequestBus(
        bus.Event,
        "UpdateController",
        motorcycle_id,
        motorcycle_controller,
    )

    tracked_input = jolt.TrackedVehicleInput()
    tracked_input.forward = 0.2
    tracked_enabled = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "EnableSimulation",
        tracked_vehicle_id,
    )
    wheeled_body_handle = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "GetBodyHandle",
        wheeled_vehicle_id,
    )
    motorcycle_body_handle = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "GetBodyHandle",
        motorcycle_id,
    )
    tracked_body_handle = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "GetBodyHandle",
        tracked_vehicle_id,
    )
    tracked_state = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "GetState",
        tracked_vehicle_id,
    )
    tracked_wheels = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "CopyWheelStates",
        tracked_vehicle_id,
    )
    tracked_input_set = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "SetInput",
        tracked_vehicle_id,
        tracked_input,
    )
    track_velocity_set = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "SetTrackAngularVelocity",
        tracked_vehicle_id,
        0,
        0.25,
    )
    track_configuration = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "GetTrackConfiguration",
        tracked_vehicle_id,
        0,
    )
    track_updated = jolt.JoltTrackedVehicleRequestBus(
        bus.Event,
        "UpdateTrackConfiguration",
        tracked_vehicle_id,
        0,
        track_configuration,
    )
    Report.info(
        "Vehicle state: "
        f"chassis=({wheeled_body_handle.IsValid()}, {motorcycle_body_handle.IsValid()}, "
        f"{tracked_body_handle.IsValid()}), "
        f"wheeled=({wheeled_enabled}, {wheeled_state.bodyHandle.IsValid()}, {wheeled_state.wheelCount}, "
        f"{wheeled_input_set}, {wheel_motion_set}, {powertrain_set}, {engine_updated}), "
        f"motorcycle=({motorcycle_enabled}, {motorcycle_state.wheeled.bodyHandle.IsValid()}, "
        f"{motorcycle_state.wheeled.wheelCount}, {motorcycle_input_set}, "
        f"{motorcycle_controller_updated}), "
        f"tracked=({tracked_enabled}, {tracked_state.bodyHandle.IsValid()}, {tracked_state.wheelCount}, "
        f"{tracked_input_set}, {track_velocity_set}, {track_updated})"
    )
    Report.result(
        Tests.vehicles_operational,
        wheeled_enabled
        and wheeled_state.bodyHandle.IsValid()
        and wheeled_state.wheelCount == 4
        and len(wheeled_wheels) == wheeled_state.wheelCount
        and len(wheeled_anti_roll_bars) == 2
        and len(wheeled_differentials) == 1
        and wheeled_input_set
        and wheel_motion_set
        and powertrain_set
        and engine_updated
        and motorcycle_enabled
        and motorcycle_state.wheeled.bodyHandle.IsValid()
        and motorcycle_state.wheeled.wheelCount == 2
        and len(motorcycle_wheels) == motorcycle_state.wheeled.wheelCount
        and motorcycle_input_set
        and motorcycle_controller_updated
        and tracked_enabled
        and tracked_state.bodyHandle.IsValid()
        and tracked_state.wheelCount == 6
        and len(tracked_wheels) == tracked_state.wheelCount
        and tracked_input_set
        and track_velocity_set
        and track_updated,
    )

    body_mutated = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetVelocities",
        rigid_body_id,
        math.Vector3(2.0, 0.0, 0.0),
        math.Vector3(0.0, 0.0, 0.5),
    )
    body_mutated = body_mutated and jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "AddImpulse",
        rigid_body_id,
        math.Vector3(0.0, 0.0, 1.0),
    )
    state = jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", rigid_body_id)
    Report.result(
        Tests.body_mutated,
        body_mutated and state.shapeHandle.IsValid() and state.isActive,
    )

    Report.info("Exiting game mode")
    general.exit_game_mode()
    exit_deadline = time.monotonic() + 3.0
    while general.is_in_game_mode() and time.monotonic() < exit_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.exit_game_mode, not general.is_in_game_mode())


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_FeatureComponents)
