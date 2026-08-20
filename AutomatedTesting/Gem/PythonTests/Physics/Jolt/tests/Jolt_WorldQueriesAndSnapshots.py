"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Exercises Jolt world notifications, query families, diagnostics, and deterministic state snapshots.
"""


class Tests:
    level_opened = ("Opened the Jolt query level", "Failed to open the Jolt query level")
    components_created = ("Created the Jolt query scene", "Failed to create the Jolt query scene")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    world_events = ("Received world movement and contact events", "World events were incomplete")
    raycasts = ("Completed scalar and batched raycasts", "A raycast query failed")
    overlaps = ("Completed point and shape overlaps", "An overlap query failed")
    broad_phase = ("Completed broad-phase overlap and cast queries", "A broad-phase query failed")
    geometry_queries = ("Collected shapes, faces, and triangles", "A geometry query failed")
    shape_details = ("Queried standalone and retained shape details", "A detailed shape query failed")
    diagnostics = ("Read world bodies and statistics", "World diagnostics were incomplete")
    debug_capture = ("Captured native simulation diagnostics", "Simulation diagnostics were not captured")
    snapshot_restored = ("Restored and validated a world snapshot", "World snapshot validation failed")
    snapshot_archive = ("Exported and imported a world snapshot archive", "World snapshot archive failed")
    multipart_snapshot = ("Restored a multipart world snapshot", "Multipart world snapshot restore failed")
    rollback_replay = ("Replayed a deterministic multi-frame rollback", "Multi-frame rollback diverged")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def create_world_position(jolt, x, y, z):
    position = jolt.WorldPosition()
    position.x = x
    position.y = y
    position.z = z
    return position


def is_restore_complete(jolt, result):
    return getattr(result, "status", None) == jolt.StateRestoreStatus_Complete


def Jolt_WorldQueriesAndSnapshots():
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

    floor = EditorEntity.create_editor_entity("Jolt Query Floor")
    floor.add_component("Jolt Collider")
    floor.add_component("Jolt Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", floor.id, math.Vector3(0.0, 0.0, -6.0))
    components.TransformBus(bus.Event, "SetLocalUniformScale", floor.id, 12.0)

    query_body = EditorEntity.create_editor_entity("Jolt Query Body")
    query_body.add_component("Jolt Collider")
    query_body.add_component("Jolt Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", query_body.id, math.Vector3(0.0, 0.0, 5.0))

    Report.result(
        Tests.components_created,
        floor.has_component("Jolt Static Rigid Body")
        and query_body.has_component("Jolt Rigid Body"),
    )

    Report.info("Entering game mode")
    general.enter_game_mode()
    enter_deadline = time.monotonic() + 3.0
    while not general.is_in_game_mode() and time.monotonic() < enter_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.enter_game_mode, general.is_in_game_mode())
    query_body_id = general.find_game_entity("Jolt Query Body")
    world_handle = jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", query_body_id)
    body_handle = jolt.JoltRigidBodyRequestBus(bus.Event, "GetBodyHandle", query_body_id)
    shape_handle = jolt.JoltColliderRequestBus(bus.Event, "GetRootShapeHandle", query_body_id)

    received_events = {
        "contact": False,
        "movement": False,
    }

    def on_contact(args):
        received_events["contact"] = args[0].firstBodyHandle.IsValid()

    def on_body_moved(args):
        received_events["movement"] = args[0].bodyHandle.IsValid()

    world_handler = jolt.JoltWorldNotificationBusHandler()
    world_handler.connect(world_handle)
    world_handler.add_callback("OnContact", on_contact)
    world_handler.add_callback("OnBodyMoved", on_body_moved)
    runtime_configuration = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetRuntimeConfiguration",
        world_handle,
    )
    runtime_configuration.collectContactEvents = True
    events_enabled = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "UpdateRuntimeConfiguration",
        world_handle,
        runtime_configuration,
    )
    event_deadline = time.monotonic() + 5.0
    events_received = received_events["contact"] and received_events["movement"]
    while events_enabled and not events_received and time.monotonic() < event_deadline:
        general.idle_wait(0.01)
        events_received = received_events["contact"] and received_events["movement"]
    Report.result(Tests.world_events, bool(events_received))

    frozen = jolt.JoltRigidBodyRequestBus(bus.Event, "SetGravityFactor", query_body_id, 0.0)
    frozen = frozen and jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetVelocities",
        query_body_id,
        math.Vector3(0.0, 0.0, 0.0),
        math.Vector3(0.0, 0.0, 0.0),
    )
    frozen = frozen and jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetPosition",
        query_body_id,
        create_world_position(jolt, 0.0, 0.0, 3.0),
        True,
    )
    general.idle_wait(0.05)

    raycast = jolt.RaycastRequest()
    raycast.start = create_world_position(jolt, 0.0, 0.0, 8.0)
    raycast.displacement = math.Vector3(0.0, 0.0, -12.0)
    closest_raycast = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastClosest",
        world_handle,
        raycast,
    )
    any_raycast = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastAny",
        world_handle,
        raycast,
    )
    all_raycasts = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastAll",
        world_handle,
        raycast,
        16,
    )
    raycast_batch = jolt.RaycastRequestCollection()
    batch_populated = raycast_batch.AddRequest(raycast) and raycast_batch.AddRequest(raycast)
    batch_results = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastClosestBatch",
        world_handle,
        raycast_batch,
    )
    Report.result(
        Tests.raycasts,
        bool(
            frozen
            and closest_raycast.found
            and any_raycast
            and all_raycasts.GetHitCount() >= 2
            and batch_populated
            and batch_results.GetResultCount() == 2
            and batch_results.GetResult(0).found
            and not batch_results.HasOverflow()
        ),
    )

    point_overlap = jolt.PointOverlapRequest()
    point_overlap.position = create_world_position(jolt, 0.0, 0.0, 3.0)
    point_hits = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "OverlapPoint",
        world_handle,
        point_overlap,
        16,
    )
    point_found = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "OverlapPointAny",
        world_handle,
        point_overlap,
    )

    shape_overlap = jolt.ShapeOverlapRequest()
    shape_overlap.shapeHandle = shape_handle
    overlap_transform = jolt.WorldTransform()
    overlap_transform.position = create_world_position(jolt, 0.0, 0.0, 3.0)
    shape_overlap.transform = overlap_transform
    shape_collisions = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollideShape",
        world_handle,
        shape_overlap,
        16,
    )
    shape_overlaps = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "OverlapShape",
        world_handle,
        shape_overlap,
        16,
    )
    shape_found = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "OverlapShapeAny",
        world_handle,
        shape_overlap,
    )
    Report.result(
        Tests.overlaps,
        bool(
            body_handle.IsValid()
            and shape_handle.IsValid()
            and point_found
            and point_hits.GetHitCount() > 0
            and shape_found
            and shape_collisions.GetHitCount() > 0
            and shape_overlaps.GetHitCount() > 0
        ),
    )

    shape_cast = jolt.ShapeCastRequest()
    shape_cast.shapeHandle = shape_handle
    cast_start = jolt.WorldTransform()
    cast_start.position = create_world_position(jolt, 0.0, 0.0, 7.0)
    shape_cast.start = cast_start
    shape_cast.displacement = math.Vector3(0.0, 0.0, -8.0)
    closest_shape_cast = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CastShapeClosest",
        world_handle,
        shape_cast,
    )
    all_shape_casts = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CastShapeAll",
        world_handle,
        shape_cast,
        16,
    )

    broad_sphere = jolt.BroadPhaseSphere()
    broad_sphere.center = create_world_position(jolt, 0.0, 0.0, 3.0)
    broad_sphere.radius = 2.0
    broad_overlap = jolt.BroadPhaseOverlapRequest()
    broad_overlap.SetSphere(broad_sphere)
    broad_hits = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "OverlapBroadPhase",
        world_handle,
        broad_overlap,
        16,
    )
    broad_found = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "OverlapBroadPhaseAny",
        world_handle,
        broad_overlap,
    )

    broad_ray = jolt.BroadPhaseRay()
    broad_ray.start = create_world_position(jolt, 0.0, 0.0, 8.0)
    broad_ray.displacement = math.Vector3(0.0, 0.0, -12.0)
    broad_cast = jolt.BroadPhaseCastRequest()
    broad_cast.SetRay(broad_ray)
    closest_broad_cast = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CastBroadPhaseClosest",
        world_handle,
        broad_cast,
    )
    all_broad_casts = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CastBroadPhaseAll",
        world_handle,
        broad_cast,
        16,
    )
    Report.result(
        Tests.broad_phase,
        bool(
            closest_shape_cast.found
            and all_shape_casts.GetHitCount() > 0
            and broad_found
            and broad_hits.GetHitCount() > 0
            and closest_broad_cast.found
            and all_broad_casts.GetHitCount() > 0
        ),
    )

    bounds = jolt.BroadPhaseAabb()
    bounds.center = create_world_position(jolt, 0.0, 0.0, 0.0)
    bounds.halfExtents = math.Vector3(8.0, 8.0, 8.0)
    shape_collection_request = jolt.ShapeCollectionRequest()
    shape_collection_request.bounds = bounds
    collected_shapes = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollectShapesInBounds",
        world_handle,
        shape_collection_request,
        32,
    )

    floor_hit = all_raycasts.GetHit(0)
    for hit_index in range(1, all_raycasts.GetHitCount()):
        candidate_hit = all_raycasts.GetHit(hit_index)
        if candidate_hit.position.z < floor_hit.position.z:
            floor_hit = candidate_hit
    supporting_face_request = jolt.SupportingFaceRequest()
    supporting_face_request.bodyHandle = floor_hit.bodyHandle
    supporting_face_request.subShapeId = floor_hit.subShapeId
    supporting_face_request.incidentDirection = math.Vector3(0.0, 0.0, -1.0)
    supporting_face = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetSupportingFace",
        world_handle,
        supporting_face_request,
        16,
    )

    triangle_request = jolt.TriangleCollectionRequest()
    triangle_request.bodyHandle = floor_hit.bodyHandle
    triangle_request.bounds = bounds
    triangles = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollectTriangles",
        world_handle,
        triangle_request,
        32,
    )
    Report.info(
        "Geometry query counts: "
        f"shapes={collected_shapes.GetShapeCount()}, "
        f"faceVertices={supporting_face.GetVertexCount()}, "
        f"triangles={triangles.GetTriangleCount()}"
    )
    Report.result(
        Tests.geometry_queries,
        bool(
            collected_shapes.GetShapeCount() > 0
            and supporting_face.GetVertexCount() > 0
            and triangles.GetTriangleCount() > 0
        ),
    )

    shape_raycast_request = jolt.ShapeRaycastRequest()
    shape_raycast_request.shapeHandle = shape_handle
    shape_raycast_request.start = math.Vector3(0.0, 0.0, 2.0)
    shape_raycast_request.displacement = math.Vector3(0.0, 0.0, -4.0)
    closest_shape_raycast = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastShapeClosest",
        world_handle,
        shape_raycast_request,
    )
    all_shape_raycasts = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastShapeAll",
        world_handle,
        shape_raycast_request,
        8,
    )
    shape_point_hits = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollideShapePoint",
        world_handle,
        shape_handle,
        math.Vector3(0.0, 0.0, 0.0),
        8,
    )
    shape_point_found = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollideShapePointAny",
        world_handle,
        shape_handle,
        math.Vector3(0.0, 0.0, 0.0),
    )
    transformed_shape = collected_shapes.GetShape(0)
    transformed_bounds = transformed_shape.bounds
    shape_triangle_transform = math.Transform_CreateIdentity()
    shape_triangle_transform.SetTranslation(math.Vector3(0.0, 0.0, 3.0))
    shape_triangle_request = jolt.ShapeTriangleCollectionRequest()
    shape_triangle_request.shapeHandle = shape_handle
    shape_triangle_request.transform = shape_triangle_transform
    shape_triangle_request.boundsCenter = math.Vector3(0.0, 0.0, 3.0)
    shape_triangle_request.boundsHalfExtents = math.Vector3(2.0, 2.0, 2.0)
    shape_triangles = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollectShapeTriangles",
        world_handle,
        shape_triangle_request,
        32,
    )

    transformed_raycast_request = jolt.TransformedShapeRaycastRequest()
    transformed_raycast_request.start = create_world_position(
        jolt,
        transformed_bounds.center.x,
        transformed_bounds.center.y,
        transformed_bounds.center.z + transformed_bounds.halfExtents.z + 1.0,
    )
    transformed_raycast_request.displacement = math.Vector3(
        0.0,
        0.0,
        -2.0 * (transformed_bounds.halfExtents.z + 1.0),
    )
    closest_transformed_raycast = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastTransformedShapeClosest",
        world_handle,
        transformed_shape,
        transformed_raycast_request,
    )
    all_transformed_raycasts = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RaycastTransformedShapeAll",
        world_handle,
        transformed_shape,
        transformed_raycast_request,
        8,
    )
    transformed_point_hits = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollideTransformedShapePoint",
        world_handle,
        transformed_shape,
        transformed_bounds.center,
        8,
    )
    transformed_point_found = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollideTransformedShapePointAny",
        world_handle,
        transformed_shape,
        transformed_bounds.center,
    )
    transformed_children = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollectTransformedShapeChildren",
        world_handle,
        transformed_shape,
        transformed_bounds,
        8,
    )
    transformed_triangles = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CollectTransformedShapeTriangles",
        world_handle,
        transformed_shape,
        transformed_bounds,
        32,
    )
    transformed_normal = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetTransformedShapeSurfaceNormal",
        world_handle,
        transformed_shape,
        closest_transformed_raycast.hit.subShapeId,
        closest_transformed_raycast.hit.position,
    )
    transformed_face = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetTransformedShapeSupportingFace",
        world_handle,
        transformed_shape,
        closest_transformed_raycast.hit.subShapeId,
        math.Vector3(0.0, 0.0, 1.0),
        16,
    )
    Report.result(
        Tests.shape_details,
        bool(
            closest_shape_raycast.found
            and all_shape_raycasts.GetHitCount() > 0
            and shape_point_found
            and shape_point_hits.GetHitCount() > 0
            and shape_triangles.GetTriangleCount() > 0
            and closest_transformed_raycast.found
            and all_transformed_raycasts.GetHitCount() > 0
            and transformed_point_found
            and transformed_point_hits.GetHitCount() > 0
            and transformed_children.GetShapeCount() > 0
            and transformed_triangles.GetTriangleCount() > 0
            and transformed_normal.found
            and transformed_face.GetVertexCount() > 0
        ),
    )

    body_state = jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", query_body_id)
    bodies = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetBodies",
        world_handle,
        body_state.kind,
        False,
        16,
    )
    statistics = jolt.WorldStatistics()
    statistics_read = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetWorldStatistics",
        world_handle,
        statistics,
    )
    Report.result(
        Tests.diagnostics,
        bool(
            bodies.GetBodyCount() >= 2
            and not bodies.HasOverflow()
            and statistics_read
            and statistics.bodyCount >= 2
            and statistics.shapeCount >= 2
        ),
    )

    debug_configuration = jolt.DebugCaptureConfiguration()
    debug_configuration.flags = jolt.DebugCaptureFlags_SubmergedVolumes
    debug_configured = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "ConfigureDebugCapture",
        world_handle,
        debug_configuration,
    )
    buoyancy_configuration = jolt.BuoyancyConfiguration()
    buoyancy_configuration.surfacePosition = create_world_position(
        jolt,
        body_state.transform.position.x,
        body_state.transform.position.y,
        body_state.transform.position.z + 1.0,
    )
    debug_operation_captured = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "ApplyBuoyancyImpulse",
        query_body_id,
        buoyancy_configuration,
    )
    debug_statistics = jolt.DebugCaptureStatistics()
    debug_statistics_read = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetDebugCaptureStatistics",
        world_handle,
        debug_statistics,
    )
    debug_configuration.flags = jolt.DebugCaptureFlags_None
    debug_disabled = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "ConfigureDebugCapture",
        world_handle,
        debug_configuration,
    )
    Report.result(
        Tests.debug_capture,
        bool(
            debug_configured
            and debug_operation_captured
            and debug_statistics_read
            and (
                debug_statistics.geometryCount
                + debug_statistics.lineCount
                + debug_statistics.textCount
                + debug_statistics.triangleCount
            )
            > 0
            and debug_disabled
        ),
    )

    snapshot = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CaptureWorldState",
        world_handle,
    )
    snapshot_archive = jolt.StateSnapshotArchive()
    snapshot_exported = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "ExportWorldStateArchive",
        world_handle,
        [snapshot],
        snapshot_archive,
    )
    digest_before = jolt.WorldStateDigest()
    digest_before_read = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetWorldStateDigest",
        world_handle,
        digest_before,
    )
    moved = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetPosition",
        query_body_id,
        create_world_position(jolt, 0.0, 0.0, 7.0),
        True,
    )
    digest_after_move = jolt.WorldStateDigest()
    digest_after_move_read = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetWorldStateDigest",
        world_handle,
        digest_after_move,
    )
    restored = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RestoreWorldState",
        world_handle,
        snapshot,
    )
    validation = jolt.StateValidationResult()
    validated = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "ValidateWorldState",
        world_handle,
        snapshot,
        validation,
    )
    digest_after_restore = jolt.WorldStateDigest()
    digest_after_restore_read = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetWorldStateDigest",
        world_handle,
        digest_after_restore,
    )
    snapshot_destroyed = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "DestroyStateSnapshot",
        world_handle,
        snapshot,
    )
    Report.result(
        Tests.snapshot_restored,
        bool(
            snapshot.IsValid()
            and digest_before_read
            and moved
            and digest_after_move_read
            and digest_before.hash != digest_after_move.hash
            and is_restore_complete(jolt, restored)
            and validated
            and validation.matches
            and digest_after_restore_read
            and digest_before.hash == digest_after_restore.hash
            and snapshot_destroyed
        ),
    )

    moved_after_archive = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetPosition",
        query_body_id,
        create_world_position(jolt, 0.0, 0.0, 8.0),
        True,
    )
    imported_snapshots = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "ImportWorldStateArchive",
        world_handle,
        snapshot_archive,
    )
    archive_restored = len(imported_snapshots) == 1 and jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RestoreWorldState",
        world_handle,
        imported_snapshots[0],
    )
    digest_after_archive = jolt.WorldStateDigest()
    archive_digest_read = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetWorldStateDigest",
        world_handle,
        digest_after_archive,
    )
    archive_snapshot_destroyed = len(imported_snapshots) == 1 and jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "DestroyStateSnapshot",
        world_handle,
        imported_snapshots[0],
    )
    Report.result(
        Tests.snapshot_archive,
        bool(
            snapshot_exported
            and snapshot_archive.snapshotCount == 1
            and snapshot_archive.binaryState
            and moved_after_archive
            and is_restore_complete(jolt, archive_restored)
            and archive_digest_read
            and digest_before.hash == digest_after_archive.hash
            and archive_snapshot_destroyed
        ),
    )

    multipart_configuration = jolt.StateSnapshotConfiguration()
    multipart_configuration.flags = jolt.StateSnapshotFlags_Bodies
    multipart_configuration.restoreSafety = jolt.RestoreSafety_Validated
    multipart_configuration.filterBodies = True
    multipart_bodies = [bodies.GetBody(index) for index in range(bodies.GetBodyCount())]
    partition_body_counts = [1 for _ in multipart_bodies]
    multipart_snapshots = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CaptureWorldStateParts",
        world_handle,
        multipart_configuration,
        multipart_bodies,
        partition_body_counts,
    )
    multipart_moved = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetPosition",
        query_body_id,
        create_world_position(jolt, 0.0, 0.0, 6.0),
        True,
    )
    multipart_restored = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RestoreWorldStateParts",
        world_handle,
        multipart_snapshots,
    )
    multipart_state = jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", query_body_id)
    multipart_snapshots_valid = len(multipart_snapshots) == len(partition_body_counts)
    multipart_snapshots_destroyed = multipart_snapshots_valid
    for multipart_snapshot in multipart_snapshots:
        multipart_snapshots_valid = multipart_snapshots_valid and multipart_snapshot.IsValid()
        multipart_snapshots_destroyed = multipart_snapshots_destroyed and jolt.JoltWorldQueryRequestBus(
            bus.Broadcast,
            "DestroyStateSnapshot",
            world_handle,
            multipart_snapshot,
        )
    Report.result(
        Tests.multipart_snapshot,
        bool(
            multipart_bodies
            and multipart_snapshots_valid
            and multipart_moved
            and is_restore_complete(jolt, multipart_restored)
            and abs(multipart_state.transform.position.z - 3.0) < 0.01
            and multipart_snapshots_destroyed
        ),
    )

    rollback_configuration = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetRuntimeConfiguration",
        world_handle,
    )
    rollback_configuration.autoSimulate = False
    manual_stepping_enabled = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "UpdateRuntimeConfiguration",
        world_handle,
        rollback_configuration,
    )
    replay_velocity_set = jolt.JoltRigidBodyRequestBus(
        bus.Event,
        "SetVelocities",
        query_body_id,
        math.Vector3(0.75, 0.0, 0.25),
        math.Vector3(0.0, 0.5, 0.0),
    )
    rollback_snapshot = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "CaptureWorldState",
        world_handle,
    )

    def simulate_digest_sequence():
        sequence = []
        steps_succeeded = True
        for _ in range(8):
            step_result = jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "StepWorld",
                world_handle,
                1.0 / 60.0,
            )
            digest = jolt.WorldStateDigest()
            digest_read = jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "GetWorldStateDigest",
                world_handle,
                digest,
            )
            steps_succeeded = steps_succeeded and step_result.stepCount == 1 and digest_read
            sequence.append((digest.hash, digest.stateByteCount))
        return steps_succeeded, sequence

    first_steps_succeeded, first_digest_sequence = simulate_digest_sequence()
    rollback_restored = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "RestoreWorldState",
        world_handle,
        rollback_snapshot,
    )
    second_steps_succeeded, second_digest_sequence = simulate_digest_sequence()
    rollback_snapshot_destroyed = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "DestroyStateSnapshot",
        world_handle,
        rollback_snapshot,
    )
    rollback_configuration.autoSimulate = True
    automatic_stepping_restored = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "UpdateRuntimeConfiguration",
        world_handle,
        rollback_configuration,
    )
    Report.result(
        Tests.rollback_replay,
        bool(
            manual_stepping_enabled
            and replay_velocity_set
            and rollback_snapshot.IsValid()
            and first_steps_succeeded
            and is_restore_complete(jolt, rollback_restored)
            and second_steps_succeeded
            and first_digest_sequence == second_digest_sequence
            and rollback_snapshot_destroyed
            and automatic_stepping_restored
        ),
    )

    world_handler.disconnect()
    Report.info("Exiting game mode")
    general.exit_game_mode()
    exit_deadline = time.monotonic() + 3.0
    while general.is_in_game_mode() and time.monotonic() < exit_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.exit_game_mode, not general.is_in_game_mode())


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_WorldQueriesAndSnapshots)
