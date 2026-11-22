#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

import argparse
from pathlib import Path
import shlex
import subprocess
import sys

from build import ANDROID_PROFILES, DESKTOP_PROFILES, IOS_PROFILES, PROFILES, ROOT


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", required=True, type=Path)
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--python", type=Path)
    parser.add_argument(
        "--cross", action="store_true", help="Compile and link the target probes"
    )
    profiles = parser.add_mutually_exclusive_group()
    profiles.add_argument("--profile", choices=PROFILES)
    profiles.add_argument("--android-profile", choices=ANDROID_PROFILES)
    parser.add_argument("--ndk", type=Path)
    parser.add_argument(
        "--prebuilt", action="store_true", help="Also check the other desktop OCI SDKs"
    )
    parser.add_argument("--cmake-driver", help="Optional local CMake driver invocation")
    args = parser.parse_args()
    profile = args.profile or args.android_profile
    android = profile in ANDROID_PROFILES
    desktop = profile in DESKTOP_PROFILES or profile is None
    if android and not args.ndk:
        parser.error("--ndk is required for Android verification")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests" / "oci_consumer"
    fixture = output / source.name
    driver = shlex.split(args.cmake_driver) if args.cmake_driver else []

    def cmake(operation, *arguments):
        command = {
            "configure": ["cmake"],
            "build": ["cmake", "--build"],
            "test": ["ctest"],
        }[operation]
        if driver:
            command = [*driver, operation]
        subprocess.run([*command, *map(str, arguments)], check=True)

    initial_cache = output / "inputs.cmake"
    inputs = {
        "LY_PACKAGE_DOWNLOAD_CACHE_LOCATION": args.cache,
    }
    if android:
        inputs["LY_NDK_DIR"] = args.ndk
    if desktop:
        inputs["O3DE_IMAGING_OCI_CATALOG"] = args.catalog
        if args.python:
            inputs["Python3_EXECUTABLE"] = args.python
    if args.prebuilt or not desktop:
        inputs["O3DE_PREBUILT_OCI_CATALOG"] = args.catalog
    initial_cache.write_text(
        f"set(O3DE_OCI_CROSS {'ON' if args.cross else 'OFF'})\n"
        + "".join(
            f'set({key} [==[{path.resolve().as_posix()}]==] CACHE FILEPATH "" FORCE)\n'
            for key, path in inputs.items()
        ),
        encoding="utf-8",
    )
    configure = ["--preset", profile] if profile else ["-G", "Ninja Multi-Config"]
    cmake(
        "configure",
        "-S",
        source,
        "-B",
        fixture,
        *configure,
        "-C",
        initial_cache,
    )
    if android:
        for config in ("Debug", "Profile", "Release"):
            cmake("build", fixture, "--config", config)
        return
    configs = ["Release"]
    if profile in ("windows-x64", "windows-a64") or (
        profile is None and sys.platform == "win32"
    ):
        configs.append("Debug")
    for config in configs:
        cmake("build", fixture, "--config", config)
        if not args.cross and profile not in IOS_PROFILES:
            cmake("test", "--test-dir", fixture, "-C", config, "--output-on-failure")


if __name__ == "__main__":
    main()
