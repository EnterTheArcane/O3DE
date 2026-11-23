#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

import yaml

ROOT = Path(__file__).resolve().parents[1]
DESKTOP_PROFILES = ("windows-x64", "windows-a64", "linux-x64", "linux-a64", "macos-a64")
ANDROID_PROFILES = ("android-a64", "android-x64")
ANDROID_BUILD_PROFILES = ("linux-x64", "windows-x64", "macos-a64")
IOS_PROFILES = ("ios-a64", "ios-simulator-a64")
WASM_PROFILES = ("emscripten-w32", "emscripten-w64")
PROFILES = (*DESKTOP_PROFILES, *ANDROID_PROFILES, *IOS_PROFILES, *WASM_PROFILES)
ANDROID_NDK_VERSION = "30.0.16248370"
EMSDK_VERSION = "7.0.1"
SYMBOLS_CONF = "user.package:debug_symbols"


def detect_build_profile():
    system = {"linux": "linux", "win32": "windows", "darwin": "macos"}.get(sys.platform)
    machine = platform.machine().lower()
    arch = {"amd64": "x64", "x86_64": "x64", "arm64": "a64", "aarch64": "a64"}.get(
        machine
    )
    profile = f"{system}-{arch}"
    if profile not in DESKTOP_PROFILES:
        raise ValueError(
            f"Unsupported build machine ({sys.platform}, {machine}). "
            "Specify --build-profile to select a checked-in profile."
        )
    return profile


def symbol_arguments(profile):
    configs = {
        SYMBOLS_CONF: 1,
        "tools.info.package_id:confs+": [SYMBOLS_CONF],
    }
    if profile.startswith("windows-"):
        # MSYS2 treats slash-prefixed arguments as paths.
        flags = ["-Zi", "-FS"]
        linker = "/DEBUG:FULL /OPT:REF /OPT:ICF"
        variables = {
            "CMAKE_POLICY_DEFAULT_CMP0141": "NEW",
            "CMAKE_MSVC_DEBUG_INFORMATION_FORMAT": "ProgramDatabase",
        }
        for target in ("EXE", "SHARED", "MODULE"):
            for config in ("RELEASE", "RELWITHDEBINFO", "PROFILE", "CHECKED"):
                variables[f"CMAKE_{target}_LINKER_FLAGS_{config}_INIT"] = linker
        configs["tools.cmake.cmaketoolchain:extra_variables"] = variables
    else:
        flags = ["-g"]
        if profile in WASM_PROFILES:
            configs["tools.build:exelinkflags+"] = ["-gseparate-dwarf"]
            configs["tools.build:sharedlinkflags+"] = ["-gseparate-dwarf"]
    configs["tools.build:cflags+"] = flags
    configs["tools.build:cxxflags+"] = flags
    return [
        argument
        for name, value in configs.items()
        for argument in ("-c:h", f"{name}={json.dumps(value)}")
    ]


def cache_folder(output, exclusions):
    cache = output / "cache"
    config = cache / "remotes.json"
    if config.exists():
        remotes = json.loads(config.read_text(encoding="utf-8"))["remotes"]
        center = next(
            (remote for remote in remotes if remote["name"] == "conancenter"), {}
        )
        if sorted(center.get("allowed_packages", [])) != exclusions:
            # Conan checks cached recipes before remotes. A change of ownership
            # must not inherit recipes from the prior remote configuration.
            digest = hashlib.sha256("\n".join(exclusions).encode()).hexdigest()
            return output / f"cache-{digest[:12]}"
    return cache


def export_cache(env, archive):
    if not (Path(env["CONAN_HOME"]) / "p").is_dir():
        return

    excluded = set()
    for conandata in (ROOT / "recipes").glob("*/*/conandata.yml"):
        data = yaml.safe_load(conandata.read_text(encoding="utf-8"))
        if data and data.get("ci_cache") is False:
            excluded.add(conandata.parents[1].name)

    packages = json.loads(
        subprocess.check_output(["conan", "list", "*#*:*#*", "--format=json"], env=env)
    )
    for ref in tuple(packages["Local Cache"]):
        if ref.split("/", 1)[0] in excluded:
            del packages["Local Cache"][ref]

    with TemporaryDirectory(dir=archive.parent) as directory:
        package_list = Path(directory) / "packages.json"
        package_list.write_text(json.dumps(packages), encoding="utf-8")
        temporary_archive = Path(directory) / archive.name
        subprocess.run(
            [
                "conan",
                "cache",
                "save",
                "--no-source",
                "--list",
                package_list,
                "--file",
                temporary_archive,
            ],
            env=env,
            check=True,
        )
        temporary_archive.replace(archive)


def test_packages(conan, stage, deployed, *arguments, android=False, wasm=False):
    packages = json.loads((deployed / "packages.json").read_text(encoding="utf-8"))
    references = {name: package["identity"] for name, package in packages.items()}
    if android:
        references["android-ndk"] = f"android-ndk/{ANDROID_NDK_VERSION}"
    if wasm:
        references["emsdk"] = f"emsdk/{EMSDK_VERSION}"
    for name, reference in references.items():
        test = ROOT / "recipes" / name / "all" / "test_package"
        if (test / "conanfile.py").is_file():
            conan(
                "test",
                test,
                reference.split(":", 1)[0],
                *arguments,
                "--build=never",
                "--no-remote",
                "-c:h",
                f"tools.cmake.cmake_layout:test_folder={stage / 'tests' / name}",
            )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "command",
        choices=("lock", "build", "android-ndk", "build-profile", "export-cache"),
    )
    parser.add_argument("profile", choices=PROFILES, nargs="?")
    parser.add_argument("--build-profile", choices=DESKTOP_PROFILES)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cache-archive", type=Path)
    parser.add_argument(
        "--deploy",
        choices=("oci", "archive", "both"),
        default="oci",
        help="Package output format for builds (default: oci)",
    )
    args = parser.parse_intermixed_args()
    try:
        native_profile = args.build_profile or detect_build_profile()
    except ValueError as error:
        parser.error(str(error))
    if args.command == "build-profile":
        print(native_profile)
        return
    if args.command == "export-cache":
        if args.output is None or args.cache_archive is None:
            parser.error("export-cache requires --output and --cache-archive")
    elif not args.profile or args.output is None:
        parser.error(f"{args.command} requires a target profile and --output")

    android = args.profile in ANDROID_PROFILES
    if args.command == "android-ndk" and not android:
        parser.error("android-ndk requires an Android profile")
    apple = args.profile in IOS_PROFILES or args.profile == "macos-a64"
    wasm = args.profile in WASM_PROFILES
    if args.command == "build" and apple and sys.platform != "darwin":
        parser.error("The Apple profiles require Xcode on macOS")
    if args.command == "build" and wasm and sys.platform not in ("linux", "darwin"):
        parser.error("The Emscripten profiles require Linux x64 or Apple Silicon")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    local_packages = sorted(
        f"{recipe.parent.name}/*" for recipe in (ROOT / "recipes").glob("*/config.yml")
    )
    exclusions = [f"~{package}" for package in local_packages]
    env = dict(os.environ, CONAN_HOME=str(cache_folder(output, exclusions)))
    env.pop("CONAN_USER_HOME", None)
    if args.command == "export-cache":
        export_cache(env, args.cache_archive.resolve())
        return

    profile = ROOT / "profiles" / args.profile
    # Older caches may still contain the pruning hook.
    env.pop("O3DE_CONAN_PRUNE_BUILD", None)
    toolchain_settings = []
    build_profile = ROOT / "profiles" / native_profile
    if apple and sys.platform == "darwin":
        compiler_version = (
            subprocess.check_output(["xcrun", "clang", "-dumpversion"], text=True)
            .strip()
            .partition(".")[0]
        )
        toolchain_settings.extend(("-s:a", f"compiler.version={compiler_version}"))
    if (
        (android or wasm)
        and args.command == "build"
        and build_profile.name == "macos-a64"
        and sys.platform == "darwin"
    ):
        compiler_version = (
            subprocess.check_output(["xcrun", "clang", "-dumpversion"], text=True)
            .strip()
            .partition(".")[0]
        )
        toolchain_settings.extend(("-s:b", f"compiler.version={compiler_version}"))

    def conan(*command):
        subprocess.run(["conan", *map(str, command)], cwd=ROOT, env=env, check=True)

    conan(
        "remote",
        "add",
        "o3de",
        ROOT,
        "-t",
        "local-recipes-index",
        "--index",
        0,
        "--force",
        *(f"--allowed-packages={package}" for package in local_packages),
    )
    conan(
        "remote",
        "add",
        "conancenter",
        "https://center2.conan.io",
        "--index",
        1,
        "--force",
        *(f"--allowed-packages={pattern}" for pattern in exclusions),
    )

    if args.cache_archive and args.cache_archive.is_file():
        conan("cache", "restore", args.cache_archive.resolve())

    if args.command == "build":
        conan(
            "config",
            "install",
            ROOT / "extensions" / "hooks",
            "--target-folder",
            "extensions/hooks",
        )
        if (
            args.cache_archive
            and args.profile == "linux-a64"
            and build_profile != profile
        ):
            toolchain_settings.extend(("-c:a", "user.package:prune_build=True"))

    configurations = ["Release"]
    if args.profile.startswith("windows-"):
        configurations.append("Debug")

    def profile_arguments(config, native_profile):
        common = [
            "-pr:b",
            native_profile,
            "-pr:h",
            profile,
            "-s:h",
            f"build_type={config}",
            *toolchain_settings,
            *symbol_arguments(args.profile),
        ]
        if config == "Debug":
            common.extend(("-s:h", "compiler.runtime_type=Debug"))
        if android and native_profile.name.startswith("windows-"):
            common.extend(("-c:h", "tools.microsoft.bash:subsystem=msys2"))
        return common

    lock = ROOT / "locks" / f"{args.profile}.lock"
    remotes = ["--build=missing", "-r", "o3de", "-r", "conancenter"]
    if args.command == "android-ndk":
        conan(
            "install",
            f"--tool-requires=android-ndk/{ANDROID_NDK_VERSION}",
            "-pr:b",
            build_profile,
            "-pr:h",
            profile,
            "--lockfile",
            lock,
            "--build=missing",
            "-r",
            "o3de",
            "--output-folder",
            output / "release",
        )
        return
    if args.command == "lock":
        lock.parent.mkdir(exist_ok=True)
        build_profiles = [build_profile]
        if android:
            build_profiles = [
                ROOT / "profiles" / name for name in ANDROID_BUILD_PROFILES
            ]
        if wasm:
            build_profiles = [
                ROOT / "profiles" / name for name in ("linux-x64", "macos-a64")
            ]
        with TemporaryDirectory(dir=lock.parent) as directory:
            candidates = []
            for native_profile in build_profiles:
                for config in configurations:
                    candidate = (
                        Path(directory) / f"{native_profile.name}-{config.lower()}.lock"
                    )
                    conan(
                        "lock",
                        "create",
                        ROOT,
                        *profile_arguments(config, native_profile),
                        *remotes,
                        "--update",
                        "--lockfile=",
                        "--lockfile-out",
                        candidate,
                    )
                    candidates.append(candidate)
            merged = candidates[0]
            if len(candidates) > 1:
                merged = Path(directory) / lock.name
                conan(
                    "lock",
                    "merge",
                    *(
                        arg
                        for candidate in candidates
                        for arg in ("--lockfile", candidate)
                    ),
                    "--lockfile-out",
                    merged,
                )
            merged.replace(lock)
        return

    deployer_dir = ROOT / "extensions" / "deployers"
    deployers = []
    if args.deploy in ("oci", "both"):
        deployers.extend(("--deployer", deployer_dir / "oci_deployer.py"))
    if args.deploy in ("archive", "both"):
        deployers.extend(("--deployer", deployer_dir / "archive_deployer.py"))
    for config in configurations:
        stage = output / config.lower()
        packages = stage / "packages"
        locked = [*profile_arguments(config, build_profile), "--lockfile", lock]
        conan(
            "install",
            ROOT,
            *locked,
            *remotes,
            "--output-folder",
            stage,
            *deployers,
            "--deployer-folder",
            packages,
        )
        deployed = packages / "archives" if args.deploy == "archive" else packages
        test_packages(conan, stage, deployed, *locked, android=android, wasm=wasm)


if __name__ == "__main__":
    main()
