"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Focused integration scenarios for the standalone Jolt Gem.
"""

try:
    from .Jolt_ScenarioRecorder import ScenarioRecorder
    from .Jolt_ScenarioUtilities import create_basic_world
    from .Jolt_ScenarioUtilities import create_editor_entity
    from .Jolt_ScenarioUtilities import create_world_position
    from .Jolt_ScenarioUtilities import enter_game_mode
    from .Jolt_ScenarioUtilities import exit_game_mode
    from .Jolt_ScenarioUtilities import is_restore_complete
    from .Jolt_ScenarioUtilities import open_level
    from .Jolt_ScenarioUtilities import wait_for_condition
    from .Jolt_ScenarioUtilities import wait_for_runtime_entity
except ImportError:
    from Jolt_ScenarioRecorder import ScenarioRecorder
    from Jolt_ScenarioUtilities import create_basic_world
    from Jolt_ScenarioUtilities import create_editor_entity
    from Jolt_ScenarioUtilities import create_world_position
    from Jolt_ScenarioUtilities import enter_game_mode
    from Jolt_ScenarioUtilities import exit_game_mode
    from Jolt_ScenarioUtilities import is_restore_complete
    from Jolt_ScenarioUtilities import open_level
    from Jolt_ScenarioUtilities import wait_for_condition
    from Jolt_ScenarioUtilities import wait_for_runtime_entity


def _run_game_scenario(name, setup, runtime):
    recorder = ScenarioRecorder(name)
    entered_game_mode = False
    try:
        if setup(recorder):
            entered_game_mode = enter_game_mode(recorder)
        if entered_game_mode:
            runtime(recorder)
    except Exception as exception:
        recorder.check("scenario completed without an unhandled exception", False, exception)
    finally:
        if entered_game_mode:
            exit_game_mode(recorder)
        recorder.finish()


def _open_gallery(recorder):
    return open_level(recorder, "Physics/Jolt", "FeatureGallery")


def _open_base_with_world(recorder, prefix):
    import azlmbr.math as math

    if not open_level(recorder, "", "Base"):
        return False

    create_basic_world(recorder, prefix)
    return recorder.check(
        "basic world authored",
        lambda: math.Vector3(0.0, 0.0, 1.0).IsFinite(),
    )


def run_shapes_and_cooking():
    shape_names = (
        "Box",
        "Capsule",
        "Convex Hull",
        "Cylinder",
        "Empty",
        "Heightfield",
        "Mesh",
        "Plane",
        "Sphere",
        "Tapered Capsule",
        "Tapered Cylinder",
        "Triangle",
    )

    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt

        for shape_name in shape_names:
            entity_id = wait_for_runtime_entity(f"Jolt Gallery {shape_name}")
            recorder.check(f"{shape_name} runtime entity", entity_id.IsValid())
            if entity_id.IsValid():
                shape_count = recorder.capture(
                    f"{shape_name} shape count",
                    lambda entity_id=entity_id: jolt.JoltColliderRequestBus(
                        bus.Event,
                        "GetShapeCount",
                        entity_id,
                    ),
                    lambda value: value == 1,
                    0,
                )
                if shape_count == 1:
                    recorder.capture(
                        f"{shape_name} root handle",
                        lambda entity_id=entity_id: jolt.JoltColliderRequestBus(
                            bus.Event,
                            "GetRootShapeHandle",
                            entity_id,
                        ),
                        lambda handle: handle.IsValid(),
                    )

        scene_id = wait_for_runtime_entity("Jolt Gallery Scene")
        recorder.capture(
            "cooked gallery scene ready",
            lambda: wait_for_condition(
                lambda: jolt.JoltSceneRequestBus(bus.Event, "IsReady", scene_id),
            ),
        )
        recorder.capture(
            "cooked gallery scene instantiated",
            lambda: jolt.JoltSceneRequestBus(bus.Event, "CopyBodies", scene_id),
            lambda bodies: len(bodies) > 0,
            [],
        )

    _run_game_scenario("Jolt_ShapesAndCooking", _open_gallery, runtime)


def run_constraints():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt

        path_id = wait_for_runtime_entity("Jolt Gallery Path")
        recorder.check("path runtime entity", path_id.IsValid())

        constraint_names = (
            "Cone",
            "Custom",
            "Distance",
            "Fixed",
            "Gear",
            "Hinge",
            "Path",
            "Point",
            "Pulley",
            "Rack And Pinion",
            "Six Dof",
            "Slider",
            "Swing Twist",
        )
        for constraint_name in constraint_names:
            constraint_id = wait_for_runtime_entity(f"Jolt Gallery Constraint {constraint_name}")
            recorder.check(f"{constraint_name} constraint runtime entity", constraint_id.IsValid())
            if constraint_name == "Custom":
                recorder.capture(
                    "missing custom-constraint provider rejected safely",
                    lambda constraint_id=constraint_id: jolt.JoltConstraintRequestBus(
                        bus.Event,
                        "GetConstraintHandle",
                        constraint_id,
                    ),
                    lambda handle: not handle.IsValid(),
                )
                continue

            recorder.capture(
                f"{constraint_name} constraint handle",
                lambda constraint_id=constraint_id: jolt.JoltConstraintRequestBus(
                    bus.Event,
                    "GetConstraintHandle",
                    constraint_id,
                ),
                lambda handle: handle.IsValid(),
            )

        constraint_id = wait_for_runtime_entity("Jolt Gallery Constraint Cone")
        recorder.capture(
            "constraint user data mutation",
            lambda: jolt.JoltConstraintRequestBus(
                bus.Event,
                "SetUserData",
                constraint_id,
                0x4A6F6C74,
            ),
        )
        recorder.capture(
            "constraint user data readback",
            lambda: jolt.JoltConstraintRequestBus(
                bus.Event,
                "GetUserData",
                constraint_id,
            ),
            lambda value: value == 0x4A6F6C74,
            0,
        )
        recorder.capture(
            "constraint warm start reset",
            lambda: jolt.JoltConstraintRequestBus(bus.Event, "ResetWarmStart", constraint_id),
        )
        recorder.capture(
            "path handle",
            lambda: jolt.JoltPathRequestBus(bus.Event, "GetPathHandle", path_id),
            lambda handle: handle.IsValid(),
        )

    _run_game_scenario("Jolt_Constraints", _open_gallery, runtime)


def run_events_and_filters():
    def setup(recorder):
        import azlmbr.math as math

        if not _open_base_with_world(recorder, "Jolt Events"):
            return False

        create_editor_entity(
            recorder,
            "Box3D Coexistence Body",
            ("Box3D Collider", "Box3D Static Rigid Body"),
            math.Vector3(20.0, 0.0, 0.0),
        )
        create_editor_entity(
            recorder,
            "PhysX Coexistence Body",
            ("PhysX Primitive Collider", "PhysX Static Rigid Body"),
            math.Vector3(40.0, 0.0, 0.0),
        )
        create_editor_entity(
            recorder,
            "Jolt Filter Target",
            ("Jolt Collider", "Jolt Static Rigid Body"),
            math.Vector3(3.0, 0.0, 0.0),
        )
        return True

    def runtime(recorder):
        import time

        import azlmbr.bus as bus
        import azlmbr.components as components
        import azlmbr.jolt as jolt
        import azlmbr.legacy.general as general
        import azlmbr.math as math

        body_id = wait_for_runtime_entity("Jolt Events Body")
        recorder.check(
            "Box3D coexists with Jolt",
            wait_for_runtime_entity("Box3D Coexistence Body").IsValid(),
        )
        recorder.check(
            "PhysX coexists with Jolt",
            wait_for_runtime_entity("PhysX Coexistence Body").IsValid(),
        )
        filter_target_id = wait_for_runtime_entity("Jolt Filter Target")
        recorder.check("filter target runtime entity", filter_target_id.IsValid())
        components.TransformBus(
            bus.Event,
            "SetWorldTranslation",
            filter_target_id,
            math.Vector3(3.0, 0.0, 0.0),
        )
        general.idle_wait(0.05)
        runtime_target_position = components.TransformBus(
            bus.Event,
            "GetWorldTranslation",
            filter_target_id,
        )
        recorder.check(
            "filter target transform position",
            runtime_target_position.IsClose(math.Vector3(3.0, 0.0, 0.0)),
            runtime_target_position,
        )
        recorder.capture(
            "filter target body handle",
            lambda: jolt.JoltStaticRigidBodyRequestBus(
                bus.Event,
                "GetBodyHandle",
                filter_target_id,
            ),
            lambda handle: handle.IsValid(),
        )
        recorder.capture(
            "filter target state",
            lambda: jolt.JoltStaticRigidBodyRequestBus(
                bus.Event,
                "GetState",
                filter_target_id,
            ),
            lambda state: state.isInSimulation and state.shapeHandle.IsValid(),
        )
        filter_target_world_handle = recorder.capture(
            "filter target world handle",
            lambda: jolt.JoltStaticRigidBodyRequestBus(
                bus.Event,
                "GetWorldHandle",
                filter_target_id,
            ),
            lambda handle: handle.IsValid(),
        )
        world_handle = recorder.capture(
            "world handle",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", body_id),
            lambda handle: handle.IsValid(),
        )

        received = {"contact": False, "movement": False}

        def on_contact(args):
            received["contact"] = args[0].firstBodyHandle.IsValid()

        def on_body_moved(args):
            received["movement"] = args[0].bodyHandle.IsValid()

        handler = jolt.JoltWorldNotificationBusHandler()
        handler.connect(world_handle)
        handler.add_callback("OnContact", on_contact)
        handler.add_callback("OnBodyMoved", on_body_moved)

        configuration = recorder.capture(
            "world configuration read",
            lambda: jolt.JoltWorldRequestBus(
                bus.Broadcast,
                "GetRuntimeConfiguration",
                world_handle,
            ),
            lambda value: value is not None,
        )
        if configuration is not None:
            configuration.collectContactEvents = True
            recorder.capture(
                "contact event collection enabled",
                lambda: jolt.JoltWorldRequestBus(
                    bus.Broadcast,
                    "UpdateRuntimeConfiguration",
                    world_handle,
                    configuration,
                ),
            )

        collision_start = create_world_position(jolt, 0.0, 0.0, 1.75)
        recorder.capture(
            "contact trajectory started",
            lambda: jolt.JoltRigidBodyRequestBus(
                bus.Event,
                "SetPosition",
                body_id,
                collision_start,
                True,
            )
            and jolt.JoltRigidBodyRequestBus(
                bus.Event,
                "SetVelocities",
                body_id,
                math.Vector3(0.0, 0.0, -5.0),
                math.Vector3(0.0, 0.0, 0.0),
            )
            and jolt.JoltRigidBodyRequestBus(
                bus.Event,
                "ActivateBody",
                body_id,
            ),
        )

        allowed_filter = jolt.JoltQueryFilter()
        allowed_filter.collisionLayer = jolt.JoltObjectLayer(2)
        allowed_raycast = jolt.JoltRaycastRequest()
        allowed_raycast.start = create_world_position(jolt, 3.0, 0.0, 4.0)
        allowed_raycast.displacement = math.Vector3(0.0, 0.0, -8.0)
        allowed_raycast.filter = allowed_filter
        unfiltered_raycast = jolt.JoltRaycastRequest()
        unfiltered_raycast.start = allowed_raycast.start
        unfiltered_raycast.displacement = allowed_raycast.displacement
        unfiltered_hit = jolt.JoltWorldQueryRequestBus(
            bus.Broadcast,
            "RaycastClosest",
            filter_target_world_handle,
            unfiltered_raycast,
        )
        recorder.check(
            "unfiltered ray reaches static target",
            unfiltered_hit.found
            and unfiltered_hit.hit.bodyHandle.IsValid()
            and unfiltered_hit.hit.shapeHandle.IsValid()
            and abs(unfiltered_hit.hit.position.x - 3.0) < 0.01
            and abs(unfiltered_hit.hit.position.y) < 0.01,
            (
                f"found={unfiltered_hit.found}, "
                f"position=({unfiltered_hit.hit.position.x}, "
                f"{unfiltered_hit.hit.position.y}, {unfiltered_hit.hit.position.z})"
            ),
        )
        allowed_hit = recorder.capture(
            "moving query layer includes non-moving geometry",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "RaycastClosest",
                filter_target_world_handle,
                allowed_raycast,
            ),
            lambda result: result.found
            and result.hit.bodyHandle.IsValid()
            and result.hit.shapeHandle.IsValid(),
        )

        denied_filter = jolt.JoltQueryFilter()
        denied_filter.collisionLayer = jolt.JoltObjectLayer(1)
        denied_raycast = jolt.JoltRaycastRequest()
        denied_raycast.start = allowed_raycast.start
        denied_raycast.displacement = allowed_raycast.displacement
        denied_raycast.filter = denied_filter
        denied_hit = recorder.capture(
            "non-moving query layer rejects non-moving geometry",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "RaycastClosest",
                filter_target_world_handle,
                denied_raycast,
            ),
            lambda result: not result.found,
        )
        recorder.check(
            "allowed and denied filters produced distinct results",
            allowed_hit.found and not denied_hit.found,
        )

        deadline = time.monotonic() + 5.0
        import azlmbr.legacy.general as general

        while not all(received.values()) and time.monotonic() < deadline:
            general.idle_wait(0.01)

        recorder.check("body movement event received", received["movement"])
        recorder.check("contact event received", received["contact"])
        handler.disconnect()

    _run_game_scenario("Jolt_EventsAndFilters", setup, runtime)


def run_characters():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt
        import azlmbr.math as math

        character_id = wait_for_runtime_entity("Jolt Gallery Character")
        virtual_id = wait_for_runtime_entity("Jolt Gallery Virtual Character")
        recorder.check("physical character entity", character_id.IsValid())
        recorder.check("virtual character entity", virtual_id.IsValid())

        recorder.capture(
            "physical character enabled",
            lambda: jolt.JoltCharacterRequestBus(bus.Event, "IsSimulationEnabled", character_id),
        )
        recorder.capture(
            "physical character velocity",
            lambda: jolt.JoltCharacterRequestBus(
                bus.Event,
                "SetVelocity",
                character_id,
                math.Vector3(0.25, 0.0, 0.0),
            ),
        )
        recorder.capture(
            "physical character state",
            lambda: jolt.JoltCharacterRequestBus(bus.Event, "GetState", character_id),
            lambda state: state.bodyHandle.IsValid()
            and state.shapeHandle.IsValid()
            and state.isInSimulation,
        )
        recorder.capture(
            "physical character disabled without destroying identity",
            lambda: jolt.JoltCharacterRequestBus(bus.Event, "DisableSimulation", character_id),
        )
        recorder.capture(
            "physical character reports disabled",
            lambda: not jolt.JoltCharacterRequestBus(bus.Event, "IsSimulationEnabled", character_id),
        )
        recorder.capture(
            "physical character resources remain while disabled",
            lambda: jolt.JoltCharacterRequestBus(bus.Event, "GetState", character_id),
            lambda state: state.bodyHandle.IsValid()
            and state.shapeHandle.IsValid()
            and not state.isInSimulation,
        )
        recorder.capture(
            "physical character re-enabled",
            lambda: jolt.JoltCharacterRequestBus(bus.Event, "EnableSimulation", character_id),
        )
        recorder.capture(
            "physical character membership restored",
            lambda: jolt.JoltCharacterRequestBus(bus.Event, "GetState", character_id),
            lambda state: state.bodyHandle.IsValid()
            and state.shapeHandle.IsValid()
            and state.isInSimulation,
        )
        recorder.capture(
            "virtual character contact tracking",
            lambda: jolt.JoltVirtualCharacterRequestBus(
                bus.Event,
                "BeginContactTracking",
                virtual_id,
            ),
        )
        recorder.capture(
            "virtual character velocity",
            lambda: jolt.JoltVirtualCharacterRequestBus(
                bus.Event,
                "SetVelocity",
                virtual_id,
                math.Vector3(0.25, 0.0, 0.0),
            ),
        )
        recorder.capture(
            "virtual character contacts refreshed",
            lambda: jolt.JoltVirtualCharacterRequestBus(
                bus.Event,
                "RefreshContacts",
                virtual_id,
            ),
        )
        recorder.capture(
            "virtual character state",
            lambda: jolt.JoltVirtualCharacterRequestBus(bus.Event, "GetState", virtual_id),
            lambda state: state.shapeHandle.IsValid(),
        )
        recorder.capture(
            "virtual character contact tracking ended",
            lambda: jolt.JoltVirtualCharacterRequestBus(
                bus.Event,
                "EndContactTracking",
                virtual_id,
            ),
        )

    _run_game_scenario("Jolt_Characters", _open_gallery, runtime)


def run_vehicles():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt

        vehicle_specs = (
            (
                "Wheeled Vehicle",
                jolt.JoltWheeledVehicleRequestBus,
                jolt.WheeledVehicleInput,
                4,
            ),
            (
                "Motorcycle",
                jolt.JoltMotorcycleRequestBus,
                jolt.WheeledVehicleInput,
                2,
            ),
            (
                "Tracked Vehicle",
                jolt.JoltTrackedVehicleRequestBus,
                jolt.TrackedVehicleInput,
                6,
            ),
        )
        for vehicle_name, request_bus, input_type, expected_wheel_count in vehicle_specs:
            entity_id = wait_for_runtime_entity(f"Jolt Gallery {vehicle_name}")
            recorder.check(f"{vehicle_name} runtime entity", entity_id.IsValid())
            recorder.capture(
                f"{vehicle_name} enabled",
                lambda request_bus=request_bus, entity_id=entity_id: request_bus(
                    bus.Event,
                    "EnableSimulation",
                    entity_id,
                ),
            )
            state = recorder.capture(
                f"{vehicle_name} state",
                lambda request_bus=request_bus, entity_id=entity_id: request_bus(
                    bus.Event,
                    "GetState",
                    entity_id,
                ),
                lambda value: value is not None,
            )
            if state is not None:
                wheel_count = state.wheelCount
                if vehicle_name == "Motorcycle":
                    wheel_count = state.wheeled.wheelCount
                recorder.check(f"{vehicle_name} wheel count", wheel_count == expected_wheel_count)

            vehicle_input = input_type()
            vehicle_input.forward = 0.25
            recorder.capture(
                f"{vehicle_name} input",
                lambda request_bus=request_bus, entity_id=entity_id, vehicle_input=vehicle_input: request_bus(
                    bus.Event,
                    "SetInput",
                    entity_id,
                    vehicle_input,
                ),
            )

    _run_game_scenario("Jolt_Vehicles", _open_gallery, runtime)


def run_soft_bodies():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt
        import azlmbr.math as math

        entity_id = wait_for_runtime_entity("Jolt Gallery Soft Body")
        recorder.check("soft body runtime entity", entity_id.IsValid())
        recorder.capture(
            "soft body enabled",
            lambda: jolt.JoltSoftBodyRequestBus(bus.Event, "IsSimulationEnabled", entity_id),
        )
        vertices = recorder.capture(
            "soft body vertices",
            lambda: jolt.JoltSoftBodyRequestBus(bus.Event, "CopyVertices", entity_id),
            lambda value: len(value) >= 4,
            [],
        )
        recorder.capture(
            "soft body faces",
            lambda: jolt.JoltSoftBodyRequestBus(bus.Event, "CopyFaces", entity_id),
            lambda value: len(value) >= 2,
            [],
        )
        if vertices:
            recorder.capture(
                "soft body vertex velocity",
                lambda: jolt.JoltSoftBodyRequestBus(
                    bus.Event,
                    "SetVertexVelocity",
                    entity_id,
                    0,
                    math.Vector3(0.0, 0.0, 0.25),
                ),
            )
        runtime_configuration = jolt.SoftBodyRuntimeConfiguration()
        recorder.capture(
            "soft body runtime configuration",
            lambda: jolt.JoltSoftBodyRequestBus(
                bus.Event,
                "GetRuntimeConfiguration",
                entity_id,
                runtime_configuration,
            ),
        )
        recorder.check(
            "soft body runtime configuration values",
            runtime_configuration.iterationCount > 0
            and runtime_configuration.maximumLinearVelocity > 0.0,
        )

    _run_game_scenario("Jolt_SoftBodies", _open_gallery, runtime)


def run_ragdolls_and_skeletons():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt

        ragdoll_id = wait_for_runtime_entity("Jolt Gallery Ragdoll")
        skeleton_id = wait_for_runtime_entity("Jolt Gallery Skeleton")
        recorder.capture(
            "ragdoll enabled",
            lambda: jolt.JoltRagdollRequestBus(bus.Event, "IsSimulationEnabled", ragdoll_id),
        )
        recorder.capture(
            "ragdoll handle",
            lambda: jolt.JoltRagdollRequestBus(bus.Event, "GetRagdollHandle", ragdoll_id),
            lambda handle: handle.IsValid(),
        )
        pose = recorder.capture(
            "ragdoll pose",
            lambda: jolt.JoltRagdollRequestBus(bus.Event, "CopyPose", ragdoll_id),
            lambda value: len(value) > 0,
            [],
        )
        if pose:
            recorder.capture(
                "ragdoll motor pose mutation",
                lambda: jolt.JoltRagdollRequestBus(
                    bus.Event,
                    "DriveMotors",
                    ragdoll_id,
                    pose,
                ),
            )

        recorder.capture(
            "skeleton asset ready",
            lambda: wait_for_condition(
                lambda: jolt.JoltSkeletonComponentRequestBus(bus.Event, "IsReady", skeleton_id),
            ),
        )
        recorder.capture(
            "skeleton handle",
            lambda: jolt.JoltSkeletonComponentRequestBus(
                bus.Event,
                "GetSkeletonHandle",
                skeleton_id,
            ),
            lambda handle: handle.IsValid(),
        )
        recorder.capture(
            "skeleton animations",
            lambda: jolt.JoltSkeletonComponentRequestBus(
                bus.Event,
                "CopyAnimationNames",
                skeleton_id,
            ),
            lambda names: len(names) > 0,
            [],
        )

    _run_game_scenario("Jolt_RagdollsAndSkeletons", _open_gallery, runtime)


def run_hair():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt
        import azlmbr.math as math

        entity_id = wait_for_runtime_entity("Jolt Gallery Hair")
        recorder.capture(
            "CPU Hair handle",
            lambda: jolt.JoltHairRequestBus(bus.Event, "GetHairHandle", entity_id),
            lambda handle: handle.IsValid(),
        )
        definition = recorder.capture(
            "CPU Hair definition",
            lambda: jolt.JoltHairRequestBus(bus.Event, "GetDefinitionState", entity_id),
            lambda state: state.simulationVertexCount > 0,
        )
        recorder.capture(
            "CPU Hair vertex readback",
            lambda: jolt.JoltHairRequestBus(bus.Event, "CopyVertexStates", entity_id),
            lambda states: len(states) == definition.simulationVertexCount,
            [],
        )
        recorder.capture(
            "CPU Hair transform update",
            lambda: jolt.JoltHairRequestBus(
                bus.Event,
                "SetScalpToHeadTransform",
                entity_id,
                math.Transform_CreateIdentity(),
            ),
        )
        recorder.capture(
            "CPU Hair automatic update disabled",
            lambda: jolt.JoltHairRequestBus(bus.Event, "DisableAutoUpdate", entity_id),
        )
        recorder.capture(
            "CPU Hair automatic update enabled",
            lambda: jolt.JoltHairRequestBus(
                bus.Event,
                "EnableAutoUpdate",
                entity_id,
                math.Transform_CreateIdentity(),
                [],
            ),
        )

    _run_game_scenario("Jolt_Hair", _open_gallery, runtime)


def run_scenes_and_assets():
    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt

        scene_id = wait_for_runtime_entity("Jolt Gallery Scene")
        skeleton_id = wait_for_runtime_entity("Jolt Gallery Skeleton")
        recorder.capture(
            "scene asset ready",
            lambda: wait_for_condition(
                lambda: jolt.JoltSceneRequestBus(bus.Event, "IsReady", scene_id),
            ),
        )
        recorder.capture(
            "scene definition handle",
            lambda: jolt.JoltSceneRequestBus(bus.Event, "GetDefinitionHandle", scene_id),
            lambda handle: handle.IsValid(),
        )
        recorder.capture(
            "scene instance handle",
            lambda: jolt.JoltSceneRequestBus(bus.Event, "GetInstanceHandle", scene_id),
            lambda handle: handle.IsValid(),
        )
        recorder.capture(
            "scene bodies",
            lambda: jolt.JoltSceneRequestBus(bus.Event, "CopyBodies", scene_id),
            lambda bodies: len(bodies) > 0,
            [],
        )
        recorder.capture(
            "skeleton asset ready",
            lambda: wait_for_condition(
                lambda: jolt.JoltSkeletonComponentRequestBus(bus.Event, "IsReady", skeleton_id),
            ),
        )
        animation_names = recorder.capture(
            "skeleton animation asset",
            lambda: jolt.JoltSkeletonComponentRequestBus(
                bus.Event,
                "CopyAnimationNames",
                skeleton_id,
            ),
            lambda names: len(names) == 1,
            [],
        )
        if animation_names:
            recorder.capture(
                "skeleton animation handle",
                lambda: jolt.JoltSkeletonComponentRequestBus(
                    bus.Event,
                    "FindAnimation",
                    skeleton_id,
                    animation_names[0],
                ),
                lambda handle: handle.IsValid(),
            )

    _run_game_scenario("Jolt_ScenesAndAssets", _open_gallery, runtime)


def run_queries():
    def setup(recorder):
        import azlmbr.math as math

        if not _open_base_with_world(recorder, "Jolt Queries"):
            return False

        create_editor_entity(
            recorder,
            "Jolt Queries Second Body",
            ("Jolt Collider", "Jolt Rigid Body"),
            math.Vector3(0.0, 0.0, 6.0),
        )
        return True

    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt
        import azlmbr.math as math

        body_id = wait_for_runtime_entity("Jolt Queries Body")
        second_body_id = wait_for_runtime_entity("Jolt Queries Second Body")
        world_handle = recorder.capture(
            "query world handle",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", body_id),
            lambda handle: handle.IsValid(),
        )
        recorder.capture(
            "query body frozen",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "SetGravityFactor", body_id, 0.0),
        )
        recorder.capture(
            "query body positioned",
            lambda: jolt.JoltRigidBodyRequestBus(
                bus.Event,
                "SetPosition",
                body_id,
                create_world_position(jolt, 0.0, 0.0, 3.0),
                True,
            ),
        )
        recorder.capture(
            "second query body frozen",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "SetGravityFactor", second_body_id, 0.0),
        )
        recorder.capture(
            "second query body positioned",
            lambda: jolt.JoltRigidBodyRequestBus(
                bus.Event,
                "SetPosition",
                second_body_id,
                create_world_position(jolt, 0.0, 0.0, 6.0),
                True,
            ),
        )

        raycast = jolt.JoltRaycastRequest()
        raycast.start = create_world_position(jolt, 0.0, 0.0, 8.0)
        raycast.displacement = math.Vector3(0.0, 0.0, -12.0)
        recorder.capture(
            "closest raycast",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "RaycastClosest",
                world_handle,
                raycast,
            ),
            lambda result: result.found,
        )
        recorder.capture(
            "all-hit raycast",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "RaycastAll",
                world_handle,
                raycast,
                16,
            ),
            lambda results: results.GetHitCount() >= 2 and not results.HasOverflow(),
        )

        point = jolt.PointOverlapRequest()
        point.position = create_world_position(jolt, 0.0, 0.0, 3.0)
        recorder.capture(
            "point overlap",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "OverlapPoint",
                world_handle,
                point,
                16,
            ),
            lambda results: results.GetHitCount() > 0 and not results.HasOverflow(),
        )
        recorder.capture(
            "broadphase bounds",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "GetBroadPhaseBounds",
                world_handle,
            ),
            lambda result: result.found,
        )

    _run_game_scenario("Jolt_Queries", setup, runtime)


def run_rollback_and_determinism():
    def setup(recorder):
        return _open_base_with_world(recorder, "Jolt Rollback")

    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt
        import azlmbr.math as math

        body_id = wait_for_runtime_entity("Jolt Rollback Body")
        world_handle = recorder.capture(
            "rollback world handle",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", body_id),
            lambda handle: handle.IsValid(),
        )
        configuration = recorder.capture(
            "rollback world configuration",
            lambda: jolt.JoltWorldRequestBus(
                bus.Broadcast,
                "GetRuntimeConfiguration",
                world_handle,
            ),
            lambda value: value is not None,
        )
        if configuration is None:
            return

        configuration.autoSimulate = False
        recorder.capture(
            "manual deterministic stepping enabled",
            lambda: jolt.JoltWorldRequestBus(
                bus.Broadcast,
                "UpdateRuntimeConfiguration",
                world_handle,
                configuration,
            ),
        )
        recorder.capture(
            "deterministic velocity applied",
            lambda: jolt.JoltRigidBodyRequestBus(
                bus.Event,
                "SetVelocities",
                body_id,
                math.Vector3(0.75, 0.0, 0.25),
                math.Vector3(0.0, 0.5, 0.0),
            ),
        )
        snapshot = recorder.capture(
            "rollback snapshot captured",
            lambda: jolt.JoltWorldRollbackRequestBus(
                bus.Broadcast,
                "CaptureWorldState",
                world_handle,
            ),
            lambda handle: handle.IsValid(),
        )

        def simulate_digests():
            digests = []
            for _ in range(16):
                step_result = jolt.JoltWorldSimulationRequestBus(
                    bus.Broadcast,
                    "StepWorld",
                    world_handle,
                    1.0 / 60.0,
                )
                if step_result.stepCount != 1:
                    return []
                digest = jolt.WorldStateDigest()
                if not jolt.JoltWorldRollbackRequestBus(
                    bus.Broadcast,
                    "GetWorldStateDigest",
                    world_handle,
                    digest,
                ):
                    return []
                digests.append((digest.hash, digest.stateByteCount))
            return digests

        first_digests = recorder.capture(
            "first deterministic digest sequence",
            simulate_digests,
            lambda values: len(values) == 16,
            [],
        )
        recorder.capture(
            "rollback snapshot restored",
            lambda: jolt.JoltWorldRollbackRequestBus(
                bus.Broadcast,
                "RestoreWorldState",
                world_handle,
                snapshot,
            ),
            lambda result: is_restore_complete(jolt, result),
        )
        second_digests = recorder.capture(
            "second deterministic digest sequence",
            simulate_digests,
            lambda values: len(values) == 16,
            [],
        )
        recorder.check("rollback replay digest equality", first_digests == second_digests)
        recorder.capture(
            "rollback snapshot destroyed",
            lambda: jolt.JoltWorldRollbackRequestBus(
                bus.Broadcast,
                "DestroyStateSnapshot",
                world_handle,
                snapshot,
            ),
        )
        recorder.capture(
            "destroyed snapshot became stale",
            lambda: jolt.JoltWorldRollbackRequestBus(
                bus.Broadcast,
                "IsStateSnapshotValid",
                world_handle,
                snapshot,
            ),
            lambda valid: not valid,
            True,
        )
        recorder.capture(
            "stale snapshot restore rejected",
            lambda: jolt.JoltWorldRollbackRequestBus(
                bus.Broadcast,
                "RestoreWorldState",
                world_handle,
                snapshot,
            ),
            lambda result: result.status == jolt.StateRestoreStatus_Rejected,
            True,
        )

    _run_game_scenario("Jolt_RollbackAndDeterminism", setup, runtime)


def run_diagnostics():
    def setup(recorder):
        return _open_base_with_world(recorder, "Jolt Diagnostics")

    def runtime(recorder):
        import azlmbr.bus as bus
        import azlmbr.jolt as jolt
        import azlmbr.math as math

        body_id = wait_for_runtime_entity("Jolt Diagnostics Body")
        world_handle = recorder.capture(
            "diagnostic world handle",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", body_id),
            lambda handle: handle.IsValid(),
        )
        invalid_world = jolt.JoltWorldHandle()
        recorder.capture(
            "invalid world handle rejected",
            lambda: jolt.JoltWorldRequestBus(
                bus.Broadcast,
                "IsWorldValid",
                invalid_world,
            ),
            lambda valid: not valid,
            True,
        )
        statistics = jolt.WorldStatistics()
        recorder.capture(
            "world statistics",
            lambda: jolt.JoltWorldDiagnosticsRequestBus(
                bus.Broadcast,
                "GetWorldStatistics",
                world_handle,
                statistics,
            ),
        )
        recorder.check("world resource counts", statistics.bodyCount >= 2 and statistics.shapeCount >= 2)

        recorder.capture(
            "performance statistics enabled",
            lambda: jolt.JoltWorldDiagnosticsRequestBus(
                bus.Broadcast,
                "ConfigurePerformanceStatistics",
                world_handle,
                jolt.PerformanceStatisticsFlags_All,
            ),
        )
        performance_raycast = jolt.JoltRaycastRequest()
        performance_raycast.start = create_world_position(jolt, 0.0, 0.0, 8.0)
        performance_raycast.displacement = math.Vector3(0.0, 0.0, -12.0)
        recorder.capture(
            "instrumented diagnostic query dispatched",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "RaycastClosest",
                world_handle,
                performance_raycast,
            ),
            lambda result: result is not None,
        )
        performance_statistics = jolt.WorldPerformanceStatistics()
        recorder.capture(
            "performance statistics snapshot",
            lambda: jolt.JoltWorldDiagnosticsRequestBus(
                bus.Broadcast,
                "GetPerformanceStatistics",
                world_handle,
                performance_statistics,
                False,
            ),
        )
        recorder.check(
            "performance counters contain the instrumented workload",
            performance_statistics.queryCount > 0
            and performance_statistics.lockCount > 0
            and performance_statistics.processNativeAllocatedBytes > 0
            and performance_statistics.wrapperRetainedBytes > 0,
        )

        debug_configuration = jolt.DebugCaptureConfiguration()
        debug_configuration.flags = jolt.DebugCaptureFlags_SubmergedVolumes
        recorder.capture(
            "debug capture enabled",
            lambda: jolt.JoltWorldDiagnosticsRequestBus(
                bus.Broadcast,
                "ConfigureDebugCapture",
                world_handle,
                debug_configuration,
            ),
        )
        body_state = recorder.capture(
            "diagnostic body state",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", body_id),
            lambda value: value is not None,
        )
        if body_state is not None:
            buoyancy = jolt.JoltBuoyancyConfiguration()
            buoyancy.surfacePosition = create_world_position(
                jolt,
                body_state.transform.position.x,
                body_state.transform.position.y,
                body_state.transform.position.z + 1.0,
            )
            recorder.capture(
                "debug operation captured",
                lambda: jolt.JoltRigidBodyRequestBus(
                    bus.Event,
                    "ApplyBuoyancyImpulse",
                    body_id,
                    buoyancy,
                ),
            )
        debug_statistics = jolt.DebugCaptureStatistics()
        recorder.capture(
            "debug capture statistics",
            lambda: jolt.JoltWorldDiagnosticsRequestBus(
                bus.Broadcast,
                "GetDebugCaptureStatistics",
                world_handle,
                debug_statistics,
            ),
        )
        primitive_count = (
            debug_statistics.geometryCount
            + debug_statistics.lineCount
            + debug_statistics.textCount
            + debug_statistics.triangleCount
        )
        recorder.check("debug capture contains primitives", primitive_count > 0)

        runtime_info = recorder.capture(
            "diagnostic runtime information",
            lambda: jolt.JoltWorldRequestBus(bus.Broadcast, "GetRuntimeInfo"),
            lambda value: value is not None,
        )
        recorder.check(
            "native diagnostic modes report their build configuration",
            lambda: isinstance(runtime_info.broadPhaseStatistics, bool)
            and isinstance(runtime_info.narrowPhaseStatistics, bool),
        )

    _run_game_scenario("Jolt_Diagnostics", setup, runtime)


def run_saved_feature_gallery():
    import hashlib
    import json
    from pathlib import Path

    import azlmbr.legacy.general as general
    import azlmbr.paths

    recorder = ScenarioRecorder("Jolt_SavedFeatureGallery")
    entered_game_mode = False
    try:
        gallery_path = (
            Path(azlmbr.paths.projectroot)
            / "Levels"
            / "Physics"
            / "Jolt"
            / "FeatureGallery"
            / "FeatureGallery.prefab"
        )
        original_gallery_hash = hashlib.sha256(gallery_path.read_bytes()).digest()
        gallery = json.loads(gallery_path.read_text(encoding="utf-8"))
        gallery_entity_names = sorted(
            entity["Name"]
            for entity in gallery["Entities"].values()
            if entity.get("Name", "").startswith("Jolt Gallery ")
        )
        recorder.check("feature gallery entity inventory", len(gallery_entity_names) == 51)
        if not _open_gallery(recorder):
            return

        from editor_python_test_tools.editor_entity_utils import EditorEntity
        from editor_python_test_tools.utils import TestHelper as helper

        authored_components = (
            ("Jolt Gallery Dynamic Body", "Jolt Rigid Body"),
            ("Jolt Gallery Static Body", "Jolt Static Rigid Body"),
            ("Jolt Gallery Box", "Jolt Collider"),
            ("Jolt Gallery Constraint Cone", "Jolt Constraint"),
            ("Jolt Gallery Character", "Jolt Character Controller"),
            ("Jolt Gallery Virtual Character", "Jolt Virtual Character Controller"),
            ("Jolt Gallery Wheeled Vehicle", "Jolt Wheeled Vehicle"),
            ("Jolt Gallery Motorcycle", "Jolt Motorcycle"),
            ("Jolt Gallery Tracked Vehicle", "Jolt Tracked Vehicle"),
            ("Jolt Gallery Soft Body", "Jolt Soft Body"),
            ("Jolt Gallery Ragdoll", "Jolt Ragdoll"),
            ("Jolt Gallery Skeleton", "Jolt Skeleton"),
            ("Jolt Gallery Path", "Jolt Path"),
            ("Jolt Gallery Scene", "Jolt Scene"),
            ("Jolt Gallery Hair", "Jolt Hair"),
        )

        def verify_authored_entities(stage):
            for entity_name in gallery_entity_names:
                entity = EditorEntity.find_editor_entity(entity_name)
                recorder.check(f"{stage} entity {entity_name}", entity.exists())

            for entity_name, component_name in authored_components:
                entity = EditorEntity.find_editor_entity(entity_name)
                recorder.check(
                    f"{stage} {component_name}",
                    lambda entity=entity, component_name=component_name: entity.exists()
                    and entity.has_component(component_name),
                )

        verify_authored_entities("authored")

        helper.open_level("Physics/Jolt", "FeatureGallery")
        recorder.check("feature gallery reloaded", general.get_current_level_name() == "FeatureGallery")
        recorder.check(
            "feature gallery reload preserved its checked-in source",
            hashlib.sha256(gallery_path.read_bytes()).digest() == original_gallery_hash,
        )
        verify_authored_entities("reloaded")
        entered_game_mode = enter_game_mode(recorder)
        if entered_game_mode:
            for entity_name in gallery_entity_names:
                recorder.check(
                    f"runtime export {entity_name}",
                    wait_for_runtime_entity(entity_name).IsValid(),
                )
    except Exception as exception:
        recorder.check("feature gallery completed without an unhandled exception", False, exception)
    finally:
        if entered_game_mode:
            exit_game_mode(recorder)
        recorder.finish()
