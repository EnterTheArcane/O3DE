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
    from .Jolt_ScenarioUtilities import open_level
    from .Jolt_ScenarioUtilities import wait_for_runtime_entity
except ImportError:
    from Jolt_ScenarioRecorder import ScenarioRecorder
    from Jolt_ScenarioUtilities import create_world_position
    from Jolt_ScenarioUtilities import enter_game_mode
    from Jolt_ScenarioUtilities import exit_game_mode
    from Jolt_ScenarioUtilities import open_level
    from Jolt_ScenarioUtilities import wait_for_runtime_entity


def _tick_count():
    mode = os.environ.get("JOLT_STRESS_MODE", "quick").lower()
    tick_counts = {
        "quick": 600,
        "review": 3_600,
        "full": 36_000,
    }
    return mode, tick_counts.get(mode, 600)


def Jolt_StressAndSoak():
    import azlmbr.bus as bus
    import azlmbr.jolt as jolt
    import azlmbr.math as math

    recorder = ScenarioRecorder("Jolt_StressAndSoak")
    entered_game_mode = False
    snapshots = []
    world_handle = None
    try:
        if not open_level(recorder, "Physics/Jolt", "Stress"):
            return

        entered_game_mode = enter_game_mode(recorder, 10.0)
        if not entered_game_mode:
            return

        driver_id = wait_for_runtime_entity("Jolt Stress Driver", 10.0)
        recorder.check("stress driver runtime entity", driver_id.IsValid())
        world_handle = recorder.capture(
            "stress world handle",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetWorldHandle", driver_id),
            lambda handle: handle.IsValid(),
        )
        configuration = recorder.capture(
            "stress world configuration",
            lambda: jolt.JoltWorldQueryRequestBus(
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
            "stress manual stepping enabled",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "UpdateRuntimeConfiguration",
                world_handle,
                configuration,
            ),
        )

        initial_snapshot = recorder.capture(
            "stress initial snapshot",
            lambda: jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "CaptureWorldState",
                world_handle,
            ),
            lambda handle: handle.IsValid(),
        )
        if initial_snapshot is not None and initial_snapshot.IsValid():
            snapshots.append(initial_snapshot)

        mode, tick_count = _tick_count()
        recorder.check("stress duration selected", tick_count in (600, 3_600, 36_000), mode)
        worker_digests = []
        maximum_observed_temp_bytes = 0
        temp_capacity_bytes = 0
        peak_active_bodies = 0
        peak_update_nanoseconds = 0
        query_count = 0
        snapshot_count = 0
        simulation_errors = []

        raycast = jolt.RaycastRequest()
        raycast.start = create_world_position(jolt, 0.0, 0.0, 32.0)
        raycast.displacement = math.Vector3(0.0, 0.0, -64.0)

        point_overlap = jolt.PointOverlapRequest()
        point_overlap.position = create_world_position(jolt, 0.0, 0.0, -4.0)

        broad_phase_sphere = jolt.BroadPhaseSphere()
        broad_phase_sphere.center = create_world_position(jolt, 0.0, 0.0, 4.0)
        broad_phase_sphere.radius = 16.0
        broad_phase_overlap = jolt.BroadPhaseOverlapRequest()
        broad_phase_overlap.SetSphere(broad_phase_sphere)

        driver_state = recorder.capture(
            "stress driver state",
            lambda: jolt.JoltRigidBodyRequestBus(bus.Event, "GetState", driver_id),
            lambda value: value.shapeHandle.IsValid(),
        )
        shape_cast = jolt.ShapeCastRequest()
        if driver_state is not None:
            shape_cast.shapeHandle = driver_state.shapeHandle
        shape_cast_start = jolt.WorldTransform()
        shape_cast_start.position = create_world_position(jolt, 0.0, 0.0, 24.0)
        shape_cast.start = shape_cast_start
        shape_cast.displacement = math.Vector3(0.0, 0.0, -32.0)

        for worker_count in (1, 4, 8):
            if worker_digests:
                recorder.capture(
                    f"restore initial state for {worker_count} workers",
                    lambda: jolt.JoltWorldQueryRequestBus(
                        bus.Broadcast,
                        "RestoreWorldState",
                        world_handle,
                        initial_snapshot,
                    ),
                )

            configuration.workerCount = worker_count
            recorder.capture(
                f"configure {worker_count} workers",
                lambda: jolt.JoltWorldQueryRequestBus(
                    bus.Broadcast,
                    "UpdateRuntimeConfiguration",
                    world_handle,
                    configuration,
                ),
            )
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

            worker_errors = []
            for tick in range(tick_count):
                if tick % 120 == 0:
                    jolt.JoltRigidBodyRequestBus(bus.Event, "ActivateBody", driver_id)
                if tick % 240 == 0:
                    jolt.JoltWorldQueryRequestBus(
                        bus.Broadcast,
                        "OptimizeBroadPhase",
                        world_handle,
                    )
                if tick % 30 == 0:
                    hit = jolt.JoltWorldQueryRequestBus(
                        bus.Broadcast,
                        "RaycastClosest",
                        world_handle,
                        raycast,
                    )
                    query_count += 1
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
                    query_count += 3

                result = jolt.JoltWorldQueryRequestBus(
                    bus.Broadcast,
                    "StepWorld",
                    world_handle,
                    1.0 / 60.0,
                )
                if result.stepCount != 1 or result.errors != jolt.SimulationError_None:
                    worker_errors.append(f"step:{tick}:{result.errors}")

                if tick % 60 == 0 or tick + 1 == tick_count:
                    statistics = jolt.WorldStatistics()
                    if jolt.JoltWorldQueryRequestBus(
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
                        peak_active_bodies = max(peak_active_bodies, statistics.activeDynamicBodyCount)
                        peak_update_nanoseconds = max(
                            peak_update_nanoseconds,
                            statistics.lastUpdateNanoseconds,
                        )
                        if statistics.lastUpdateErrors != jolt.SimulationError_None:
                            worker_errors.append(
                                f"statistics:{tick}:{statistics.lastUpdateErrors}"
                            )

                if tick > 0 and tick % 300 == 0:
                    checkpoint = jolt.JoltWorldQueryRequestBus(
                        bus.Broadcast,
                        "CaptureWorldState",
                        world_handle,
                    )
                    if checkpoint.IsValid():
                        snapshots.append(checkpoint)
                        snapshot_count += 1
                        if len(snapshots) > 9:
                            expired_checkpoint = snapshots.pop(1)
                            if not jolt.JoltWorldQueryRequestBus(
                                bus.Broadcast,
                                "DestroyStateSnapshot",
                                world_handle,
                                expired_checkpoint,
                            ):
                                worker_errors.append(f"snapshot-destroy:{tick}")

            digest = jolt.WorldStateDigest()
            digest_read = jolt.JoltWorldQueryRequestBus(
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
            simulation_errors.extend(worker_errors)
            recorder.check(
                f"{worker_count}-worker stress completed without errors",
                not worker_errors,
                worker_errors[:16],
            )

        recorder.check("1/4/8-worker deterministic digest", len(set(worker_digests)) == 1, worker_digests)
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

        churn_entity = wait_for_runtime_entity("Jolt Stress Body")
        recorder.check("topology churn entity resolved", churn_entity.IsValid())
        churn_errors = []
        for churn_tick in range(min(tick_count, 600)):
            if churn_tick % 30 == 0:
                if not jolt.JoltRigidBodyRequestBus(
                    bus.Event,
                    "DisableSimulation",
                    churn_entity,
                ):
                    churn_errors.append(f"disable:{churn_tick}")
                if not jolt.JoltRigidBodyRequestBus(
                    bus.Event,
                    "EnableSimulation",
                    churn_entity,
                ):
                    churn_errors.append(f"enable:{churn_tick}")
            if churn_tick % 120 == 0:
                jolt.JoltRigidBodyRequestBus(
                    bus.Event,
                    "ActivateBody",
                    churn_entity,
                )

            result = jolt.JoltWorldQueryRequestBus(
                bus.Broadcast,
                "StepWorld",
                world_handle,
                1.0 / 60.0,
            )
            if result.stepCount != 1 or result.errors != jolt.SimulationError_None:
                churn_errors.append(f"step:{churn_tick}:{result.errors}")

        recorder.check(
            "topology and sleep-wake churn completed without errors",
            not churn_errors,
            churn_errors[:32],
        )
    except Exception as exception:
        recorder.check("stress scenario completed without an unhandled exception", False, exception)
    finally:
        if entered_game_mode:
            for snapshot in reversed(snapshots):
                if world_handle is not None and snapshot.IsValid():
                    jolt.JoltWorldQueryRequestBus(
                        bus.Broadcast,
                        "DestroyStateSnapshot",
                        world_handle,
                        snapshot,
                    )
            exit_game_mode(recorder, 10.0)
        recorder.finish()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_StressAndSoak)
