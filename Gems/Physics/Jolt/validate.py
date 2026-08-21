"""Run reproducible Jolt qualification checks from one cross-platform entry point."""

from __future__ import annotations

import argparse
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
    "Jolt_Characters",
    "Jolt_Constraints",
    "Jolt_Diagnostics",
    "Jolt_EventsAndFilters",
    "Jolt_Hair",
    "Jolt_Queries",
    "Jolt_RagdollsAndSkeletons",
    "Jolt_RollbackAndDeterminism",
    "Jolt_SavedFeatureGallery",
    "Jolt_ScenesAndAssets",
    "Jolt_ShapesAndCooking",
    "Jolt_SoftBodies",
    "Jolt_StressAndSoak",
    "Jolt_Vehicles",
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
        return base_environment.copy()

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
    registered = set(re.findall(r"from \.tests import (Jolt_[A-Za-z0-9_]+)", suite_text + benchmark_text))
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


def validate_private_native_boundary(engine_root: Path) -> str:
    gem_root = engine_root / "Gems" / "Physics" / "Jolt"
    native_cmake = (gem_root / "3rdParty" / "JoltNative.cmake").read_text(encoding="utf-8")
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

    gpu_terms = ("Atom_RHI", "Atom_RPI", "JPH_USE_DX12 ON", "JPH_USE_MTL ON", "JPH_USE_VK ON")
    for term in gpu_terms:
        if term in native_code or term in target_code:
            errors.append(f"obsolete GPU dependency or backend remains: {term}")

    if errors:
        raise ValueError("; ".join(errors))
    return "Validated private native target, license-only install, and CPU-only dependency boundary."


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

    cmake_text = (consumer_root / "CMakeLists.txt").read_text(encoding="utf-8")
    if "Gem::Jolt.API" not in cmake_text:
        errors.append("public consumer does not link Gem::Jolt.API")
    if "O3DE_ENGINE_ROOT" not in cmake_text:
        errors.append("public consumer cannot select a source or installed engine")

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

    permutation = "Default"
    if monolithic:
        permutation = "Monolithic"
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
    runner.run_check("private-native-boundary", validate_private_native_boundary)
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


def add_primary_build_and_tests(
    runner: ValidationRunner,
    build_directory: Path,
    configuration: str,
    parallel: int,
    review: bool,
) -> None:
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
    )
    if review:
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
    )


def add_installed_consumer(
    runner: ValidationRunner,
    primary_build_directory: Path,
    install_root: Path,
    consumer_build_directory: Path,
    configuration: str,
    parallel: int,
    monolithic: bool,
    environment: dict[str, str] | None = None,
) -> None:
    permutation = "modular"
    binary_component = f"DEFAULT_{configuration.upper()}"
    if monolithic:
        permutation = "monolithic"
        binary_component = f"MONOLITHIC_{configuration.upper()}"

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
            str(primary_build_directory),
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

    if not core_installed or not binaries_installed:
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

    add_public_consumer(
        runner,
        primary_build_directory,
        consumer_build_directory,
        install_root.resolve(),
        configuration,
        parallel,
        f"installed-{permutation}",
        monolithic,
        environment,
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
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + ("LY_UNITY_BUILD=ON", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Debug", "Profile", "Release"),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="clang-no-unity",
            preset="windows-ninja",
            definitions=common
            + clang_definitions
            + ("LY_UNITY_BUILD=OFF", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Profile",),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="clang-double",
            preset="windows-ninja",
            definitions=common + clang_definitions + ("LY_JOLT_DOUBLE_PRECISION=ON",),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
        MatrixVariant(
            name="clang-diagnostics",
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
            preset="windows-ninja",
            definitions=common + clang_definitions + ("LY_BUILD_WITH_ADDRESS_SANITIZER=ON",),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
        MatrixVariant(
            name="clang-monolithic",
            preset="windows-ninja",
            definitions=common + clang_definitions + ("LY_MONOLITHIC_GAME=ON",),
            configurations=("Release",),
            targets=("Jolt.API", "Jolt.Module"),
            run_tests=False,
        ),
        MatrixVariant(
            name="msvc-unity-modular",
            preset="windows-ninja",
            definitions=common + ("LY_UNITY_BUILD=ON", "LY_MONOLITHIC_GAME=OFF"),
            configurations=("Debug", "Profile", "Release"),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="msvc-monolithic",
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
            preset=preset,
            definitions=("LY_PROJECTS=AutomatedTesting", "LY_UNITY_BUILD=ON"),
            configurations=("Profile", "Release"),
            targets=("Jolt.Tests", "Jolt.Module", "Jolt.Editor"),
        ),
        MatrixVariant(
            name="native-no-unity",
            preset=preset,
            definitions=("LY_PROJECTS=AutomatedTesting", "LY_UNITY_BUILD=OFF"),
            configurations=("Profile",),
            targets=("Jolt.Tests",),
        ),
        MatrixVariant(
            name="native-double",
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
        build_directory = matrix_root / variant.name
        command_environment: dict[str, str] | None = None
        if variant.name.startswith("msvc-"):
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
        "generated_at_utc": datetime.datetime.now(datetime.UTC).isoformat(),
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

    timestamp = datetime.datetime.now(datetime.UTC).strftime("%Y%m%dT%H%M%SZ")
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
        add_primary_build_and_tests(
            runner,
            build_directory,
            options.configuration,
            max(1, options.parallel),
            review,
        )
        if review:
            add_source_consumer(
                runner,
                build_directory,
                matrix_root / "consumer-source",
                options.configuration,
                max(1, options.parallel),
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
                if monolithic_outcome.configured:
                    add_installed_consumer(
                        runner,
                        monolithic_outcome.build_directory,
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
                        "MSVC monolithic matrix configuration failed",
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
