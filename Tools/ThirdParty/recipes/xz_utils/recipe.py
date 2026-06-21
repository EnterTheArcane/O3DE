import os
import textwrap

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import copy, get, replace_in_file, rmdir, save
from thirdparty.microsoft import check_min_vs, is_msvc_static_runtime, MSBuild, MSBuildToolchain
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "xz_utils"
    version = "5.8.3"
    license = "Unlicense", "LGPL-2.1-or-later",  "GPL-2.0-or-later", "GPL-3.0-or-later"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tools": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": False,
    }

    @property
    def _effective_msbuild_type(self):
        # treat "RelWithDebInfo" and "MinSizeRel" as "Release"
        # there is no DebugMT configuration in upstream vcxproj, we patch Debug configuration afterwards
        return "{}{}".format(
            "Debug" if self.settings.build_type == "Debug" else "Release",
            "MT" if is_msvc_static_runtime(self) and self.settings.build_type != "Debug" else "",
        )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def build_requirements(self):
        if self.settings_build.os == "Windows" and self.settings.os == "Android":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")
        self.tool_requires("cmake")

    def latest_version(self):
        repo = GithubRepository(self, "tukaani-project/xz")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://tukaani.org/xz/xz-5.8.3.tar.xz",
            sha256="fff1ffcf2b0da84d308a14de513a1aa23d4e9aa3464d17e64b9714bfdd0bbfb6",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        if not self.options.with_tools:
            tc.cache_variables["XZ_TOOL_XZ"] = False
            tc.cache_variables["XZ_TOOL_XZDEC"] = False
            tc.cache_variables["XZ_TOOL_LZMADEC"] = False
            tc.cache_variables["XZ_TOOL_LZMAINFO"] = False
            tc.cache_variables["ENABLE_SCRIPTS"] = False
            tc.cache_variables["XZ_TOOL_SYMLINKS"] = False
            tc.cache_variables["XZ_TOOL_SYMLINKS_LZMA"] = False
            tc.cache_variables["XZ_DOC"] = False
            # sandbox should only apply to the tools, so if the tools are enabled
            # the sandboxing features will be enabled.
            tc.cache_variables["XZ_SANDBOX"] = "no"
        tc.generate()
        if self.settings.build_type == "Debug":
            tc.configure_args.append("--enable-debug")
        tc.generate()

    def _build_msvc(self):
        is_msvc_modern = check_min_vs(self, 191)
        build_script_folder = os.path.join(self.source_folder, "windows", "vs2017" if is_msvc_modern else "vs2013")

        #==============================
        # TODO: to remove once upstream PR 12817 available in recipe client.
        vcxproj_files = [
            os.path.join(build_script_folder, "liblzma.vcxproj"),
            os.path.join(build_script_folder, "liblzma_dll.vcxproj"),
        ]
        old_toolset = "v141" if is_msvc_modern else "v120"
        new_toolset = MSBuildToolchain(self).toolset
        recipe_toolchain_props = os.path.join(self.generators_folder, MSBuildToolchain.filename)
        for vcxproj_file in vcxproj_files:
            replace_in_file(
                self, vcxproj_file,
                f"<PlatformToolset>{old_toolset}</PlatformToolset>",
                f"<PlatformToolset>{new_toolset}</PlatformToolset>",
            )
            replace_in_file(
                self, vcxproj_file,
                "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />",
                f"<Import Project=\"{recipe_toolchain_props}\" /><Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />",
            )

            if self.settings.arch == "ARM":
                replace_in_file(self, vcxproj_file, "x64", "ARM64")

        solution_file = os.path.join(build_script_folder, "xz_win.sln")
        if self.settings.arch == "ARM":
            replace_in_file(self, solution_file, "x64", "ARM64")

        #==============================

        msbuild = MSBuild(self)
        msbuild.build_type = self._effective_msbuild_type
        msbuild.platform = "Win32" if self.settings.arch == "x86" else msbuild.platform
        msbuild.build(os.path.join(build_script_folder, solution_file), targets=["liblzma_dll" if self.options.shared else "liblzma"])

    def build(self):
        cmake = CMake(self)
        rmdir(self, os.path.join(self.source_folder, "tests")) # optionally included
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        self._create_cmake_module_variables(os.path.join(self.package_folder, self._module_file_rel_path))

    def _create_cmake_module_variables(self, module_file):
        # TODO: also add LIBLZMA_HAS_AUTO_DECODER, LIBLZMA_HAS_EASY_ENCODER & LIBLZMA_HAS_LZMA_PRESET
        content = textwrap.dedent(f"""\
            set(LIBLZMA_FOUND TRUE)
            if(DEFINED LibLZMA_INCLUDE_DIRS)
                set(LIBLZMA_INCLUDE_DIRS ${{LibLZMA_INCLUDE_DIRS}})
            endif()
            if(DEFINED LibLZMA_LIBRARIES)
                set(LIBLZMA_LIBRARIES ${{LibLZMA_LIBRARIES}})
            endif()
            set(LIBLZMA_VERSION_MAJOR {Version(self.version).major})
            set(LIBLZMA_VERSION_MINOR {Version(self.version).minor})
            set(LIBLZMA_VERSION_PATCH {Version(self.version).patch})
            set(LIBLZMA_VERSION_STRING "{self.version}")
        """)
        save(self, module_file, content)

    @property
    def _module_file_rel_path(self):
        return os.path.join("lib", "cmake", f"recipe-official-{self.name}-variables.cmake")

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "LibLZMA")
        self.cpp_info.set_property("cmake_target_name", "LibLZMA::LibLZMA")
        self.cpp_info.set_property("cmake_build_modules", [self._module_file_rel_path])
        self.cpp_info.set_property("pkg_config_name", "liblzma")
        self.cpp_info.libs = ["lzma"]
        if not self.options.shared:
            self.cpp_info.defines.append("LZMA_API_STATIC")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("pthread")
