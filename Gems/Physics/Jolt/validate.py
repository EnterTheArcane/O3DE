"""Run reproducible Jolt qualification checks from one cross-platform entry point."""

from __future__ import annotations

import argparse
import ast
import dataclasses
import datetime
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as xml_tree
from pathlib import Path
from typing import Callable, Iterable, Sequence


PUBLIC_CLASSIFICATION = "Publicly exposed"
INVENTORY_HEADER = "Jolt 5.6 subsystem"
REQUIRED_SCENARIOS = {
    "Jolt_AdvancedComponents",
    "Jolt_Characters",
    "Jolt_ComponentSmoke",
    "Jolt_Constraints",
    "Jolt_CpuHair",
    "Jolt_Diagnostics",
    "Jolt_EventsAndFilters",
    "Jolt_FeatureComponents",
    "Jolt_Hair",
    "Jolt_Queries",
    "Jolt_RagdollsAndSkeletons",
    "Jolt_RollbackAndDeterminism",
    "Jolt_SavedComponents",
    "Jolt_SavedFeatureGallery",
    "Jolt_ScenesAndAssets",
    "Jolt_ShapesAndCooking",
    "Jolt_SoftBodies",
    "Jolt_StressAndSoak",
    "Jolt_Vehicles",
    "Jolt_WorldQueriesAndSnapshots",
}
BENCHMARK_SCENARIOS = {"Jolt_PerformanceCapture"}
WORLD_SCRIPT_BUS_SOURCES = {
    "JoltWorldRequestBus": "WorldBus.cpp",
    "JoltWorldDiagnosticsRequestBus": "WorldDiagnosticsBus.cpp",
    "JoltWorldQueryRequestBus": "WorldQueryBus.cpp",
    "JoltWorldRollbackRequestBus": "WorldRollbackBus.cpp",
    "JoltWorldSimulationRequestBus": "WorldSimulationBus.cpp",
}


@dataclasses.dataclass(frozen=True)
class ValidationResult:
    name: str
    status: str
    duration_seconds: float
    log_path: str
    command: tuple[str, ...] = ()
    message: str = ""


@dataclasses.dataclass(frozen=True)
class MatrixVariant:
    name: str
    build_directory_name: str
    preset: str
    definitions: tuple[str, ...]
    configurations: tuple[str, ...]
    targets: tuple[str, ...]
    run_tests: bool = True


@dataclasses.dataclass(frozen=True)
class MatrixOutcome:
    build_directory: Path
    configured: bool
    environment: dict[str, str] | None = None


def find_engine_root(script_path: Path) -> Path:
    for candidate in (script_path.parent, *script_path.parents):
        if (candidate / "engine.json").is_file() and (candidate / "CMakeLists.txt").is_file():
            return candidate
    raise ValueError(f"Unable to locate the engine root from {script_path}")


def ensure_output_is_under_build(engine_root: Path, output_directory: Path) -> Path:
    resolved_root = engine_root.resolve()
    build_root = (resolved_root / "build").resolve()
    resolved_output = output_directory.resolve()
    if not resolved_output.is_relative_to(build_root):
        raise ValueError(f"Qualification output must be beneath {build_root}")
    return resolved_output


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run_capture(engine_root: Path, command: Sequence[str]) -> bytes:
    completed = subprocess.run(
        command,
        cwd=engine_root,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return completed.stdout


def load_msvc_environment(base_environment: dict[str, str]) -> dict[str, str]:
    existing_compiler = shutil.which("cl.exe", path=base_environment.get("PATH"))
    if base_environment.get("VSCMD_VER") and existing_compiler:
        environment = base_environment.copy()
        resource_compiler = shutil.which("rc.exe", path=environment.get("PATH"))
        if not resource_compiler:
            raise ValueError("The Visual Studio developer environment did not provide rc.exe.")
        environment["RC"] = resource_compiler
        return environment

    program_files_x86 = base_environment.get("ProgramFiles(x86)", "C:/Program Files (x86)")
    vswhere_path = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    installation_path = ""
    if vswhere_path.is_file():
        completed = subprocess.run(
            (
                str(vswhere_path),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ),
            check=False,
            env=base_environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if completed.returncode == 0:
            installation_path = completed.stdout.strip()

    vsdevcmd_path = Path(installation_path) / "Common7" / "Tools" / "VsDevCmd.bat"
    if not installation_path or not vsdevcmd_path.is_file():
        raise ValueError("A Visual Studio installation with the x64 C++ tools was not found.")

    command = f'cmd.exe /d /c call "{vsdevcmd_path}" -no_logo -arch=x64 >nul && set'
    completed = subprocess.run(
        command,
        check=False,
        env=base_environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if completed.returncode != 0:
        details = completed.stdout.strip()
        raise ValueError(
            "Visual Studio developer environment setup failed with exit code "
            f"{completed.returncode}: {details}"
        )

    environment: dict[str, str] = {}
    for line in completed.stdout.splitlines():
        name, separator, value = line.partition("=")
        if separator and name:
            environment[name] = value
    if "PYTHONPYCACHEPREFIX" in base_environment:
        environment["PYTHONPYCACHEPREFIX"] = base_environment["PYTHONPYCACHEPREFIX"]
    if not shutil.which("cl.exe", path=environment.get("PATH")):
        raise ValueError("Visual Studio developer environment did not provide cl.exe.")
    resource_compiler = shutil.which("rc.exe", path=environment.get("PATH"))
    if not resource_compiler:
        raise ValueError("Visual Studio developer environment did not provide rc.exe.")
    environment["RC"] = resource_compiler
    return environment


def capture_git_state(engine_root: Path) -> dict[str, object]:
    head = run_capture(engine_root, ("git", "rev-parse", "HEAD")).decode().strip()
    branch = run_capture(engine_root, ("git", "branch", "--show-current")).decode().strip()
    status = run_capture(engine_root, ("git", "status", "--porcelain=v1", "-z"))
    index_diff = run_capture(engine_root, ("git", "diff", "--cached", "--binary"))
    worktree_diff = run_capture(engine_root, ("git", "diff", "--binary"))
    untracked_paths = run_capture(
        engine_root,
        ("git", "ls-files", "--others", "--exclude-standard", "-z"),
    ).split(b"\0")

    untracked_hash = hashlib.sha256()
    untracked_count = 0
    for encoded_path in sorted(path for path in untracked_paths if path):
        relative_path = encoded_path.decode("utf-8", errors="surrogateescape")
        path = engine_root / relative_path
        if not path.is_file():
            continue
        untracked_hash.update(encoded_path)
        untracked_hash.update(b"\0")
        untracked_hash.update(path.read_bytes())
        untracked_count += 1

    combined = hashlib.sha256()
    combined.update(head.encode())
    combined.update(index_diff)
    combined.update(worktree_diff)
    combined.update(untracked_hash.digest())
    return {
        "branch": branch,
        "head": head,
        "index_sha256": sha256_bytes(index_diff),
        "status_sha256": sha256_bytes(status),
        "untracked_count": untracked_count,
        "untracked_sha256": untracked_hash.hexdigest(),
        "working_tree_sha256": sha256_bytes(worktree_diff),
        "combined_sha256": combined.hexdigest(),
    }


def split_markdown_row(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def validate_feature_ledger(engine_root: Path) -> str:
    ledger_path = engine_root / "Gems" / "Physics" / "Jolt" / "FEATURE_COVERAGE.md"
    lines = ledger_path.read_text(encoding="utf-8").splitlines()
    inventory_rows = 0
    exposed_rows = 0
    errors: list[str] = []
    in_inventory = False

    for line_number, line in enumerate(lines, start=1):
        if not line.startswith("|"):
            in_inventory = False
            continue

        cells = split_markdown_row(line)
        if cells and cells[0] == INVENTORY_HEADER:
            in_inventory = True
            continue
        if not in_inventory or not cells or set(cells[0]) <= {"-", ":"}:
            continue
        if len(cells) != 5:
            errors.append(f"line {line_number} has {len(cells)} cells instead of 5")
            continue

        inventory_rows += 1
        subsystem, classification, boundary, cpp_proof, scenario_proof = cells
        if classification == "Missing":
            errors.append(f"line {line_number} leaves {subsystem!r} classified as Missing")
        if classification == PUBLIC_CLASSIFICATION:
            exposed_rows += 1
            for label, evidence in (
                ("AZ-facing boundary", boundary),
                ("C++ and authoring proof", cpp_proof),
                ("scenario and performance proof", scenario_proof),
            ):
                if not evidence:
                    errors.append(f"line {line_number} leaves {subsystem!r} without {label}")

    if inventory_rows == 0:
        errors.append("the native feature inventory contains no rows")
    if errors:
        raise ValueError("; ".join(errors))
    return f"Validated {inventory_rows} feature rows, including {exposed_rows} exposed rows."


def validate_public_headers(engine_root: Path) -> str:
    include_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "Include" / "Jolt"
    probe_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "Tests" / "Headers"
    public_headers = sorted(include_root.rglob("*.h"))
    errors: list[str] = []

    expected_probes = {
        header.relative_to(include_root).with_suffix(".cpp").as_posix()
        for header in public_headers
    }
    actual_probes = {
        probe.relative_to(probe_root).as_posix()
        for probe in probe_root.rglob("*.cpp")
    }
    missing_probes = sorted(expected_probes - actual_probes)
    extra_probes = sorted(actual_probes - expected_probes)
    if missing_probes:
        errors.append(f"missing header probes: {', '.join(missing_probes)}")
    if extra_probes:
        errors.append(f"orphan header probes: {', '.join(extra_probes)}")

    include_pattern = re.compile(r"^\s*#\s*include\s*[<\"]Jolt/([^>\"]+)[>\"]", re.MULTILINE)
    native_token_pattern = re.compile(r"\bJPH::|\bnamespace\s+JPH\b")
    for header in public_headers:
        text = header.read_text(encoding="utf-8")
        if native_token_pattern.search(text):
            errors.append(f"{header.relative_to(engine_root)} contains a native JPH token")
        for match in include_pattern.finditer(text):
            included_header = include_root / match.group(1)
            if not included_header.is_file():
                errors.append(
                    f"{header.relative_to(engine_root)} includes non-public Jolt/{match.group(1)}"
                )

    if errors:
        raise ValueError("; ".join(errors))
    return f"Validated {len(public_headers)} public headers and isolated probes."


def validate_source_manifests(engine_root: Path) -> str:
    code_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code"
    manifest_paths = sorted(code_root.glob("jolt_*_files.cmake"))
    source_pattern = re.compile(r"(?m)^\s*([A-Za-z0-9_./-]+\.(?:cpp|h|inl|mm|rc))\s*$")
    errors: list[str] = []
    entry_count = 0

    for manifest_path in manifest_paths:
        text = manifest_path.read_text(encoding="utf-8")
        for match in source_pattern.finditer(text):
            relative_path = match.group(1)
            entry_count += 1
            if not (code_root / relative_path).is_file():
                errors.append(f"{manifest_path.name} references missing {relative_path}")

    if not manifest_paths:
        errors.append("no explicit Jolt source manifests were found")
    if errors:
        raise ValueError("; ".join(errors))
    return f"Validated {entry_count} entries across {len(manifest_paths)} explicit manifests."


def validate_scenario_registration(engine_root: Path) -> str:
    suite_root = engine_root / "AutomatedTesting" / "Gem" / "PythonTests" / "Physics" / "Jolt"
    test_root = suite_root / "tests"
    suite_text = (suite_root / "TestSuite_Main.py").read_text(encoding="utf-8")
    benchmark_text = (suite_root / "TestSuite_Benchmark.py").read_text(encoding="utf-8")
    registered_main = set(re.findall(r"from \.tests import (Jolt_[A-Za-z0-9_]+)", suite_text))
    registered_benchmarks = set(re.findall(r"from \.tests import (Jolt_[A-Za-z0-9_]+)", benchmark_text))
    registered = registered_main | registered_benchmarks
    executable = {
        path.stem
        for path in test_root.glob("Jolt_*.py")
        if "Report.start_test(" in path.read_text(encoding="utf-8")
    }

    errors: list[str] = []
    missing_registration = sorted(executable - registered)
    stale_registration = sorted(registered - executable)
    missing_required = sorted(REQUIRED_SCENARIOS - registered)
    if missing_registration:
        errors.append(f"unregistered executable scenarios: {', '.join(missing_registration)}")
    if stale_registration:
        errors.append(f"registered non-executable scenarios: {', '.join(stale_registration)}")
    if missing_required:
        errors.append(f"missing required scenarios: {', '.join(missing_required)}")
    if registered_main != REQUIRED_SCENARIOS:
        errors.append("the main suite and authoritative scenario policy do not match")
    if registered_benchmarks != BENCHMARK_SCENARIOS:
        errors.append("the benchmark suite and authoritative benchmark policy do not match")

    minimum_check_counts = read_scenario_minimum_check_counts(engine_root)
    if set(minimum_check_counts) != REQUIRED_SCENARIOS:
        errors.append("the scenario minimum-check policy and main suite do not match")
    invalid_minimums = sorted(
        scenario for scenario, minimum in minimum_check_counts.items() if minimum <= 0
    )
    if invalid_minimums:
        errors.append(f"scenarios have invalid minimum check counts: {', '.join(invalid_minimums)}")
    benchmark_minimum_check_counts = read_benchmark_minimum_check_counts(engine_root)
    if set(benchmark_minimum_check_counts) != BENCHMARK_SCENARIOS:
        errors.append("the benchmark minimum-check policy and benchmark suite do not match")
    invalid_benchmark_minimums = sorted(
        scenario for scenario, minimum in benchmark_minimum_check_counts.items() if minimum <= 0
    )
    if invalid_benchmark_minimums:
        errors.append(
            f"benchmark scenarios have invalid minimum check counts: {', '.join(invalid_benchmark_minimums)}"
        )

    gallery_path = engine_root / "AutomatedTesting" / "Levels" / "Physics" / "Jolt" / "FeatureGallery"
    stress_path = engine_root / "AutomatedTesting" / "Levels" / "Physics" / "Jolt" / "Stress"
    gallery = json.loads((gallery_path / "FeatureGallery.prefab").read_text(encoding="utf-8"))
    stress = json.loads((stress_path / "Stress.prefab").read_text(encoding="utf-8"))
    gallery_entities = [
        entity
        for entity in gallery.get("Entities", {}).values()
        if entity.get("Name", "").startswith("Jolt Gallery ")
    ]
    if len(gallery_entities) != 51:
        errors.append(f"feature gallery has {len(gallery_entities)} representative entities instead of 51")
    if len(stress.get("Instances", {})) != 64:
        errors.append(f"stress level has {len(stress.get('Instances', {}))} body instances instead of 64")

    if errors:
        raise ValueError("; ".join(errors))
    return f"Validated {len(registered)} registered scenarios, gallery, and stress manifests."


def read_minimum_check_counts(
    engine_root: Path,
    variable_name: str,
) -> dict[str, int]:
    recorder_path = (
        engine_root
        / "AutomatedTesting"
        / "Gem"
        / "PythonTests"
        / "Physics"
        / "Jolt"
        / "tests"
        / "Jolt_ScenarioRecorder.py"
    )
    tree = ast.parse(recorder_path.read_text(encoding="utf-8"), filename=str(recorder_path))
    for statement in tree.body:
        if not isinstance(statement, ast.Assign):
            continue
        if not any(
            isinstance(target, ast.Name) and target.id == variable_name
            for target in statement.targets
        ):
            continue

        values = ast.literal_eval(statement.value)
        if not isinstance(values, dict):
            break
        if not all(isinstance(name, str) and isinstance(count, int) for name, count in values.items()):
            break
        return values

    raise ValueError(f"{recorder_path} does not define a literal {variable_name} mapping")


def read_scenario_minimum_check_counts(engine_root: Path) -> dict[str, int]:
    return read_minimum_check_counts(engine_root, "SCENARIO_MINIMUM_CHECK_COUNTS")


def read_benchmark_minimum_check_counts(engine_root: Path) -> dict[str, int]:
    return read_minimum_check_counts(engine_root, "BENCHMARK_MINIMUM_CHECK_COUNTS")


def validate_scenario_results(
    engine_root: Path,
    result_directory: Path,
    minimum_check_counts: dict[str, int] | None = None,
) -> str:
    if minimum_check_counts is None:
        minimum_check_counts = read_scenario_minimum_check_counts(engine_root)
    if not result_directory.is_dir():
        raise ValueError(f"scenario result directory does not exist: {result_directory}")

    result_paths = sorted(result_directory.iterdir())
    expected_names = {f"{scenario}.json" for scenario in minimum_check_counts}
    observed_names = {path.name for path in result_paths}
    errors: list[str] = []
    missing = sorted(expected_names - observed_names)
    unexpected = sorted(observed_names - expected_names)
    if missing:
        errors.append(f"missing scenario results: {', '.join(missing)}")
    if unexpected:
        errors.append(f"unexpected scenario results: {', '.join(unexpected)}")

    for result_path in result_paths:
        if result_path.name not in expected_names or not result_path.is_file():
            continue

        try:
            document = json.loads(result_path.read_text(encoding="utf-8"))
            envelope = document["envelope"]
            result = document["result"]
        except (KeyError, TypeError, json.JSONDecodeError) as exception:
            errors.append(f"{result_path.name} is not a valid scenario result: {exception}")
            continue

        scenario = result_path.stem
        checks = result.get("checks")
        if not isinstance(checks, list):
            errors.append(f"{result_path.name} has no check list")
            continue

        check_names = [check.get("name") for check in checks if isinstance(check, dict)]
        check_results = [check.get("passed") for check in checks if isinstance(check, dict)]
        minimum = minimum_check_counts[scenario]
        expected_passed = (
            len(checks) >= minimum
            and len(check_names) == len(checks)
            and len(set(check_names)) == len(check_names)
            and all(passed is True for passed in check_results)
        )
        encoded_result = json.dumps(result, separators=(",", ":"), sort_keys=True)
        encoded_bytes = encoded_result.encode("utf-8")
        expected_sha256 = hashlib.sha256(encoded_bytes).hexdigest()
        expected_chunk_count = max(1, (len(encoded_result) + 999) // 1000)

        if result.get("schemaVersion") != 1 or envelope.get("schemaVersion") != 1:
            errors.append(f"{result_path.name} has an unsupported schema")
        if result.get("scenario") != scenario or envelope.get("scenario") != scenario:
            errors.append(f"{result_path.name} has inconsistent scenario identity")
        if result.get("minimumCheckCount") != minimum:
            errors.append(f"{result_path.name} has the wrong minimum check count")
        if result.get("checkCount") != len(checks):
            errors.append(f"{result_path.name} has an inconsistent check count")
        if result.get("failedCheckCount") != sum(passed is not True for passed in check_results):
            errors.append(f"{result_path.name} has an inconsistent failed-check count")
        if result.get("contractErrors") != []:
            errors.append(f"{result_path.name} violated its evidence contract")
        if result.get("passed") is not expected_passed or not expected_passed:
            errors.append(f"{result_path.name} did not pass every required unique check")
        if envelope.get("byteCount") != len(encoded_bytes):
            errors.append(f"{result_path.name} has an inconsistent byte count")
        if envelope.get("chunkCount") != expected_chunk_count:
            errors.append(f"{result_path.name} has an inconsistent chunk count")
        if envelope.get("sha256") != expected_sha256:
            errors.append(f"{result_path.name} failed SHA-256 verification")

    if errors:
        raise ValueError("; ".join(errors))
    return f"Validated {len(result_paths)} retained scenario result envelopes."


def validate_application_benchmark_results(result_directory: Path) -> str:
    benchmark_directory = result_directory / "Jolt_FallingBodies"
    if not benchmark_directory.is_dir():
        raise ValueError(f"application benchmark result directory does not exist: {benchmark_directory}")

    metadata_path = benchmark_directory / "benchmark_metadata.json"
    update_paths = [
        benchmark_directory / f"physics_update{sample}_time.json"
        for sample in range(1, 31)
    ]
    missing = [path.name for path in (metadata_path, *update_paths) if not path.is_file()]
    if missing:
        raise ValueError(f"application benchmark is missing artifacts: {', '.join(missing)}")

    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exception:
        raise ValueError(f"application benchmark metadata is invalid: {exception}") from exception
    if not isinstance(metadata, dict) or not metadata:
        raise ValueError("application benchmark metadata is empty")

    observed_indices = set()
    for update_path in update_paths:
        try:
            update = json.loads(update_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exception:
            raise ValueError(f"{update_path.name} is not valid JSON: {exception}") from exception
        if update.get("schemaVersion") != 1 or update.get("workload") != "Jolt_FallingBodies":
            raise ValueError(f"{update_path.name} contains invalid workload metadata")
        sample_index = update.get("sampleIndex")
        update_time = update.get("updateTimeNanoseconds")
        if not isinstance(sample_index, int) or sample_index in observed_indices:
            raise ValueError(f"{update_path.name} contains a missing or duplicate sample index")
        if not isinstance(update_time, int) or update_time <= 0:
            raise ValueError(f"{update_path.name} contains an invalid physics-update time")
        observed_indices.add(sample_index)
    if observed_indices != set(range(1, 31)):
        raise ValueError("The application benchmark does not contain the complete sample sequence")
    return f"Validated benchmark metadata and {len(update_paths)} positive physics-update samples."


def validate_world_bus_script_parity(engine_root: Path) -> str:
    source_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "Source" / "Jolt"
    script_root = engine_root / "AutomatedTesting" / "Gem" / "PythonTests" / "Physics" / "Jolt" / "tests"
    reflected_events: dict[str, set[str]] = {}
    errors: list[str] = []

    for bus_name, source_name in WORLD_SCRIPT_BUS_SOURCES.items():
        source_path = source_root / source_name
        if not source_path.is_file():
            errors.append(f"missing reflection source {source_path.relative_to(engine_root)}")
            continue

        source_text = source_path.read_text(encoding="utf-8")
        block_pattern = re.compile(
            rf'behaviorContext->EBus<[^>]+>\("{re.escape(bus_name)}"\)(.*?);',
            re.DOTALL,
        )
        blocks = block_pattern.findall(source_text)
        if len(blocks) != 1:
            errors.append(f"{bus_name} has {len(blocks)} reflection blocks instead of 1")
            continue

        events = re.findall(r'->Event\("([A-Za-z0-9_]+)"', blocks[0])
        if not events:
            errors.append(f"{bus_name} reflects no events")
            continue
        if len(events) != len(set(events)):
            errors.append(f"{bus_name} reflects duplicate events")
        reflected_events[bus_name] = set(events)

    script_paths = sorted(script_root.glob("Jolt_*.py"))
    if not script_paths:
        errors.append("no Jolt AutomatedTesting scripts were found")

    called_events = {bus_name: set() for bus_name in WORLD_SCRIPT_BUS_SOURCES}
    for script_path in script_paths:
        script_text = script_path.read_text(encoding="utf-8")
        try:
            script_tree = ast.parse(script_text, filename=str(script_path))
        except SyntaxError as error:
            errors.append(f"{script_path.relative_to(engine_root)} could not be parsed: {error.msg}")
            continue

        for node in ast.walk(script_tree):
            if (
                not isinstance(node, ast.Call)
                or not isinstance(node.func, ast.Attribute)
                or not isinstance(node.func.value, ast.Name)
                or node.func.value.id != "jolt"
                or not node.func.attr.startswith("JoltWorld")
                or not node.func.attr.endswith("RequestBus")
            ):
                continue

            bus_name = node.func.attr
            if bus_name not in called_events:
                errors.append(
                    f"{script_path.relative_to(engine_root)} calls unexpected world bus {bus_name}"
                )
                continue

            is_broadcast = (
                len(node.args) >= 2
                and isinstance(node.args[0], ast.Attribute)
                and isinstance(node.args[0].value, ast.Name)
                and node.args[0].value.id == "bus"
                and node.args[0].attr == "Broadcast"
            )
            if not is_broadcast or not isinstance(node.args[1], ast.Constant) or not isinstance(node.args[1].value, str):
                errors.append(
                    f"{script_path.relative_to(engine_root)} has a malformed {bus_name} call on line {node.lineno}"
                )
                continue
            called_events[bus_name].add(node.args[1].value)

    for bus_name, events in reflected_events.items():
        missing_calls = sorted(events - called_events[bus_name])
        stale_calls = sorted(called_events[bus_name] - events)
        if missing_calls:
            errors.append(f"{bus_name} has untested reflected events: {', '.join(missing_calls)}")
        if stale_calls:
            errors.append(f"{bus_name} has unreflected script calls: {', '.join(stale_calls)}")

    if errors:
        raise ValueError("; ".join(errors))
    event_count = sum(len(events) for events in reflected_events.values())
    return f"Validated {len(reflected_events)} world buses and {event_count} reflected script operations."


def validate_private_native_boundary(engine_root: Path) -> str:
    gem_root = engine_root / "Gems" / "Physics" / "Jolt"
    native_cmake = (gem_root / "3rdParty" / "JoltNative.cmake").read_text(encoding="utf-8")
    native_identity = gem_root / "3rdParty" / "JoltNativeBuildIdentity.cpp"
    code_cmake = (gem_root / "Code" / "CMakeLists.txt").read_text(encoding="utf-8")
    native_code = "\n".join(line for line in native_cmake.splitlines() if not line.lstrip().startswith("#"))
    target_code = "\n".join(line for line in code_cmake.splitlines() if not line.lstrip().startswith("#"))
    errors: list[str] = []

    if list(gem_root.rglob("FindJolt.cmake")):
        errors.append("FindJolt.cmake must not be restored")
    if "3rdParty::Jolt" in native_code or "3rdParty::Jolt" in target_code:
        errors.append("the private native target is published as 3rdParty::Jolt")
    if re.search(r"PUBLIC\s+Jolt(?:\s|\))", target_code):
        errors.append("the public API target publishes the native Jolt target")
    installed_lines = [line.strip() for line in native_cmake.splitlines() if "ly_install" in line]
    if len(installed_lines) != 1:
        errors.append("JoltNative.cmake must contain exactly one license-only install declaration")
    if "${jolt_source_dir}/LICENSE" not in native_cmake:
        errors.append("the private dependency license is not installed")
    if not re.search(r"set\(jolt_native_content_base_name\s+\S+_5_6_0\)", native_code):
        errors.append("the private dependency does not use a versioned provider-owned FetchContent identity")
    if not re.search(
        r'set\(jolt_native_content_name\s+"?\$\{jolt_native_content_base_name\}_'
        r'\$\{jolt_native_patch_identity\}"?\)',
        native_code,
    ):
        errors.append("the private dependency identity does not include the native patch fingerprint")
    if not re.search(
        r'string\(SUBSTRING\s+"?\$\{jolt_native_patch_hash\}"?\s+0\s+16\s+'
        r'jolt_native_patch_identity\)',
        native_code,
    ):
        errors.append("the private dependency does not derive a bounded patch identity")
    if "foreign target named Jolt" not in native_cmake:
        errors.append("a foreign target named Jolt is not rejected")
    if "foreign FetchContent declaration" not in native_cmake:
        errors.append("a foreign versioned FetchContent declaration is not rejected")
    if "cannot override the private" not in native_cmake:
        errors.append("a private native source override is not rejected")
    if not native_identity.is_file() or "JoltNativeBuildIdentity.cpp" not in native_cmake:
        errors.append("the configured native fingerprint is not compiled into the native object library")
    if "JOLT_NATIVE_BUILD_FINGERPRINT" not in native_cmake:
        errors.append("the private native build fingerprint is not configured")
    architecture_inputs = (
        "CMAKE_OSX_ARCHITECTURES",
        "CMAKE_VS_PLATFORM_NAME",
        "CMAKE_CXX_COMPILER_TARGET",
        "CMAKE_SYSTEM_PROCESSOR",
    )
    if any(architecture_input not in native_cmake for architecture_input in architecture_inputs):
        errors.append("native ISA selection is not derived from target architectures")
    if "check_ipo_supported" not in native_code:
        errors.append("private native IPO is not support-checked")
    if "if(LY_MONOLITHIC_GAME)" not in native_code:
        errors.append("private IPO is not disabled when no final link is owned")
    if "WIN32 AND JOLT_NATIVE_TARGET_HAS_ARM" not in native_code:
        errors.append("private IPO does not retain the upstream Windows ARM exclusion")
    if re.search(r"target_link_options\([^)]*\.API\s+INTERFACE", target_code, re.DOTALL):
        errors.append("the public API target leaks private IPO link options")
    if native_identity.is_file():
        native_identity_code = native_identity.read_text(encoding="utf-8")
        if "The private Jolt x86 objects require SSE4.1" not in native_identity_code:
            errors.append("the native object library does not compile its own ISA guard")
        if "The private Jolt ARM objects cannot use x86" not in native_identity_code:
            errors.append("the native object library does not guard ARM objects from x86 features")

    gpu_terms = ("Atom_RHI", "Atom_RPI", "JPH_USE_DX12 ON", "JPH_USE_MTL ON", "JPH_USE_VK ON")
    for term in gpu_terms:
        if term in native_code or term in target_code:
            errors.append(f"obsolete GPU dependency or backend remains: {term}")

    if errors:
        raise ValueError("; ".join(errors))
    return "Validated private native identity, target, license-only install, and CPU-only dependency boundary."


def validate_clang_address_sanitizer_configuration(engine_root: Path) -> str:
    clang_configuration_path = (
        engine_root / "cmake" / "Platform" / "Common" / "MSVC" / "Configurations_clang.cmake"
    )
    azcore_cmake_path = engine_root / "Code" / "Framework" / "AzCore" / "CMakeLists.txt"
    system_allocator_path = (
        engine_root / "Code" / "Framework" / "AzCore" / "AzCore" / "Memory" / "SystemAllocator.cpp"
    )
    clang_configuration = clang_configuration_path.read_text(encoding="utf-8")
    azcore_cmake = azcore_cmake_path.read_text(encoding="utf-8")
    system_allocator = system_allocator_path.read_text(encoding="utf-8")
    asan_block = clang_configuration.split("if(LY_BUILD_WITH_ADDRESS_SANITIZER)", maxsplit=1)[-1]
    errors: list[str] = []

    if "GENERATOR_IS_MULTI_CONFIG" not in asan_block:
        errors.append("clang-cl ASan configuration policy does not inspect the actual generator kind")
    if "set(CMAKE_CONFIGURATION_TYPES profile" not in asan_block:
        errors.append("multi-configuration clang-cl ASan builds are not restricted to Profile")
    if "O3DE_CLANG_ASAN_BUILD_TYPE STREQUAL \"profile\"" not in asan_block:
        errors.append("single-configuration clang-cl ASan builds do not reject unsupported configurations")
    if not re.search(r"COMPILATION_PROFILE\s+-fsanitize=address\s+/Oy-", asan_block):
        errors.append("clang-cl Profile compilation is not instrumented")
    if not re.search(
        r"LINK_NON_STATIC_PROFILE\s+"
        r'"\$\{O3DE_CLANG_ASAN_DYNAMIC_LIBRARY\}"\s+'
        r"/include:__asan_seh_interceptor\s+"
        r'"/wholearchive:\$\{O3DE_CLANG_ASAN_THUNK_LIBRARY\}"',
        asan_block,
    ):
        errors.append("clang-cl Profile does not link the ASan runtime and thunk")
    if "CMAKE_RUNTIME_OUTPUT_DIRECTORY_PROFILE" not in asan_block:
        errors.append("clang-cl Profile does not receive the ASan runtime DLL")

    for architecture_input in (
        "CMAKE_CXX_COMPILER_TARGET",
        "CMAKE_VS_PLATFORM_NAME",
        "CMAKE_SYSTEM_PROCESSOR",
    ):
        if architecture_input not in asan_block:
            errors.append(f"clang-cl ASan runtime selection ignores {architecture_input}")

    sanitizer_definition = "$<$<CONFIG:Profile>:AZCORE_ADDRESS_SANITIZER_ENABLED>"
    if "AZCORE_USE_MALLOC_SYSTEM_ALLOCATOR" not in azcore_cmake:
        errors.append("AzCore cannot validate the malloc allocator without ASan's custom header")
    if sanitizer_definition not in azcore_cmake:
        errors.append("AzCore's custom ASan allocation header is not enabled for Profile")
    if "address.m_size = SystemAllocatorPrivate::GetAllocatedSize" not in system_allocator:
        errors.append("SystemAllocator allocation results do not report the accounted byte size")
    if "newAddress.m_size = SystemAllocatorPrivate::GetAllocatedSize" not in system_allocator:
        errors.append("SystemAllocator reallocation results do not report the accounted byte size")

    if errors:
        raise ValueError("; ".join(errors))
    return "Validated Profile-only clang-cl ASan deployment and symmetrical malloc accounting policy."


def validate_public_consumer(engine_root: Path) -> str:
    consumer_root = (
        engine_root
        / "Gems"
        / "Physics"
        / "Jolt"
        / "Code"
        / "Tests"
        / "InstalledConsumer"
    )
    expected_files = {
        "CMakeLists.txt",
        "jolt_installed_consumer_files.cmake",
        "main.cpp",
        "project.json",
    }
    actual_files = {
        path.relative_to(consumer_root).as_posix()
        for path in consumer_root.rglob("*")
        if path.is_file()
    }
    errors: list[str] = []
    if actual_files != expected_files:
        missing = sorted(expected_files - actual_files)
        extra = sorted(actual_files - expected_files)
        if missing:
            errors.append(f"public consumer is missing: {', '.join(missing)}")
        if extra:
            errors.append(f"public consumer contains unexpected files: {', '.join(extra)}")

    source_text = "\n".join(
        (consumer_root / relative_path).read_text(encoding="utf-8")
        for relative_path in sorted(expected_files & actual_files)
    )
    forbidden_terms = ("3rdParty::Jolt", "JPH::", "JoltNative", "<Jolt/Jolt.h>", "_deps/joltphysics")
    for term in forbidden_terms:
        if term in source_text:
            errors.append(f"public consumer contains private native term {term!r}")

    required_callable_terms = (
        "Jolt::ReflectDebugDraw",
        "Jolt::ReflectDiagnostics",
        "Jolt::ReflectEvents",
        "Jolt::ReflectQueries",
        "Jolt::ReflectWorlds",
        "Jolt::ReflectWorldDiagnostics",
        "Jolt::ReflectWorldQueries",
        "Jolt::ReflectWorldRollback",
        "Jolt::ReflectWorldSimulation",
        "emptyEventBatch.GetId",
        "GetQueryFace",
        "GetTargetFace",
    )
    missing_callable_terms = [term for term in required_callable_terms if term not in source_text]
    if missing_callable_terms:
        errors.append(f"public consumer does not call exported API: {', '.join(missing_callable_terms)}")

    cmake_text = (consumer_root / "CMakeLists.txt").read_text(encoding="utf-8")
    if "Gem::Jolt.API" not in cmake_text:
        errors.append("public consumer does not link Gem::Jolt.API")
    if "O3DE_ENGINE_ROOT" not in cmake_text:
        errors.append("public consumer cannot select a source or installed engine")
    if "list(APPEND CMAKE_MODULE_PATH ${O3DE_ENGINE_ROOT}/cmake)" not in cmake_text:
        errors.append("public consumer does not expose installed engine find modules")

    project = json.loads((consumer_root / "project.json").read_text(encoding="utf-8"))
    if project.get("gem_names") != ["Jolt"]:
        errors.append("public consumer must enable only the Jolt Gem")

    if errors:
        raise ValueError("; ".join(errors))
    return "Validated the public-only source and installed-engine consumer."


def validate_installed_boundary(
    engine_root: Path,
    install_root: Path,
    configuration: str,
    monolithic: bool,
) -> str:
    source_include_root = engine_root / "Gems" / "Physics" / "Jolt" / "Code" / "Include"
    installed_gem_root = install_root / "Gems" / "Physics" / "Jolt"
    installed_include_root = installed_gem_root / "Code" / "Include"
    errors: list[str] = []

    source_headers = {
        path.relative_to(source_include_root).as_posix()
        for path in source_include_root.rglob("*.h")
    }
    installed_headers = {
        path.relative_to(installed_include_root).as_posix()
        for path in installed_include_root.rglob("*.h")
    }
    if installed_headers != source_headers:
        missing = sorted(source_headers - installed_headers)
        extra = sorted(installed_headers - source_headers)
        if missing:
            errors.append(f"installed public headers are missing: {', '.join(missing)}")
        if extra:
            errors.append(f"installed public headers contain unexpected files: {', '.join(extra)}")

    native_include_root = install_root / "include" / "Jolt"
    native_files = {
        path.relative_to(native_include_root).as_posix()
        for path in native_include_root.rglob("*")
        if path.is_file()
    }
    if native_files != {"LICENSE"}:
        errors.append(
            "the installed native include boundary must contain only the Jolt license; "
            f"found {', '.join(sorted(native_files)) or 'nothing'}"
        )

    forbidden_paths = [
        path.relative_to(install_root).as_posix()
        for path in install_root.rglob("*")
        if path.is_file() and path.name in {"FindJolt.cmake", "JoltNative.cmake"}
    ]
    if forbidden_paths:
        errors.append(f"private native integration was installed: {', '.join(forbidden_paths)}")

    permutation = "Default"
    if monolithic:
        permutation = "Monolithic"
    permutation_descriptions = [
        path
        for path in installed_gem_root.rglob("permutation.cmake")
        if permutation.lower() in {part.lower() for part in path.parts}
    ]
    if len(permutation_descriptions) != 1:
        errors.append(
            f"the installed engine must contain exactly one {permutation} Jolt permutation; "
            f"found {len(permutation_descriptions)}"
        )
    else:
        permutation_text = permutation_descriptions[0].read_text(encoding="utf-8", errors="replace")
        canonical_alias = re.search(
            r"ly_create_alias\(\s*NAME\s+Jolt\s+NAMESPACE\s+Gem\s+"
            r"INTERNAL_NAME\s+Jolt\.GemAlias\s+TARGETS\s+Gem::Jolt\.Module\s*\)",
            permutation_text,
        )
        if not canonical_alias:
            errors.append("the installed permutation does not recreate the canonical Gem::Jolt alias")

        api_target = re.search(
            r"ly_add_target\(\s*NAME\s+Jolt\.API\b(?P<body>.*?)\n\)",
            permutation_text,
            re.DOTALL,
        )
        runtime_dependencies = None
        if api_target:
            runtime_dependencies = re.search(
                r"RUNTIME_DEPENDENCIES(?P<body>.*?)TARGET_PROPERTIES",
                api_target.group("body"),
                re.DOTALL,
            )
        if not api_target or not runtime_dependencies:
            errors.append("the installed permutation does not describe Jolt.API runtime dependencies")
        elif runtime_dependencies.group("body").strip():
            errors.append("the installed Jolt.API target retains a private native runtime dependency")

    configuration_lower = configuration.lower()
    target_descriptions = list(installed_gem_root.rglob(f"Jolt.API_{configuration_lower}.cmake"))
    if len(target_descriptions) != 1:
        errors.append(
            "the installed engine must contain exactly one Jolt.API target description for "
            f"{configuration}; found {len(target_descriptions)}"
        )
    else:
        target_text = target_descriptions[0].read_text(encoding="utf-8", errors="replace")
        for term in ("3rdParty::Jolt", "JoltNative", "JPH::"):
            if term in target_text:
                errors.append(f"installed Jolt.API target exposes private native term {term!r}")

    binary_suffixes = {".a", ".dll", ".dylib", ".lib", ".so"}
    binaries = [
        path
        for path in install_root.rglob("*Jolt.API*")
        if path.is_file()
        and path.suffix.lower() in binary_suffixes
        and configuration_lower in {part.lower() for part in path.parts}
        and permutation.lower() in {part.lower() for part in path.parts}
    ]
    if not binaries:
        errors.append(f"the installed engine contains no {configuration} {permutation} Jolt.API binary")

    if errors:
        raise ValueError("; ".join(errors))
    return (
        f"Validated {len(installed_headers)} public headers and the "
        f"{configuration} {permutation} installed Jolt.API boundary."
    )


class ValidationRunner:
    def __init__(
        self,
        engine_root: Path,
        output_directory: Path,
        dry_run: bool,
    ) -> None:
        self.engine_root = engine_root
        self.output_directory = output_directory
        self.dry_run = dry_run
        self.environment = os.environ.copy()
        self.environment["PYTHONPYCACHEPREFIX"] = str(output_directory / "python-cache")
        temporary_directory = output_directory / "temporary"
        temporary_directory.mkdir(parents=True, exist_ok=True)
        self.environment["TEMP"] = str(temporary_directory)
        self.environment["TMP"] = str(temporary_directory)
        self.environment["TMPDIR"] = str(temporary_directory)
        self.results: list[ValidationResult] = []

    def run_check(self, name: str, check: Callable[[Path], str]) -> bool:
        log_path = self.output_directory / f"{len(self.results):03d}-{name}.log"
        start = time.monotonic()
        status = "passed"
        message = ""
        try:
            message = check(self.engine_root)
        except Exception as exception:
            status = "failed"
            message = str(exception)
        log_path.write_text(message + "\n", encoding="utf-8")
        self.results.append(
            ValidationResult(
                name=name,
                status=status,
                duration_seconds=time.monotonic() - start,
                log_path=str(log_path.relative_to(self.output_directory)),
                message=message,
            )
        )
        print(f"[{status.upper()}] {name}: {message}", flush=True)
        return status != "failed"

    def run_command(
        self,
        name: str,
        command: Sequence[str],
        timeout_seconds: int,
        environment: dict[str, str] | None = None,
    ) -> bool:
        log_path = self.output_directory / f"{len(self.results):03d}-{name}.log"
        start = time.monotonic()
        status = "passed"
        message = ""
        print(f"[RUN] {name}: {' '.join(command)}", flush=True)
        if self.dry_run:
            status = "skipped"
            message = "dry run"
            log_path.write_text(" ".join(command) + "\n", encoding="utf-8")
        else:
            try:
                with log_path.open("w", encoding="utf-8", errors="replace") as log:
                    completed = subprocess.run(
                        command,
                        cwd=self.engine_root,
                        check=False,
                        env=environment or self.environment,
                        stdout=log,
                        stderr=subprocess.STDOUT,
                        timeout=timeout_seconds,
                    )
                if completed.returncode != 0:
                    status = "failed"
                    message = f"exit code {completed.returncode}"
            except subprocess.TimeoutExpired:
                status = "failed"
                message = f"timed out after {timeout_seconds} seconds"

        self.results.append(
            ValidationResult(
                name=name,
                status=status,
                duration_seconds=time.monotonic() - start,
                log_path=str(log_path.relative_to(self.output_directory)),
                command=tuple(command),
                message=message,
            )
        )
        print(f"[{status.upper()}] {name} {message}", flush=True)
        return status != "failed"

    def skip(self, name: str, message: str) -> None:
        log_path = self.output_directory / f"{len(self.results):03d}-{name}.log"
        log_path.write_text(message + "\n", encoding="utf-8")
        self.results.append(
            ValidationResult(
                name=name,
                status="skipped",
                duration_seconds=0.0,
                log_path=str(log_path.relative_to(self.output_directory)),
                message=message,
            )
        )
        print(f"[SKIPPED] {name}: {message}", flush=True)


def add_static_checks(runner: ValidationRunner) -> None:
    runner.run_check("feature-ledger", validate_feature_ledger)
    runner.run_check("public-headers", validate_public_headers)
    runner.run_check("source-manifests", validate_source_manifests)
    runner.run_check("scenario-registration", validate_scenario_registration)
    runner.run_check("world-bus-script-parity", validate_world_bus_script_parity)
    runner.run_check("private-native-boundary", validate_private_native_boundary)
    runner.run_check("clang-address-sanitizer-configuration", validate_clang_address_sanitizer_configuration)
    runner.run_check("public-consumer", validate_public_consumer)


def add_python_tests(runner: ValidationRunner) -> None:
    runner.run_command(
        "python-validation-tests",
        (
            sys.executable,
            "-m",
            "unittest",
            "discover",
            "-s",
            "Gems/Physics/Jolt/Code/Tests",
            "-p",
            "test_*.py",
        ),
        300,
    )


def get_automated_testing_environment(
    base_environment: dict[str, str],
    qualification_mode: str,
    scenario_result_directory: Path | None = None,
    benchmark_result_directory: Path | None = None,
) -> dict[str, str]:
    if qualification_mode not in ("review", "full"):
        raise ValueError(f"AutomatedTesting is not part of {qualification_mode!r} qualification")

    stress_mode = "review"
    if qualification_mode == "full":
        stress_mode = "full"

    automated_testing_environment = base_environment.copy()
    automated_testing_environment["JOLT_STRESS_MODE"] = stress_mode
    if scenario_result_directory:
        automated_testing_environment["JOLT_SCENARIO_RESULT_DIRECTORY"] = str(scenario_result_directory)
    if benchmark_result_directory:
        automated_testing_environment["JOLT_BENCHMARK_RESULT_DIRECTORY"] = str(benchmark_result_directory)
    return automated_testing_environment


def get_clang_address_sanitizer_environment(
    base_environment: dict[str, str],
) -> dict[str, str]:
    address_sanitizer_environment = base_environment.copy()
    # clang-cl 22 reports false ODR violations for folded Windows string literals.
    # ODR indicators do not link with the engine's COMDAT model, so disable only that detector.
    address_sanitizer_environment["ASAN_OPTIONS"] = "detect_odr_violation=0"
    return address_sanitizer_environment


def add_primary_build_and_tests(
    runner: ValidationRunner,
    build_directory: Path,
    configuration: str,
    parallel: int,
    qualification_mode: str,
    environment: dict[str, str] | None = None,
) -> None:
    review = qualification_mode in ("review", "full")
    targets = ["Jolt.Tests", "Jolt.Module", "Jolt.Editor"]
    if review:
        targets.extend(("AssetProcessor", "Editor", "AutomatedTesting.Assets"))
    built = runner.run_command(
        "primary-build",
        (
            "cmake",
            "--build",
            str(build_directory),
            "--config",
            configuration,
            "--target",
            *targets,
            "--parallel",
            str(parallel),
        ),
        14_400,
        environment,
    )
    if not built:
        runner.skip("jolt-cpp-tests", "primary build failed")
        if review:
            runner.skip("jolt-automated-testing", "primary build failed")
        return
    runner.run_command(
        "jolt-cpp-tests",
        (
            "ctest",
            "--test-dir",
            str(build_directory),
            "-C",
            configuration,
            "-R",
            r"Gem::Jolt\.Tests\.main::TEST_RUN",
            "--output-on-failure",
        ),
        1_800,
        environment,
    )
    if review:
        scenario_result_directory = runner.output_directory / "scenario-results-main"
        if scenario_result_directory.exists():
            shutil.rmtree(scenario_result_directory)
        scenario_result_directory.mkdir(parents=True)
        automated_testing_environment = get_automated_testing_environment(
            environment or runner.environment,
            qualification_mode,
            scenario_result_directory,
        )
        runner.run_command(
            "jolt-automated-testing",
            (
                "ctest",
                "--test-dir",
                str(build_directory),
                "-C",
                configuration,
                "-R",
                r"AutomatedTesting::JoltTests_Main\.main::TEST_RUN",
                "--output-on-failure",
            ),
            7_200,
            automated_testing_environment,
        )
        if runner.dry_run:
            runner.skip("jolt-scenario-results", "dry run")
        else:
            runner.run_check(
                "jolt-scenario-results",
                lambda engine_root: validate_scenario_results(engine_root, scenario_result_directory),
            )


def read_cmake_cache_value(build_directory: Path, name: str) -> str:
    cache_path = build_directory / "CMakeCache.txt"
    if not cache_path.is_file():
        raise ValueError(f"CMake cache does not exist: {cache_path}")
    pattern = re.compile(rf"^{re.escape(name)}(?::[^=]+)?=(.*)$")
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            return match.group(1)
    raise ValueError(f"CMake cache does not define {name}: {cache_path}")


def load_build_environment(
    base_environment: dict[str, str],
    build_directory: Path,
) -> dict[str, str] | None:
    if platform.system() != "Windows" or not (build_directory / "CMakeCache.txt").is_file():
        return None

    compiler = Path(read_cmake_cache_value(build_directory, "CMAKE_CXX_COMPILER"))
    if compiler.name.lower() not in ("cl.exe", "clang-cl.exe"):
        return None

    return load_msvc_environment(base_environment)


def get_compiler_metadata(
    build_directory: Path,
    environment: dict[str, str] | None = None,
) -> tuple[str, str]:
    compiler = Path(read_cmake_cache_value(build_directory, "CMAKE_CXX_COMPILER"))
    compiler_name = compiler.name.lower()
    compiler_id = "Unknown"
    command = (str(compiler), "--version")
    version_pattern = r"version\s+([^\s]+)"
    if compiler_name == "cl.exe":
        compiler_id = "MSVC"
        command = (str(compiler),)
        version_pattern = r"Version\s+([0-9.]+)"
    elif compiler_name in ("clang", "clang++", "clang-cl", "clang-cl.exe"):
        compiler_id = "Clang"
    elif compiler_name in ("g++", "gcc"):
        compiler_id = "GNU"

    completed = subprocess.run(
        command,
        check=False,
        capture_output=True,
        env=environment,
        text=True,
        timeout=60,
    )
    compiler_output = completed.stdout + completed.stderr
    match = re.search(version_pattern, compiler_output, re.IGNORECASE)
    if not match:
        raise ValueError(f"Could not identify the compiler version from {compiler}: {compiler_output.strip()}")
    return compiler_id, match.group(1)


def add_performance_qualification(
    runner: ValidationRunner,
    build_directory: Path,
    parallel: int,
    qualification_mode: str,
    environment: dict[str, str] | None = None,
) -> None:
    targets = ["Jolt.Tests", "Editor", "AutomatedTesting.Assets"]
    if qualification_mode == "full":
        targets.extend(("Box3D.Tests", "PhysX5.Tests"))
    built = runner.run_command(
        "build-release-performance-targets",
        (
            "cmake",
            "--build",
            str(build_directory),
            "--config",
            "Release",
            "--target",
            *targets,
            "--parallel",
            str(parallel),
        ),
        28_800,
        environment,
    )
    if not built:
        runner.skip("native-benchmark-qualification", "Release performance targets failed to build")
        if platform.system() == "Windows":
            runner.skip("application-benchmark", "Release performance targets failed to build")
            runner.skip("application-benchmark-scenario-result", "application benchmark did not run")
            runner.skip("application-benchmark-artifacts", "application benchmark did not run")
        return

    compiler_metadata = None
    if runner.dry_run:
        compiler_metadata = ("Unknown", "dry-run")
        runner.skip("performance-compiler-metadata", "dry run")
    else:
        def read_compiler_metadata(_: Path) -> str:
            nonlocal compiler_metadata
            compiler_metadata = get_compiler_metadata(build_directory, environment)
            return f"Qualified {compiler_metadata[0]} {compiler_metadata[1]} Release benchmark binaries."

        runner.run_check("performance-compiler-metadata", read_compiler_metadata)
    if not compiler_metadata:
        runner.skip("native-benchmark-qualification", "compiler metadata is unavailable")
        return

    binary_directory = build_directory / "bin" / "release"
    native_result_directory = runner.output_directory / "native-benchmarks"
    runner.run_command(
        "native-benchmark-qualification",
        (
            sys.executable,
            str(
                runner.engine_root
                / "Gems"
                / "Physics"
                / "Jolt"
                / "Code"
                / "Tests"
                / "run_benchmark_qualification.py"
            ),
            qualification_mode,
            "--engine-root",
            str(runner.engine_root),
            "--binary-directory",
            str(binary_directory),
            "--output-directory",
            str(native_result_directory),
            "--compiler-id",
            compiler_metadata[0],
            "--compiler-version",
            compiler_metadata[1],
        ),
        172_800,
        environment,
    )

    if platform.system() != "Windows":
        runner.skip("application-benchmark", "the application benchmark currently requires Windows DX12")
        return

    benchmark_scenario_directory = runner.output_directory / "scenario-results-benchmark"
    benchmark_artifact_directory = runner.output_directory / "application-benchmark"
    benchmark_scenario_directory.mkdir(parents=True, exist_ok=True)
    benchmark_artifact_directory.mkdir(parents=True, exist_ok=True)
    benchmark_environment = get_automated_testing_environment(
        environment or runner.environment,
        qualification_mode,
        benchmark_scenario_directory,
        benchmark_artifact_directory,
    )
    runner.run_command(
        "application-benchmark",
        (
            "ctest",
            "--test-dir",
            str(build_directory),
            "-C",
            "Release",
            "-R",
            r"AutomatedTesting::JoltTests_Benchmark\.benchmark::TEST_RUN",
            "--output-on-failure",
        ),
        1_200,
        benchmark_environment,
    )
    if runner.dry_run:
        runner.skip("application-benchmark-scenario-result", "dry run")
        runner.skip("application-benchmark-artifacts", "dry run")
        return

    runner.run_check(
        "application-benchmark-scenario-result",
        lambda engine_root: validate_scenario_results(
            engine_root,
            benchmark_scenario_directory,
            read_benchmark_minimum_check_counts(engine_root),
        ),
    )
    runner.run_check(
        "application-benchmark-artifacts",
        lambda _: validate_application_benchmark_results(benchmark_artifact_directory),
    )


def prepare_consumer_source(engine_root: Path, destination: Path) -> Path:
    source = (
        engine_root
        / "Gems"
        / "Physics"
        / "Jolt"
        / "Code"
        / "Tests"
        / "InstalledConsumer"
    )
    destination.mkdir(parents=True, exist_ok=True)
    for relative_path in (
        "CMakeLists.txt",
        "jolt_installed_consumer_files.cmake",
        "main.cpp",
        "project.json",
    ):
        shutil.copy2(source / relative_path, destination / relative_path)
    return destination


def add_public_consumer(
    runner: ValidationRunner,
    primary_build_directory: Path,
    consumer_build_directory: Path,
    consumer_engine_root: Path,
    configuration: str,
    parallel: int,
    name_prefix: str,
    monolithic: bool,
    environment: dict[str, str] | None = None,
) -> None:
    monolithic_option = "OFF"
    if monolithic:
        monolithic_option = "ON"

    consumer_source = prepare_consumer_source(
        runner.engine_root,
        consumer_build_directory.with_name(f"{consumer_build_directory.name}-project"),
    )
    try:
        generator = read_cmake_cache_value(primary_build_directory, "CMAKE_GENERATOR")
        c_compiler = read_cmake_cache_value(primary_build_directory, "CMAKE_C_COMPILER")
        cxx_compiler = read_cmake_cache_value(primary_build_directory, "CMAKE_CXX_COMPILER")
    except ValueError as exception:
        runner.skip(f"configure-{name_prefix}-consumer", str(exception))
        runner.skip(f"build-{name_prefix}-consumer", f"{name_prefix} consumer was not configured")
        runner.skip(f"run-{name_prefix}-consumer", f"{name_prefix} consumer was not built")
        return

    configured = runner.run_command(
        f"configure-{name_prefix}-consumer",
        (
            "cmake",
            "-S",
            str(consumer_source),
            "-B",
            str(consumer_build_directory),
            "-G",
            generator,
            f"-DO3DE_ENGINE_ROOT={consumer_engine_root.as_posix()}",
            f"-DCMAKE_C_COMPILER={Path(c_compiler).as_posix()}",
            f"-DCMAKE_CXX_COMPILER={Path(cxx_compiler).as_posix()}",
            f"-DCMAKE_TRY_COMPILE_CONFIGURATION={configuration}",
            f"-DLY_MONOLITHIC_GAME={monolithic_option}",
            "-DLY_UNITY_BUILD=ON",
        ),
        3_600,
        environment,
    )
    if not configured:
        runner.skip(f"build-{name_prefix}-consumer", f"{name_prefix} consumer configuration failed")
        runner.skip(f"run-{name_prefix}-consumer", f"{name_prefix} consumer configuration failed")
        return

    built = runner.run_command(
        f"build-{name_prefix}-consumer",
        (
            "cmake",
            "--build",
            str(consumer_build_directory),
            "--config",
            configuration,
            "--target",
            "JoltInstalledConsumer",
            "--parallel",
            str(parallel),
        ),
        14_400,
        environment,
    )
    if not built:
        runner.skip(f"run-{name_prefix}-consumer", f"{name_prefix} consumer build failed")
        return

    executable_name = "JoltInstalledConsumer.exe"
    if platform.system() != "Windows":
        executable_name = "JoltInstalledConsumer"
    runner.run_command(
        f"run-{name_prefix}-consumer",
        (str(consumer_build_directory / "bin" / configuration.lower() / executable_name),),
        120,
        environment,
    )


def add_source_consumer(
    runner: ValidationRunner,
    primary_build_directory: Path,
    consumer_build_directory: Path,
    configuration: str,
    parallel: int,
    environment: dict[str, str] | None = None,
) -> None:
    add_public_consumer(
        runner,
        primary_build_directory,
        consumer_build_directory,
        runner.engine_root,
        configuration,
        parallel,
        "source",
        False,
        environment,
    )


def get_source_consumer_build_directory(
    matrix_root: Path,
    primary_build_directory: Path,
) -> Path:
    return matrix_root / f"consumer-source-{primary_build_directory.name}"


def prepare_installed_consumer_environment(
    output_directory: Path,
    install_root: Path,
    base_environment: dict[str, str],
    dry_run: bool,
) -> tuple[dict[str, str], str]:
    environment = base_environment.copy()
    if platform.system() != "Windows":
        return environment, "The installed consumer uses the existing non-Windows user environment."

    user_profile_value = environment.get("USERPROFILE")
    if not user_profile_value:
        raise ValueError("USERPROFILE is required to prepare an installed Windows consumer.")

    user_profile = Path(user_profile_value).resolve()
    if user_profile.drive.casefold() == install_root.resolve().drive.casefold():
        return environment, "The installed engine and Python user environment already share a drive."

    python_root = user_profile / ".o3de" / "Python"
    required_directories = ("packages", "downloaded_packages")
    missing_directories = [name for name in required_directories if not (python_root / name).is_dir()]
    if missing_directories:
        raise ValueError(
            "The installed consumer Python environment is missing: "
            + ", ".join(str(python_root / name) for name in missing_directories)
        )

    local_profile = output_directory / "installed-user"
    if not dry_run:
        for directory_name in required_directories:
            shutil.copytree(
                python_root / directory_name,
                local_profile / ".o3de" / "Python" / directory_name,
                dirs_exist_ok=True,
            )

    environment["USERPROFILE"] = str(local_profile.resolve())
    environment.setdefault("LY_3RDPARTY_PATH", str(user_profile / ".o3de" / "3rdParty"))
    return environment, (
        f"Prepared a same-drive Python environment for installed consumers at {local_profile.resolve()}."
    )


def add_installed_consumer(
    runner: ValidationRunner,
    primary_build_directory: Path,
    core_build_directory: Path,
    install_root: Path,
    consumer_build_directory: Path,
    configuration: str,
    parallel: int,
    monolithic: bool,
    environment: dict[str, str] | None = None,
) -> None:
    permutation = "modular"
    configuration_component = "DEFAULT"
    binary_component = f"DEFAULT_{configuration.upper()}"
    if monolithic:
        permutation = "monolithic"
        configuration_component = "MONOLITHIC"
        binary_component = f"MONOLITHIC_{configuration.upper()}"

    configured = runner.run_command(
        f"configure-installed-{permutation}-engine",
        (
            "cmake",
            "-S",
            str(runner.engine_root),
            "-B",
            str(primary_build_directory),
            "-DLY_PROJECTS:STRING=",
        ),
        3_600,
        environment,
    )
    if not configured:
        runner.skip(f"build-installed-{permutation}-engine", f"{permutation} install configuration failed")
        runner.skip(f"install-{permutation}-core", f"{permutation} install configuration failed")
        runner.skip(f"install-{permutation}-configuration", f"{permutation} install configuration failed")
        runner.skip(f"install-{permutation}-binaries", f"{permutation} install configuration failed")
        runner.skip(f"audit-installed-{permutation}-boundary", f"{permutation} install configuration failed")
        runner.skip(f"configure-installed-{permutation}-consumer", f"{permutation} install configuration failed")
        runner.skip(f"build-installed-{permutation}-consumer", f"{permutation} install configuration failed")
        runner.skip(f"run-installed-{permutation}-consumer", f"{permutation} install configuration failed")
        return

    built = runner.run_command(
        f"build-installed-{permutation}-engine",
        (
            "cmake",
            "--build",
            str(primary_build_directory),
            "--config",
            configuration,
            "--parallel",
            str(parallel),
        ),
        28_800,
        environment,
    )
    if not built:
        runner.skip(f"install-{permutation}-core", f"{permutation} engine build failed")
        runner.skip(f"install-{permutation}-configuration", f"{permutation} engine build failed")
        runner.skip(f"install-{permutation}-binaries", f"{permutation} engine build failed")
        runner.skip(f"audit-installed-{permutation}-boundary", f"{permutation} engine build failed")
        runner.skip(f"configure-installed-{permutation}-consumer", f"{permutation} engine build failed")
        runner.skip(f"build-installed-{permutation}-consumer", f"{permutation} engine build failed")
        runner.skip(f"run-installed-{permutation}-consumer", f"{permutation} engine build failed")
        return

    core_installed = runner.run_command(
        f"install-{permutation}-core",
        (
            "cmake",
            "--install",
            str(core_build_directory),
            "--config",
            configuration,
            "--component",
            "CORE",
            "--prefix",
            str(install_root.resolve()),
        ),
        7_200,
        environment,
    )
    configuration_installed = runner.run_command(
        f"install-{permutation}-configuration",
        (
            "cmake",
            "--install",
            str(primary_build_directory),
            "--config",
            configuration,
            "--component",
            configuration_component,
            "--prefix",
            str(install_root.resolve()),
        ),
        7_200,
        environment,
    )
    binaries_installed = runner.run_command(
        f"install-{permutation}-binaries",
        (
            "cmake",
            "--install",
            str(primary_build_directory),
            "--config",
            configuration,
            "--component",
            binary_component,
            "--prefix",
            str(install_root.resolve()),
        ),
        7_200,
        environment,
    )
    if runner.dry_run and not install_root.is_dir():
        runner.skip(f"audit-installed-{permutation}-boundary", "dry run")
        runner.skip(f"configure-installed-{permutation}-consumer", "dry run")
        runner.skip(f"build-installed-{permutation}-consumer", "dry run")
        runner.skip(f"run-installed-{permutation}-consumer", "dry run")
        return

    if not core_installed or not configuration_installed or not binaries_installed:
        runner.skip(f"audit-installed-{permutation}-boundary", f"{permutation} install failed")
        runner.skip(f"configure-installed-{permutation}-consumer", f"{permutation} install failed")
        runner.skip(f"build-installed-{permutation}-consumer", f"{permutation} install failed")
        runner.skip(f"run-installed-{permutation}-consumer", f"{permutation} install failed")
        return

    boundary_valid = runner.run_check(
        f"audit-installed-{permutation}-boundary",
        lambda engine_root: validate_installed_boundary(
            engine_root,
            install_root,
            configuration,
            monolithic,
        ),
    )
    if not boundary_valid:
        runner.skip(
            f"configure-installed-{permutation}-consumer",
            f"installed {permutation} boundary audit failed",
        )
        runner.skip(
            f"build-installed-{permutation}-consumer",
            f"installed {permutation} boundary audit failed",
        )
        runner.skip(
            f"run-installed-{permutation}-consumer",
            f"installed {permutation} boundary audit failed",
        )
        return

    consumer_environment: dict[str, str] = {}

    def prepare_environment(_: Path) -> str:
        nonlocal consumer_environment
        consumer_environment, message = prepare_installed_consumer_environment(
            runner.output_directory,
            install_root,
            environment or runner.environment,
            runner.dry_run,
        )
        return message

    environment_prepared = runner.run_check(
        f"prepare-installed-{permutation}-consumer-environment",
        prepare_environment,
    )
    if not environment_prepared:
        runner.skip(
            f"configure-installed-{permutation}-consumer",
            f"installed {permutation} consumer environment preparation failed",
        )
        runner.skip(
            f"build-installed-{permutation}-consumer",
            f"installed {permutation} consumer environment preparation failed",
        )
        runner.skip(
            f"run-installed-{permutation}-consumer",
            f"installed {permutation} consumer environment preparation failed",
        )
        return

    add_public_consumer(
        runner,
        primary_build_directory,
        consumer_build_directory,
        install_root.resolve(),
        configuration,
        parallel,
        f"installed-{permutation}",
        monolithic,
        consumer_environment,
    )


def windows_full_matrix(
    clang_root: Path = Path("D:/LLVM/22.1.8"),
) -> tuple[MatrixVariant, ...]:
    common = ("LY_PROJECTS=AutomatedTesting",)
    clang_compiler = (clang_root / "bin" / "clang-cl.exe").as_posix()
    clang_definitions = (
        f"CMAKE_C_COMPILER={clang_compiler}",
        f"CMAKE_CXX_COMPILER={clang_compiler}",
    )
    return (
        MatrixVariant(
            name="clang-unity-modular",
            build_directory_name="cu",
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + ("LY_UNITY_BUILD=ON", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Debug", "Profile", "Release"),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="clang-no-unity",
            build_directory_name="cn",
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + ("LY_UNITY_BUILD=OFF", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Profile",),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="clang-double",
            build_directory_name="cd",
            preset="windows-ninja",
            definitions=common + clang_definitions + ("LY_JOLT_DOUBLE_PRECISION=ON",),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
        MatrixVariant(
            name="clang-diagnostics",
            build_directory_name="cg",
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + (
                "LY_JOLT_ENABLE_BROADPHASE_STATISTICS=ON",
                "LY_JOLT_ENABLE_DEBUG_RENDERING=OFF",
                "LY_JOLT_ENABLE_DETAILED_PROFILING=ON",
                "LY_JOLT_ENABLE_NARROWPHASE_STATISTICS=ON",
                "LY_JOLT_ENABLE_SIMULATION_STATISTICS=ON",
            ),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
        MatrixVariant(
            name="clang-asan",
            build_directory_name="ca",
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + (
                "CMAKE_TRY_COMPILE_CONFIGURATION=Release",
                "LY_BUILD_WITH_ADDRESS_SANITIZER=ON",
            ),
            configurations=("Profile",),
            targets=("AzCore.Tests", "Jolt.Tests"),
        ),
        MatrixVariant(
            name="clang-malloc-accounting",
            build_directory_name="cm",
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + ("AZCORE_USE_MALLOC_SYSTEM_ALLOCATOR=ON",),
            configurations=("Release",),
            targets=("AzCore.Tests", "Jolt.Tests"),
        ),
        MatrixVariant(
            name="clang-monolithic",
            build_directory_name="cmo",
            preset="windows-ninja",
            definitions=common + clang_definitions + ("LY_MONOLITHIC_GAME=ON",),
            configurations=("Release",),
            targets=("Jolt.API", "Jolt.Module"),
            run_tests=False,
        ),
        MatrixVariant(
            name="msvc-unity-modular",
            build_directory_name="mu",
            preset="windows-ninja",
            definitions=common + ("LY_UNITY_BUILD=ON", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Debug", "Profile", "Release"),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="msvc-no-unity",
            build_directory_name="mn",
            preset="windows-ninja",
            definitions=common + ("LY_UNITY_BUILD=OFF", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Profile",),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="msvc-monolithic",
            build_directory_name="mmo",
            preset="windows-ninja",
            definitions=common + ("LY_UNITY_BUILD=ON", "LY_MONOLITHIC_GAME=ON"),
            configurations=("Release",),
            targets=("Jolt.API", "Jolt.Module"),
            run_tests=False,
        ),
    )


def unix_full_matrix() -> tuple[MatrixVariant, ...]:
    preset = "linux-ninja"
    if platform.system() == "Darwin":
        preset = "mac-ninja"
    return (
        MatrixVariant(
            name="native-unity-modular",
            build_directory_name="nu",
            preset=preset,
            definitions=("LY_PROJECTS=AutomatedTesting", "LY_UNITY_BUILD=ON"),
            configurations=("Profile", "Release"),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="native-no-unity",
            build_directory_name="nn",
            preset=preset,
            definitions=("LY_PROJECTS=AutomatedTesting", "LY_UNITY_BUILD=OFF"),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
        MatrixVariant(
            name="native-double",
            build_directory_name="nd",
            preset=preset,
            definitions=("LY_PROJECTS=AutomatedTesting", "LY_JOLT_DOUBLE_PRECISION=ON"),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
    )


def add_full_matrix(
    runner: ValidationRunner,
    matrix_root: Path,
    parallel: int,
    clang_root: Path,
) -> dict[str, MatrixOutcome]:
    variants = unix_full_matrix()
    msvc_environment: dict[str, str] | None = None
    msvc_error = ""
    if platform.system() == "Windows":
        variants = windows_full_matrix(clang_root)
        clang_compiler = clang_root / "bin" / "clang-cl.exe"
        runner.run_command(
            "clang-toolchain-version",
            (str(clang_compiler), "--version"),
            60,
        )
        try:
            msvc_environment = load_msvc_environment(runner.environment)
        except ValueError as exception:
            msvc_error = str(exception)

        def validate_msvc_environment(_: Path) -> str:
            if msvc_error:
                raise ValueError(msvc_error)
            if not msvc_environment:
                raise ValueError("Visual Studio developer environment is unavailable.")
            compiler = shutil.which("cl.exe", path=msvc_environment.get("PATH"))
            return f"Loaded the Visual Studio x64 developer environment from {compiler}."

        runner.run_check("msvc-environment", validate_msvc_environment)

    outcomes: dict[str, MatrixOutcome] = {}
    for variant in variants:
        build_directory = matrix_root / variant.build_directory_name
        command_environment: dict[str, str] | None = None
        if platform.system() == "Windows":
            if not msvc_environment:
                runner.skip(f"configure-{variant.name}", msvc_error)
                for configuration in variant.configurations:
                    runner.skip(
                        f"build-{variant.name}-{configuration.lower()}",
                        "MSVC developer environment is unavailable",
                    )
                    if variant.run_tests:
                        runner.skip(
                            f"test-{variant.name}-{configuration.lower()}",
                            "MSVC developer environment is unavailable",
                        )
                outcomes[variant.name] = MatrixOutcome(build_directory, False)
                continue
            command_environment = msvc_environment
            if variant.name == "clang-asan":
                command_environment = get_clang_address_sanitizer_environment(command_environment)

        configure_command = [
            "cmake",
            "--preset",
            variant.preset,
            "-S",
            str(runner.engine_root),
            "-B",
            str(build_directory),
        ]
        for definition in variant.definitions:
            configure_command.append(f"-D{definition}")
        configured = runner.run_command(
            f"configure-{variant.name}",
            configure_command,
            3_600,
            command_environment,
        )
        outcomes[variant.name] = MatrixOutcome(
            build_directory,
            configured,
            command_environment,
        )

        for configuration in variant.configurations:
            build_name = f"build-{variant.name}-{configuration.lower()}"
            test_name = f"test-{variant.name}-{configuration.lower()}"
            if not configured:
                runner.skip(build_name, f"{variant.name} configuration failed")
                if variant.run_tests:
                    runner.skip(test_name, f"{variant.name} configuration failed")
                continue

            built = runner.run_command(
                build_name,
                (
                    "cmake",
                    "--build",
                    str(build_directory),
                    "--config",
                    configuration,
                    "--target",
                    *variant.targets,
                    "--parallel",
                    str(parallel),
                ),
                14_400,
                command_environment,
            )
            if variant.run_tests:
                if not built:
                    runner.skip(test_name, f"{variant.name} {configuration} build failed")
                    continue
                if variant.name == "clang-malloc-accounting":
                    test_runner = build_directory / "bin" / configuration.lower() / "AzTestRunner.exe"
                    runner.run_command(
                        test_name,
                        (
                            str(test_runner),
                            str(build_directory / "bin" / configuration.lower() / "Jolt.Tests.Gem.dll"),
                            "AzRunUnitTests",
                            "--gtest_filter=AllocatorTests.*",
                        ),
                        1_800,
                        command_environment,
                    )
                    runner.run_command(
                        f"test-{variant.name}-{configuration.lower()}-azcore",
                        (
                            str(test_runner),
                            str(build_directory / "bin" / configuration.lower() / "AzCore.Tests.dll"),
                            "AzRunUnitTests",
                            "--gtest_filter=ChildAllocatorTests.*",
                        ),
                        1_800,
                        command_environment,
                    )
                    continue
                runner.run_command(
                    test_name,
                    (
                        "ctest",
                        "--test-dir",
                        str(build_directory),
                        "-C",
                        configuration,
                        "-R",
                        r"Gem::Jolt\.Tests\.main::TEST_RUN",
                        "--output-on-failure",
                    ),
                    1_800,
                    command_environment,
                )
                if variant.name == "clang-asan":
                    test_runner = build_directory / "bin" / configuration.lower() / "AzTestRunner.exe"
                    runner.run_command(
                        f"test-{variant.name}-{configuration.lower()}-azcore",
                        (
                            str(test_runner),
                            str(build_directory / "bin" / configuration.lower() / "AzCore.Tests.dll"),
                            "AzRunUnitTests",
                            "--gtest_filter=ChildAllocatorTests.*",
                        ),
                        1_800,
                        command_environment,
                    )

    return outcomes


def write_reports(
    output_directory: Path,
    mode: str,
    initial_git_state: dict[str, object],
    final_git_state: dict[str, object],
    results: Iterable[ValidationResult],
) -> None:
    results = list(results)
    metadata = {
        "mode": mode,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "host": {
            "machine": platform.machine(),
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": sys.version,
        },
        "initial_git_state": initial_git_state,
        "final_git_state": final_git_state,
        "results": [dataclasses.asdict(result) for result in results],
    }
    (output_directory / "summary.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    test_suite = xml_tree.Element(
        "testsuite",
        {
            "name": f"JoltQualification.{mode}",
            "tests": str(len(results)),
            "failures": str(sum(result.status == "failed" for result in results)),
            "skipped": str(sum(result.status == "skipped" for result in results)),
            "time": f"{sum(result.duration_seconds for result in results):.6f}",
        },
    )
    for result in results:
        test_case = xml_tree.SubElement(
            test_suite,
            "testcase",
            {"name": result.name, "time": f"{result.duration_seconds:.6f}"},
        )
        if result.status == "failed":
            failure = xml_tree.SubElement(test_case, "failure", {"message": result.message})
            failure.text = result.log_path
        elif result.status == "skipped":
            xml_tree.SubElement(test_case, "skipped", {"message": result.message})
        system_out = xml_tree.SubElement(test_case, "system-out")
        system_out.text = result.log_path
    xml_tree.ElementTree(test_suite).write(
        output_directory / "summary.junit.xml",
        encoding="utf-8",
        xml_declaration=True,
    )


def parse_arguments(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("quick", "review", "full"))
    parser.add_argument("--engine-root", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--configuration", default="Profile", choices=("Debug", "Profile", "Release"))
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--matrix-root", type=Path)
    parser.add_argument("--clang-root", type=Path, default=Path("D:/LLVM/22.1.8"))
    parser.add_argument("--parallel", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--static-only", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    if arguments is None:
        arguments = sys.argv[1:]
    options = parse_arguments(arguments)
    engine_root = options.engine_root
    if engine_root is None:
        engine_root = find_engine_root(Path(__file__).resolve())
    engine_root = engine_root.resolve()

    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_directory = options.output_dir
    if output_directory is None:
        output_directory = engine_root / "build" / "jolt-qualification" / f"{timestamp}-{options.mode}"
    output_directory = ensure_output_is_under_build(engine_root, output_directory)
    output_directory.mkdir(parents=True, exist_ok=True)

    build_directory = options.build_dir
    if build_directory is None:
        build_directory = engine_root / "build" / "windows_jolt_clangcl_ninja"
    if not build_directory.is_absolute():
        build_directory = engine_root / build_directory

    matrix_root = options.matrix_root
    if matrix_root is None:
        matrix_root = engine_root / "build" / "jolt-qualification" / "matrix"
    if not matrix_root.is_absolute():
        matrix_root = engine_root / matrix_root
    matrix_root = ensure_output_is_under_build(engine_root, matrix_root)

    initial_git_state = capture_git_state(engine_root)
    runner = ValidationRunner(engine_root, output_directory, options.dry_run)
    add_static_checks(runner)
    runner.run_command("git-diff-check", ("git", "diff", "--check"), 120)
    add_python_tests(runner)

    if not options.static_only:
        review = options.mode in ("review", "full")
        primary_environment = load_build_environment(runner.environment, build_directory)
        add_primary_build_and_tests(
            runner,
            build_directory,
            options.configuration,
            max(1, options.parallel),
            options.mode,
            primary_environment,
        )
        if review:
            add_performance_qualification(
                runner,
                build_directory,
                max(1, options.parallel),
                options.mode,
                primary_environment,
            )
            add_source_consumer(
                runner,
                build_directory,
                get_source_consumer_build_directory(matrix_root, build_directory),
                options.configuration,
                max(1, options.parallel),
                primary_environment,
            )
        if options.mode == "full":
            matrix_outcomes = add_full_matrix(
                runner,
                matrix_root,
                max(1, options.parallel),
                options.clang_root.resolve(),
            )
            if platform.system() == "Windows":
                modular_outcome = matrix_outcomes["msvc-unity-modular"]
                if modular_outcome.configured:
                    add_installed_consumer(
                        runner,
                        modular_outcome.build_directory,
                        modular_outcome.build_directory,
                        output_directory / "installed-msvc-modular",
                        output_directory / "consumer-installed-msvc-modular",
                        "Release",
                        max(1, options.parallel),
                        False,
                        modular_outcome.environment,
                    )
                else:
                    runner.skip(
                        "installed-msvc-modular-qualification",
                        "MSVC modular matrix configuration failed",
                    )

                monolithic_outcome = matrix_outcomes["msvc-monolithic"]
                if monolithic_outcome.configured and modular_outcome.configured:
                    add_installed_consumer(
                        runner,
                        monolithic_outcome.build_directory,
                        modular_outcome.build_directory,
                        output_directory / "installed-msvc-monolithic",
                        output_directory / "consumer-installed-msvc-monolithic",
                        "Release",
                        max(1, options.parallel),
                        True,
                        monolithic_outcome.environment,
                    )
                else:
                    runner.skip(
                        "installed-msvc-monolithic-qualification",
                        "MSVC modular or monolithic matrix configuration failed",
                    )

    final_git_state = capture_git_state(engine_root)
    if initial_git_state["combined_sha256"] != final_git_state["combined_sha256"]:
        message = "Tracked, staged, or untracked source state changed during validation."
        log_path = output_directory / f"{len(runner.results):03d}-baseline-integrity.log"
        log_path.write_text(message + "\n", encoding="utf-8")
        runner.results.append(
            ValidationResult(
                name="baseline-integrity",
                status="failed",
                duration_seconds=0.0,
                log_path=str(log_path.relative_to(output_directory)),
                message=message,
            )
        )
    else:
        runner.run_check("baseline-integrity", lambda _: "Source baseline remained unchanged.")

    write_reports(output_directory, options.mode, initial_git_state, final_git_state, runner.results)
    failed = [result for result in runner.results if result.status == "failed"]
    print(f"Qualification artifacts: {output_directory}", flush=True)
    if failed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
