"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Shared editor and runtime setup for focused Jolt scenarios.
"""

import time


def create_world_position(jolt, x, y, z):
    position = jolt.WorldPosition()
    position.x = x
    position.y = y
    position.z = z
    return position


def is_restore_complete(jolt, result):
    return getattr(result, "status", None) == jolt.StateRestoreStatus_Complete


def open_level(recorder, directory, level):
    import os

    import azlmbr.legacy.general as general

    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    try:
        helper.open_level(os.path.normpath(directory), level)
    except Exception as exception:
        recorder.check("level opened", False, exception)
        return False

    return recorder.check("level opened", general.get_current_level_name() == level)


def create_editor_entity(recorder, name, component_names, position, parent_id=None):
    import azlmbr.bus as bus
    import azlmbr.components as components

    from editor_python_test_tools.editor_entity_utils import EditorEntity

    if parent_id is None:
        entity = EditorEntity.create_editor_entity(name)
    else:
        entity = EditorEntity.create_editor_entity(name, parent_id)

    for component_name in component_names:
        entity.add_component(component_name)

    components.TransformBus(bus.Event, "SetWorldTranslation", entity.id, position)
    recorder.check(
        f"editor entity {name}",
        lambda: entity.exists()
        and all(entity.has_component(component_name) for component_name in component_names),
    )
    return entity


def enter_game_mode(recorder, timeout_seconds=5.0, check_name="entered game mode"):
    import azlmbr.legacy.general as general

    general.enter_game_mode()
    deadline = time.monotonic() + timeout_seconds
    while not general.is_in_game_mode() and time.monotonic() < deadline:
        general.idle_wait(0.01)

    return recorder.check(check_name, general.is_in_game_mode())


def exit_game_mode(recorder, timeout_seconds=5.0, check_name="exited game mode"):
    import azlmbr.legacy.general as general

    if general.is_in_game_mode():
        general.exit_game_mode()

    deadline = time.monotonic() + timeout_seconds
    while general.is_in_game_mode() and time.monotonic() < deadline:
        general.idle_wait(0.01)

    return recorder.check(check_name, not general.is_in_game_mode())


def wait_for_runtime_entity(name, timeout_seconds=5.0):
    import azlmbr.legacy.general as general

    entity_id = general.find_game_entity(name)
    deadline = time.monotonic() + timeout_seconds
    while not entity_id.IsValid() and time.monotonic() < deadline:
        general.idle_wait(0.01)
        entity_id = general.find_game_entity(name)
    return entity_id


def wait_for_condition(condition, timeout_seconds=5.0):
    import azlmbr.legacy.general as general

    result = condition()
    deadline = time.monotonic() + timeout_seconds
    while not result and time.monotonic() < deadline:
        general.idle_wait(0.01)
        result = condition()
    return result


def create_basic_world(recorder, prefix):
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.math as math

    floor_name = f"{prefix} Floor"
    body_name = f"{prefix} Body"
    floor = create_editor_entity(
        recorder,
        floor_name,
        ("Jolt Collider", "Jolt Static Rigid Body"),
        math.Vector3(0.0, 0.0, -2.0),
    )
    components.TransformBus(
        bus.Event,
        "SetLocalUniformScale",
        floor.id,
        8.0,
    )
    body = create_editor_entity(
        recorder,
        body_name,
        ("Jolt Collider", "Jolt Rigid Body"),
        math.Vector3(0.0, 0.0, 3.0),
    )
    return floor_name, body_name, floor, body
