"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Verifies the deterministic CPU-only Hair component and scripting contract.
"""

try:
    from .Jolt_ScenarioRecorder import record_scenario
except ImportError:
    from Jolt_ScenarioRecorder import record_scenario


class Tests:
    level_opened = ("Opened the CPU Hair test level", "Failed to open the CPU Hair test level")
    component_created = ("Created the CPU Hair component", "Failed to create the CPU Hair component")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    runtime_contract = ("Verified the CPU-only runtime contract", "The CPU-only runtime contract was not exposed")
    hair_operational = ("Read and updated CPU Hair", "CPU Hair was not operational")
    lifecycle_operational = ("Recreated CPU Hair", "Failed to recreate CPU Hair")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


@record_scenario("Jolt_CpuHair")
def Jolt_CpuHair(recorder):
    import time

    import azlmbr.bus as bus
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.prefab_utils import PrefabWaiter
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("", "Base")
    recorder.result(Tests.level_opened, general.get_current_level_name() == "Base")

    editor_entity = EditorEntity.create_editor_entity("Jolt CPU Hair")
    editor_entity.add_component("Jolt Hair")
    PrefabWaiter.wait_for_propagation()
    recorder.result(Tests.component_created, editor_entity.has_component("Jolt Hair"))

    Report.info("Entering game mode")
    general.enter_game_mode()
    enter_deadline = time.monotonic() + 3.0
    while not general.is_in_game_mode() and time.monotonic() < enter_deadline:
        general.idle_wait(0.01)
    recorder.result(Tests.enter_game_mode, general.is_in_game_mode())

    hair_entity_id = general.find_game_entity("Jolt CPU Hair")
    runtime_info = jolt.JoltWorldRequestBus(bus.Broadcast, "GetRuntimeInfo")
    Report.info(
        "CPU Hair runtime: "
        f"determinism={runtime_info.hairDeterminism}, "
        f"expected={jolt.DeterminismCertification_SameBinary}"
    )
    recorder.result(
        Tests.runtime_contract,
        runtime_info.hairDeterminism == jolt.DeterminismCertification_SameBinary,
    )

    hair_operational = False
    hair_deadline = time.monotonic() + 1.0
    while not hair_operational and time.monotonic() < hair_deadline:
        hair_handle = jolt.JoltHairRequestBus(bus.Event, "GetHairHandle", hair_entity_id)
        hair_definition = jolt.JoltHairRequestBus(bus.Event, "GetDefinitionState", hair_entity_id)
        hair_state = jolt.JoltHairRequestBus(bus.Event, "GetState", hair_entity_id)
        vertex_states = jolt.JoltHairRequestBus(bus.Event, "CopyVertexStates", hair_entity_id)
        render_positions = jolt.JoltHairRequestBus(bus.Event, "CopyRenderPositions", hair_entity_id)
        scalp_positions = jolt.JoltHairRequestBus(bus.Event, "CopyScalpPositions", hair_entity_id)
        grid_cells = jolt.JoltHairRequestBus(bus.Event, "CopyGridCellStates", hair_entity_id)
        neutral_density = jolt.JoltHairRequestBus(bus.Event, "CopyNeutralDensity", hair_entity_id)
        transform_updated = jolt.JoltHairRequestBus(
            bus.Event,
            "SetScalpToHeadTransform",
            hair_entity_id,
            math.Transform_CreateIdentity(),
        )
        hair_operational = (
            hair_entity_id.IsValid()
            and hair_handle.IsValid()
            and hair_definition.simulationVertexCount == 3
            and hair_state.simulationVertexCount == 3
            and len(vertex_states) == hair_state.simulationVertexCount
            and len(render_positions) == hair_state.renderVertexCount
            and len(scalp_positions) == hair_state.scalpVertexCount
            and len(grid_cells) == hair_state.gridCellCount
            and len(neutral_density) == hair_state.gridCellCount
            and transform_updated
        )
        if not hair_operational:
            general.idle_wait(0.01)

    Report.info(
        "CPU Hair state: "
        f"handle={hair_handle.IsValid()}, definition={hair_definition.simulationVertexCount}, "
        f"state={hair_state.simulationVertexCount}/{hair_state.renderVertexCount}/"
        f"{hair_state.scalpVertexCount}/{hair_state.gridCellCount}, "
        f"copies={len(vertex_states)}/{len(render_positions)}/"
        f"{len(scalp_positions)}/{len(grid_cells)}/{len(neutral_density)}, "
        f"updated={transform_updated}"
    )
    recorder.result(Tests.hair_operational, hair_operational)

    disabled = jolt.JoltHairRequestBus(bus.Event, "DisableSimulation", hair_entity_id)
    enabled = jolt.JoltHairRequestBus(bus.Event, "EnableSimulation", hair_entity_id)
    recreated_handle = jolt.JoltHairRequestBus(bus.Event, "GetHairHandle", hair_entity_id)
    recorder.result(
        Tests.lifecycle_operational,
        disabled and enabled and recreated_handle.IsValid(),
    )

    Report.info("Exiting game mode")
    general.exit_game_mode()
    exit_deadline = time.monotonic() + 3.0
    while general.is_in_game_mode() and time.monotonic() < exit_deadline:
        general.idle_wait(0.01)
    recorder.result(Tests.exit_game_mode, not general.is_in_game_mode())


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_CpuHair)
