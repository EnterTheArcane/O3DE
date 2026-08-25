"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Runs the deterministic Jolt stress level for quick, review, or full-soak durations.
"""

import os

try:
    from .Jolt_ScenarioRecorder import ScenarioRecorder
    from .Jolt_ScenarioUtilities import create_world_position
    from .Jolt_ScenarioUtilities import enter_game_mode
    from .Jolt_ScenarioUtilities import exit_game_mode
    from .Jolt_ScenarioUtilities import is_restore_complete
    from .Jolt_ScenarioUtilities import open_level
    from .Jolt_ScenarioUtilities import wait_for_runtime_entity
except ImportError:
    from Jolt_ScenarioRecorder import ScenarioRecorder
    from Jolt_ScenarioUtilities import create_world_position
    from Jolt_ScenarioUtilities import enter_game_mode
    from Jolt_ScenarioUtilities import exit_game_mode
    from Jolt_ScenarioUtilities import is_restore_complete
    from Jolt_ScenarioUtilities import open_level
    from Jolt_ScenarioUtilities import wait_for_runtime_entity


def _tick_count():
    mode = os.environ.get("JOLT_STRESS_MODE", "quick").lower()
    tick_counts = {
        "quick": 600,
        "review": 3_600,
        "full": 36_000,
    }
    if mode not in tick_counts:
        raise ValueError(f"Unknown JOLT_STRESS_MODE '{mode}'")

    return mode, tick_counts[mode]


def Jolt_StressAndSoak():
    import azlmbr.bus as bus
    import azlmbr.jolt as jolt
    import azlmbr.math as math

    recorder = ScenarioRecorder("Jolt_StressAndSoak")
    try:
        if not open_level(recorder, "Physics/Jolt", "Stress"):
            return

        mode, tick_count = _tick_count()
        recorder.check(
            "stress duration selected",
            (mode, tick_count) in (("quick", 600), ("review", 3_600), ("full", 36_000)),
            f"mode={mode}, ticks={tick_count}",
        )

        worker_digests = []
        worker_state_signatures = []
        maximum_observed_temp_bytes = 0
        temp_capacity_bytes = 0
        peak_active_bodies = 0
        peak_update_nanoseconds = 0
        query_count = 0
        snapshot_count = 0
        simulation_errors = []
        total_event_counts = {"contact": 0, "movement": 0}

        vehicle_input = jolt.WheeledVehicleInput()
        vehicle_input.forward = 0.4
        identity_transform = math.Transform_CreateIdentity()

        raycast = jolt.JoltRaycastRequest()
        raycast.start = create_world_position(jolt, 0.0, 0.0, 32.0)
        raycast.displacement = math.Vector3(0.0, 0.0, -64.0)

        point_overlap = jolt.PointOverlapRequest()
        point_overlap.position = create_world_position(jolt, 0.0, 0.0, -4.0)

        broad_phase_sphere = jolt.BroadPhaseSphere()
        broad_phase_sphere.center = create_world_position(jolt, 0.0, 0.0, 4.0)
        broad_phase_sphere.radius = 16.0
        broad_phase_overlap = jolt.BroadPhaseOverlapRequest()
        broad_phase_overlap.SetSphere(broad_phase_sphere)

        for worker_count in (1, 4, 8):
            entered_game_mode = False
            event_handler = None
            snapshots = []
            world_handle = None
            worker_event_counts = {"contact": 0, "movement": 0}
            worker_query_count = 0
            worker_snapshot_count = 0
            worker_errors = []
            initial_digest = None
            try:
                entered_game_mode = enter_game_mode(recorder, 10.0)
                if not entered_game_mode:
                    worker_errors.append("enter-game-mode")
                    simulation_errors.extend(worker_errors)
                    continue

                driver_id = wait_for_runtime_entity("Jolt Stress Driver", 10.0)
                recorder.check(f"{worker_count}-worker stress driver runtime entity", driver_id.IsValid())
                world_handle = recorder.capture(
                    f"{worker_count}-worker stress world handle",
                    lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", driver_id),
                    lambda handle: handle.IsValid(),
                )
                configuration = recorder.capture(
                    f"{worker_count}-worker stress world configuration",
                    lambda: jolt.JoltWorldRequestBus(
                        bus.Broadcast,
                        "GetRuntimeConfiguration",
                        world_handle,
                    ),
                    lambda value: value is not None,
                )
                if configuration is None:
                    worker_errors.append("runtime-configuration")
                    simulation_errors.extend(worker_errors)
                    continue

                configuration.autoSimulate = False
                configuration.collectContactEvents = True
                configuration.workerCount = worker_count
                recorder.capture(
                    f"configure {worker_count} workers",
                    lambda: jolt.JoltWorldRequestBus(
                        bus.Broadcast,
                        "UpdateRuntimeConfiguration",
                        world_handle,
                        configuration,
                    ),
                )

                recorder.capture(
                    f"{worker_count}-worker stress performance statistics enabled",
                    lambda: jolt.JoltWorldDiagnosticsRequestBus(
                        bus.Broadcast,
                        "ConfigurePerformanceStatistics",
                        world_handle,
                        jolt.PerformanceStatisticsFlags_All,
                    ),
                )

                def on_contact(_):
                    worker_event_counts["contact"] += 1

                def on_body_moved(_):
                    worker_event_counts["movement"] += 1

                event_handler = jolt.JoltWorldNotificationBusHandler()
                event_handler.connect(world_handle)
                event_handler.add_callback("OnContact", on_contact)
                event_handler.add_callback("OnBodyMoved", on_body_moved)

                character_id = wait_for_runtime_entity("Jolt Stress Character", 10.0)
                virtual_character_id = wait_for_runtime_entity("Jolt Stress Virtual Character", 10.0)
                vehicle_id = wait_for_runtime_entity("Jolt Stress Vehicle", 10.0)
                soft_body_id = wait_for_runtime_entity("Jolt Stress Soft Body", 10.0)
                ragdoll_id = wait_for_runtime_entity("Jolt Stress Ragdoll", 10.0)
                hair_id = wait_for_runtime_entity("Jolt Stress Hair", 10.0)
                advanced_entities = (
                    ("character", character_id),
                    ("virtual character", virtual_character_id),
                    ("vehicle", vehicle_id),
                    ("soft body", soft_body_id),
                    ("ragdoll", ragdoll_id),
                    ("CPU Hair", hair_id),
                )
                for subsystem_name, entity_id in advanced_entities:
                    recorder.check(
                        f"{worker_count}-worker stress {subsystem_name} runtime entity",
                        entity_id.IsValid(),
                    )

                contact_body_id = wait_for_runtime_entity("Jolt Stress Body", 10.0)
                recorder.check(
                    f"{worker_count}-worker stress contact body runtime entity",
                    contact_body_id.IsValid(),
                )
                recorder.capture(
                    f"seed {worker_count}-worker contact body position",
                    lambda: jolt.JoltRigidBodyRequestBus(
                        bus.Event,
                        "SetPosition",
                        contact_body_id,
                        create_world_position(jolt, 0.0, 0.0, -2.0),
                        True,
                    ),
                )
                recorder.capture(
                    f"seed {worker_count}-worker contact body velocity",
                    lambda: jolt.JoltRigidBodyRequestBus(
                        bus.Event,
                        "SetVelocities",
                        contact_body_id,
                        math.Vector3(0.0, 0.0, -1.0),
                        math.Vector3(0.0, 0.0, 0.0),
                    ),
                )

                ragdoll_pose = recorder.capture(
                    f"{worker_count}-worker stress ragdoll pose",
                    lambda: jolt.JoltRagdollRequestBus(bus.Event, "CopyPose", ragdoll_id),
                    lambda value: len(value) > 0,
                    [],
                )
                soft_body_vertices = recorder.capture(
                    f"{worker_count}-worker stress soft-body vertices",
                    lambda: jolt.JoltSoftBodyRequestBus(bus.Event, "CopyVertices", soft_body_id),
                    lambda value: len(value) > 0,
                    [],
                )
                hair_definition = recorder.capture(
                    f"{worker_count}-worker stress CPU Hair definition",
                    lambda: jolt.JoltHairRequestBus(bus.Event, "GetDefinitionState", hair_id),
                    lambda value: value.simulationVertexCount > 0,
                )
                recorder.capture(
                    f"{worker_count}-worker initialization step",
                    lambda: jolt.JoltWorldSimulationRequestBus(
                        bus.Broadcast,
                        "StepWorld",
                        world_handle,
                        1.0 / 60.0,
                    ),
                    lambda result: result.stepCount == 1 and result.errors == jolt.SimulationError_None,
                )
                recorder.capture(
                    f"{worker_count}-worker stress CPU Hair readback",
                    lambda: jolt.JoltHairRequestBus(bus.Event, "CopyVertexStates", hair_id),
                    lambda value: hair_definition is not None
                    and len(value) == hair_definition.simulationVertexCount,
                    [],
                )

                recorder.capture(
                    f"isolate {worker_count}-worker stress driver",
                    lambda: jolt.JoltRigidBodyRequestBus(
                        bus.Event,
                        "SetPosition",
                        driver_id,
                        create_world_position(jolt, 0.0, 50.0, 20.0),
                        True,
                    ),
                )
                recorder.capture(
                    f"disable {worker_count}-worker stress driver gravity",
                    lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "SetGravityFactor", driver_id, 0.0),
                )

                initial_snapshot = recorder.capture(
                    f"{worker_count}-worker stress initial snapshot",
                    lambda: jolt.JoltWorldRollbackRequestBus(
                        bus.Broadcast,
                        "CaptureWorldState",
                        world_handle,
                    ),
                    lambda handle: handle.IsValid(),
                )
                if initial_snapshot is not None and initial_snapshot.IsValid():
                    snapshots.append(initial_snapshot)
                    worker_snapshot_count += 1

                initial_digest = jolt.WorldStateDigest()
                recorder.check(
                    f"{worker_count}-worker initial digest captured",
                    jolt.JoltWorldRollbackRequestBus(
                        bus.Broadcast,
                        "GetWorldStateDigest",
                        world_handle,
                        initial_digest,
                    )
                    and initial_digest.stateByteCount > 0,
                )

                driver_state = recorder.capture(
                    f"{worker_count}-worker stress driver state",
                    lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", driver_id),
                    lambda value: value.shapeHandle.IsValid(),
                )
                shape_cast = jolt.JoltShapeCastRequest()
                if driver_state is not None:
                    shape_cast.shapeHandle = driver_state.shapeHandle
                shape_cast_start = jolt.WorldTransform()
                shape_cast_start.position = create_world_position(jolt, 0.0, 0.0, 24.0)
                shape_cast.start = shape_cast_start
                shape_cast.displacement = math.Vector3(0.0, 0.0, -32.0)

                recorder.capture(
                    f"seed velocity for {worker_count} workers",
                    lambda: jolt.JoltRigidBodyRequestBus(
                        bus.Event,
                        "SetVelocities",
                        driver_id,
                        math.Vector3(0.5, 0.0, 0.25),
                        math.Vector3(0.0, 0.25, 0.0),
                    ),
                )

                actuation_results = (
                    (
                        "character",
                        jolt.JoltCharacterRequestBus(
                            bus.Event,
                            "SetVelocity",
                            character_id,
                            math.Vector3(0.2, 0.0, 0.0),
                        ),
                    ),
                    (
                        "virtual-character",
                        jolt.JoltVirtualCharacterRequestBus(
                            bus.Event,
                            "SetVelocity",
                            virtual_character_id,
                            math.Vector3(0.2, 0.0, 0.0),
                        ),
                    ),
                    (
                        "vehicle",
                        jolt.JoltWheeledVehicleRequestBus(bus.Event, "SetInput", vehicle_id, vehicle_input),
                    ),
                    (
                        "soft-body",
                        jolt.JoltSoftBodyRequestBus(
                            bus.Event,
                            "SetVertexVelocity",
                            soft_body_id,
                            0,
                            math.Vector3(0.0, 0.0, 0.2),
                        ),
                    ),
                    (
                        "ragdoll",
                        jolt.JoltRagdollRequestBus(bus.Event, "DriveMotors", ragdoll_id, ragdoll_pose),
                    ),
                    (
                        "CPU-Hair",
                        jolt.JoltHairRequestBus(
                            bus.Event,
                            "SetScalpToHeadTransform",
                            hair_id,
                            identity_transform,
                        ),
                    ),
                )
                for subsystem_name, succeeded in actuation_results:
                    if not succeeded:
                        worker_errors.append(f"{subsystem_name}-actuation")

                for tick in range(tick_count):
                    if tick % 120 == 0:
                        jolt.JoltRigidBodyRequestBus(bus.Event, "ActivateBody", driver_id)
                    if tick % 240 == 0:
                        jolt.JoltWorldQueryRequestBus(bus.Broadcast, "OptimizeBroadPhase", world_handle)
                    if tick % 30 == 0:
                        hit = jolt.JoltWorldQueryRequestBus(
                            bus.Broadcast,
                            "RaycastClosest",
                            world_handle,
                            raycast,
                        )
                        worker_query_count += 1
                        if not hit.found:
                            worker_errors.append(f"raycast:{tick}")
                        if not jolt.JoltWorldQueryRequestBus(
                            bus.Broadcast,
                            "OverlapPointAny",
                            world_handle,
                            point_overlap,
                        ):
                            worker_errors.append(f"point-overlap:{tick}")
                        broad_phase_hits = jolt.JoltWorldQueryRequestBus(
                            bus.Broadcast,
                            "OverlapBroadPhase",
                            world_handle,
                            broad_phase_overlap,
                            256,
                        )
                        if broad_phase_hits.GetHitCount() == 0:
                            worker_errors.append(f"broadphase-overlap:{tick}")
                        shape_cast_hit = jolt.JoltWorldQueryRequestBus(
                            bus.Broadcast,
                            "CastShapeClosest",
                            world_handle,
                            shape_cast,
                        )
                        if not shape_cast_hit.found:
                            worker_errors.append(f"shape-cast:{tick}")
                        worker_query_count += 3

                    if tick % 60 == 0:
                        if not jolt.JoltVirtualCharacterRequestBus(
                            bus.Event,
                            "RefreshContacts",
                            virtual_character_id,
                        ):
                            worker_errors.append(f"virtual-character-refresh:{tick}")

                    result = jolt.JoltWorldSimulationRequestBus(
                        bus.Broadcast,
                        "StepWorld",
                        world_handle,
                        1.0 / 60.0,
                    )
                    if result.stepCount != 1 or result.errors != jolt.SimulationError_None:
                        worker_errors.append(f"step:{tick}:{result.errors}")

                    if tick % 60 == 0 or tick + 1 == tick_count:
                        statistics = jolt.WorldStatistics()
                        if jolt.JoltWorldDiagnosticsRequestBus(
                            bus.Broadcast,
                            "GetWorldStatistics",
                            world_handle,
                            statistics,
                        ):
                            maximum_observed_temp_bytes = max(
                                maximum_observed_temp_bytes,
                                statistics.tempAllocatorUsageBytes,
                            )
                            temp_capacity_bytes = max(
                                temp_capacity_bytes,
                                statistics.tempAllocatorCapacityBytes,
                            )
                            peak_active_bodies = max(
                                peak_active_bodies,
                                statistics.activeDynamicBodyCount,
                            )
                            peak_update_nanoseconds = max(
                                peak_update_nanoseconds,
                                statistics.lastUpdateNanoseconds,
                            )
                            if statistics.lastUpdateErrors != jolt.SimulationError_None:
                                worker_errors.append(f"statistics:{tick}:{statistics.lastUpdateErrors}")

                    if tick > 0 and tick % 300 == 0:
                        checkpoint = jolt.JoltWorldRollbackRequestBus(
                            bus.Broadcast,
                            "CaptureWorldState",
                            world_handle,
                        )
                        if checkpoint.IsValid():
                            snapshots.append(checkpoint)
                            worker_snapshot_count += 1
                            if len(snapshots) > 9:
                                expired_checkpoint = snapshots.pop(1)
                                if not jolt.JoltWorldRollbackRequestBus(
                                    bus.Broadcast,
                                    "DestroyStateSnapshot",
                                    world_handle,
                                    expired_checkpoint,
                                ):
                                    worker_errors.append(f"snapshot-destroy:{tick}")

                rollback_snapshot = jolt.JoltWorldRollbackRequestBus(
                    bus.Broadcast,
                    "CaptureWorldState",
                    world_handle,
                )
                if rollback_snapshot.IsValid():
                    snapshots.append(rollback_snapshot)
                    worker_snapshot_count += 1
                    rollback_digest = jolt.WorldStateDigest()
                    rollback_digest_read = jolt.JoltWorldRollbackRequestBus(
                        bus.Broadcast,
                        "GetWorldStateDigest",
                        world_handle,
                        rollback_digest,
                    )
                    rollback_step = jolt.JoltWorldSimulationRequestBus(
                        bus.Broadcast,
                        "StepWorld",
                        world_handle,
                        1.0 / 60.0,
                    )
                    recorder.check(
                        f"{worker_count}-worker rollback step",
                        rollback_step.stepCount == 1 and rollback_step.errors == jolt.SimulationError_None,
                    )
                    recorder.capture(
                        f"{worker_count}-worker restore checkpoint",
                        lambda: jolt.JoltWorldRollbackRequestBus(
                            bus.Broadcast,
                            "RestoreWorldState",
                            world_handle,
                            rollback_snapshot,
                        ),
                        lambda result: is_restore_complete(jolt, result),
                    )
                    restored_digest = jolt.WorldStateDigest()
                    restored_digest_read = jolt.JoltWorldRollbackRequestBus(
                        bus.Broadcast,
                        "GetWorldStateDigest",
                        world_handle,
                        restored_digest,
                    )
                    recorder.check(
                        f"{worker_count}-worker restore reproduced checkpoint digest",
                        restored_digest_read
                        and rollback_digest_read
                        and restored_digest.hash == rollback_digest.hash
                        and restored_digest.stateByteCount == rollback_digest.stateByteCount,
                        (
                            (rollback_digest.hash, rollback_digest.stateByteCount),
                            (restored_digest.hash, restored_digest.stateByteCount),
                        ),
                    )
                else:
                    worker_errors.append("rollback-snapshot")

                churn_entity = contact_body_id
                for churn_tick in range(min(tick_count, 600)):
                    if churn_tick % 30 == 0:
                        if not jolt.JoltRigidBodyRequestBus(bus.Event, "DisableSimulation", churn_entity):
                            worker_errors.append(f"disable:{churn_tick}")
                        if not jolt.JoltRigidBodyRequestBus(bus.Event, "EnableSimulation", churn_entity):
                            worker_errors.append(f"enable:{churn_tick}")
                    if churn_tick % 120 == 0:
                        jolt.JoltRigidBodyRequestBus(bus.Event, "ActivateBody", churn_entity)

                    result = jolt.JoltWorldSimulationRequestBus(
                        bus.Broadcast,
                        "StepWorld",
                        world_handle,
                        1.0 / 60.0,
                    )
                    if result.stepCount != 1 or result.errors != jolt.SimulationError_None:
                        worker_errors.append(f"churn-step:{churn_tick}:{result.errors}")

                digest = jolt.WorldStateDigest()
                digest_read = jolt.JoltWorldRollbackRequestBus(
                    bus.Broadcast,
                    "GetWorldStateDigest",
                    world_handle,
                    digest,
                )
                recorder.check(
                    f"{worker_count}-worker digest captured",
                    digest_read and digest.stateByteCount > 0,
                )
                worker_digests.append((digest.hash, digest.stateByteCount))

                character_state = jolt.JoltCharacterRequestBus(bus.Event, "GetState", character_id)
                driver_state = jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", driver_id)
                worker_state_signatures.append(
                    (
                        driver_state.transform.position.x,
                        driver_state.transform.position.y,
                        driver_state.transform.position.z,
                        driver_state.transform.rotation.x,
                        driver_state.transform.rotation.y,
                        driver_state.transform.rotation.z,
                        driver_state.transform.rotation.w,
                        driver_state.linearVelocity.x,
                        driver_state.linearVelocity.y,
                        driver_state.linearVelocity.z,
                        driver_state.angularVelocity.x,
                        driver_state.angularVelocity.y,
                        driver_state.angularVelocity.z,
                    )
                )
                virtual_character_state = jolt.JoltVirtualCharacterRequestBus(
                    bus.Event,
                    "GetState",
                    virtual_character_id,
                )
                vehicle_state = jolt.JoltWheeledVehicleRequestBus(bus.Event, "GetState", vehicle_id)
                current_soft_body_vertices = jolt.JoltSoftBodyRequestBus(
                    bus.Event,
                    "CopyVertices",
                    soft_body_id,
                )
                current_ragdoll_pose = jolt.JoltRagdollRequestBus(bus.Event, "CopyPose", ragdoll_id)
                current_hair_vertices = jolt.JoltHairRequestBus(
                    bus.Event,
                    "CopyVertexStates",
                    hair_id,
                )
                if not character_state.shapeHandle.IsValid():
                    worker_errors.append("character-state")
                if not virtual_character_state.shapeHandle.IsValid():
                    worker_errors.append("virtual-character-state")
                if vehicle_state.wheelCount == 0:
                    worker_errors.append("vehicle-state")
                if len(current_soft_body_vertices) != len(soft_body_vertices):
                    worker_errors.append("soft-body-state")
                if len(current_ragdoll_pose) != len(ragdoll_pose):
                    worker_errors.append("ragdoll-state")
                if hair_definition is None or len(current_hair_vertices) != hair_definition.simulationVertexCount:
                    worker_errors.append("CPU-Hair-state")

                performance_statistics = jolt.WorldPerformanceStatistics()
                recorder.capture(
                    f"{worker_count}-worker stress performance statistics snapshot",
                    lambda: jolt.JoltWorldDiagnosticsRequestBus(
                        bus.Broadcast,
                        "GetPerformanceStatistics",
                        world_handle,
                        performance_statistics,
                        False,
                    ),
                )
                recorder.check(
                    f"{worker_count}-worker stress published body-move events",
                    worker_event_counts["movement"] > 0,
                    worker_event_counts,
                )
                recorder.check(
                    f"{worker_count}-worker stress published contact events",
                    worker_event_counts["contact"] > 0,
                    worker_event_counts,
                )
                recorder.check(
                    f"{worker_count}-worker provider published events without drops",
                    performance_statistics.publishedEventCount > 0
                    and performance_statistics.contactEventCount > 0
                    and performance_statistics.droppedEventCount == 0,
                    (
                        performance_statistics.publishedEventCount,
                        performance_statistics.contactEventCount,
                        performance_statistics.droppedEventCount,
                    ),
                )
                recorder.check(
                    f"{worker_count}-worker provider counted every scripted query",
                    performance_statistics.queryCount >= worker_query_count,
                    f"provider={performance_statistics.queryCount}, scripted={worker_query_count}",
                )
                recorder.check(
                    f"{worker_count}-worker provider counted snapshot churn",
                    performance_statistics.snapshotCaptureCount >= worker_snapshot_count,
                    (
                        f"provider={performance_statistics.snapshotCaptureCount}, "
                        f"scripted={worker_snapshot_count}"
                    ),
                )

                query_count += worker_query_count
                snapshot_count += worker_snapshot_count
                total_event_counts["contact"] += worker_event_counts["contact"]
                total_event_counts["movement"] += worker_event_counts["movement"]
                simulation_errors.extend(worker_errors)
                recorder.check(
                    f"{worker_count}-worker stress completed without errors",
                    not worker_errors,
                    worker_errors[:16],
                )
            except Exception as exception:
                worker_errors.append(f"unhandled:{exception}")
                simulation_errors.extend(worker_errors)
                recorder.check(
                    f"{worker_count}-worker stress completed without an unhandled exception",
                    False,
                    exception,
                )
            finally:
                if event_handler is not None:
                    event_handler.disconnect()
                if entered_game_mode:
                    for snapshot in reversed(snapshots):
                        if world_handle is not None and snapshot.IsValid():
                            jolt.JoltWorldRollbackRequestBus(
                                bus.Broadcast,
                                "DestroyStateSnapshot",
                                world_handle,
                                snapshot,
                            )
                    exit_game_mode(recorder, 10.0)

        recorder.check(
            "1/4/8-worker state digests captured",
            len(worker_digests) == 3 and all(digest[1] > 0 for digest in worker_digests),
            worker_digests,
        )
        recorder.check(
            "1/4/8-worker deterministic representative state",
            len(worker_state_signatures) == 3 and len(set(worker_state_signatures)) == 1,
            worker_state_signatures,
        )
        recorder.check("stress workload produced queries", query_count > 0, query_count)
        recorder.check("stress workload activated bodies", peak_active_bodies > 0, peak_active_bodies)
        recorder.check(
            "stress temporary memory remained bounded",
            temp_capacity_bytes > 0
            and maximum_observed_temp_bytes <= temp_capacity_bytes,
            f"observed={maximum_observed_temp_bytes}, capacity={temp_capacity_bytes}",
        )
        recorder.check("stress simulation time was measured", peak_update_nanoseconds > 0, peak_update_nanoseconds)
        recorder.check("stress snapshots were captured", snapshot_count > 0, snapshot_count)
        recorder.check("stress reported no simulation errors", not simulation_errors, simulation_errors[:32])
        recorder.check(
            "stress workload published events",
            total_event_counts["contact"] > 0 and total_event_counts["movement"] > 0,
            total_event_counts,
        )
    except Exception as exception:
        recorder.check("stress scenario completed without an unhandled exception", False, exception)
    finally:
        recorder.finish()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_StressAndSoak)
