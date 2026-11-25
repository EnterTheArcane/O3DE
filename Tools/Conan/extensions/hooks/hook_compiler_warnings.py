# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT


def pre_generate(conanfile):
    compiler = conanfile.settings.get_safe("compiler")
    if compiler not in ("gcc", "clang", "apple-clang", "emcc", "msvc"):
        return

    conanfile.buildenv.append("NVCC_APPEND_FLAGS", ["-w", "-Xcompiler=-w"])
    conanfile.buildenv.append("ISPCFLAGS", "--woff")

    os_name = conanfile.settings.get_safe("os")
    if compiler == "msvc":
        # MSVC uses the last warning level on its command line.
        conanfile.buildenv.append("_CL_", "-w")
        return

    for language in ("c", "cxx"):
        conanfile.conf.append(f"tools.build:{language}flags", "-w")

    if (
        compiler == "clang"
        and os_name == "Windows"
        and conanfile.settings.get_safe("compiler.runtime")
    ):
        # clang-cl uses -w even before -Wall, and CMake may end options with --.
        conanfile.buildenv.append("CL", "-w")
    else:
        # Direct Configure and Make builds may only read the environment.
        for variable in ("CFLAGS", "CXXFLAGS"):
            conanfile.buildenv.append(variable, "-w")

    if os_name in ("Macos", "iOS"):
        # Some configure scripts replace CFLAGS but still honor CPPFLAGS.
        for variable in ("CPPFLAGS", "OBJCFLAGS", "OBJCXXFLAGS"):
            conanfile.buildenv.append(variable, "-w")
