"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Verifies that advanced Jolt editor components create usable authored runtime resources.
"""


class Tests:
    level_opened = ("Opened the Jolt advanced feature level", "Failed to open the Jolt advanced feature level")
    components_created = ("Created the advanced Jolt components", "Failed to create the advanced Jolt components")
    enter_game_mode = ("Entered game mode", "Failed to enter game mode")
    resources_created = ("Created every advanced runtime resource", "Failed to create an advanced runtime resource")
    hair_operational = ("Read and updated the default hair", "The default hair was not operational")
    ragdoll_operational = ("Read and updated the default ragdoll", "The default ragdoll was not operational")
    scene_asset_operational = (
        "Instantiated the authored physics scene",
        "The authored physics scene was not operational",
    )
    skeleton_asset_operational = ("Loaded the authored skeleton asset component", "The skeleton asset component was not operational")
    skeleton_workflow = ("Sampled and mapped an authored skeleton", "Skeleton animation or mapping failed")
    soft_body_operational = ("Read and updated the authored soft body", "The authored soft body was not operational")
    soft_body_contact = ("Verified authored soft-body collision response", "Soft-body collision response failed")
    soft_body_skinning = ("Applied an authored soft-body skin pose", "Soft-body skinning was not operational")
    path_operational = ("Sampled the default path", "The default path was not operational")
    lifecycle_operational = ("Recreated every simulated resource", "Failed to recreate an advanced resource")
    exit_game_mode = ("Exited game mode", "Failed to exit game mode")


def Jolt_AdvancedComponents():
    import time

    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.jolt as jolt
    import azlmbr.legacy.general as general
    import azlmbr.math as math

    from editor_python_test_tools.asset_utils import Asset
    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.prefab_utils import PrefabWaiter
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    helper.open_level("", "Base")
    Report.result(Tests.level_opened, general.get_current_level_name() == "Base")

    component_names = (
        "Jolt Hair",
        "Jolt Ragdoll",
        "Jolt Soft Body",
        "Jolt Path",
        "Jolt Skeleton",
        "Jolt Scene",
    )
    editor_entities = []
    editor_components = []
    for component_index, component_name in enumerate(component_names):
        entity = EditorEntity.create_editor_entity(component_name)
        component = entity.add_component(component_name)
        components.TransformBus(
            bus.Event,
            "SetWorldTranslation",
            entity.id,
            math.Vector3(10.0 * component_index, 0.0, 0.0),
        )
        editor_entities.append(entity)
        editor_components.append(component)

    skeleton_asset = Asset.find_asset_by_path(
        "assets/physics/jolt/test_skeleton.joltskeleton"
    )
    editor_components[4].set_component_property_value(
        "Configuration|Asset",
        skeleton_asset.id,
    )
    scene_asset = Asset.find_asset_by_path(
        "assets/physics/jolt/test_scene.joltscene"
    )
    editor_components[5].set_component_property_value(
        "Configuration|Asset",
        scene_asset.id,
    )

    soft_body_floor = EditorEntity.create_editor_entity("Jolt Soft Body Floor")
    soft_body_floor.add_component("Jolt Collider")
    soft_body_floor.add_component("Jolt Static Rigid Body")
    components.TransformBus(
        bus.Event,
        "SetWorldTranslation",
        soft_body_floor.id,
        math.Vector3(20.0, 0.0, -2.5),
    )
    components.TransformBus(bus.Event, "SetLocalUniformScale", soft_body_floor.id, 2.0)

    inverse_bind = jolt.SoftBodyInverseBind()
    inverse_bind.transform = math.Transform_CreateIdentity()
    inverse_bind.jointIndex = 0
    soft_body_component = editor_components[2]
    soft_body_component.append_container_item(
        "Configuration|Definition|Inverse binds",
        inverse_bind,
    )
    for vertex_index in range(4):
        weight = jolt.SoftBodySkinWeight()
        weight.inverseBindIndex = 0
        weight.weight = 1.0
        constraint = jolt.SoftBodySkinConstraint()
        constraint.vertex = vertex_index
        constraint.maximumDistance = 0.0
        constraint.SetWeight(0, weight)
        soft_body_component.append_container_item(
            "Configuration|Definition|Skin constraints",
            constraint,
        )

    inverse_binds = soft_body_component.get_component_property_value(
        "Configuration|Definition|Inverse binds"
    )
    soft_body_component.set_component_property_value(
        "Configuration|Definition|Inverse binds",
        inverse_binds,
    )
    skin_constraints = soft_body_component.get_component_property_value(
        "Configuration|Definition|Skin constraints"
    )
    soft_body_component.set_component_property_value(
        "Configuration|Definition|Skin constraints",
        skin_constraints,
    )

    Report.info(
        "Authored soft body state: "
        f"inverseBinds={soft_body_component.get_container_count('Configuration|Definition|Inverse binds')}, "
        f"skinConstraints={soft_body_component.get_container_count('Configuration|Definition|Skin constraints')}"
    )
    PrefabWaiter.wait_for_propagation()

    Report.result(
        Tests.components_created,
        all(
            entity.has_component(component_name)
            for entity, component_name in zip(editor_entities, component_names)
        )
        and soft_body_floor.has_component("Jolt Static Rigid Body"),
    )

    Report.info("Entering game mode")
    general.enter_game_mode()
    enter_deadline = time.monotonic() + 3.0
    while not general.is_in_game_mode() and time.monotonic() < enter_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.enter_game_mode, general.is_in_game_mode())
    hair_id = general.find_game_entity("Jolt Hair")
    ragdoll_id = general.find_game_entity("Jolt Ragdoll")
    soft_body_id = general.find_game_entity("Jolt Soft Body")
    path_id = general.find_game_entity("Jolt Path")
    skeleton_id = general.find_game_entity("Jolt Skeleton")
    scene_id = general.find_game_entity("Jolt Scene")
    Report.info(
        "Advanced entity state: "
        f"hair={hair_id.IsValid()}, ragdoll={ragdoll_id.IsValid()}, "
        f"softBody={soft_body_id.IsValid()}, path={path_id.IsValid()}, "
        f"skeleton={skeleton_id.IsValid()}, scene={scene_id.IsValid()}"
    )

    def get_resource_state():
        return (
            jolt.JoltHairRequestBus(bus.Event, "IsSimulationEnabled", hair_id),
            jolt.JoltRagdollRequestBus(bus.Event, "IsSimulationEnabled", ragdoll_id),
            jolt.JoltSoftBodyRequestBus(bus.Event, "IsSimulationEnabled", soft_body_id),
            jolt.JoltPathRequestBus(bus.Event, "GetPathHandle", path_id),
            jolt.JoltSkeletonComponentRequestBus(bus.Event, "IsReady", skeleton_id),
            jolt.JoltSceneRequestBus(bus.Event, "IsReady", scene_id),
        )

    def resources_are_created():
        (
            hair_enabled,
            ragdoll_enabled,
            soft_body_enabled,
            path_handle,
            skeleton_ready,
            scene_ready,
        ) = get_resource_state()
        return (
            hair_enabled
            and ragdoll_enabled
            and soft_body_enabled
            and path_handle.IsValid()
            and skeleton_ready
            and scene_ready
        )

    resources_created = resources_are_created()
    resource_deadline = time.monotonic() + 3.0
    while not resources_created and time.monotonic() < resource_deadline:
        general.idle_wait(0.01)
        resources_created = resources_are_created()
    (
        hair_enabled,
        ragdoll_enabled,
        soft_body_enabled,
        path_handle,
        skeleton_ready,
        scene_ready,
    ) = get_resource_state()
    Report.info(
        "Advanced resource state: "
        f"hair={hair_enabled}, ragdoll={ragdoll_enabled}, "
        f"softBody={soft_body_enabled}, path={path_handle.IsValid()}, "
        f"skeleton={skeleton_ready}, scene={scene_ready}"
    )
    Report.result(Tests.resources_created, resources_created)

    skeleton_handle = jolt.JoltSkeletonComponentRequestBus(
        bus.Event,
        "GetSkeletonHandle",
        skeleton_id,
    )
    asset_animation_names = jolt.JoltSkeletonComponentRequestBus(
        bus.Event,
        "CopyAnimationNames",
        skeleton_id,
    )
    asset_animation_valid = False
    if len(asset_animation_names) == 1:
        asset_animation_handle = jolt.JoltSkeletonComponentRequestBus(
            bus.Event,
            "FindAnimation",
            skeleton_id,
            asset_animation_names[0],
        )
        asset_animation_valid = asset_animation_handle.IsValid()
    Report.result(
        Tests.skeleton_asset_operational,
        skeleton_handle.IsValid()
        and asset_animation_valid
        and len(asset_animation_names) == 1,
    )

    scene_definition_handle = jolt.JoltSceneRequestBus(
        bus.Event,
        "GetDefinitionHandle",
        scene_id,
    )
    scene_instance_handle = jolt.JoltSceneRequestBus(
        bus.Event,
        "GetInstanceHandle",
        scene_id,
    )
    scene_bodies = jolt.JoltSceneRequestBus(bus.Event, "CopyBodies", scene_id)
    scene_constraints = jolt.JoltSceneRequestBus(bus.Event, "CopyConstraints", scene_id)
    Report.result(
        Tests.scene_asset_operational,
        scene_definition_handle.IsValid()
        and scene_instance_handle.IsValid()
        and len(scene_bodies) == 1
        and len(scene_constraints) == 0,
    )

    hair_deadline = time.monotonic() + 1.0
    hair_operational = False
    while not hair_operational and time.monotonic() < hair_deadline:
        hair_handle = jolt.JoltHairRequestBus(bus.Event, "GetHairHandle", hair_id)
        hair_definition = jolt.JoltHairRequestBus(bus.Event, "GetDefinitionState", hair_id)
        hair_state = jolt.JoltHairRequestBus(bus.Event, "GetState", hair_id)
        hair_vertex_states = jolt.JoltHairRequestBus(bus.Event, "CopyVertexStates", hair_id)
        hair_render_positions = jolt.JoltHairRequestBus(bus.Event, "CopyRenderPositions", hair_id)
        hair_scalp_positions = jolt.JoltHairRequestBus(bus.Event, "CopyScalpPositions", hair_id)
        hair_grid_cells = jolt.JoltHairRequestBus(bus.Event, "CopyGridCellStates", hair_id)
        hair_neutral_density = jolt.JoltHairRequestBus(bus.Event, "CopyNeutralDensity", hair_id)
        hair_updated = jolt.JoltHairRequestBus(
            bus.Event,
            "SetScalpToHeadTransform",
            hair_id,
            math.Transform_CreateIdentity(),
        )
        hair_operational = (
            hair_handle.IsValid()
            and hair_definition.simulationVertexCount == 3
            and hair_state.simulationVertexCount == 3
            and len(hair_vertex_states) == hair_state.simulationVertexCount
            and len(hair_render_positions) == hair_state.renderVertexCount
            and len(hair_scalp_positions) == hair_state.scalpVertexCount
            and len(hair_grid_cells) == hair_state.gridCellCount
            and len(hair_neutral_density) == hair_state.gridCellCount
            and hair_updated
        )
        if not hair_operational:
            general.idle_wait(0.01)
    Report.info(
        "Hair state: "
        f"handle={hair_handle.IsValid()}, definition={hair_definition.simulationVertexCount}, "
        f"state={hair_state.simulationVertexCount}/{hair_state.renderVertexCount}/"
        f"{hair_state.scalpVertexCount}/{hair_state.gridCellCount}, "
        f"copies={len(hair_vertex_states)}/{len(hair_render_positions)}/"
        f"{len(hair_scalp_positions)}/{len(hair_grid_cells)}/{len(hair_neutral_density)}, "
        f"updated={hair_updated}"
    )
    Report.result(
        Tests.hair_operational,
        hair_operational,
    )

    ragdoll_handle = jolt.JoltRagdollRequestBus(bus.Event, "GetRagdollHandle", ragdoll_id)
    ragdoll_state = jolt.JoltRagdollRequestBus(bus.Event, "GetState", ragdoll_id)
    ragdoll_bodies = jolt.JoltRagdollRequestBus(bus.Event, "CopyBodies", ragdoll_id)
    ragdoll_constraints = jolt.JoltRagdollRequestBus(bus.Event, "CopyConstraints", ragdoll_id)
    ragdoll_pose = jolt.JoltRagdollRequestBus(bus.Event, "CopyPose", ragdoll_id)
    ragdoll_updated = jolt.JoltRagdollRequestBus(
        bus.Event,
        "AddImpulse",
        ragdoll_id,
        math.Vector3(0.0, 0.0, 0.1),
    )
    ragdoll_updated = ragdoll_updated and jolt.JoltRagdollRequestBus(
        bus.Event,
        "SetLinearVelocity",
        ragdoll_id,
        math.Vector3(0.1, 0.0, 0.0),
    )
    ragdoll_updated = ragdoll_updated and jolt.JoltRagdollRequestBus(
        bus.Event,
        "SetCollisionGroupId",
        ragdoll_id,
        17,
    )
    ragdoll_updated = ragdoll_updated and jolt.JoltRagdollRequestBus(
        bus.Event,
        "DriveMotors",
        ragdoll_id,
        ragdoll_pose,
    )
    ragdoll_updated = ragdoll_updated and jolt.JoltRagdollRequestBus(
        bus.Event,
        "DriveMotorsWithVelocity",
        ragdoll_id,
        ragdoll_pose,
        ragdoll_pose,
        1.0 / 60.0,
    )
    ragdoll_state = jolt.JoltRagdollRequestBus(bus.Event, "GetState", ragdoll_id)
    Report.info(
        "Ragdoll state: "
        f"handle={ragdoll_handle.IsValid()}, bodies={ragdoll_state.bodyCount}, "
        f"constraints={ragdoll_state.constraintCount}, "
        f"updated={ragdoll_updated}"
    )
    Report.result(
        Tests.ragdoll_operational,
        ragdoll_handle.IsValid()
        and ragdoll_state.bodyCount == 2
        and ragdoll_state.constraintCount == 1
        and len(ragdoll_bodies) == ragdoll_state.bodyCount
        and len(ragdoll_constraints) == ragdoll_state.constraintCount
        and len(ragdoll_pose) == ragdoll_state.bodyCount
        and ragdoll_state.collisionGroupId == 17
        and ragdoll_updated,
    )

    source_joint = jolt.SkeletonJoint()
    source_joint.name = "root"
    source_joint.parentIndex = -1
    source_skeleton_configuration = jolt.SkeletonDefinitionConfiguration()
    source_skeleton_configuration.joints = [source_joint]
    source_skeleton_handle = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "CreateSkeletonDefinition",
        source_skeleton_configuration,
    )

    target_joint = jolt.SkeletonJoint()
    target_joint.name = "root"
    target_joint.parentIndex = -1
    target_skeleton_configuration = jolt.SkeletonDefinitionConfiguration()
    target_skeleton_configuration.joints = [target_joint]
    target_skeleton_handle = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "CreateSkeletonDefinition",
        target_skeleton_configuration,
    )

    first_keyframe = jolt.SkeletalAnimationKeyframe()
    first_keyframe.time = 0.0
    second_keyframe = jolt.SkeletalAnimationKeyframe()
    second_keyframe.translation = math.Vector3(2.0, 0.0, 0.0)
    second_keyframe.time = 1.0
    animated_joint = jolt.SkeletalAnimatedJoint()
    animated_joint.name = "root"
    animated_joint.keyframes = [first_keyframe, second_keyframe]
    animation_configuration = jolt.SkeletalAnimationConfiguration()
    animation_configuration.joints = [animated_joint]
    animation_handle = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "CreateSkeletalAnimation",
        animation_configuration,
    )
    pose_handle = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "CreateSkeletonPose",
        source_skeleton_handle,
    )
    sampled = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "SampleSkeletalAnimation",
        animation_handle,
        pose_handle,
        0.5,
    )
    sampled_pose = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "CopySkeletonPoseModelTransforms",
        pose_handle,
    )

    mapper_configuration = jolt.SkeletonMapperConfiguration()
    mapper_configuration.sourceSkeletonHandle = source_skeleton_handle
    mapper_configuration.targetSkeletonHandle = target_skeleton_handle
    mapper_configuration.sourceNeutralModelTransforms = [math.Transform_CreateIdentity()]
    mapper_configuration.targetNeutralModelTransforms = [math.Transform_CreateIdentity()]
    mapper_handle = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "CreateSkeletonMapper",
        mapper_configuration,
    )
    mapped_pose = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "MapSkeletonPose",
        mapper_handle,
        sampled_pose,
        mapper_configuration.targetNeutralModelTransforms,
    )
    reverse_mapped_pose = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "MapSkeletonPoseReverse",
        mapper_handle,
        mapped_pose,
    )
    mapped_joint_index = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "FindMappedSkeletonJoint",
        mapper_handle,
        0,
    )
    skeleton_cleanup = jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "DestroySkeletonMapper",
        mapper_handle,
    )
    skeleton_cleanup = skeleton_cleanup and jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "DestroySkeletonPose",
        pose_handle,
    )
    skeleton_cleanup = skeleton_cleanup and jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "DestroySkeletalAnimation",
        animation_handle,
    )
    skeleton_cleanup = skeleton_cleanup and jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "DestroySkeletonDefinition",
        source_skeleton_handle,
    )
    skeleton_cleanup = skeleton_cleanup and jolt.JoltSkeletonRequestBus(
        bus.Broadcast,
        "DestroySkeletonDefinition",
        target_skeleton_handle,
    )
    Report.result(
        Tests.skeleton_workflow,
        bool(
            source_skeleton_handle.IsValid()
            and target_skeleton_handle.IsValid()
            and animation_handle.IsValid()
            and pose_handle.IsValid()
            and sampled
            and len(sampled_pose) == 1
            and abs(sampled_pose[0].GetTranslation().x - 1.0) < 1.0e-4
            and mapper_handle.IsValid()
            and mapped_joint_index == 0
            and len(mapped_pose) == 1
            and len(reverse_mapped_pose) == 1
            and abs(reverse_mapped_pose[0].GetTranslation().x - 1.0) < 1.0e-4
            and skeleton_cleanup
        ),
    )

    soft_body_handle = jolt.JoltSoftBodyRequestBus(bus.Event, "GetBodyHandle", soft_body_id)
    soft_body_definition_handle = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "GetDefinitionHandle",
        soft_body_id,
    )
    soft_body_definition = jolt.JoltSoftBodyRequestBus(bus.Event, "GetDefinitionState", soft_body_id)
    soft_body_vertices = jolt.JoltSoftBodyRequestBus(bus.Event, "CopyVertices", soft_body_id)
    soft_body_faces = jolt.JoltSoftBodyRequestBus(bus.Event, "CopyFaces", soft_body_id)
    soft_body_materials = jolt.JoltSoftBodyRequestBus(bus.Event, "CopyMaterials", soft_body_id)
    soft_body_edge_constraints = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "CopyDefinitionEdgeConstraints",
        soft_body_id,
    )
    soft_body_inverse_binds = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "CopyDefinitionInverseBinds",
        soft_body_id,
    )
    soft_body_skin_constraints = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "CopyDefinitionSkinConstraints",
        soft_body_id,
    )
    soft_body_updated = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "SetVertexVelocity",
        soft_body_id,
        2,
        math.Vector3(0.0, 0.0, -0.5),
    )
    Report.info(
        "Soft body state: "
        f"body={soft_body_handle.IsValid()}, definition={soft_body_definition_handle.IsValid()}, "
        f"vertices={soft_body_definition.vertexCount}, faces={soft_body_definition.faceCount}, "
        f"inverseBinds={soft_body_definition.inverseBindCount}/{len(soft_body_inverse_binds)}, "
        f"skinConstraints={soft_body_definition.skinConstraintCount}/{len(soft_body_skin_constraints)}, "
        f"updated={soft_body_updated}"
    )
    Report.result(
        Tests.soft_body_operational,
        soft_body_handle.IsValid()
        and soft_body_definition_handle.IsValid()
        and soft_body_definition.vertexCount == 4
        and soft_body_definition.faceCount == 2
        and len(soft_body_vertices) == soft_body_definition.vertexCount
        and len(soft_body_faces) == soft_body_definition.faceCount
        and len(soft_body_materials) == soft_body_definition.materialCount
        and len(soft_body_edge_constraints) == soft_body_definition.edgeConstraintCount
        and len(soft_body_inverse_binds) == soft_body_definition.inverseBindCount == 1
        and len(soft_body_skin_constraints) == soft_body_definition.skinConstraintCount == 4
        and soft_body_updated,
    )

    soft_body_world_handle = jolt.JoltSoftBodyRequestBus(bus.Event, "GetWorldHandle", soft_body_id)
    soft_body_world_configuration = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "GetRuntimeConfiguration",
        soft_body_world_handle,
    )
    soft_body_world_configuration.autoSimulate = False
    manual_soft_body_stepping_enabled = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "UpdateRuntimeConfiguration",
        soft_body_world_handle,
        soft_body_world_configuration,
    )
    dynamic_vertices = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "SetVertexInverseMass",
        soft_body_id,
        0,
        1.0,
    )
    dynamic_vertices = dynamic_vertices and jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "SetVertexInverseMass",
        soft_body_id,
        1,
        1.0,
    )
    for vertex_index in range(4):
        dynamic_vertices = dynamic_vertices and jolt.JoltSoftBodyRequestBus(
            bus.Event,
            "SetVertexVelocity",
            soft_body_id,
            vertex_index,
            math.Vector3(0.0, 0.0, -2.0),
        )
    soft_body_steps_succeeded = True
    for _ in range(60):
        step_result = jolt.JoltWorldQueryRequestBus(
            bus.Broadcast,
            "StepWorld",
            soft_body_world_handle,
            1.0 / 60.0,
        )
        soft_body_steps_succeeded = soft_body_steps_succeeded and step_result.stepCount == 1
    contacted_vertices = jolt.JoltSoftBodyRequestBus(bus.Event, "CopyVertices", soft_body_id)
    soft_body_world_configuration.autoSimulate = True
    automatic_soft_body_stepping_restored = jolt.JoltWorldQueryRequestBus(
        bus.Broadcast,
        "UpdateRuntimeConfiguration",
        soft_body_world_handle,
        soft_body_world_configuration,
    )
    Report.result(
        Tests.soft_body_contact,
        bool(
            manual_soft_body_stepping_enabled
            and dynamic_vertices
            and soft_body_steps_succeeded
            and len(contacted_vertices) == 4
            and min(vertex.position.z for vertex in contacted_vertices) > -1.6
            and automatic_soft_body_stepping_restored
        ),
    )

    bind_pose = [math.Transform_CreateIdentity()]
    bind_pose_applied = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "ApplySkinPose",
        soft_body_id,
        bind_pose,
        True,
    )
    vertices_at_bind_pose = jolt.JoltSoftBodyRequestBus(bus.Event, "CopyVertices", soft_body_id)
    target_pose = [math.Transform_CreateTranslation(math.Vector3(0.25, 0.0, 0.0))]
    target_pose_applied = jolt.JoltSoftBodyRequestBus(
        bus.Event,
        "ApplySkinPose",
        soft_body_id,
        target_pose,
        True,
    )
    vertices_at_target_pose = jolt.JoltSoftBodyRequestBus(bus.Event, "CopyVertices", soft_body_id)
    vertices_moved_with_pose = (
        len(vertices_at_bind_pose) == len(vertices_at_target_pose) == 4
        and all(
            abs(target.position.x - source.position.x - 0.25) < 1.0e-4
            for source, target in zip(vertices_at_bind_pose, vertices_at_target_pose)
        )
    )
    Report.info(
        "Soft body skinning state: "
        f"bindApplied={bind_pose_applied}, targetApplied={target_pose_applied}, "
        f"bindVertices={len(vertices_at_bind_pose)}, targetVertices={len(vertices_at_target_pose)}, "
        f"moved={vertices_moved_with_pose}"
    )
    Report.result(
        Tests.soft_body_skinning,
        bind_pose_applied and target_pose_applied and vertices_moved_with_pose,
    )

    path_handle = jolt.JoltPathRequestBus(bus.Event, "GetPathHandle", path_id)
    path_state = jolt.JoltPathRequestBus(bus.Event, "GetState", path_id)
    path_points = jolt.JoltPathRequestBus(bus.Event, "CopyPoints", path_id)
    path_sample = jolt.JoltPathRequestBus(bus.Event, "Sample", path_id, 0.5)
    closest_sample = jolt.JoltPathRequestBus(
        bus.Event,
        "FindClosestPoint",
        path_id,
        math.Vector3(0.0, 1.0, 0.0),
        0.5,
    )
    Report.info(
        "Path state: "
        f"handle={path_handle.IsValid()}, maximumFraction={path_state.maximumFraction}, "
        f"sample={path_sample.valid}, closest={closest_sample.valid}"
    )
    Report.result(
        Tests.path_operational,
        path_handle.IsValid()
        and len(path_points) == 2
        and path_state.maximumFraction == 1.0
        and path_sample.valid
        and closest_sample.valid,
    )

    hair_disabled = jolt.JoltHairRequestBus(bus.Event, "DisableSimulation", hair_id)
    ragdoll_disabled = jolt.JoltRagdollRequestBus(bus.Event, "DisableSimulation", ragdoll_id)
    soft_body_disabled = jolt.JoltSoftBodyRequestBus(bus.Event, "DisableSimulation", soft_body_id)
    hair_enabled = jolt.JoltHairRequestBus(bus.Event, "EnableSimulation", hair_id)
    ragdoll_enabled = jolt.JoltRagdollRequestBus(bus.Event, "EnableSimulation", ragdoll_id)
    soft_body_enabled = jolt.JoltSoftBodyRequestBus(bus.Event, "EnableSimulation", soft_body_id)
    Report.info(
        "Advanced lifecycle state: "
        f"hair={hair_disabled}/{hair_enabled}, "
        f"ragdoll={ragdoll_disabled}/{ragdoll_enabled}, "
        f"softBody={soft_body_disabled}/{soft_body_enabled}"
    )
    Report.result(
        Tests.lifecycle_operational,
        hair_disabled
        and ragdoll_disabled
        and soft_body_disabled
        and hair_enabled
        and ragdoll_enabled
        and soft_body_enabled,
    )

    Report.info("Exiting game mode")
    general.exit_game_mode()
    exit_deadline = time.monotonic() + 3.0
    while general.is_in_game_mode() and time.monotonic() < exit_deadline:
        general.idle_wait(0.01)
    Report.critical_result(Tests.exit_game_mode, not general.is_in_game_mode())


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(Jolt_AdvancedComponents)
