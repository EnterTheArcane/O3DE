"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

import json
import re
from pathlib import Path

import pytest

from ly_test_tools.o3de.editor_test import EditorSingleTest, EditorTestSuite

from .tests.Jolt_ScenarioRecorder import SCENARIO_MINIMUM_CHECK_COUNTS


@pytest.mark.SUITE_main
@pytest.mark.parametrize("launcher_platform", ["windows_editor"])
@pytest.mark.parametrize("project", ["AutomatedTesting"])
class TestAutomation(EditorTestSuite):
    global_extra_cmdline_args = ["-BatchMode", "-autotest_mode", "-rhi=Null", "-NullRenderer"]

    class test_Jolt_AdvancedComponents(EditorSingleTest):
        from .tests import Jolt_AdvancedComponents as test_module

    class test_Jolt_Characters(EditorSingleTest):
        from .tests import Jolt_Characters as test_module

    class test_Jolt_ComponentSmoke(EditorSingleTest):
        from .tests import Jolt_ComponentSmoke as test_module

    class test_Jolt_Constraints(EditorSingleTest):
        from .tests import Jolt_Constraints as test_module

    class test_Jolt_CpuHair(EditorSingleTest):
        from .tests import Jolt_CpuHair as test_module

    class test_Jolt_Diagnostics(EditorSingleTest):
        timeout = 600
        from .tests import Jolt_Diagnostics as test_module

    class test_Jolt_EventsAndFilters(EditorSingleTest):
        from .tests import Jolt_EventsAndFilters as test_module

    class test_Jolt_FeatureComponents(EditorSingleTest):
        from .tests import Jolt_FeatureComponents as test_module

    class test_Jolt_Hair(EditorSingleTest):
        from .tests import Jolt_Hair as test_module

    class test_Jolt_Queries(EditorSingleTest):
        from .tests import Jolt_Queries as test_module

    class test_Jolt_RagdollsAndSkeletons(EditorSingleTest):
        from .tests import Jolt_RagdollsAndSkeletons as test_module

    class test_Jolt_RollbackAndDeterminism(EditorSingleTest):
        from .tests import Jolt_RollbackAndDeterminism as test_module

    class test_Jolt_SavedComponents(EditorSingleTest):
        from .tests import Jolt_SavedComponents as test_module

    class test_Jolt_SavedFeatureGallery(EditorSingleTest):
        from .tests import Jolt_SavedFeatureGallery as test_module

    class test_Jolt_ScenesAndAssets(EditorSingleTest):
        from .tests import Jolt_ScenesAndAssets as test_module

    class test_Jolt_ShapesAndCooking(EditorSingleTest):
        from .tests import Jolt_ShapesAndCooking as test_module

    class test_Jolt_SoftBodies(EditorSingleTest):
        from .tests import Jolt_SoftBodies as test_module

    class test_Jolt_StressAndSoak(EditorSingleTest):
        timeout = 3600
        from .tests import Jolt_StressAndSoak as test_module

    class test_Jolt_Vehicles(EditorSingleTest):
        from .tests import Jolt_Vehicles as test_module

    class test_Jolt_WorldQueriesAndSnapshots(EditorSingleTest):
        timeout = 600
        from .tests import Jolt_WorldQueriesAndSnapshots as test_module


def _validate_test_module_registration():
    test_directory = Path(__file__).parent / "tests"
    executable_modules = {
        path.stem
        for path in test_directory.glob("Jolt_*.py")
        if "Report.start_test(" in path.read_text(encoding="utf-8")
    }
    registered_modules = {
        test_class.test_module.__name__.rsplit(".", 1)[-1]
        for name, test_class in vars(TestAutomation).items()
        if name.startswith("test_")
    }
    modules_in_other_suites = {"Jolt_PerformanceCapture"}
    unregistered_modules = executable_modules - registered_modules - modules_in_other_suites
    if unregistered_modules:
        names = ", ".join(sorted(unregistered_modules))
        raise RuntimeError(f"Executable Jolt test modules are not registered: {names}")
    if registered_modules != set(SCENARIO_MINIMUM_CHECK_COUNTS):
        raise RuntimeError("Registered Jolt scenarios do not match the minimum-check policy")


def _validate_ragdoll_parts(label, parts, expected_constraint_id):
    if len(parts) != 2 or any(len(part.get("Shapes", [])) != 1 for part in parts):
        raise RuntimeError(f"{label} must contain two single-shape parts")
    if parts[0].get("HasParentConstraint") is not False:
        raise RuntimeError(f"{label} root part must not have a parent constraint")

    child_part = parts[1]
    parent_constraint = child_part.get("ParentConstraint", {})
    geometry = parent_constraint.get("Geometry", {})
    if child_part.get("HasParentConstraint") is not True:
        raise RuntimeError(f"{label} child part must enable its parent constraint")
    if not geometry.get("$type", "").endswith("ConstraintConfiguration"):
        raise RuntimeError(f"{label} child part must contain constraint geometry")
    if parent_constraint.get("Id") != expected_constraint_id:
        raise RuntimeError(f"{label} child part must use its stable constraint identity")
    if any("ToParent" in json.dumps(part) for part in parts):
        raise RuntimeError(f"{label} uses the obsolete ToParent constraint schema")


def _validate_feature_gallery_manifest():
    gallery_path = (
        Path(__file__).parents[4]
        / "Levels"
        / "Physics"
        / "Jolt"
        / "FeatureGallery"
        / "FeatureGallery.prefab"
    )
    gallery = json.loads(gallery_path.read_text(encoding="utf-8"))
    entities_by_name = {
        entity.get("Name", ""): entity
        for entity in gallery["Entities"].values()
    }
    gallery_entities = {
        name: entity
        for name, entity in entities_by_name.items()
        if name.startswith("Jolt Gallery ")
    }
    if len(gallery_entities) != 51:
        raise RuntimeError(
            f"Feature gallery must contain exactly 51 Jolt entities, found {len(gallery_entities)}"
        )

    for entity_name, entity in gallery_entities.items():
        for component in entity.get("Components", {}).values():
            if "ColliderComponent" not in component.get("$type", ""):
                continue

            shapes = component.get("Shapes", [])
            if len(shapes) != 1:
                raise RuntimeError(f"{entity_name} must contain exactly one collider shape")

    expected_components = (
        ("Dynamic Body", "RigidBodyComponent"),
        ("Static Body", "StaticRigidBodyComponent"),
        ("Character", "CharacterControllerComponent"),
        ("Virtual Character", "VirtualCharacterControllerComponent"),
        ("Wheeled Vehicle", "WheeledVehicleComponent"),
        ("Motorcycle", "MotorcycleComponent"),
        ("Tracked Vehicle", "TrackedVehicleComponent"),
        ("Soft Body", "SoftBodyComponent"),
        ("Ragdoll", "RagdollComponent"),
        ("Skeleton", "SkeletonComponent"),
        ("Path", "PathComponent"),
        ("Scene", "SceneComponent"),
        ("Hair", "HairComponent"),
    )
    for entity_suffix, component_name in expected_components:
        entity_name = f"Jolt Gallery {entity_suffix}"
        entity = gallery_entities.get(entity_name)
        if not entity:
            raise RuntimeError(f"Feature gallery is missing {entity_name}")

        component_types = [
            component.get("$type", "")
            for component in entity.get("Components", {}).values()
        ]
        if not any(component_name in component_type for component_type in component_types):
            raise RuntimeError(f"{entity_name} is missing {component_name}")

    expected_shapes = (
        ("Box", "BoxShapeConfiguration"),
        ("Capsule", "CapsuleShapeConfiguration"),
        ("Convex Hull", "ConvexHullShapeConfiguration"),
        ("Cylinder", "CylinderShapeConfiguration"),
        ("Empty", "EmptyShapeConfiguration"),
        ("Heightfield", "HeightfieldShapeConfiguration"),
        ("Mesh", "MeshShapeConfiguration"),
        ("Plane", "PlaneShapeConfiguration"),
        ("Sphere", "SphereShapeConfiguration"),
        ("Tapered Capsule", "TaperedCapsuleShapeConfiguration"),
        ("Tapered Cylinder", "TaperedCylinderShapeConfiguration"),
        ("Triangle", "TriangleShapeConfiguration"),
    )
    for shape_index, (shape_name, configuration_name) in enumerate(expected_shapes):
        entity_name = f"Jolt Gallery {shape_name}"
        entity = entities_by_name.get(entity_name)
        if not entity:
            raise RuntimeError(f"Feature gallery is missing {entity_name}")

        colliders = [
            component
            for component in entity.get("Components", {}).values()
            if "ColliderComponent" in component.get("$type", "")
        ]
        if len(colliders) != 1:
            raise RuntimeError(f"{entity_name} must contain exactly one collider")

        shapes = colliders[0].get("Shapes", [])
        if len(shapes) != 1:
            raise RuntimeError(f"{entity_name} must contain exactly one shape")

        shape = shapes[0]["Shape"]
        if configuration_name not in shape["Geometry"]["$type"]:
            raise RuntimeError(f"{entity_name} has the wrong shape configuration")
        if shape.get("Density") != 975.0 + shape_index:
            raise RuntimeError(f"{entity_name} has the wrong density sentinel")
        if shape.get("UserData") != 0x4A6F6C740000 + shape_index:
            raise RuntimeError(f"{entity_name} has the wrong user-data sentinel")

    expected_constraints = (
        ("Cone", "ConeConstraintConfiguration"),
        ("Custom", "CustomConstraintConfiguration"),
        ("Distance", "DistanceConstraintConfiguration"),
        ("Fixed", "FixedConstraintConfiguration"),
        ("Gear", "GearConstraintComponentConfiguration"),
        ("Hinge", "HingeConstraintConfiguration"),
        ("Path", "PathConstraintComponentConfiguration"),
        ("Point", "PointConstraintConfiguration"),
        ("Pulley", "PulleyConstraintConfiguration"),
        ("Rack And Pinion", "RackAndPinionConstraintComponentConfiguration"),
        ("Six Dof", "SixDofConstraintConfiguration"),
        ("Slider", "SliderConstraintConfiguration"),
        ("Swing Twist", "SwingTwistConstraintConfiguration"),
    )
    for constraint_index, (constraint_name, configuration_name) in enumerate(expected_constraints):
        entity_name = f"Jolt Gallery Constraint {constraint_name}"
        anchor_name = f"{entity_name} Anchor"
        entity = gallery_entities.get(entity_name)
        if not entity or anchor_name not in gallery_entities:
            raise RuntimeError(f"Feature gallery is missing the {constraint_name} constraint pair")

        constraints = [
            component
            for component in entity.get("Components", {}).values()
            if "ConstraintComponent" in component.get("$type", "")
        ]
        if len(constraints) != 1:
            raise RuntimeError(f"{entity_name} must contain exactly one constraint")

        configuration = constraints[0].get("Configuration", {})
        geometry = configuration.get("Geometry", {})
        if configuration_name not in geometry.get("$type", ""):
            raise RuntimeError(f"{entity_name} has the wrong constraint configuration")
        if configuration.get("UserData") != 0x4A6F6C740100 + constraint_index:
            raise RuntimeError(f"{entity_name} has the wrong user-data sentinel")
        if configuration.get("Priority") != constraint_index + 1:
            raise RuntimeError(f"{entity_name} has the wrong priority sentinel")

    vehicle_paths = (
        ("Jolt Gallery Wheeled Vehicle", "WheeledVehicleComponent", ("Vehicle",), 4),
        ("Jolt Gallery Motorcycle", "MotorcycleComponent", ("Motorcycle", "Wheeled"), 2),
        ("Jolt Gallery Tracked Vehicle", "TrackedVehicleComponent", ("Vehicle",), 6),
    )
    for entity_name, component_name, vehicle_path, wheel_count in vehicle_paths:
        entity = gallery_entities[entity_name]
        components = [
            component
            for component in entity.get("Components", {}).values()
            if component_name in component.get("$type", "")
        ]
        if len(components) != 1:
            raise RuntimeError(f"{entity_name} must contain exactly one {component_name}")

        vehicle = components[0]["Configuration"]
        for path_element in vehicle_path:
            vehicle = vehicle[path_element]
        if len(vehicle.get("Wheels", [])) != wheel_count:
            raise RuntimeError(f"{entity_name} has the wrong wheel count")
        transmission = vehicle.get("Transmission", {})
        if transmission.get("ForwardGearRatios") != [2.66, 1.78, 1.3, 1.0, 0.74]:
            raise RuntimeError(f"{entity_name} has a contaminated forward-gear list")
        if transmission.get("ReverseGearRatios") != [-2.9]:
            raise RuntimeError(f"{entity_name} has a contaminated reverse-gear list")

    ragdoll = gallery_entities["Jolt Gallery Ragdoll"]
    ragdoll_components = [
        component
        for component in ragdoll.get("Components", {}).values()
        if "RagdollComponent" in component.get("$type", "")
    ]
    if len(ragdoll_components) != 1:
        raise RuntimeError("Feature gallery must contain exactly one configured ragdoll")
    ragdoll_configuration = ragdoll_components[0].get("Configuration", {})
    ragdoll_parts = ragdoll_configuration.get("Parts", [])
    _validate_ragdoll_parts(
        "Feature-gallery ragdoll",
        ragdoll_parts,
        "{03000000-0000-4000-8000-000000000101}",
    )
    if ragdoll_configuration.get("BaseConstraintPriority") != 3:
        raise RuntimeError("Feature-gallery ragdoll has the wrong priority sentinel")


def _validate_stress_manifest():
    project_root = Path(__file__).parents[4]
    stress_path = project_root / "Levels" / "Physics" / "Jolt" / "Stress" / "Stress.prefab"
    stress_body_path = project_root / "Prefabs" / "Physics" / "Jolt" / "StressBody.prefab"
    stress = json.loads(stress_path.read_text(encoding="utf-8"))
    stress_body = json.loads(stress_body_path.read_text(encoding="utf-8"))

    if len(stress.get("Instances", {})) != 64:
        raise RuntimeError("Jolt stress level must contain exactly 64 body-prefab instances")

    entities_by_name = {
        entity.get("Name", ""): entity
        for entity in stress["Entities"].values()
    }
    expected_components = (
        ("Jolt Stress Driver", "RigidBodyComponent"),
        ("Jolt Stress Character", "CharacterControllerComponent"),
        ("Jolt Stress Virtual Character", "VirtualCharacterControllerComponent"),
        ("Jolt Stress Vehicle", "WheeledVehicleComponent"),
        ("Jolt Stress Soft Body", "SoftBodyComponent"),
        ("Jolt Stress Ragdoll", "RagdollComponent"),
        ("Jolt Stress Hair", "HairComponent"),
    )
    for entity_name, component_name in expected_components:
        entity = entities_by_name.get(entity_name)
        if not entity:
            raise RuntimeError(f"Jolt stress level is missing {entity_name}")

        component_types = [
            component.get("$type", "")
            for component in entity.get("Components", {}).values()
        ]
        if not any(component_name in component_type for component_type in component_types):
            raise RuntimeError(f"{entity_name} is missing {component_name}")

    for entity_name, entity in entities_by_name.items():
        if not entity_name.startswith("Jolt Stress "):
            continue
        for component in entity.get("Components", {}).values():
            if "ColliderComponent" not in component.get("$type", ""):
                continue
            if len(component.get("Shapes", [])) != 1:
                raise RuntimeError(f"{entity_name} must contain exactly one collider shape")

    stress_body_entities = list(stress_body.get("Entities", {}).values())
    if len(stress_body_entities) != 1:
        raise RuntimeError("Jolt stress body prefab must contain exactly one entity")
    stress_body_colliders = [
        component
        for component in stress_body_entities[0].get("Components", {}).values()
        if "ColliderComponent" in component.get("$type", "")
    ]
    if len(stress_body_colliders) != 1 or len(stress_body_colliders[0].get("Shapes", [])) != 1:
        raise RuntimeError("Jolt stress body prefab must contain exactly one collider shape")

    stress_vehicle = entities_by_name["Jolt Stress Vehicle"]
    vehicle_components = [
        component
        for component in stress_vehicle.get("Components", {}).values()
        if "WheeledVehicleComponent" in component.get("$type", "")
    ]
    if len(vehicle_components) != 1:
        raise RuntimeError("Jolt stress level must contain exactly one configured vehicle")
    transmission = vehicle_components[0]["Configuration"]["Vehicle"].get("Transmission", {})
    if transmission.get("ForwardGearRatios") != [2.66, 1.78, 1.3, 1.0, 0.74]:
        raise RuntimeError("Jolt stress vehicle has a contaminated forward-gear list")
    if transmission.get("ReverseGearRatios") != [-2.9]:
        raise RuntimeError("Jolt stress vehicle has a contaminated reverse-gear list")

    stress_ragdoll = entities_by_name["Jolt Stress Ragdoll"]
    ragdoll_components = [
        component
        for component in stress_ragdoll.get("Components", {}).values()
        if "RagdollComponent" in component.get("$type", "")
    ]
    if len(ragdoll_components) != 1:
        raise RuntimeError("Jolt stress level must contain exactly one configured ragdoll")
    ragdoll_parts = ragdoll_components[0].get("Configuration", {}).get("Parts", [])
    _validate_ragdoll_parts(
        "Jolt stress ragdoll",
        ragdoll_parts,
        "{03000000-0000-4000-8000-000000000102}",
    )


def _validate_public_header_tests():
    engine_root = Path(__file__).parents[5]
    include_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "Include" / "Jolt"
    probe_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "Tests" / "Headers"
    public_headers = {
        path.relative_to(include_root).with_suffix("").as_posix()
        for path in include_root.rglob("*.h")
    }
    header_probes = {
        path.relative_to(probe_root).with_suffix("").as_posix()
        for path in probe_root.rglob("*.cpp")
    }
    if public_headers != header_probes:
        missing = ", ".join(sorted(public_headers - header_probes))
        extra = ", ".join(sorted(header_probes - public_headers))
        raise RuntimeError(f"Jolt public-header probes differ: missing=[{missing}], extra=[{extra}]")

    manifest_path = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "jolt_tests_files.cmake"
    manifest = manifest_path.read_text(encoding="utf-8")
    manifest_probes = {
        Path(match).relative_to("Tests/Headers").with_suffix("").as_posix()
        for match in re.findall(r"Tests/Headers/[A-Za-z0-9_/]+\.cpp", manifest)
    }
    if manifest_probes != header_probes:
        missing = ", ".join(sorted(header_probes - manifest_probes))
        extra = ", ".join(sorted(manifest_probes - header_probes))
        raise RuntimeError(f"Jolt header-test manifest differs: missing=[{missing}], extra=[{extra}]")


_validate_test_module_registration()
_validate_feature_gallery_manifest()
_validate_stress_manifest()
_validate_public_header_tests()
