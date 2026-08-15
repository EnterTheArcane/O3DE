"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Regenerates the checked-in Jolt component smoke level through the Editor serializer.
"""


def GenerateComponentSmoke():
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.editor as editor
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.asset_utils import Asset
    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.prefab_utils import PrefabWaiter
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    if not helper.create_level("JoltSavedComponents"):
        raise RuntimeError("Failed to create the Jolt saved-component level")

    level_id = editor.ToolsApplicationRequestBus(bus.Broadcast, "GetCurrentLevelEntityId")
    EditorEntity.find_editor_entity("Atom Default Environment").set_parent_entity(level_id)

    floor = EditorEntity.create_editor_entity("Jolt Saved Floor", level_id)
    floor.add_component("Jolt Collider")
    floor.add_component("Jolt Static Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", floor.id, math.Vector3(0.0, 0.0, -5.0))
    components.TransformBus(bus.Event, "SetLocalUniformScale", floor.id, 10.0)

    falling_body = EditorEntity.create_editor_entity("Jolt Saved Falling Body", level_id)
    falling_body.add_component("Jolt Collider")
    falling_body.add_component("Jolt Rigid Body")
    components.TransformBus(bus.Event, "SetWorldTranslation", falling_body.id, math.Vector3(0.0, 0.0, 5.0))

    scene = EditorEntity.create_editor_entity("Jolt Saved Scene", level_id)
    scene_component = scene.add_component("Jolt Scene")
    scene_asset = Asset.find_asset_by_path("assets/physics/jolt/test_scene.joltscene")
    scene_component.set_component_property_value("Configuration|Asset", scene_asset.id)
    components.TransformBus(bus.Event, "SetWorldTranslation", scene.id, math.Vector3(10.0, 0.0, 0.0))

    skeleton = EditorEntity.create_editor_entity("Jolt Saved Skeleton", level_id)
    skeleton_component = skeleton.add_component("Jolt Skeleton")
    skeleton_asset = Asset.find_asset_by_path("assets/physics/jolt/test_skeleton.joltskeleton")
    skeleton_component.set_component_property_value("Configuration|Asset", skeleton_asset.id)
    components.TransformBus(bus.Event, "SetWorldTranslation", skeleton.id, math.Vector3(20.0, 0.0, 0.0))

    PrefabWaiter.wait_for_propagation()
    Report.result(
        ("Authored the Jolt component smoke level", "Failed to author the Jolt component smoke level"),
        floor.has_component("Jolt Static Rigid Body")
        and falling_body.has_component("Jolt Rigid Body")
        and scene.has_component("Jolt Scene")
        and skeleton.has_component("Jolt Skeleton"),
    )
    general.save_level()


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(GenerateComponentSmoke)
