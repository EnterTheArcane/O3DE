# Ported from conan-center-index/spirv-cross by port_recipe.py
# REVIEW: verify all transforms are correct before building

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rm, rmdir, save
import os
import textwrap


class Recipe(RecipeBase):
    name = "spirv-cross"
    version = "1.4.321.0"
    license = "Apache-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_executable": [True, False],
        "exceptions": [True, False],
        "glsl": [True, False],
        "hlsl": [True, False],
        "msl": [True, False],
        "cpp": [True, False],
        "reflect": [True, False],
        "c_api": [True, False],
        "util": [True, False],
        "namespace": ["ANY"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_executable": True,
        "exceptions": True,
        "glsl": True,
        "hlsl": True,
        "msl": True,
        "cpp": True,
        "reflect": True,
        "c_api": True,
        "util": True,
        "namespace": "spirv_cross",
    }

    def source(self):
        get(
            url="https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/vulkan-sdk-1.4.321.0.tar.gz",
            dest=self.source_folder,
            sha256="6037555620c27105bf1d4068a6eeb4b0d7953630d556a1ca9799dfe06fd2fb68",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS"] = (
            not self.options.exceptions
        )
        tc.variables["SPIRV_CROSS_SHARED"] = self.options.shared
        tc.variables["SPIRV_CROSS_STATIC"] = (
            not self.options.shared or self.options.build_executable
        )
        tc.variables["SPIRV_CROSS_CLI"] = self.options.build_executable
        tc.variables["SPIRV_CROSS_ENABLE_TESTS"] = False
        tc.variables["SPIRV_CROSS_ENABLE_GLSL"] = self.options.glsl
        tc.variables["SPIRV_CROSS_ENABLE_HLSL"] = self.options.hlsl
        tc.variables["SPIRV_CROSS_ENABLE_MSL"] = self.options.msl
        tc.variables["SPIRV_CROSS_ENABLE_CPP"] = self.options.cpp
        tc.variables["SPIRV_CROSS_ENABLE_REFLECT"] = self.options.reflect
        tc.variables["SPIRV_CROSS_ENABLE_C_API"] = self.options.get("c_api", True)
        tc.variables["SPIRV_CROSS_ENABLE_UTIL"] = (
            self.options.get("util", False) or self.options.build_executable
        )
        tc.variables["SPIRV_CROSS_SKIP_INSTALL"] = False
        tc.variables["SPIRV_CROSS_FORCE_PIC"] = self.options.get("fPIC", True)
        tc.variables["SPIRV_CROSS_NAMESPACE_OVERRIDE"] = self.options.namespace
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(os.path.join(self.package_folder, "share"))
        rm("*.ilk", os.path.join(self.package_folder, "bin"))
        rm("*.pdb", os.path.join(self.package_folder, "bin"))
        if self.options.shared and self.options.build_executable:
            for static_lib in [
                "spirv-cross-core",
                "spirv-cross-glsl",
                "spirv-cross-hlsl",
                "spirv-cross-msl",
                "spirv-cross-cpp",
                "spirv-cross-reflect",
                "spirv-cross-c",
                "spirv-cross-util",
            ]:
                rm(f"*{static_lib}.*", os.path.join(self.package_folder, "lib"))

        # TODO: to remove in conan v2 once legacy generators removed
        self._create_cmake_module_alias_targets(
            os.path.join(self.package_folder, self._module_file_rel_path),
            {
                target: f"spirv-cross::{target}"
                for target in self._spirv_cross_components.keys()
            },
        )

    def _create_cmake_module_alias_targets(self, module_file, targets):
        content = ""
        for alias, aliased in targets.items():
            content += textwrap.dedent(
                f"""\
                if(TARGET {aliased} AND NOT TARGET {alias})
                    add_library({alias} INTERFACE IMPORTED)
                    set_property(TARGET {alias} PROPERTY INTERFACE_LINK_LIBRARIES {aliased})
                endif()
            """
            )
        save(module_file, content)

    @property
    def _module_file_rel_path(self):
        return os.path.join("lib", "cmake", f"conan-official-{self.name}-targets.cmake")

    @property
    def _spirv_cross_components(self):
        components = {}
        if self.options.shared:
            components.update({"spirv-cross-c-shared": []})
        else:
            components.update({"spirv-cross-core": []})
            if self.options.glsl:
                components.update({"spirv-cross-glsl": ["spirv-cross-core"]})
                if self.options.hlsl:
                    components.update({"spirv-cross-hlsl": ["spirv-cross-glsl"]})
                if self.options.msl:
                    components.update({"spirv-cross-msl": ["spirv-cross-glsl"]})
                if self.options.cpp:
                    components.update({"spirv-cross-cpp": ["spirv-cross-glsl"]})
                if self.options.reflect:
                    components.update({"spirv-cross-reflect": []})
            if self.options.c_api:
                c_api_requires = []
                if self.options.glsl:
                    c_api_requires.append("spirv-cross-glsl")
                    if self.options.hlsl:
                        c_api_requires.append("spirv-cross-hlsl")
                    if self.options.msl:
                        c_api_requires.append("spirv-cross-msl")
                    if self.options.cpp:
                        c_api_requires.append("spirv-cross-cpp")
                    if self.options.reflect:
                        c_api_requires.append("spirv-cross-reflect")
                components.update({"spirv-cross-c": c_api_requires})
            if self.options.util:
                components.update({"spirv-cross-util": ["spirv-cross-core"]})
        return components
