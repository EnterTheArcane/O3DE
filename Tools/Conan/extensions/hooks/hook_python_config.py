# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT

from pathlib import Path

from conan.tools.files import save


def pre_generate(conanfile):
    python = next(
        (
            dep
            for dep in conanfile.dependencies.host.values()
            if dep.ref.name == "python"
        ),
        None,
    )
    if not python or not python.package_folder:
        return

    sdk = Path(python.package_folder)
    if not (sdk / "PythonConfig.cmake").is_file():
        return

    lines = ["include_guard(GLOBAL)"]
    build_python = next(
        (
            dep
            for dep in conanfile.dependencies.build.values()
            if dep.ref.name == "python"
        ),
        None,
    )
    if build_python and build_python.package_folder and build_python.cpp_info.bindirs:
        bindir = Path(build_python.package_folder) / build_python.cpp_info.bindirs[0]
        executables = [("EXECUTABLE", "python3")]
        if conanfile.settings_build.os == "Windows":
            executables = [
                ("EXECUTABLE", "python.exe"),
                ("EXECUTABLE_DEBUG", "python_d.exe"),
            ]
        for variable, file in executables:
            interpreter = bindir / file
            if not interpreter.is_file():
                continue
            for name in ("Python", "Python3"):
                lines += [
                    f"if(NOT {name}_{variable})",
                    f'    set({name}_{variable} [==[{interpreter.as_posix()}]==] CACHE FILEPATH "Python build interpreter")',
                    "endif()",
                ]

    # Redirect just Python. Config preference for unrelated packages is left to the project.
    lines.append("if(DEFINED CMAKE_FIND_PACKAGE_REDIRECTS_DIR)")
    for name in ("Python", "Python3"):
        for suffix in ("Config.cmake", "ConfigVersion.cmake"):
            file = name + suffix
            lines.append(
                f'    file(WRITE "${{CMAKE_FIND_PACKAGE_REDIRECTS_DIR}}/{file}" '
                f'[==[include("{(sdk / file).as_posix()}")\n]==])'
            )
    lines.append("endif()")

    cmake = Path(conanfile.generators_folder) / "python_sdk.cmake"
    save(conanfile, cmake, "\n".join(lines) + "\n")
    conanfile.conf.append("tools.cmake.cmaketoolchain:user_toolchain", cmake.as_posix())
    conanfile.buildenv.define_path("CONAN_PYTHON_SDK_CMAKE", cmake.as_posix())
