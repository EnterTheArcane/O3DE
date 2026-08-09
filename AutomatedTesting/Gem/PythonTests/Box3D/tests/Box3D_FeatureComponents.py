"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Exercises mutable terrain, character movement, wind, and explosion components through their public script buses.
"""


class Tests:
    level_opened = ("Opened the Box3D feature level", "Failed to open the Box3D feature level")
    components_created = ("Created the Box3D feature entities", "Failed to create the Box3D feature entities")
    components_serialized = ("Serialized Box3D component configurations", "Failed to serialize Box3D component configurations")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    heightfield_dimensions = ("Read the heightfield dimensions", "Heightfield dimensions were incorrect")
    heightfield_updated = ("Updated heightfield samples", "Failed to update heightfield samples")
    character_moved = ("Moved the Box3D character", "Box3D character did not move")
    body_properties_updated = ("Updated dynamic body properties", "Failed to update dynamic body properties")
    material_created = ("Created and updated a Box3D material", "Failed to create or update a Box3D material")
    collider_updated = ("Updated Box3D collider geometry and materials", "Failed to update Box3D collider geometry or materials")
    cooked_shape_queried = ("Cooked and queried reusable Box3D geometry", "Failed to cook or query reusable Box3D geometry")
    world_query_ready = ("Resolved the Box3D query world", "Failed to resolve the Box3D query world")
    statistics_read = ("Read Box3D world statistics", "Failed to read Box3D world statistics")
    recording_validated = ("Recorded and validated Box3D simulation", "Failed to record or validate Box3D simulation")
    replay_inspected = ("Inspected deterministic Box3D replay data", "Failed to inspect deterministic Box3D replay data")
    body_state_reset = ("Reset Box3D body state through the physics API", "Failed to reset Box3D body state")
    static_tree_rebuilt = ("Rebuilt the Box3D static tree", "Failed to rebuild the Box3D static tree")
    raycast_found_body = ("Raycast found a Box3D body", "Raycast did not find the expected Box3D body")
    batch_raycast_found_body = (
        "Batch raycast returned ordered Box3D results",
        "Batch raycast did not return the expected ordered results",
    )
    aabb_overlap_found_body = ("AABB overlap found a Box3D body", "AABB overlap did not find a Box3D body")
    sphere_overlap_found_body = ("Sphere overlap found a Box3D body", "Sphere overlap did not find a Box3D body")
    contact_event_received = ("Received a Box3D contact event", "Did not receive a Box3D contact event")
    contact_point_read = ("Read a Box3D contact point", "Failed to read a Box3D contact point")
    event_families_received = (
        "Received every Box3D world notification family",
        "Did not receive every Box3D world notification family",
    )
    joint_configuration_updated = ("Updated a Box3D joint", "Failed to update a Box3D joint")
    joint_measurements_read = ("Read Box3D joint measurements", "Failed to read Box3D joint measurements")
    all_joint_families = ("Exercised every Box3D joint family", "Failed to exercise every Box3D joint family")
    wind_moved_body = ("Wind moved a dynamic body", "Wind did not move the dynamic body")
    explosion_triggered = ("Triggered a Box3D explosion", "Failed to trigger a Box3D explosion")
    explosion_positioned = ("Positioned the Box3D explosion", "Failed to position the Box3D explosion")
    explosion_moved_body = ("Explosion moved a dynamic body", "Explosion did not move the dynamic body")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Box3D_FeatureComponents():
    import azlmbr.box3d as box3d
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper
    from editor_python_test_tools.utils import Tracer

    helper.init_idle()
    helper.open_level("Box3D", "ComponentSmoke")
    Report.result(Tests.level_opened, general.get_current_level_name() == "ComponentSmoke")

    with Tracer() as serialization_tracer:
        heightfield = EditorEntity.create_editor_entity("Box3D Mutable Heightfield")
        heightfield.add_component("Box3D Heightfield Collider")
        components.TransformBus(bus.Event, "SetWorldTranslation", heightfield.id, math.Vector3(10.0, 0.0, 0.0))

        character = EditorEntity.create_editor_entity("Box3D Moving Character")
        character.add_component("Box3D Character Controller")

        wind_body = EditorEntity.create_editor_entity("Box3D Wind Body")
        wind_body.add_component("Box3D Collider")
        wind_body.add_component("Box3D Rigid Body")
        wind_body.add_component("Box3D Wind")
        components.TransformBus(bus.Event, "SetWorldTranslation", wind_body.id, math.Vector3(20.0, 0.0, 5.0))

        explosion_body = EditorEntity.create_editor_entity("Box3D Explosion Body")
        explosion_body.add_component("Box3D Collider")
        explosion_body.add_component("Box3D Rigid Body")
        components.TransformBus(bus.Event, "SetWorldTranslation", explosion_body.id, math.Vector3(41.0, 0.0, 5.0))

        explosion = EditorEntity.create_editor_entity("Box3D Explosion Source")
        explosion.add_component("Box3D Explosion")
        components.TransformBus(bus.Event, "SetWorldTranslation", explosion.id, math.Vector3(40.0, 0.0, 5.0))

        sensor = EditorEntity.create_editor_entity("Box3D Trigger Volume")
        sensor.add_component("Box3D Collider")
        sensor.add_component("Box3D Static Rigid Body")
        components.TransformBus(bus.Event, "SetWorldTranslation", sensor.id, math.Vector3(70.0, 0.0, 0.0))

        sensor_visitor = EditorEntity.create_editor_entity("Box3D Sensor Probe")
        sensor_visitor.add_component("Box3D Collider")
        sensor_visitor.add_component("Box3D Rigid Body")
        components.TransformBus(bus.Event, "SetWorldTranslation", sensor_visitor.id, math.Vector3(66.0, 0.0, 0.0))

        joint_parent = EditorEntity.create_editor_entity("Box3D Distance Joint Parent")
        joint_parent.add_component("Box3D Collider")
        joint_parent.add_component("Box3D Static Rigid Body")
        components.TransformBus(bus.Event, "SetWorldTranslation", joint_parent.id, math.Vector3(50.0, 0.0, 0.0))

        joint_body = EditorEntity.create_editor_entity("Box3D Distance Joint Body")
        joint_body.add_component("Box3D Collider")
        joint_body.add_component("Box3D Rigid Body")
        distance_joint = joint_body.add_component("Box3D Distance Joint")
        parent_property_path = next(
            path
            for path in distance_joint.get_property_type_visibility()
            if path.endswith("Parent entity")
        )
        distance_joint.set_component_property_value(parent_property_path, joint_parent.id)
        components.TransformBus(bus.Event, "SetWorldTranslation", joint_body.id, math.Vector3(50.0, 0.0, 2.0))

        additional_joint_families = (
            "Parallel",
            "Filter",
            "Motor",
            "Prismatic",
            "Revolute",
            "Spherical",
            "Weld",
            "Wheel",
        )
        additional_joint_entities = []
        for joint_index, joint_family in enumerate(additional_joint_families):
            joint_entity = EditorEntity.create_editor_entity(f"Box3D {joint_family} Joint Body")
            joint_entity.add_component("Box3D Collider")
            joint_entity.add_component("Box3D Rigid Body")
            joint_component = joint_entity.add_component(f"Box3D {joint_family} Joint")
            parent_property_path = next(
                path
                for path in joint_component.get_property_type_visibility()
                if path.endswith("Parent entity")
            )
            joint_component.set_component_property_value(parent_property_path, joint_parent.id)
            components.TransformBus(
                bus.Event,
                "SetWorldTranslation",
                joint_entity.id,
                math.Vector3(52.0 + 2.0 * joint_index, 0.0, 2.0),
            )
            additional_joint_entities.append((joint_family, joint_entity))

        Report.result(
            Tests.components_created,
            heightfield.has_component("Box3D Heightfield Collider")
            and character.has_component("Box3D Character Controller")
            and wind_body.has_component("Box3D Wind")
            and explosion.has_component("Box3D Explosion")
            and sensor.has_component("Box3D Static Rigid Body")
            and sensor_visitor.has_component("Box3D Rigid Body")
            and joint_body.has_component("Box3D Distance Joint")
            and all(
                entity.has_component(f"Box3D {joint_family} Joint")
                for joint_family, entity in additional_joint_entities
            ),
        )

    variant_serialization_warnings = [
        warning
        for warning in serialization_tracer.warnings
        if warning.window == "Prefab Serialization" and "AZStd::variant" in warning.message
    ]
    Report.result(
        Tests.components_serialized,
        not variant_serialization_warnings,
    )

    helper.enter_game_mode(Tests.enter_game_mode)
    heightfield_id = general.find_game_entity("Box3D Mutable Heightfield")
    character_id = general.find_game_entity("Box3D Moving Character")
    wind_body_id = general.find_game_entity("Box3D Wind Body")
    explosion_body_id = general.find_game_entity("Box3D Explosion Body")
    explosion_id = general.find_game_entity("Box3D Explosion Source")
    sensor_id = general.find_game_entity("Box3D Trigger Volume")
    sensor_visitor_id = general.find_game_entity("Box3D Sensor Probe")
    joint_body_id = general.find_game_entity("Box3D Distance Joint Body")

    wind_body_properties = box3d.Box3DRigidBodyRequestBus(bus.Event, "GetProperties", wind_body_id)
    wind_body_properties.gravityScale = 0.0
    wind_properties_updated = box3d.Box3DRigidBodyRequestBus(
        bus.Event, "SetProperties", wind_body_id, wind_body_properties
    )
    explosion_body_properties = box3d.Box3DRigidBodyRequestBus(bus.Event, "GetProperties", explosion_body_id)
    explosion_body_properties.gravityScale = 0.0
    explosion_properties_updated = box3d.Box3DRigidBodyRequestBus(
        bus.Event, "SetProperties", explosion_body_id, explosion_body_properties
    )
    hit_events_enabled = box3d.Box3DRigidBodyRequestBus(
        bus.Event, "SetHitEventsEnabled", wind_body_id, True
    )
    Report.result(
        Tests.body_properties_updated,
        wind_properties_updated and explosion_properties_updated and hit_events_enabled,
    )

    material_configuration = box3d.MaterialConfiguration()
    material_configuration.friction = 0.35
    material_configuration.restitution = 0.2
    material_handle = box3d.Box3DMaterialRequestBus(
        bus.Broadcast,
        "CreateMaterial",
        material_configuration,
    )
    material_configuration.friction = 0.75
    material_updated = box3d.Box3DMaterialRequestBus(
        bus.Broadcast,
        "UpdateMaterial",
        material_handle,
        material_configuration,
    )
    material_result = box3d.Box3DMaterialRequestBus(
        bus.Broadcast,
        "GetMaterial",
        material_handle,
    )
    Report.result(
        Tests.material_created,
        material_handle.IsValid()
        and material_updated
        and material_result.found
        and abs(material_result.configuration.friction - 0.75) < 0.001,
    )

    sphere_configuration = box3d.SphereShapeConfiguration()
    sphere_configuration.radius = 0.75
    material_handles = box3d.MaterialHandleCollection()
    material_handles.Add(material_handle)
    shape_properties = box3d.ShapeProperties()
    shape_properties.SetMaterials(material_handles)
    shape_properties.enableContactEvents = True
    collider_updated = box3d.Box3DColliderRequestBus(
        bus.Event,
        "UpdateSphere",
        wind_body_id,
        0,
        sphere_configuration,
        shape_properties,
    )
    materials_updated = box3d.Box3DColliderRequestBus(
        bus.Event,
        "SetMaterials",
        wind_body_id,
        0,
        material_handles,
    )
    Report.result(Tests.collider_updated, collider_updated and materials_updated)

    cooked_shape = box3d.Box3DCookingRequestBus(
        bus.Broadcast,
        "CookSphere",
        sphere_configuration,
        shape_properties,
    )
    cooked_bounds = box3d.Box3DCookingRequestBus(
        bus.Broadcast,
        "GetCookedShapeAabb",
        cooked_shape,
    )
    cooked_hit = box3d.Box3DCookingRequestBus(
        bus.Broadcast,
        "RaycastCookedShape",
        cooked_shape,
        math.Vector3(0.0, 0.0, 2.0),
        math.Vector3(0.0, 0.0, -1.0),
        4.0,
    )
    cooked_shape_destroyed = box3d.Box3DCookingRequestBus(
        bus.Broadcast,
        "DestroyCookedShape",
        cooked_shape,
    )
    Report.result(
        Tests.cooked_shape_queried,
        cooked_bounds.IsValid()
        and cooked_hit.found
        and 1.0 < cooked_hit.hit.distance < 2.0
        and cooked_shape_destroyed,
    )

    general.idle_wait_frames(1)
    default_world_handle = box3d.Box3DWorldRequestBus(bus.Broadcast, "GetDefaultWorldHandle")
    world_handle = box3d.Box3DRigidBodyRequestBus(bus.Event, "GetWorldHandle", wind_body_id)
    body_handle = box3d.Box3DRigidBodyRequestBus(bus.Event, "GetBodyHandle", wind_body_id)
    shape_count = box3d.Box3DColliderRequestBus(bus.Event, "GetShapeCount", wind_body_id)
    Report.result(
        Tests.world_query_ready,
        default_world_handle.IsValid() and world_handle.IsValid() and body_handle.IsValid() and shape_count > 0,
    )
    statistics = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "GetWorldStatistics",
        world_handle,
        box3d.StatisticsFlags_All,
    )
    Report.result(
        Tests.statistics_read,
        statistics.found
        and statistics.counters.workerCount > 0
        and statistics.counters.bodyCount > 0
        and statistics.counters.shapeCount > 0
        and statistics.simulationTick > 0,
    )
    recording_started = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "StartRecording",
        world_handle,
        4096,
    )
    recording_raycast = box3d.RaycastRequest()
    recording_position = components.TransformBus(bus.Event, "GetWorldTranslation", wind_body_id)
    recording_raycast.start = math.Vector3(recording_position.x, recording_position.y, recording_position.z + 2.0)
    recording_raycast.direction = math.Vector3(0.0, 0.0, -1.0)
    recording_raycast.distance = 4.0
    box3d.Box3DWorldRequestBus(
        bus.Broadcast,
        "RaycastClosest",
        world_handle,
        recording_raycast,
    )
    recording_steps_completed = all(
        box3d.Box3DWorldRequestBus(
            bus.Broadcast,
            "StepWorld",
            world_handle,
            1.0 / 60.0,
        )
        for _ in range(3)
    )
    recording = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "StopRecording",
        world_handle,
    )
    recording_valid_one_worker = recording.stopped and box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "ValidateRecording",
        recording.data,
        1,
    )
    recording_valid_four_workers = recording.stopped and box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "ValidateRecording",
        recording.data,
        4,
    )
    Report.result(
        Tests.recording_validated,
        recording_started
        and recording_steps_completed
        and recording.data.GetByteCount() > 0
        and recording_valid_one_worker
        and recording_valid_four_workers,
    )
    replay_handle = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "CreateReplay",
        recording.data,
        1,
    )
    replay_info = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "GetReplayInfo",
        replay_handle,
    )
    replay_policy_set = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "SetReplayKeyframePolicy",
        replay_handle,
        65536,
        2,
    )
    replay_worker_set = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "SetReplayWorkerCount",
        replay_handle,
        1,
    )
    replay_body_found = False
    replay_query_found = False
    for _ in range(replay_info.info.frameCount if replay_info.found else 0):
        if not box3d.Box3DDiagnosticsRequestBus(bus.Broadcast, "StepReplay", replay_handle):
            break
        if box3d.Box3DDiagnosticsRequestBus(bus.Broadcast, "GetReplayBodyCount", replay_handle) > 0:
            replay_body_found = box3d.Box3DDiagnosticsRequestBus(
                bus.Broadcast,
                "GetReplayBody",
                replay_handle,
                0,
            ).found
        replay_query_count = box3d.Box3DDiagnosticsRequestBus(
            bus.Broadcast,
            "GetReplayQueryCount",
            replay_handle,
        )
        if replay_query_count > 0:
            replay_query = box3d.Box3DDiagnosticsRequestBus(
                bus.Broadcast,
                "GetReplayQuery",
                replay_handle,
                0,
            )
            replay_query_found = replay_query.found
            if replay_query.query.hitCount > 0:
                replay_query_hit = box3d.Box3DDiagnosticsRequestBus(
                    bus.Broadcast,
                    "GetReplayQueryHit",
                    replay_handle,
                    0,
                    0,
                )
                replay_query_found = replay_query_found and replay_query_hit.found and replay_query_hit.hit.shapeId.IsValid()
    replay_diverged = box3d.Box3DDiagnosticsRequestBus(bus.Broadcast, "HasReplayDiverged", replay_handle)
    replay_divergence_frame = box3d.Box3DDiagnosticsRequestBus(
        bus.Broadcast,
        "GetReplayDivergenceFrame",
        replay_handle,
    )
    replay_restarted = box3d.Box3DDiagnosticsRequestBus(bus.Broadcast, "RestartReplay", replay_handle)
    replay_destroyed = box3d.Box3DDiagnosticsRequestBus(bus.Broadcast, "DestroyReplay", replay_handle)
    wind_transform_reset = box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetTransform",
        wind_body_id,
        math.Transform_CreateTranslation(math.Vector3(20.0, 0.0, 5.0)),
    )
    wind_linear_velocity_reset = box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetLinearVelocity",
        wind_body_id,
        math.Vector3(0.0, 0.0, 0.0),
    )
    wind_angular_velocity_reset = box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetAngularVelocity",
        wind_body_id,
        math.Vector3(0.0, 0.0, 0.0),
    )
    Report.result(
        Tests.body_state_reset,
        wind_transform_reset and wind_linear_velocity_reset and wind_angular_velocity_reset,
    )
    Report.result(
        Tests.replay_inspected,
        replay_handle.IsValid()
        and replay_info.found
        and replay_info.info.frameCount > 0
        and replay_policy_set
        and replay_worker_set
        and replay_body_found
        and replay_query_found
        and not replay_diverged
        and replay_divergence_frame < 0
        and replay_restarted
        and replay_destroyed,
    )
    Report.result(
        Tests.static_tree_rebuilt,
        box3d.Box3DDiagnosticsRequestBus(
            bus.Broadcast,
            "RebuildStaticTree",
            world_handle,
        ),
    )
    query_filter = box3d.QueryFilter()
    raycast_request = box3d.RaycastRequest()
    wind_position = box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "GetState",
        wind_body_id,
    ).transform.GetTranslation()
    raycast_request.start = math.Vector3(wind_position.x, wind_position.y, wind_position.z + 2.0)
    raycast_request.direction = math.Vector3(0.0, 0.0, -1.0)
    raycast_request.filter = query_filter
    raycast_request.distance = 4.0
    closest_hit = box3d.Box3DWorldRequestBus(
        bus.Broadcast,
        "RaycastClosest",
        world_handle,
        raycast_request,
    )
    raycast_hits = box3d.Box3DWorldRequestBus(
        bus.Broadcast,
        "Raycast",
        world_handle,
        raycast_request,
        16,
    )
    raycast_found_expected_body = any(
        raycast_hits.GetHit(index).bodyHandle == body_handle
        for index in range(raycast_hits.GetHitCount())
    )
    Report.result(
        Tests.raycast_found_body,
        closest_hit.found and raycast_found_expected_body,
    )
    miss_request = box3d.RaycastRequest()
    miss_request.start = math.Vector3(wind_position.x, wind_position.y, wind_position.z + 2.0)
    miss_request.direction = math.Vector3(0.0, 1.0, 0.0)
    miss_request.distance = 1.0
    batch_requests = box3d.RaycastRequestCollection()
    batch_requests.AddRequest(raycast_request)
    batch_requests.AddRequest(miss_request)
    batch_results = box3d.Box3DWorldRequestBus(
        bus.Broadcast,
        "RaycastClosestBatch",
        world_handle,
        batch_requests,
    )
    Report.result(
        Tests.batch_raycast_found_body,
        batch_results.GetResultCount() == 2
        and batch_results.GetResult(0).found
        and batch_results.GetResult(0).hit.bodyHandle == body_handle
        and not batch_results.GetResult(1).found
        and not batch_results.HasOverflow(),
    )

    aabb_request = box3d.AabbOverlapRequest()
    aabb_request.aabb = box3d.Box3DColliderRequestBus(bus.Event, "GetAabb", wind_body_id)
    aabb_request.filter = query_filter
    aabb_hits = box3d.Box3DWorldRequestBus(
        bus.Broadcast,
        "OverlapAabb",
        world_handle,
        aabb_request,
        8,
    )
    Report.result(
        Tests.aabb_overlap_found_body,
        aabb_hits.requiredHitCount > 0 and aabb_hits.GetHitCount() > 0,
    )

    overlap_geometry = box3d.SphereShapeConfiguration()
    overlap_geometry.radius = 2.0
    overlap_parameters = box3d.ConvexOverlapParameters()
    overlap_parameters.transform = box3d.Box3DRigidBodyRequestBus(bus.Event, "GetState", wind_body_id).transform
    overlap_parameters.filter = query_filter
    sphere_hits = box3d.Box3DWorldRequestBus(
        bus.Broadcast,
        "OverlapSphere",
        world_handle,
        overlap_geometry,
        overlap_parameters,
    )
    Report.result(
        Tests.sphere_overlap_found_body,
        sphere_hits.requiredHitCount > 0 and sphere_hits.GetHitCount() > 0,
    )

    sensor_geometry = box3d.SphereShapeConfiguration()
    sensor_geometry.radius = 1.0
    sensor_properties = box3d.ShapeProperties()
    sensor_properties.isSensor = True
    sensor_properties.enableSensorEvents = True
    sensor_properties.enableContactEvents = False
    sensor_properties.enableHitEvents = False
    visitor_geometry = box3d.SphereShapeConfiguration()
    visitor_geometry.radius = 0.5
    visitor_properties = box3d.ShapeProperties()
    visitor_properties.enableSensorEvents = True
    visitor_properties.enableContactEvents = False
    visitor_properties.enableHitEvents = False
    sensor_ready = box3d.Box3DColliderRequestBus(
        bus.Event,
        "UpdateSphere",
        sensor_id,
        0,
        sensor_geometry,
        sensor_properties,
    )
    sensor_ready = sensor_ready and box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetTransform",
        sensor_id,
        math.Transform_CreateTranslation(math.Vector3(70.0, 0.0, 0.0)),
    )
    sensor_visitor_ready = box3d.Box3DColliderRequestBus(
        bus.Event,
        "UpdateSphere",
        sensor_visitor_id,
        0,
        visitor_geometry,
        visitor_properties,
    )
    sensor_visitor_properties = box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "GetProperties",
        sensor_visitor_id,
    )
    sensor_visitor_properties.gravityScale = 0.0
    sensor_visitor_ready = (
        sensor_visitor_ready
        and box3d.Box3DRigidBodyRequestBus(
            bus.Event,
            "SetProperties",
            sensor_visitor_id,
            sensor_visitor_properties,
        )
    )

    contact_state = {
        "body_moved": False,
        "sensor": False,
        "received": False,
        "point_read": False,
        "contact_hit": False,
        "joint_threshold": False,
    }

    def on_body_moved(args):
        contact_state["body_moved"] = args[0].bodyHandle.IsValid()

    def on_sensor(args):
        event = args[0]
        contact_state["sensor"] = (
            event.sensorBody.IsValid()
            and event.visitorBody.IsValid()
            and event.sensorShape.IsValid()
            and event.visitorShape.IsValid()
        )

    def on_contact(args):
        event = args[0]
        if event.pointCount == 0:
            return
        point_count = box3d.Box3DWorldRequestBus(
            bus.Broadcast,
            "GetContactPointCount",
            world_handle,
        )
        if point_count <= event.firstPoint:
            return
        point = box3d.Box3DWorldRequestBus(
            bus.Broadcast,
            "GetContactPointAt",
            world_handle,
            event.firstPoint,
        )
        contact_state["received"] = event.bodyA.IsValid() and event.bodyB.IsValid()
        contact_state["point_read"] = abs(point.normal.x) <= 1.01

    def on_contact_hit(args):
        event = args[0]
        contact_state["contact_hit"] = (
            event.bodyA.IsValid()
            and event.bodyB.IsValid()
            and event.shapeA.IsValid()
            and event.shapeB.IsValid()
            and event.approachSpeed >= 0.0
        )

    def on_joint_threshold(args):
        contact_state["joint_threshold"] = args[0].jointHandle.IsValid()

    world_notification_handler = box3d.Box3DWorldNotificationBusHandler()
    world_notification_handler.connect(world_handle)
    world_notification_handler.add_callback("OnBodyMoved", on_body_moved)
    world_notification_handler.add_callback("OnSensor", on_sensor)
    world_notification_handler.add_callback("OnContact", on_contact)
    world_notification_handler.add_callback("OnContactHit", on_contact_hit)
    world_notification_handler.add_callback("OnJointThresholdExceeded", on_joint_threshold)
    sensor_visitor_ready = sensor_visitor_ready and box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetTransform",
        sensor_visitor_id,
        math.Transform_CreateTranslation(math.Vector3(66.0, 0.0, 0.0)),
    )
    general.idle_wait_frames(2)
    sensor_visitor_ready = sensor_visitor_ready and box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetTransform",
        sensor_visitor_id,
        math.Transform_CreateTranslation(math.Vector3(69.0, 0.0, 0.0)),
    )
    wind_position = box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "GetState",
        wind_body_id,
    ).transform.GetTranslation()
    box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetTransform",
        explosion_body_id,
        math.Transform_CreateTranslation(math.Vector3(wind_position.x, wind_position.y, wind_position.z + 4.0)),
    )
    box3d.Box3DRigidBodyRequestBus(bus.Event, "SetHitEventsEnabled", explosion_body_id, True)
    box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetLinearVelocity",
        explosion_body_id,
        math.Vector3(0.0, 0.0, -20.0),
    )
    box3d.Box3DRigidBodyRequestBus(bus.Event, "SetAwake", wind_body_id, True)
    box3d.Box3DRigidBodyRequestBus(bus.Event, "SetAwake", explosion_body_id, True)
    distance_configuration = box3d.Box3DJointRequestBus(
        bus.Event,
        "GetDistanceConfiguration",
        joint_body_id,
    )
    joint_common_configuration = distance_configuration.common
    joint_common_configuration.forceThreshold = 0.0
    distance_configuration.common = joint_common_configuration
    joint_threshold_ready = box3d.Box3DJointRequestBus(
        bus.Event,
        "UpdateDistanceConfiguration",
        joint_body_id,
        distance_configuration,
    )
    joint_threshold_ready = joint_threshold_ready and box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "ApplyLinearImpulse",
        joint_body_id,
        math.Vector3(100.0, 0.0, 0.0),
    )
    sensor_visitor_ready = sensor_visitor_ready and box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetLinearVelocity",
        sensor_visitor_id,
        math.Vector3(1.0, 0.0, 0.0),
    )
    helper.wait_for_condition(
        lambda: all(contact_state.values()),
        3.0,
    )
    Report.result(Tests.contact_event_received, contact_state["received"])
    Report.result(Tests.contact_point_read, contact_state["point_read"])
    Report.result(
        Tests.event_families_received,
        sensor_ready
        and sensor_visitor_ready
        and joint_threshold_ready
        and contact_state["body_moved"]
        and contact_state["sensor"]
        and contact_state["contact_hit"]
        and contact_state["joint_threshold"],
    )
    world_notification_handler.disconnect()

    distance_configuration = box3d.Box3DJointRequestBus(
        bus.Event,
        "GetDistanceConfiguration",
        joint_body_id,
    )
    distance_configuration.length = 2.0
    joint_updated = box3d.Box3DJointRequestBus(
        bus.Event,
        "UpdateDistanceConfiguration",
        joint_body_id,
        distance_configuration,
    )
    updated_distance_configuration = box3d.Box3DJointRequestBus(
        bus.Event,
        "GetDistanceConfiguration",
        joint_body_id,
    )
    Report.result(
        Tests.joint_configuration_updated,
        joint_updated and abs(updated_distance_configuration.length - 2.0) < 0.001,
    )
    joint_handle = box3d.Box3DJointRequestBus(bus.Event, "GetJointHandle", joint_body_id)
    joint_bodies_woken = box3d.Box3DJointRequestBus(bus.Event, "WakeBodies", joint_body_id)
    joint_measurements = box3d.Box3DJointRequestBus(bus.Event, "GetMeasurements", joint_body_id)
    distance_state = box3d.Box3DJointRequestBus(bus.Event, "GetDistanceState", joint_body_id)
    Report.result(
        Tests.joint_measurements_read,
        joint_handle.IsValid()
        and joint_bodies_woken
        and abs(joint_measurements.linearSeparation) < 1000.0
        and abs(distance_state.length) < 1000.0,
    )
    joint_families_valid = True
    for joint_family in (
        "Parallel",
        "Distance",
        "Filter",
        "Motor",
        "Prismatic",
        "Revolute",
        "Spherical",
        "Weld",
        "Wheel",
    ):
        family_body_id = general.find_game_entity(f"Box3D {joint_family} Joint Body")
        family_handle = box3d.Box3DJointRequestBus(bus.Event, "GetJointHandle", family_body_id)
        family_configuration = box3d.Box3DJointRequestBus(
            bus.Event,
            f"Get{joint_family}Configuration",
            family_body_id,
        )
        family_updated = box3d.Box3DJointRequestBus(
            bus.Event,
            f"Update{joint_family}Configuration",
            family_body_id,
            family_configuration,
        )
        family_measurements = box3d.Box3DJointRequestBus(
            bus.Event,
            "GetMeasurements",
            family_body_id,
        )
        joint_families_valid = (
            family_handle.IsValid()
            and family_updated
            and abs(family_measurements.linearSeparation) < 1000.0
            and joint_families_valid
        )
    Report.result(Tests.all_joint_families, joint_families_valid)

    column_count = box3d.Box3DHeightfieldRequestBus(bus.Event, "GetColumnCount", heightfield_id)
    row_count = box3d.Box3DHeightfieldRequestBus(bus.Event, "GetRowCount", heightfield_id)
    Report.result(Tests.heightfield_dimensions, column_count == 2 and row_count == 2)
    updated = box3d.Box3DHeightfieldRequestBus(
        bus.Event,
        "UpdateHeights",
        heightfield_id,
        0,
        0,
        2,
        2,
        [0.0, 0.5, 1.0, 1.5],
    )
    heights = box3d.Box3DHeightfieldRequestBus(bus.Event, "GetHeights", heightfield_id)
    Report.result(Tests.heightfield_updated, updated and list(heights) == [0.0, 0.5, 1.0, 1.5])

    def move_character():
        box3d.Box3DCharacterRequestBus(
            bus.Event,
            "Move",
            character_id,
            math.Vector3(2.0, 0.0, 0.0),
            1.0 / 60.0,
        )
        return components.TransformBus(bus.Event, "GetWorldX", character_id) > 0.2

    Report.result(Tests.character_moved, helper.wait_for_condition(move_character, 3.0))

    wind_configuration = box3d.WindConfiguration()
    wind_configuration.velocity = math.Vector3(20.0, 0.0, 0.0)
    wind_configuration.drag = 5.0
    wind_configuration.maximumSpeed = 30.0
    box3d.Box3DWindRequestBus(bus.Event, "UpdateConfiguration", wind_body_id, wind_configuration)
    Report.result(
        Tests.wind_moved_body,
        helper.wait_for_condition(
            lambda: components.TransformBus(bus.Event, "GetWorldX", wind_body_id) > 20.1,
            3.0,
        ),
    )

    explosion_configuration = box3d.ExplosionConfiguration()
    explosion_configuration.radius = 5.0
    explosion_configuration.falloff = 0.0
    explosion_configuration.impulsePerArea = 50.0
    box3d.Box3DExplosionRequestBus(bus.Event, "UpdateConfiguration", explosion_id, explosion_configuration)
    components.TransformBus(bus.Event, "SetWorldTranslation", explosion_id, math.Vector3(40.0, 0.0, 5.0))
    box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetTransform",
        explosion_body_id,
        math.Transform_CreateTranslation(math.Vector3(41.0, 0.0, 5.0)),
    )
    box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetLinearVelocity",
        explosion_body_id,
        math.Vector3(0.0, 0.0, 0.0),
    )
    box3d.Box3DRigidBodyRequestBus(
        bus.Event,
        "SetAngularVelocity",
        explosion_body_id,
        math.Vector3(0.0, 0.0, 0.0),
    )
    general.idle_wait_frames(1)
    explosion_position = components.TransformBus(bus.Event, "GetWorldTranslation", explosion_id)
    explosion_body_position = components.TransformBus(bus.Event, "GetWorldTranslation", explosion_body_id)
    Report.result(
        Tests.explosion_positioned,
        abs(explosion_position.x - 40.0) < 0.01
        and abs(explosion_position.y) < 0.01
        and abs(explosion_position.z - 5.0) < 0.01
        and abs(explosion_body_position.x - 41.0) < 0.01,
    )
    exploded = box3d.Box3DExplosionRequestBus(bus.Event, "Explode", explosion_id)
    Report.result(Tests.explosion_triggered, exploded)
    def explosion_changed_body():
        state = box3d.Box3DRigidBodyRequestBus(bus.Event, "GetState", explosion_body_id)
        return state.linearVelocity.x > 0.1 or components.TransformBus(
            bus.Event, "GetWorldX", explosion_body_id
        ) > 41.1

    Report.result(
        Tests.explosion_moved_body,
        helper.wait_for_condition(explosion_changed_body, 3.0),
    )

    helper.exit_game_mode(Tests.exit_game_mode)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Box3D_FeatureComponents)
