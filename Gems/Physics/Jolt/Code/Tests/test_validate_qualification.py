"""Tests for the Jolt qualification entry point."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
import xml.etree.ElementTree as xml_tree
from pathlib import Path
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[2] / "validate.py"
SPECIFICATION = importlib.util.spec_from_file_location("jolt_qualification", MODULE_PATH)
assert SPECIFICATION and SPECIFICATION.loader
jolt_qualification = importlib.util.module_from_spec(SPECIFICATION)
sys.modules[SPECIFICATION.name] = jolt_qualification
SPECIFICATION.loader.exec_module(jolt_qualification)


def write_file(root: Path, relative_path: str, contents: str) -> Path:
    path = root / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")
    return path


class QualificationValidationTests(unittest.TestCase):
    def test_clang_address_sanitizer_environment_disables_only_odr_detection(self) -> None:
        base_environment = {"EXAMPLE": "preserved"}

        environment = jolt_qualification.get_clang_address_sanitizer_environment(base_environment)

        self.assertEqual(environment["ASAN_OPTIONS"], "detect_odr_violation=0")
        self.assertEqual(environment["EXAMPLE"], "preserved")
        self.assertNotIn("ASAN_OPTIONS", base_environment)

    def test_automated_testing_uses_the_requested_stress_duration(self) -> None:
        base_environment = {"EXAMPLE": "preserved"}

        review_environment = jolt_qualification.get_automated_testing_environment(
            base_environment,
            "review",
        )
        full_environment = jolt_qualification.get_automated_testing_environment(
            base_environment,
            "full",
        )

        self.assertEqual(review_environment["JOLT_STRESS_MODE"], "review")
        self.assertEqual(full_environment["JOLT_STRESS_MODE"], "full")
        self.assertEqual(review_environment["EXAMPLE"], "preserved")
        self.assertNotIn("JOLT_STRESS_MODE", base_environment)

        with self.assertRaisesRegex(ValueError, "not part of 'quick'"):
            jolt_qualification.get_automated_testing_environment(base_environment, "quick")

    def test_msvc_environment_preserves_an_existing_developer_shell(self) -> None:
        environment = {
            "PATH": "C:/VisualStudio/bin",
            "VSCMD_VER": "18.9.0",
        }
        with mock.patch.object(jolt_qualification.shutil, "which", return_value="cl.exe"):
            result = jolt_qualification.load_msvc_environment(environment)

        self.assertEqual(result, environment)
        self.assertIsNot(result, environment)

    def test_build_environment_loads_msvc_only_for_cl(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = Path(temporary_directory)
            cache_path = build_directory / "CMakeCache.txt"
            base_environment = {"PATH": "base"}
            developer_environment = {"PATH": "msvc"}

            cache_path.write_text(
                "CMAKE_CXX_COMPILER:FILEPATH=C:/VisualStudio/cl.exe\n",
                encoding="utf-8",
            )
            with mock.patch.object(
                jolt_qualification,
                "load_msvc_environment",
                return_value=developer_environment,
            ) as load_environment:
                result = jolt_qualification.load_build_environment(
                    base_environment,
                    build_directory,
                )

            self.assertEqual(result, developer_environment)
            load_environment.assert_called_once_with(base_environment)

            cache_path.write_text(
                "CMAKE_CXX_COMPILER:FILEPATH=C:/LLVM/clang-cl.exe\n",
                encoding="utf-8",
            )
            with mock.patch.object(jolt_qualification, "load_msvc_environment") as load_environment:
                result = jolt_qualification.load_build_environment(
                    base_environment,
                    build_directory,
                )

            self.assertIsNone(result)
            load_environment.assert_not_called()

    def test_runner_redirects_python_bytecode_beneath_its_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            output_directory = root / "build" / "qualification"
            output_directory.mkdir(parents=True)
            runner = jolt_qualification.ValidationRunner(root, output_directory, dry_run=True)

            self.assertEqual(
                runner.environment["PYTHONPYCACHEPREFIX"],
                str(output_directory / "python-cache"),
            )

    def test_output_directory_must_remain_under_build(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            accepted = jolt_qualification.ensure_output_is_under_build(
                engine_root,
                engine_root / "build" / "qualification",
            )
            self.assertEqual(accepted, (engine_root / "build" / "qualification").resolve())

            with self.assertRaisesRegex(ValueError, "beneath"):
                jolt_qualification.ensure_output_is_under_build(
                    engine_root,
                    engine_root / "qualification",
                )

    def test_source_consumer_cache_is_isolated_by_primary_build(self) -> None:
        matrix_root = Path("build/matrix")

        self.assertEqual(
            jolt_qualification.get_source_consumer_build_directory(
                matrix_root,
                Path("build/clang-unity-modular"),
            ),
            matrix_root / "consumer-source-clang-unity-modular",
        )
        self.assertEqual(
            jolt_qualification.get_source_consumer_build_directory(
                matrix_root,
                Path("build/msvc-unity-modular"),
            ),
            matrix_root / "consumer-source-msvc-unity-modular",
        )

    def test_feature_ledger_rejects_missing_and_empty_exposed_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            ledger_path = write_file(
                engine_root,
                "Gems/Physics/Jolt/FEATURE_COVERAGE.md",
                """
| Jolt 5.6 subsystem | Classification | AZ-facing boundary or rationale | C++ and authoring proof | Scenario and performance proof |
|---|---|---|---|---|
| Bodies | Publicly exposed | Bodies.h | BodyTests | Jolt_Bodies |
| Missing feature | Missing | Required | Required | Required |
| Empty proof | Publicly exposed | Bodies.h | | Jolt_Bodies |
""",
            )

            with self.assertRaisesRegex(ValueError, "classified as Missing"):
                jolt_qualification.validate_feature_ledger(engine_root)

            ledger_path.write_text(
                """
| Jolt 5.6 subsystem | Classification | AZ-facing boundary or rationale | C++ and authoring proof | Scenario and performance proof |
|---|---|---|---|---|
| Bodies | Publicly exposed | Bodies.h | BodyTests | Jolt_Bodies |
""",
                encoding="utf-8",
            )
            result = jolt_qualification.validate_feature_ledger(engine_root)
            self.assertIn("1 feature rows", result)

    def test_public_header_check_rejects_native_tokens_and_missing_probes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            header = write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/Include/Jolt/Example.h",
                "#pragma once\n",
            )
            write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/Tests/Headers/Example.cpp",
                "#include <Jolt/Example.h>\n",
            )
            self.assertIn("1 public headers", jolt_qualification.validate_public_headers(engine_root))

            header.write_text("namespace JPH {}\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "native JPH token"):
                jolt_qualification.validate_public_headers(engine_root)

    def test_source_manifest_check_rejects_missing_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            manifest = write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/jolt_test_files.cmake",
                "set(FILES\n    Tests/Example.cpp\n)\n",
            )
            write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/Tests/Example.cpp",
                "int example;\n",
            )
            self.assertIn("1 entries", jolt_qualification.validate_source_manifests(engine_root))

            manifest.write_text("set(FILES\n    Tests/Missing.cpp\n)\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "references missing"):
                jolt_qualification.validate_source_manifests(engine_root)

    def test_scenario_check_requires_registration_and_checked_in_manifests(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            suite_lines = []
            for scenario in sorted(jolt_qualification.REQUIRED_SCENARIOS):
                suite_lines.append(f"from .tests import {scenario}")
                write_file(
                    engine_root,
                    f"AutomatedTesting/Gem/PythonTests/Physics/Jolt/tests/{scenario}.py",
                    "Report.start_test(example)\n",
                )
            write_file(
                engine_root,
                "AutomatedTesting/Gem/PythonTests/Physics/Jolt/TestSuite_Main.py",
                "\n".join(suite_lines),
            )
            write_file(
                engine_root,
                "AutomatedTesting/Gem/PythonTests/Physics/Jolt/TestSuite_Benchmark.py",
                "",
            )

            gallery_entities = {
                str(index): {"Name": f"Jolt Gallery {index}"}
                for index in range(51)
            }
            write_file(
                engine_root,
                "AutomatedTesting/Levels/Physics/Jolt/FeatureGallery/FeatureGallery.prefab",
                json.dumps({"Entities": gallery_entities}),
            )
            stress_instances = {str(index): {} for index in range(64)}
            write_file(
                engine_root,
                "AutomatedTesting/Levels/Physics/Jolt/Stress/Stress.prefab",
                json.dumps({"Instances": stress_instances}),
            )

            result = jolt_qualification.validate_scenario_registration(engine_root)
            self.assertIn(f"{len(jolt_qualification.REQUIRED_SCENARIOS)} registered", result)

            missing_scenario = sorted(jolt_qualification.REQUIRED_SCENARIOS)[0]
            suite_path = (
                engine_root
                / "AutomatedTesting"
                / "Gem"
                / "PythonTests"
                / "Physics"
                / "Jolt"
                / "TestSuite_Main.py"
            )
            suite_path.write_text(
                "\n".join(line for line in suite_lines if missing_scenario not in line),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "unregistered executable scenarios"):
                jolt_qualification.validate_scenario_registration(engine_root)

    def test_private_boundary_ignores_documentation_but_rejects_published_target(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            native_path = write_file(
                engine_root,
                "Gems/Physics/Jolt/3rdParty/JoltNative.cmake",
                """
# Do not publish 3rdParty::Jolt or Atom_RHI.
set(jolt_native_patch_file jolt.patch)
file(SHA256 ${jolt_native_patch_file} jolt_native_patch_hash)
string(SUBSTRING ${jolt_native_patch_hash} 0 16 jolt_native_patch_identity)
set(jolt_native_content_base_name JoltProviderNative_5_6_0)
set(jolt_native_content_name "${jolt_native_content_base_name}_${jolt_native_patch_identity}")
set(jolt_native_target_architectures ${CMAKE_OSX_ARCHITECTURES})
set(jolt_native_target_architectures ${CMAKE_VS_PLATFORM_NAME})
set(jolt_native_target_architectures ${CMAKE_CXX_COMPILER_TARGET})
set(jolt_native_target_architectures ${CMAKE_SYSTEM_PROCESSOR})
message(FATAL_ERROR "The Jolt Gem found a foreign target named Jolt")
message(FATAL_ERROR "The Jolt Gem found a foreign FetchContent declaration")
message(FATAL_ERROR "A source override cannot override the private native source")
check_ipo_supported(RESULT ipo_supported)
if(LY_MONOLITHIC_GAME)
endif()
if(WIN32 AND JOLT_NATIVE_TARGET_HAS_ARM)
endif()
target_sources(Jolt PRIVATE JoltNativeBuildIdentity.cpp)
target_compile_definitions(Jolt PRIVATE JOLT_NATIVE_BUILD_FINGERPRINT=1)
ly_install(FILES
    ${jolt_source_dir}/LICENSE
    DESTINATION include/Jolt
    COMPONENT CORE
)
""",
            )
            write_file(
                engine_root,
                "Gems/Physics/Jolt/3rdParty/JoltNativeBuildIdentity.cpp",
                """
#error "The private Jolt x86 objects require SSE4.1"
#error "The private Jolt ARM objects cannot use x86 features"
extern "C" unsigned long long JoltNativeBuildFingerprint();
""",
            )
            write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/CMakeLists.txt",
                "BUILD_DEPENDENCIES\n    PRIVATE\n        Jolt\n",
            )
            result = jolt_qualification.validate_private_native_boundary(engine_root)
            self.assertIn("private native identity", result)

            native_path.write_text(
                native_path.read_text(encoding="utf-8") + "add_library(3rdParty::Jolt ALIAS Jolt)\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "published as 3rdParty::Jolt"):
                jolt_qualification.validate_private_native_boundary(engine_root)

    def test_clang_address_sanitizer_configuration_requires_profile_instrumentation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            clang_configuration = write_file(
                engine_root,
                "cmake/Platform/Common/MSVC/Configurations_clang.cmake",
                """
if(LY_BUILD_WITH_ADDRESS_SANITIZER)
    get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(CMAKE_CONFIGURATION_TYPES profile CACHE STRING "Supported build configurations" FORCE)
    if(NOT O3DE_CLANG_ASAN_BUILD_TYPE STREQUAL "profile")
    endif()
    set(target ${CMAKE_CXX_COMPILER_TARGET})
    set(target ${CMAKE_VS_PLATFORM_NAME})
    set(target ${CMAKE_SYSTEM_PROCESSOR})
    file(COPY runtime DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_PROFILE})
    ly_append_configurations_options(
        COMPILATION_PROFILE
            -fsanitize=address
            /Oy-
        LINK_NON_STATIC_PROFILE
            "${O3DE_CLANG_ASAN_DYNAMIC_LIBRARY}"
            /include:__asan_seh_interceptor
            "/wholearchive:${O3DE_CLANG_ASAN_THUNK_LIBRARY}"
    )
endif()
""",
            )
            write_file(
                engine_root,
                "Code/Framework/AzCore/CMakeLists.txt",
                "AZCORE_USE_MALLOC_SYSTEM_ALLOCATOR\n"
                "$<$<CONFIG:Profile>:AZCORE_ADDRESS_SANITIZER_ENABLED>\n",
            )
            write_file(
                engine_root,
                "Code/Framework/AzCore/AzCore/Memory/SystemAllocator.cpp",
                """
address.m_size = SystemAllocatorPrivate::GetAllocatedSize(address.m_value, alignment);
newAddress.m_size = SystemAllocatorPrivate::GetAllocatedSize(newAddress.m_value, newAlignment);
""",
            )

            self.assertIn(
                "Profile-only clang-cl ASan deployment",
                jolt_qualification.validate_clang_address_sanitizer_configuration(engine_root),
            )

            clang_configuration.write_text(
                clang_configuration.read_text(encoding="utf-8").replace(
                    "COMPILATION_PROFILE\n            -fsanitize=address\n            /Oy-\n",
                    "",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "Profile compilation is not instrumented"):
                jolt_qualification.validate_clang_address_sanitizer_configuration(engine_root)

    def test_public_consumer_rejects_native_dependencies(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            consumer_root = (
                engine_root
                / "Gems"
                / "Physics"
                / "Jolt"
                / "Code"
                / "Tests"
                / "InstalledConsumer"
            )
            write_file(
                consumer_root,
                "CMakeLists.txt",
                "set(O3DE_ENGINE_ROOT example)\ntarget_link_libraries(example Gem::Jolt.API)\n",
            )
            write_file(consumer_root, "jolt_installed_consumer_files.cmake", "set(FILES main.cpp)\n")
            source_path = write_file(
                consumer_root,
                "main.cpp",
                """#include <Jolt/System.h>
Jolt::ReflectDebugDraw(nullptr);
Jolt::ReflectDiagnostics(nullptr);
Jolt::ReflectEvents(nullptr);
Jolt::ReflectQueries(nullptr);
Jolt::ReflectWorldQueries(nullptr);
emptyEventBatch.GetId();
faceBuffers.GetQueryFace(0);
faceBuffers.GetTargetFace(0);
""",
            )
            write_file(
                consumer_root,
                "project.json",
                json.dumps({"gem_names": ["Jolt"]}),
            )

            self.assertIn("public-only", jolt_qualification.validate_public_consumer(engine_root))

            source_path.write_text("JPH::PhysicsSystem* system;\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "private native term"):
                jolt_qualification.validate_public_consumer(engine_root)

    def test_public_consumer_requires_all_exported_callables(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            consumer_root = (
                engine_root
                / "Gems"
                / "Physics"
                / "Jolt"
                / "Code"
                / "Tests"
                / "InstalledConsumer"
            )
            write_file(
                consumer_root,
                "CMakeLists.txt",
                "set(O3DE_ENGINE_ROOT example)\ntarget_link_libraries(example Gem::Jolt.API)\n",
            )
            write_file(consumer_root, "jolt_installed_consumer_files.cmake", "set(FILES main.cpp)\n")
            write_file(consumer_root, "main.cpp", "#include <Jolt/System.h>\n")
            write_file(consumer_root, "project.json", json.dumps({"gem_names": ["Jolt"]}))

            with self.assertRaisesRegex(ValueError, "does not call exported API"):
                jolt_qualification.validate_public_consumer(engine_root)

    def test_installed_boundary_requires_public_headers_and_hides_native_jolt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            install_root = engine_root / "build" / "installed"
            write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/Include/Jolt/Example.h",
                "#pragma once\n",
            )
            write_file(
                install_root,
                "Gems/Physics/Jolt/Code/Include/Jolt/Example.h",
                "#pragma once\n",
            )
            write_file(install_root, "include/Jolt/LICENSE", "license\n")
            permutation_path = write_file(
                install_root,
                "Gems/Physics/Jolt/Code/Platform/Windows/Default/permutation.cmake",
                """ly_add_target(
    NAME Jolt.API IMPORTED SHARED
    RUNTIME_DEPENDENCIES
    TARGET_PROPERTIES
)
ly_create_alias(
    NAME Jolt
    NAMESPACE Gem
    INTERNAL_NAME Jolt.GemAlias
    TARGETS Gem::Jolt.Module
)
""",
            )
            write_file(
                install_root,
                "Gems/Physics/Jolt/Code/Platform/Windows/Default/Jolt.API_release.cmake",
                "set_target_properties(Gem::Jolt.API PROPERTIES IMPORTED_LOCATION example)\n",
            )
            write_file(
                install_root,
                "bin/Windows/release/Default/Jolt.API.dll",
                "binary",
            )

            result = jolt_qualification.validate_installed_boundary(
                engine_root,
                install_root,
                "Release",
                False,
            )
            self.assertIn("1 public headers", result)

            permutation_path.write_text(
                permutation_path.read_text(encoding="utf-8").replace(
                    "RUNTIME_DEPENDENCIES\n",
                    "RUNTIME_DEPENDENCIES\n        Jolt\n",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "private native runtime dependency"):
                jolt_qualification.validate_installed_boundary(
                    engine_root,
                    install_root,
                    "Release",
                    False,
                )

            write_file(install_root, "include/Jolt/Jolt.h", "namespace JPH {}\n")
            with self.assertRaisesRegex(ValueError, "only the Jolt license"):
                jolt_qualification.validate_installed_boundary(
                    engine_root,
                    install_root,
                    "Release",
                    False,
                )

    def test_installed_consumer_uses_cmake_install_components(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            output_directory = engine_root / "build" / "qualification"
            output_directory.mkdir(parents=True)
            primary_build_directory = engine_root / "build" / "engine"
            core_build_directory = engine_root / "build" / "core"
            write_file(
                primary_build_directory,
                "CMakeCache.txt",
                "\n".join(
                    (
                        "CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config",
                        "CMAKE_C_COMPILER:FILEPATH=C:/LLVM/clang-cl.exe",
                        "CMAKE_CXX_COMPILER:FILEPATH=C:/LLVM/clang-cl.exe",
                    )
                ),
            )

            consumer_root = (
                engine_root
                / "Gems"
                / "Physics"
                / "Jolt"
                / "Code"
                / "Tests"
                / "InstalledConsumer"
            )
            for relative_path in (
                "CMakeLists.txt",
                "jolt_installed_consumer_files.cmake",
                "main.cpp",
                "project.json",
            ):
                write_file(consumer_root, relative_path, relative_path)

            write_file(
                engine_root,
                "Gems/Physics/Jolt/Code/Include/Jolt/Example.h",
                "#pragma once\n",
            )
            install_root = output_directory / "installed-engine"
            write_file(
                install_root,
                "Gems/Physics/Jolt/Code/Include/Jolt/Example.h",
                "#pragma once\n",
            )
            write_file(install_root, "include/Jolt/LICENSE", "license\n")
            write_file(
                install_root,
                "Gems/Physics/Jolt/Code/Platform/Windows/Default/permutation.cmake",
                """ly_add_target(
    NAME Jolt.API IMPORTED SHARED
    RUNTIME_DEPENDENCIES
    TARGET_PROPERTIES
)
ly_create_alias(
    NAME Jolt
    NAMESPACE Gem
    INTERNAL_NAME Jolt.GemAlias
    TARGETS Gem::Jolt.Module
)
""",
            )
            write_file(
                install_root,
                "Gems/Physics/Jolt/Code/Platform/Windows/Default/Jolt.API_release.cmake",
                "set_target_properties(Gem::Jolt.API PROPERTIES IMPORTED_LOCATION example)\n",
            )
            write_file(
                install_root,
                "bin/Windows/release/Default/Jolt.API.dll",
                "binary",
            )

            runner = jolt_qualification.ValidationRunner(
                engine_root,
                output_directory,
                dry_run=True,
            )
            jolt_qualification.add_installed_consumer(
                runner,
                primary_build_directory,
                core_build_directory,
                install_root,
                output_directory / "consumer-build",
                "Release",
                8,
                False,
            )

            commands = {result.name: result.command for result in runner.results if result.command}
            self.assertIn(
                "-DLY_PROJECTS:STRING=",
                commands["configure-installed-modular-engine"],
            )
            self.assertNotIn("--target", commands["build-installed-modular-engine"])
            self.assertIn("CORE", commands["install-modular-core"])
            self.assertIn(str(core_build_directory), commands["install-modular-core"])
            self.assertIn("DEFAULT", commands["install-modular-configuration"])
            self.assertIn("DEFAULT_RELEASE", commands["install-modular-binaries"])
            self.assertIn(
                f"-DO3DE_ENGINE_ROOT={install_root.resolve().as_posix()}",
                commands["configure-installed-modular-consumer"],
            )

    def test_cmake_cache_reader_requires_the_requested_entry(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = Path(temporary_directory)
            write_file(
                build_directory,
                "CMakeCache.txt",
                "CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\n",
            )

            self.assertEqual(
                jolt_qualification.read_cmake_cache_value(build_directory, "CMAKE_GENERATOR"),
                "Ninja Multi-Config",
            )
            with self.assertRaisesRegex(ValueError, "does not define"):
                jolt_qualification.read_cmake_cache_value(build_directory, "CMAKE_CXX_COMPILER")

    def test_consumer_source_is_copied_to_generated_storage(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            source = (
                engine_root
                / "Gems"
                / "Physics"
                / "Jolt"
                / "Code"
                / "Tests"
                / "InstalledConsumer"
            )
            expected_files = (
                "CMakeLists.txt",
                "jolt_installed_consumer_files.cmake",
                "main.cpp",
                "project.json",
            )
            for relative_path in expected_files:
                write_file(source, relative_path, relative_path)
            write_file(source, "user/generated.setreg", "generated")

            destination = engine_root / "build" / "consumer-project"
            result = jolt_qualification.prepare_consumer_source(engine_root, destination)

            self.assertEqual(result, destination)
            self.assertEqual(
                {path.name for path in destination.iterdir()},
                set(expected_files),
            )

    def test_reports_include_json_junit_and_git_epochs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            git_state = {"head": "abc", "combined_sha256": "def"}
            results = [
                jolt_qualification.ValidationResult(
                    name="pass",
                    status="passed",
                    duration_seconds=1.25,
                    log_path="pass.log",
                    message="ok",
                ),
                jolt_qualification.ValidationResult(
                    name="fail",
                    status="failed",
                    duration_seconds=0.5,
                    log_path="fail.log",
                    message="bad",
                ),
            ]
            jolt_qualification.write_reports(
                output_directory,
                "quick",
                git_state,
                git_state,
                results,
            )

            summary = json.loads((output_directory / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["initial_git_state"]["head"], "abc")
            self.assertEqual(len(summary["results"]), 2)
            test_suite = xml_tree.parse(output_directory / "summary.junit.xml").getroot()
            self.assertEqual(test_suite.attrib["tests"], "2")
            self.assertEqual(test_suite.attrib["failures"], "1")

    def test_windows_matrix_covers_required_build_axes(self) -> None:
        clang_root = Path("D:/LLVM/22.1.8")
        variants = {
            variant.name: variant
            for variant in jolt_qualification.windows_full_matrix(clang_root)
        }
        self.assertIn("clang-no-unity", variants)
        self.assertIn("clang-double", variants)
        self.assertIn("clang-diagnostics", variants)
        self.assertIn("clang-asan", variants)
        self.assertIn("clang-malloc-accounting", variants)
        self.assertIn("clang-monolithic", variants)
        self.assertIn("msvc-unity-modular", variants)
        self.assertIn("msvc-no-unity", variants)
        self.assertIn("msvc-monolithic", variants)
        self.assertEqual(variants["clang-unity-modular"].preset, "windows-ninja")
        self.assertIn(
            "CMAKE_CXX_COMPILER=D:/LLVM/22.1.8/bin/clang-cl.exe",
            variants["clang-unity-modular"].definitions,
        )
        self.assertEqual(
            variants["clang-unity-modular"].configurations,
            ("Debug", "Profile", "Release"),
        )
        self.assertIn(
            "CMAKE_TRY_COMPILE_CONFIGURATION=Release",
            variants["clang-asan"].definitions,
        )
        self.assertEqual(variants["clang-asan"].configurations, ("Profile",))
        self.assertIn("AzCore.Tests", variants["clang-asan"].targets)
        self.assertEqual(variants["clang-malloc-accounting"].configurations, ("Release",))
        self.assertIn(
            "AZCORE_USE_MALLOC_SYSTEM_ALLOCATOR=ON",
            variants["clang-malloc-accounting"].definitions,
        )


if __name__ == "__main__":
    unittest.main()
