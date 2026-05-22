# Ported from conan-center-index/physx by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMakeToolchain, CMake
from thirdparty.tools.files import (
    load,
    get,
    apply_patches,
    rmdir,
    copy,
    replace_in_file,
    save,
)
from thirdparty.tools.microsoft import msvc_runtime_flag, is_msvc
import os


class Recipe(RecipeBase):
    name = "physx"
    version = "4.1.2"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "release_build_type": ["profile", "release"],
        "enable_simd": [True, False],
        "enable_float_point_precise_math": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "release_build_type": "release",
        "enable_simd": True,
        "enable_float_point_precise_math": False,
    }

    def source(self):
        get(
            url="https://github.com/NVIDIAGameWorks/PhysX/archive/a2c0428acab643e60618c681b501e86f7fd558cc.zip",
            sha256="d9c1939490a990277f8c773f288294cecb10e6fad8c820acad90fd4168b8ace3",
            dest=self.source_folder,
            strip_root=True,
        )
        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)

        # Needed. See https://github.com/conan-io/conan-center-index/pull/16583#discussion_r1188548265 for details
        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get(
            "fPIC", True
        )

        # Options defined in physx/compiler/public/CMakeLists.txt
        tc.cache_variables["TARGET_BUILD_PLATFORM"] = self._get_target_build_platform()
        tc.cache_variables["PX_BUILDSNIPPETS"] = False
        tc.cache_variables["PX_BUILDPUBLICSAMPLES"] = False
        tc.cache_variables["PX_CMAKE_SUPPRESS_REGENERATION"] = False
        cmakemodules_abspath = os.path.join(
            self.build_folder,
            self.source_folder,
            "externals",
            self._get_cmakemodules_subfolder(),
        )
        tc.cache_variables["CMAKEMODULES_PATH"] = cmakemodules_abspath.replace(
            "\\", "/"
        )
        tc.cache_variables["CMAKEMODULES_VERSION"] = (
            "1.27"  # Prevents cmake from overwriting CMAKEMODULES_PATH with empty env var
        )
        tc.cache_variables["PHYSX_ROOT_DIR"] = os.path.join(
            self.source_folder, "physx"
        ).replace("\\", "/")

        # Options defined in physx/source/compiler/cmake/CMakeLists.txt
        if self.is_windows:
            tc.cache_variables["PX_SCALAR_MATH"] = (
                not self.options.enable_simd
            )  # this value doesn't matter on other os
        tc.cache_variables["PX_GENERATE_STATIC_LIBRARIES"] = not self.options.shared
        tc.cache_variables["PX_EXPORT_LOWLEVEL_PDB"] = False
        tc.cache_variables["PXSHARED_PATH"] = os.path.join(
            self.source_folder, "pxshared"
        ).replace("\\", "/")
        tc.cache_variables["PXSHARED_INSTALL_PREFIX"] = self.package_folder.replace(
            "\\", "/"
        )
        tc.cache_variables["PX_GENERATE_SOURCE_DISTRO"] = False

        # Options defined in externals/cmakemodules/NVidiaBuildOptions.cmake
        tc.cache_variables["NV_APPEND_CONFIG_NAME"] = False
        tc.cache_variables["NV_USE_GAMEWORKS_OUTPUT_DIRS"] = False
        if self.is_windows:
            tc.cache_variables["NV_USE_STATIC_WINCRT"] = "MT" in msvc_runtime_flag(self)
            tc.cache_variables["NV_USE_DEBUG_WINCRT"] = "d" in msvc_runtime_flag(self)
        tc.cache_variables["NV_FORCE_64BIT_SUFFIX"] = False
        tc.cache_variables["NV_FORCE_32BIT_SUFFIX"] = False
        tc.cache_variables["PX_ROOT_LIB_DIR"] = os.path.join(
            self.package_folder, "lib"
        ).replace("\\", "/")

        if self.is_windows:
            # Options defined in physx/source/compiler/cmake/windows/CMakeLists.txt
            tc.cache_variables["PX_COPY_EXTERNAL_DLL"] = (
                False  # External dll copy disabled, PhysXDevice dll copy is handled afterward during conan packaging
            )
            tc.cache_variables["PX_FLOAT_POINT_PRECISE_MATH"] = (
                self.options.enable_float_point_precise_math
            )
            tc.cache_variables["PX_USE_NVTX"] = (
                False  # Could be controlled by an option if NVTX had a recipe, disabled for the moment
            )
            tc.cache_variables["GPU_DLL_COPIED"] = (
                True  # PhysXGpu dll copy disabled, this copy is handled afterward during conan packaging
            )

            # Options used in physx/source/compiler/cmake/windows/PhysX.cmake
            tc.cache_variables["PX_GENERATE_GPU_PROJECTS"] = False

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=os.path.join(
                self.source_folder, "physx/compiler/public"
            )
        )
        cmake.build(build_type=self._get_physx_build_type())

    def _get_cmakemodules_subfolder(self):
        return "CMakeModules" if self.is_windows else "cmakemodules"

    def _patch_sources(self):
        apply_patches(self)

        # There is no reason to force consumer of PhysX public headers to use one of
        # NDEBUG or _DEBUG, since none of them relies on NDEBUG or _DEBUG
        replace_in_file(
            os.path.join(
                self.source_folder,
                "pxshared",
                "include",
                "foundation",
                "PxPreprocessor.h",
            ),
            "#error Exactly one of NDEBUG and _DEBUG needs to be defined!",
            "// #error Exactly one of NDEBUG and _DEBUG needs to be defined!",
        )

        physx_source_cmake_dir = os.path.join(
            self.source_folder, "physx", "source", "compiler", "cmake"
        )

        # Remove global and specifics hard-coded PIC settings
        # (conan's CMake build helper properly sets CMAKE_POSITION_INDEPENDENT_CODE
        # depending on options)
        replace_in_file(
            os.path.join(physx_source_cmake_dir, "CMakeLists.txt"),
            "SET(CMAKE_POSITION_INDEPENDENT_CODE ON)",
            "",
        )
        for cmake_file in (
            "FastXml.cmake",
            "LowLevel.cmake",
            "LowLevelAABB.cmake",
            "LowLevelDynamics.cmake",
            "PhysX.cmake",
            "PhysXCharacterKinematic.cmake",
            "PhysXCommon.cmake",
            "PhysXCooking.cmake",
            "PhysXExtensions.cmake",
            "PhysXFoundation.cmake",
            "PhysXPvdSDK.cmake",
            "PhysXTask.cmake",
            "PhysXVehicle.cmake",
            "SceneQuery.cmake",
            "SimulationController.cmake",
        ):
            target, _ = os.path.splitext(os.path.basename(cmake_file))
            replace_in_file(
                os.path.join(physx_source_cmake_dir, cmake_file),
                f"SET_TARGET_PROPERTIES({target} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)",
                "",
            )

        # No error for compiler warnings
        replace_in_file(
            os.path.join(physx_source_cmake_dir, "windows", "CMakeLists.txt"), "/WX", ""
        )
        for cmake_os in ("linux", "mac", "android", "ios"):
            replace_in_file(
                os.path.join(physx_source_cmake_dir, cmake_os, "CMakeLists.txt"),
                "-Werror",
                "",
            )

    def _get_physx_build_type(self):
        if self.build_type == "Debug":
            return "debug"
        elif self.build_type == "RelWithDebInfo":
            return "checked"
        elif self.build_type == "Release":
            if self.options.release_build_type == "profile":
                return "profile"
            else:
                return "release"

    def _get_target_build_platform(self):
        if self.is_windows:
            return "windows"
        elif self.is_macos:
            return "mac"
        elif self.is_linux:
            return "linux"
        return "windows"

    def package(self):
        cmake = CMake(self)
        cmake.install(build_type=self._get_physx_build_type())

        save(
            os.path.join(self.package_folder, "licenses", "LICENSE"),
            self._get_license(),
        )

        cmake_installation_dir = os.path.join(
            self.package_folder, "lib", self._get_physx_build_type()
        )
        package_dst_lib_dir = os.path.join(self.package_folder, "lib")
        package_dst_bin_dir = os.path.join(self.package_folder, "bin")

        copy(
            pattern="*.a",
            dst=package_dst_lib_dir,
            src=cmake_installation_dir,
            keep_path=False,
        )
        copy(
            pattern="*.so",
            dst=package_dst_lib_dir,
            src=cmake_installation_dir,
            keep_path=False,
        )
        copy(
            pattern="*.dylib*",
            dst=package_dst_lib_dir,
            src=cmake_installation_dir,
            keep_path=False,
        )
        copy(
            pattern="*.lib",
            dst=package_dst_lib_dir,
            src=cmake_installation_dir,
            keep_path=False,
        )
        copy(
            pattern="*.dll",
            dst=package_dst_bin_dir,
            src=cmake_installation_dir,
            keep_path=False,
        )

        rmdir(os.path.join(self.package_folder, "source"))
        rmdir(cmake_installation_dir)

        self._copy_external_bin()

    def _get_license(self):
        readme = load(os.path.join(self.source_folder, "README.md"))
        begin = readme.find("Copyright")
        end = readme.find("\n## Introduction", begin)
        return readme[begin:end]

    def _copy_external_bin(self):
        # For Windows and Linux 64 bits, PhysXGpu (and PhysXDevice on Windows)
        # precompiled shared libs must also be provided to end-user if
        # application uses GPU features.
        external_bin_dir = os.path.join(self.source_folder, "physx", "bin")
        physx_build_type = self._get_physx_build_type()
        compiler_version = "193"  # VS 2026 / MSVC 19.3x -> maps to vc142

        if self.is_linux and False:  # Linux x86_64 GPU support -- not used on Windows
            package_dst_lib_dir = os.path.join(self.package_folder, "lib")
            physx_gpu_dir = os.path.join(
                external_bin_dir, "linux.clang", physx_build_type
            )
            copy(
                pattern="*PhysXGpu*.so",
                dst=package_dst_lib_dir,
                src=physx_gpu_dir,
                keep_path=False,
            )
        elif self.is_windows and self.is_windows:
            physx_arch = "x86_64"
            dll_info_list = [
                {"pattern": "PhysXGpu*.dll", "vc_ver": "vc140"},  # x86_64
                {
                    "pattern": "PhysXDevice*.dll",
                    "vc_ver": {"180": "vc120", "190": "vc140", "191": "vc141"}.get(
                        str(compiler_version), "vc142"
                    ),
                },
            ]

            package_dst_bin_dir = os.path.join(self.package_folder, "bin")

            for dll_info in dll_info_list:
                dll_subdir = "win.{0}.{1}.mt".format(physx_arch, dll_info.get("vc_ver"))
                dll_dir = os.path.join(external_bin_dir, dll_subdir, physx_build_type)
                copy(
                    pattern=dll_info.get("pattern"),
                    dst=package_dst_bin_dir,
                    src=dll_dir,
                    keep_path=False,
                )
