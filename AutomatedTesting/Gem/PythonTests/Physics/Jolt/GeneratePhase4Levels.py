"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT

Regenerates the checked-in Jolt feature-gallery and stress levels through the Editor serializer.
"""


def _create_entity(recorder, editor_entity, components, bus, math, name, component_names, position, parent_id):
    import azlmbr.editor as editor

    from editor_python_test_tools.editor_entity_utils import EditorComponent
    from editor_python_test_tools.editor_entity_utils import EditorEntityType

    entity = editor_entity.create_editor_entity(name, parent_id)
    type_ids = EditorComponent.get_type_ids(list(component_names), EditorEntityType.GAME)
    for type_id in type_ids:
        add_component_outcome = editor.EditorComponentAPIBus(
            bus.Broadcast,
            "AddComponentsOfType",
            entity.id,
            [type_id],
        )
        if not add_component_outcome.IsSuccess():
            raise RuntimeError(f"Failed to add a component to {name}: {add_component_outcome.GetError()}")

    components.TransformBus(bus.Event, "SetWorldTranslation", entity.id, position)
    recorder.append(
        entity.exists()
        and all(entity.has_component(component_name) for component_name in component_names)
    )
    return entity


def _parent_optional_environment(editor_entity, level_id):
    environments = editor_entity.find_editor_entities(["Atom Default Environment"])
    if environments:
        environments[0].set_parent_entity(level_id)


def _set_component_property(editor, bus, component, property_path, value):
    outcome = editor.EditorComponentAPIBus(
        bus.Broadcast,
        "SetComponentProperty",
        component.id,
        property_path,
        value,
    )
    if not outcome.IsSuccess():
        raise RuntimeError(f"Failed to set component property {property_path}: {outcome.GetError()}")


def _remove_existing_outputs(project_root):
    import os
    import shutil

    output_paths = (
        os.path.join(project_root, "Levels", "Physics", "Jolt", "FeatureGallery"),
        os.path.join(project_root, "Levels", "Physics", "Jolt", "Stress"),
        os.path.join(project_root, "Prefabs", "Physics", "Jolt", "StressBody.prefab"),
    )
    normalized_project_root = os.path.normcase(os.path.abspath(project_root))
    for output_path in output_paths:
        normalized_output_path = os.path.normcase(os.path.abspath(output_path))
        if os.path.commonpath((normalized_project_root, normalized_output_path)) != normalized_project_root:
            raise RuntimeError(f"Refusing to remove generated output outside the project: {output_path}")

        if os.path.isdir(output_path):
            shutil.rmtree(output_path)
        elif os.path.isfile(output_path):
            os.remove(output_path)


def _write_json_atomic(path, document):
    import json
    import os

    temporary_path = f"{path}.tmp"
    with open(temporary_path, "w", encoding="utf-8", newline="\n") as output_file:
        json.dump(document, output_file, indent=4)
        output_file.write("\n")
    os.replace(temporary_path, path)


def _complete_level_container(project_root, document):
    import copy
    import json
    import os

    template_path = os.path.join(
        project_root,
        "Levels",
        "Physics",
        "Jolt",
        "ComponentSmoke",
        "ComponentSmoke.prefab",
    )
    with open(template_path, "r", encoding="utf-8") as template_file:
        template_document = json.load(template_file)

    container = document["ContainerEntity"]
    container["Components"] = copy.deepcopy(template_document["ContainerEntity"]["Components"])
    child_entity_order = []
    for entity in document["Entities"].values():
        for component in entity.get("Components", {}).values():
            if (
                "TransformComponent" in component.get("$type", "")
                and component.get("Parent Entity") == container["Id"]
            ):
                child_entity_order.append(entity["Id"])
                break

    for component in container["Components"].values():
        if component.get("$type") == "EditorEntitySortComponent":
            component["Child Entity Order"] = child_entity_order
            break


def _patch_gallery_configuration(project_root):
    import json
    import os

    level_path = os.path.join(
        project_root,
        "Levels",
        "Physics",
        "Jolt",
        "FeatureGallery",
        "FeatureGallery.prefab",
    )
    with open(level_path, "r", encoding="utf-8") as level_file:
        document = json.load(level_file)

    document["ContainerEntity"]["Name"] = "FeatureGallery"
    geometries = {
        "Box": {
            "$type": "{E564307C-50D4-4422-A367-08940A8144F2} BoxShapeConfiguration",
            "Dimensions": [1.5, 1.0, 0.75],
            "ConvexRadius": 0.04,
        },
        "Capsule": {
            "$type": "{66D56089-A4EA-4B20-ADA1-09AB7682554A} CapsuleShapeConfiguration",
            "CylinderHeight": 1.75,
            "Radius": 0.4,
        },
        "Convex Hull": {
            "$type": "{72BB7A35-821E-454A-9E8A-5F95CA77FB90} ConvexHullShapeConfiguration",
            "Points": [
                [-0.75, -0.75, -0.75],
                [-0.75, 0.75, 0.75],
                [0.75, -0.75, 0.75],
                [0.75, 0.75, -0.75],
            ],
            "HullTolerance": 0.001,
            "MaximumConvexRadius": 0.04,
            "MaximumConvexRadiusError": 0.02,
        },
        "Cylinder": {
            "$type": "{3B313DF0-8CAA-4D44-B17F-DFC56B5ED0EF} CylinderShapeConfiguration",
            "Height": 1.5,
            "Radius": 0.55,
            "ConvexRadius": 0.04,
        },
        "Empty": {
            "$type": "{73CACE27-6412-4A22-AEE4-8EDD7CCE2AF4} EmptyShapeConfiguration",
            "CenterOfMass": [0.1, 0.0, 0.0],
        },
        "Heightfield": {
            "$type": "{8F8E2D77-A806-4AC1-8690-591B9E8C1728} HeightfieldShapeConfiguration",
            "Heights": [
                0.0,
                0.1,
                0.2,
                0.3,
                0.1,
                0.2,
                0.3,
                0.4,
                0.2,
                0.3,
                0.4,
                0.5,
                0.3,
                0.4,
                0.5,
                0.6,
            ],
            "Origin": [-1.5, -1.5, 0.0],
            "Spacing": [1.0, 1.0],
            "ActiveEdgeCosineThreshold": 0.99,
            "BitsPerSample": 8,
            "BlockSize": 2,
            "MaterialCapacity": 1,
            "MaximumHeight": 1.0,
            "MinimumHeight": -1.0,
            "OverrideHeightRange": True,
            "SampleCount": 4,
        },
        "Mesh": {
            "$type": "{8B9A9F9E-6FA3-4E2F-98E4-58217003A8D3} MeshShapeConfiguration",
            "Vertices": [
                [-1.0, -1.0, 0.0],
                [1.0, -1.0, 0.0],
                [1.0, 1.0, 0.25],
                [-1.0, 1.0, 0.25],
            ],
            "Triangles": [
                {
                    "FirstVertex": 0,
                    "SecondVertex": 1,
                    "ThirdVertex": 2,
                    "UserData": 4097,
                },
                {
                    "FirstVertex": 0,
                    "SecondVertex": 2,
                    "ThirdVertex": 3,
                    "UserData": 4098,
                },
            ],
            "ActiveEdgeCosineThreshold": 0.99,
            "BuildQuality": "FavorRuntimePerformance",
            "MaximumTrianglesPerLeaf": 4,
            "PerTriangleUserData": True,
        },
        "Plane": {
            "$type": "{023A0815-EA74-4CEC-807C-3445BC03B33F} PlaneShapeConfiguration",
            "Normal": [0.0, 0.0, 1.0],
            "Distance": 0.25,
            "HalfExtent": 2.0,
        },
        "Sphere": {
            "$type": "{C4264778-5AA2-4457-84FF-EB00D3B53D03} SphereShapeConfiguration",
            "Radius": 0.7,
        },
        "Tapered Capsule": {
            "$type": "{3864A80A-0279-4277-AF41-4F18C796EEA4} TaperedCapsuleShapeConfiguration",
            "Height": 1.5,
            "BottomRadius": 0.55,
            "TopRadius": 0.3,
        },
        "Tapered Cylinder": {
            "$type": "{B93230C8-8419-4ABB-8E87-C951364F4CB2} TaperedCylinderShapeConfiguration",
            "Height": 1.5,
            "BottomRadius": 0.65,
            "TopRadius": 0.35,
            "ConvexRadius": 0.04,
        },
        "Triangle": {
            "$type": "{00E82F22-CB32-4390-BB1D-ED5A14544C7D} TriangleShapeConfiguration",
            "FirstVertex": [-0.75, -0.5, 0.0],
            "SecondVertex": [0.75, -0.5, 0.0],
            "ThirdVertex": [0.0, 0.75, 0.25],
            "ConvexRadius": 0.01,
        },
    }

    patched_shape_count = 0
    for entity in document["Entities"].values():
        prefix = "Jolt Gallery "
        name = entity.get("Name", "")
        if not name.startswith(prefix):
            continue

        geometry = geometries.get(name[len(prefix):])
        if geometry is None:
            continue

        for component in entity.get("Components", {}).values():
            component_type = component.get("$type", "")
            if "ColliderComponent" not in component_type:
                continue

            shapes = component.get("Shapes", [])
            if not shapes:
                continue

            shapes[0]["Shape"]["Geometry"] = {
                "$type": geometry["$type"],
                "Value": {
                    key: value
                    for key, value in geometry.items()
                    if key != "$type"
                },
            }
            shapes[0]["Shape"]["Density"] = 975.0 + patched_shape_count
            shapes[0]["Shape"]["UserData"] = 0x4A6F6C740000 + patched_shape_count
            patched_shape_count += 1
            break

    if patched_shape_count != len(geometries):
        raise RuntimeError(
            f"Expected to patch {len(geometries)} gallery shapes, patched {patched_shape_count}"
        )

    entities_by_name = {
        entity.get("Name", ""): entity
        for entity in document["Entities"].values()
    }
    level_entity_id = document["ContainerEntity"]["Id"]
    for entity in document["Entities"].values():
        for component in entity.get("Components", {}).values():
            if (
                "TransformComponent" in component.get("$type", "")
                and not component.get("Parent Entity")
            ):
                component["Parent Entity"] = level_entity_id

    def set_translation(entity_name, translation, uniform_scale=None):
        entity = entities_by_name[entity_name]
        for component in entity.get("Components", {}).values():
            if "TransformComponent" not in component.get("$type", ""):
                continue

            transform_data = component.setdefault("Transform Data", {})
            transform_data["Translate"] = translation
            if uniform_scale is not None:
                transform_data["UniformScale"] = uniform_scale
            return

        raise RuntimeError(f"Entity {entity_name} has no transform component")

    set_translation("Jolt Gallery Static Body", [0.0, 0.0, -2.0])
    set_translation("Jolt Gallery Dynamic Body", [0.0, 0.0, 3.0], 1.25)
    for shape_index, shape_name in enumerate(geometries):
        row = shape_index // 4
        column = shape_index % 4
        set_translation(
            f"Jolt Gallery {shape_name}",
            [-12.0 + 3.0 * column, 5.0 + 3.0 * row, 0.0],
        )

    path_entity_id = entities_by_name["Jolt Gallery Path"]["Id"]
    hinge_constraint_id = entities_by_name["Jolt Gallery Constraint Hinge"]["Id"]
    slider_constraint_id = entities_by_name["Jolt Gallery Constraint Slider"]["Id"]
    constraint_geometries = {
        "Cone": {
            "$type": "ConeConstraintConfiguration",
            "HalfConeAngle": 0.65,
            "SecondPoint": {"Z": -2.0},
        },
        "Custom": {
            "$type": "CustomConstraintConfiguration",
            "Data": [17, 34, 51],
        },
        "Distance": {
            "$type": "DistanceConstraintConfiguration",
            "MaximumDistance": 2.5,
            "MinimumDistance": 1.5,
        },
        "Fixed": {
            "$type": "FixedConstraintConfiguration",
            "AutoDetectPoint": False,
            "SecondPoint": {"Z": -2.0},
        },
        "Gear": {
            "$type": "GearConstraintComponentConfiguration",
            "FirstHingeEntityId": hinge_constraint_id,
            "SecondHingeEntityId": hinge_constraint_id,
            "Ratio": 2.5,
        },
        "Hinge": {
            "$type": "HingeConstraintConfiguration",
            "MaximumFrictionTorque": 0.25,
            "MaximumLimit": 0.75,
            "MinimumLimit": -0.5,
            "SecondPoint": {"Z": -2.0},
        },
        "Path": {
            "$type": "PathConstraintComponentConfiguration",
            "MaximumFrictionForce": 2.0,
            "PathEntityId": path_entity_id,
            "TargetPathFraction": 0.5,
            "TargetVelocity": 0.25,
        },
        "Point": {
            "$type": "PointConstraintConfiguration",
            "SecondPoint": {"Z": -2.0},
        },
        "Pulley": {
            "$type": "PulleyConstraintConfiguration",
            "FirstFixedPoint": {"Z": 2.0},
            "MaximumLength": 5.0,
            "MinimumLength": 1.0,
            "Ratio": 1.5,
            "SecondFixedPoint": {"Z": 2.0},
        },
        "Rack And Pinion": {
            "$type": "RackAndPinionConstraintComponentConfiguration",
            "PinionConstraintEntityId": hinge_constraint_id,
            "RackConstraintEntityId": slider_constraint_id,
            "Ratio": 1.75,
        },
        "Six Dof": {
            "$type": "SixDofConstraintConfiguration",
            "TargetAngularVelocity": [0.0, 0.0, 0.25],
            "TargetPosition": [0.1, 0.2, 0.3],
            "TranslationX": {
                "MaximumLimit": 0.5,
                "MinimumLimit": -0.5,
                "Mode": "Limited",
            },
        },
        "Slider": {
            "$type": "SliderConstraintConfiguration",
            "MaximumFrictionForce": 0.5,
            "MaximumLimit": 1.0,
            "MinimumLimit": -1.0,
            "SecondPoint": {"Z": -2.0},
            "TargetPosition": 0.25,
        },
        "Swing Twist": {
            "$type": "SwingTwistConstraintConfiguration",
            "MaximumFrictionTorque": 0.5,
            "NormalHalfConeAngle": 0.8,
            "PlaneHalfConeAngle": 0.6,
            "SecondPoint": {"Z": -2.0},
            "TwistMaximumAngle": 0.5,
            "TwistMinimumAngle": -0.5,
        },
    }

    disabled_constraints = {"Custom", "Gear", "Rack And Pinion"}
    patched_constraint_count = 0
    for constraint_name, geometry in constraint_geometries.items():
        entity_name = f"Jolt Gallery Constraint {constraint_name}"
        constraint_index = patched_constraint_count
        set_translation(f"{entity_name} Anchor", [3.0 * constraint_index, -10.0, 0.0])
        set_translation(entity_name, [3.0 * constraint_index, -10.0, 2.0])
        entity = entities_by_name[entity_name]
        anchor = entities_by_name[f"{entity_name} Anchor"]
        for component in entity.get("Components", {}).values():
            if "ConstraintComponent" not in component.get("$type", ""):
                continue

            configuration = component["Configuration"]
            configuration["FirstBodyEntityId"] = anchor["Id"]
            configuration["SecondBodyEntityId"] = entity["Id"]
            configuration["Geometry"] = {
                "$type": geometry["$type"],
                "Value": {
                    key: value
                    for key, value in geometry.items()
                    if key != "$type"
                },
            }
            configuration["UserData"] = 0x4A6F6C740100 + patched_constraint_count
            configuration["Priority"] = patched_constraint_count + 1
            configuration["PositionStepCount"] = 2
            configuration["VelocityStepCount"] = 2
            configuration["Enabled"] = constraint_name not in disabled_constraints
            patched_constraint_count += 1
            break

    if patched_constraint_count != len(constraint_geometries):
        raise RuntimeError(
            f"Expected to patch {len(constraint_geometries)} gallery constraints, "
            f"patched {patched_constraint_count}"
        )

    gallery_components = (
        "Character",
        "Virtual Character",
        "Wheeled Vehicle",
        "Motorcycle",
        "Tracked Vehicle",
        "Soft Body",
        "Ragdoll",
        "Path",
        "Hair",
    )
    for component_index, component_name in enumerate(gallery_components):
        set_translation(
            f"Jolt Gallery {component_name}",
            [3.0 * component_index, -6.0, 2.0],
        )

    set_translation("Jolt Gallery Skeleton", [21.0, -2.0, 2.0])
    set_translation("Jolt Gallery Scene", [24.0, -2.0, 2.0])

    _complete_level_container(project_root, document)

    _write_json_atomic(level_path, document)


def _patch_stress_assets(project_root):
    import copy
    import json
    import os

    level_path = os.path.join(
        project_root,
        "Levels",
        "Physics",
        "Jolt",
        "Stress",
        "Stress.prefab",
    )
    prefab_path = os.path.join(
        project_root,
        "Prefabs",
        "Physics",
        "Jolt",
        "StressBody.prefab",
    )
    with open(level_path, "r", encoding="utf-8") as level_file:
        document = json.load(level_file)

    document["ContainerEntity"]["Name"] = "Stress"
    entities_by_name = {
        entity.get("Name", ""): entity
        for entity in document["Entities"].values()
    }
    level_entity_id = document["ContainerEntity"]["Id"]
    for entity in document["Entities"].values():
        for component in entity.get("Components", {}).values():
            if (
                "TransformComponent" in component.get("$type", "")
                and not component.get("Parent Entity")
            ):
                component["Parent Entity"] = level_entity_id

    def set_translation(entity_name, translation):
        entity = entities_by_name[entity_name]
        for component in entity.get("Components", {}).values():
            if "TransformComponent" in component.get("$type", ""):
                component.setdefault("Transform Data", {})["Translate"] = translation
                return

        raise RuntimeError(f"Entity {entity_name} has no transform component")

    set_translation("Jolt Stress Floor", [0.0, 0.0, -4.0])
    set_translation("Jolt Stress Driver", [0.0, 0.0, 12.0])
    stress_components = (
        "Character",
        "Virtual Character",
        "Vehicle",
        "Soft Body",
        "Ragdoll",
        "Hair",
    )
    for component_index, component_name in enumerate(stress_components):
        set_translation(
            f"Jolt Stress {component_name}",
            [12.0 + 3.0 * component_index, 0.0, 3.0],
        )

    _complete_level_container(project_root, document)

    source_entity = copy.deepcopy(entities_by_name["Jolt Stress Driver"])
    source_entity["Id"] = "Entity_[1]"
    source_entity["Name"] = "Jolt Stress Body"
    for component in source_entity.get("Components", {}).values():
        if "TransformComponent" in component.get("$type", ""):
            component["Parent Entity"] = "ContainerEntity"
            component.pop("Transform Data", None)
            break

    source_document = {
        "ContainerEntity": {
            "Id": "ContainerEntity",
            "Name": "StressBody",
            "Components": {
                "EditorLockComponent": {
                    "$type": "EditorLockComponent",
                    "Id": 1,
                },
                "EditorPrefabComponent": {
                    "$type": "EditorPrefabComponent",
                    "Id": 2,
                },
                "EditorVisibilityComponent": {
                    "$type": "EditorVisibilityComponent",
                    "Id": 3,
                },
                "TransformComponent": {
                    "$type": "{27F1E1A1-8D9D-4C3B-BD3A-AFB9762449C0} TransformComponent",
                    "Id": 4,
                    "Parent Entity": "",
                },
            },
        },
        "Entities": {
            source_entity["Id"]: source_entity,
        },
    }

    _write_json_atomic(level_path, document)

    os.makedirs(os.path.dirname(prefab_path), exist_ok=True)
    _write_json_atomic(prefab_path, source_document)


def GeneratePhase4Levels():
    import azlmbr.bus as bus
    import azlmbr.components as components
    import azlmbr.editor as editor
    import azlmbr.legacy.general as general
    import azlmbr.math as math
    import azlmbr.paths
    import azlmbr.prefab as prefab

    from editor_python_test_tools.asset_utils import Asset
    from editor_python_test_tools.editor_entity_utils import EditorEntity
    from editor_python_test_tools.prefab_utils import get_prefab_file_path
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    _remove_existing_outputs(azlmbr.paths.projectroot)
    gallery_created = helper.create_level("Physics/Jolt/FeatureGallery")
    if not gallery_created:
        raise RuntimeError("Failed to create the Jolt feature-gallery level")

    level_id = editor.ToolsApplicationRequestBus(bus.Broadcast, "GetCurrentLevelEntityId")
    _parent_optional_environment(EditorEntity, level_id)

    authored = []
    _create_entity(
        authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Gallery Static Body",
        ("Jolt Collider", "Jolt Static Rigid Body"),
        math.Vector3(0.0, 0.0, -2.0),
        level_id,
    )
    dynamic_body = _create_entity(
        authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Gallery Dynamic Body",
        ("Jolt Collider", "Jolt Rigid Body"),
        math.Vector3(0.0, 0.0, 3.0),
        level_id,
    )

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
    for shape_index, shape_name in enumerate(shape_names):
        row = shape_index // 4
        column = shape_index % 4
        _create_entity(
            authored,
            EditorEntity,
            components,
            bus,
            math,
            f"Jolt Gallery {shape_name}",
            ("Jolt Collider", "Jolt Static Rigid Body"),
            math.Vector3(-12.0 + 3.0 * column, 5.0 + 3.0 * row, 0.0),
            level_id,
        )

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
    for constraint_index, constraint_name in enumerate(constraint_names):
        _create_entity(
            authored,
            EditorEntity,
            components,
            bus,
            math,
            f"Jolt Gallery Constraint {constraint_name} Anchor",
            ("Jolt Collider", "Jolt Static Rigid Body"),
            math.Vector3(3.0 * constraint_index, -10.0, 0.0),
            level_id,
        )
        _create_entity(
            authored,
            EditorEntity,
            components,
            bus,
            math,
            f"Jolt Gallery Constraint {constraint_name}",
            ("Jolt Collider", "Jolt Rigid Body", "Jolt Constraint"),
            math.Vector3(3.0 * constraint_index, -10.0, 2.0),
            level_id,
        )

    component_specs = (
        (
            "Jolt Gallery Character",
            ("Jolt Collider", "Jolt Character Controller"),
        ),
        (
            "Jolt Gallery Virtual Character",
            ("Jolt Collider", "Jolt Virtual Character Controller"),
        ),
        (
            "Jolt Gallery Wheeled Vehicle",
            ("Jolt Collider", "Jolt Rigid Body", "Jolt Wheeled Vehicle"),
        ),
        (
            "Jolt Gallery Motorcycle",
            ("Jolt Collider", "Jolt Rigid Body", "Jolt Motorcycle"),
        ),
        (
            "Jolt Gallery Tracked Vehicle",
            ("Jolt Collider", "Jolt Rigid Body", "Jolt Tracked Vehicle"),
        ),
        ("Jolt Gallery Soft Body", ("Jolt Soft Body",)),
        ("Jolt Gallery Ragdoll", ("Jolt Ragdoll",)),
        ("Jolt Gallery Path", ("Jolt Path",)),
        ("Jolt Gallery Hair", ("Jolt Hair",)),
    )
    for component_index, (name, component_names) in enumerate(component_specs):
        _create_entity(
            authored,
            EditorEntity,
            components,
            bus,
            math,
            name,
            component_names,
            math.Vector3(3.0 * component_index, -6.0, 2.0),
            level_id,
        )

    skeleton = _create_entity(
        authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Gallery Skeleton",
        ("Jolt Skeleton",),
        math.Vector3(21.0, -2.0, 2.0),
        level_id,
    )
    skeleton_component = skeleton.get_components_of_type(["Jolt Skeleton"])[0]
    skeleton_asset = Asset.find_asset_by_path("assets/physics/jolt/test_skeleton.jolt")
    _set_component_property(
        editor,
        bus,
        skeleton_component,
        "Configuration|Asset",
        skeleton_asset.id,
    )

    scene = _create_entity(
        authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Gallery Scene",
        ("Jolt Scene",),
        math.Vector3(24.0, -2.0, 2.0),
        level_id,
    )
    scene_component = scene.get_components_of_type(["Jolt Scene"])[0]
    scene_asset = Asset.find_asset_by_path("assets/physics/jolt/test_scene.jolt")
    _set_component_property(
        editor,
        bus,
        scene_component,
        "Configuration|Asset",
        scene_asset.id,
    )

    components.TransformBus(bus.Event, "SetLocalUniformScale", dynamic_body.id, 1.25)
    Report.result(
        ("Authored every Jolt gallery component", "Failed to author a Jolt gallery component"),
        all(authored),
    )
    general.save_level()

    stress_created = helper.create_level("Physics/Jolt/Stress")
    if not stress_created:
        raise RuntimeError("Failed to create the Jolt stress level")

    level_id = editor.ToolsApplicationRequestBus(bus.Broadcast, "GetCurrentLevelEntityId")
    _parent_optional_environment(EditorEntity, level_id)

    stress_authored = []
    _create_entity(
        stress_authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Stress Floor",
        ("Jolt Collider", "Jolt Static Rigid Body"),
        math.Vector3(0.0, 0.0, -4.0),
        level_id,
    )
    driver = _create_entity(
        stress_authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Stress Driver",
        ("Jolt Collider", "Jolt Rigid Body"),
        math.Vector3(0.0, 0.0, 12.0),
        level_id,
    )

    seed = _create_entity(
        stress_authored,
        EditorEntity,
        components,
        bus,
        math,
        "Jolt Stress Body",
        ("Jolt Collider", "Jolt Rigid Body"),
        math.Vector3(-4.5, -4.5, 0.5),
        level_id,
    )
    stress_prefab_path = get_prefab_file_path("Prefabs/Physics/Jolt/StressBody.prefab")
    create_prefab_outcome = prefab.PrefabPublicRequestBus(
        bus.Broadcast,
        "CreatePrefabInMemory",
        [seed.id],
        stress_prefab_path,
    )
    if not create_prefab_outcome.IsSuccess():
        raise RuntimeError(f"Failed to create the reusable stress prefab: {create_prefab_outcome.GetError()}")

    for body_index in range(1, 64):
        layer = body_index // 16
        row = (body_index // 4) % 4
        column = body_index % 4
        position = math.Vector3(
            -4.5 + 3.0 * column,
            -4.5 + 3.0 * row,
            0.5 + 1.1 * layer,
        )
        instantiate_outcome = prefab.PrefabPublicRequestBus(
            bus.Broadcast,
            "InstantiatePrefab",
            stress_prefab_path,
            level_id,
            position,
        )
        if not instantiate_outcome.IsSuccess():
            raise RuntimeError(
                f"Failed to instantiate stress body {body_index}: {instantiate_outcome.GetError()}"
            )

        editor.EditorEntityAPIBus(
            bus.Event,
            "SetName",
            instantiate_outcome.GetValue(),
            f"Jolt Stress Body {body_index}",
        )

    stress_components = (
        ("Jolt Stress Character", ("Jolt Collider", "Jolt Character Controller")),
        (
            "Jolt Stress Virtual Character",
            ("Jolt Collider", "Jolt Virtual Character Controller"),
        ),
        (
            "Jolt Stress Vehicle",
            ("Jolt Collider", "Jolt Rigid Body", "Jolt Wheeled Vehicle"),
        ),
        ("Jolt Stress Soft Body", ("Jolt Soft Body",)),
        ("Jolt Stress Ragdoll", ("Jolt Ragdoll",)),
        ("Jolt Stress Hair", ("Jolt Hair",)),
    )
    for component_index, (name, component_names) in enumerate(stress_components):
        _create_entity(
            stress_authored,
            EditorEntity,
            components,
            bus,
            math,
            name,
            component_names,
            math.Vector3(12.0 + 3.0 * component_index, 0.0, 3.0),
            level_id,
        )

    Report.result(
        ("Authored the reusable Jolt stress level", "Failed to author the Jolt stress level"),
        all(stress_authored) and driver.exists(),
    )
    general.save_level()
    helper.open_level("", "Base")
    _patch_gallery_configuration(azlmbr.paths.projectroot)
    _patch_stress_assets(azlmbr.paths.projectroot)


if __name__ == "__main__":
    from editor_python_test_tools.utils import Report

    Report.start_test(GeneratePhase4Levels)
