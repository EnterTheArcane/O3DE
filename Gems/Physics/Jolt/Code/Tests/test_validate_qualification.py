"""Tests for the Jolt qualification entry point."""

from __future__ import annotations

import hashlib
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
            Path("scenario-results"),
            Path("benchmark-results"),
        )

        self.assertEqual(review_environment["JOLT_STRESS_MODE"], "review")
        self.assertEqual(full_environment["JOLT_STRESS_MODE"], "full")
        self.assertEqual(full_environment["JOLT_SCENARIO_RESULT_DIRECTORY"], "scenario-results")
        self.assertEqual(full_environment["JOLT_BENCHMARK_RESULT_DIRECTORY"], "benchmark-results")
        self.assertEqual(review_environment["EXAMPLE"], "preserved")
        self.assertNotIn("JOLT_STRESS_MODE", base_environment)

        with self.assertRaisesRegex(ValueError, "not part of 'quick'"):
            jolt_qualification.get_automated_testing_environment(base_environment, "quick")

    def test_msvc_environment_preserves_an_existing_developer_shell(self) -> None:
        environment = {
            "PATH": "C:/VisualStudio/bin",
            "VSCMD_VER": "18.9.0",
        }
        tool_paths = {
            "cl.exe": "C:/VisualStudio/bin/cl.exe",
            "rc.exe": "C:/WindowsSDK/bin/rc.exe",
        }
        with mock.patch.object(
            jolt_qualification.shutil,
            "which",
            side_effect=lambda executable, **_: tool_paths.get(executable),
        ):
            result = jolt_qualification.load_msvc_environment(environment)

        self.assertEqual(result["PATH"], environment["PATH"])
        self.assertEqual(result["VSCMD_VER"], environment["VSCMD_VER"])
        self.assertEqual(result["RC"], "C:/WindowsSDK/bin/rc.exe")
        self.assertIsNot(result, environment)

    def test_build_environment_loads_windows_sdk_for_msvc_abi_compilers(self) -> None:
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

            for compiler in ("C:/LLVM/clang-cl.exe", "C:/LLVM/clang++.exe"):
                cache_path.write_text(
                    f"CMAKE_CXX_COMPILER:FILEPATH={compiler}\n",
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

                if compiler.endswith("clang-cl.exe"):
                    self.assertEqual(result, developer_environment)
                    load_environment.assert_called_once_with(base_environment)
                else:
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

    def test_full_qualification_schedules_native_and_application_benchmarks(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            output_directory = engine_root / "build" / "qualification"
            output_directory.mkdir(parents=True)
            build_directory = engine_root / "build" / "primary"
            runner = jolt_qualification.ValidationRunner(engine_root, output_directory, dry_run=True)

            jolt_qualification.add_performance_qualification(
                runner,
                build_directory,
                16,
                "full",
            )

            commands = {result.name: result.command for result in runner.results if result.command}
            build_command = commands["build-release-performance-targets"]
            self.assertIn("Jolt.Tests", build_command)
            self.assertIn("Box3D.Tests", build_command)
            self.assertIn("PhysX5.Tests", build_command)
            self.assertIn("Release", build_command)
            benchmark_command = commands["native-benchmark-qualification"]
            self.assertIn("run_benchmark_qualification.py", " ".join(benchmark_command))
            self.assertIn("full", benchmark_command)
            if jolt_qualification.platform.system() == "Windows":
                self.assertIn("application-benchmark", commands)
                self.assertIn("Release", commands["application-benchmark"])

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

    def test_reports_use_a_python_310_compatible_utc_timestamp(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)

            jolt_qualification.write_reports(output_directory, "quick", {}, {}, [])

            summary = json.loads((output_directory / "summary.json").read_text(encoding="utf-8"))
            self.assertTrue(summary["generated_at_utc"].endswith("+00:00"))

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

    def test_installed_consumer_uses_a_same_drive_python_environment(self) -> None:
        output_directory = Path("D:/qualification-results")
        source_profile = Path("C:/Users/Example")
        with (
            mock.patch.object(jolt_qualification.platform, "system", return_value="Windows"),
            mock.patch.object(Path, "is_dir", return_value=True),
        ):
            environment, message = jolt_qualification.prepare_installed_consumer_environment(
                output_directory,
                Path("D:/installed-engine"),
                {"USERPROFILE": str(source_profile)},
                True,
            )

        self.assertEqual(environment["USERPROFILE"], str((output_directory / "installed-user").resolve()))
        self.assertEqual(
            environment["LY_3RDPARTY_PATH"],
            str(source_profile / ".o3de" / "3rdParty"),
        )
        self.assertIn("same-drive Python environment", message)

    def test_installed_consumer_reuses_a_same_drive_profile(self) -> None:
        with mock.patch.object(jolt_qualification.platform, "system", return_value="Windows"):
            environment, message = jolt_qualification.prepare_installed_consumer_environment(
                Path("D:/results"),
                Path("D:/installed-engine"),
                {"USERPROFILE": "D:/Users/Example"},
                False,
            )

        self.assertEqual(environment["USERPROFILE"], "D:/Users/Example")
        self.assertNotIn("LY_3RDPARTY_PATH", environment)
        self.assertIn("already share a drive", message)

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
                "from .tests import Jolt_PerformanceCapture",
            )
            write_file(
                engine_root,
                "AutomatedTesting/Gem/PythonTests/Physics/Jolt/tests/Jolt_PerformanceCapture.py",
                "Report.start_test(example)\n",
            )
            write_file(
                engine_root,
                "AutomatedTesting/Gem/PythonTests/Physics/Jolt/tests/Jolt_ScenarioRecorder.py",
                (
                    f"SCENARIO_MINIMUM_CHECK_COUNTS = "
                    f"{dict.fromkeys(sorted(jolt_qualification.REQUIRED_SCENARIOS), 1)!r}\n"
                    f"BENCHMARK_MINIMUM_CHECK_COUNTS = "
                    f"{dict.fromkeys(sorted(jolt_qualification.BENCHMARK_SCENARIOS), 1)!r}\n"
                ),
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
            self.assertIn(f"{len(jolt_qualification.REQUIRED_SCENARIOS) + 1} registered", result)

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

    def test_scenario_results_require_complete_verified_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            result_directory = engine_root / "results"
            result_directory.mkdir()
            write_file(
                engine_root,
                "AutomatedTesting/Gem/PythonTests/Physics/Jolt/tests/Jolt_ScenarioRecorder.py",
                'SCENARIO_MINIMUM_CHECK_COUNTS = {"Jolt_Example": 1}\n',
            )
            result = {
                "schemaVersion": 1,
                "scenario": "Jolt_Example",
                "passed": True,
                "minimumCheckCount": 1,
                "checkCount": 1,
                "failedCheckCount": 0,
                "contractErrors": [],
                "checks": [
                    {
                        "name": "meaningful result",
                        "passed": True,
                        "details": "",
                        "exception": "",
                    }
                ],
            }
            encoded_result = json.dumps(result, separators=(",", ":"), sort_keys=True)
            encoded_bytes = encoded_result.encode("utf-8")
            document = {
                "envelope": {
                    "schemaVersion": 1,
                    "scenario": "Jolt_Example",
                    "byteCount": len(encoded_bytes),
                    "chunkCount": 1,
                    "sha256": hashlib.sha256(encoded_bytes).hexdigest(),
                },
                "result": result,
            }
            result_path = result_directory / "Jolt_Example.json"
            result_path.write_text(json.dumps(document), encoding="utf-8")

            message = jolt_qualification.validate_scenario_results(engine_root, result_directory)
            self.assertIn("1 retained scenario result", message)

            document["result"]["checks"][0]["passed"] = False
            result_path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "did not pass every required unique check"):
                jolt_qualification.validate_scenario_results(engine_root, result_directory)

    def test_application_benchmark_requires_metadata_and_thirty_positive_updates(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result_directory = Path(temporary_directory)
            benchmark_directory = result_directory / "Jolt_FallingBodies"
            benchmark_directory.mkdir()
            (benchmark_directory / "benchmark_metadata.json").write_text(
                json.dumps({"benchmark": "Jolt_FallingBodies"}),
                encoding="utf-8",
            )
            for frame in range(1, 31):
                (benchmark_directory / f"physics_update{frame}_time.json").write_text(
                    json.dumps(
                        {
                            "schemaVersion": 1,
                            "workload": "Jolt_FallingBodies",
                            "sampleIndex": frame,
                            "updateTimeNanoseconds": frame,
                        }
                    ),
                    encoding="utf-8",
                )

            message = jolt_qualification.validate_application_benchmark_results(result_directory)
            self.assertIn("30 positive physics-update samples", message)

            (benchmark_directory / "physics_update30_time.json").write_text(
                json.dumps(
                    {
                        "schemaVersion": 1,
                        "workload": "Jolt_FallingBodies",
                        "sampleIndex": 30,
                        "updateTimeNanoseconds": 0,
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "invalid physics-update time"):
                jolt_qualification.validate_application_benchmark_results(result_directory)

    def test_world_bus_script_parity_rejects_missing_and_stale_calls(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            engine_root = Path(temporary_directory)
            script_lines = ["import azlmbr.bus as bus", "import azlmbr.jolt as jolt"]
            for bus_index, (bus_name, source_name) in enumerate(
                jolt_qualification.WORLD_SCRIPT_BUS_SOURCES.items()
            ):
                event_name = f"Event{bus_index}"
                write_file(
                    engine_root,
                    f"Gems/Physics/Jolt/Code/Source/Jolt/{source_name}",
                    f'behaviorContext->EBus<Example>("{bus_name}")\n'
                    f'    ->Event("{event_name}", &Example::{event_name});\n',
                )
                script_lines.append(f'jolt.{bus_name}(bus.Broadcast, "{event_name}")')

            script_path = write_file(
                engine_root,
                "AutomatedTesting/Gem/PythonTests/Physics/Jolt/tests/Jolt_Worlds.py",
                "\n".join(script_lines),
            )
            result = jolt_qualification.validate_world_bus_script_parity(engine_root)
            self.assertIn("5 world buses", result)

            script_path.write_text(
                "\n".join(script_lines[:-1] + [f"# {script_lines[-1]}"]),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "untested reflected events"):
                jolt_qualification.validate_world_bus_script_parity(engine_root)

            script_path.write_text(
                "\n".join(script_lines + ['jolt.JoltWorldRequestBus(bus.Broadcast, "Stale")']),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "unreflected script calls"):
                jolt_qualification.validate_world_bus_script_parity(engine_root)

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
                "set(O3DE_ENGINE_ROOT example)\n"
                "list(APPEND CMAKE_MODULE_PATH ${O3DE_ENGINE_ROOT}/cmake)\n"
                "target_link_libraries(example Gem::Jolt.API)\n",
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
Jolt::ReflectWorlds(nullptr);
Jolt::ReflectWorldDiagnostics(nullptr);
Jolt::ReflectWorldQueries(nullptr);
Jolt::ReflectWorldRollback(nullptr);
Jolt::ReflectWorldSimulation(nullptr);
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
                "set(O3DE_ENGINE_ROOT example)\n"
                "list(APPEND CMAKE_MODULE_PATH ${O3DE_ENGINE_ROOT}/cmake)\n"
                "target_link_libraries(example Gem::Jolt.API)\n",
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
            self.assertIn(
                "-DCMAKE_TRY_COMPILE_CONFIGURATION=Release",
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
        self.assertEqual(
            {variant.build_directory_name for variant in variants.values()},
            {"ca", "cd", "cg", "cm", "cmo", "cn", "cu", "mmo", "mn", "mu"},
        )
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

    def test_windows_matrix_uses_the_sdk_environment_for_clang_cl(self) -> None:
        runner = mock.Mock()
        runner.engine_root = Path("D:/Engine")
        runner.environment = {"PATH": "base"}
        runner.run_command.return_value = True
        developer_environment = {
            "PATH": "sdk",
            "RC": "C:/SDK/bin/rc.exe",
            "WindowsSdkDir": "C:/SDK",
        }

        with (
            mock.patch.object(jolt_qualification.platform, "system", return_value="Windows"),
            mock.patch.object(
                jolt_qualification,
                "load_msvc_environment",
                return_value=developer_environment,
            ),
        ):
            jolt_qualification.add_full_matrix(
                runner,
                Path("D:/Engine/build/matrix"),
                16,
                Path("D:/LLVM/22.1.8"),
            )

        commands = {call.args[0]: call for call in runner.run_command.call_args_list}
        clang_environment = commands["configure-clang-unity-modular"].args[3]
        asan_environment = commands["configure-clang-asan"].args[3]
        asan_azcore_command = commands["test-clang-asan-profile-azcore"].args[1]

        self.assertEqual(clang_environment, developer_environment)
        self.assertEqual(asan_environment["PATH"], "sdk")
        self.assertEqual(asan_environment["RC"], "C:/SDK/bin/rc.exe")
        self.assertEqual(asan_environment["WindowsSdkDir"], "C:/SDK")
        self.assertEqual(asan_environment["ASAN_OPTIONS"], "detect_odr_violation=0")
        self.assertIn("--gtest_filter=ChildAllocatorTests.*", asan_azcore_command)
        self.assertNotIn("ctest", asan_azcore_command)


if __name__ == "__main__":
    unittest.main()
