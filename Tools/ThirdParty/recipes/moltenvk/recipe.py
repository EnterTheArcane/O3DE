import os

from thirdparty import RecipeBase
from thirdparty.apple import is_apple_os
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import apply_patches, copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "moltenvk"
    version = "1.4.1"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "hide_vulkan_symbols": [True, False],
        "tools": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "hide_vulkan_symbols": False,
        "tools": True,
    }

    def validate(self):
        if not is_apple_os(self):
            raise RecipeInvalidConfiguration(
                f"{self.name} is only supported on Apple platforms (Mac, iOS, tvOS)")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        else:
            self.options.rm_safe("hide_vulkan_symbols")

    def requirements(self):
        self.requires("cereal")
        self.requires("glslang")
        self.requires("spirv-cross")
        self.requires("spirv-tools")
        self.requires("vulkan-headers", transitive_headers=True)

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/MoltenVK")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/MoltenVK/archive/refs/tags/v1.4.1.tar.gz",
            sha256="9985f141902a17de818e264d17c1ce334b748e499ee02fcb4703e4dc0038f89c",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MVK_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.variables["MVK_VERSION"] = self.version
        tc.variables["MVK_WITH_SPIRV_TOOLS"] = True
        tc.variables["MVK_BUILD_SHADERCONVERTER_TOOL"] = self.options.tools
        if self.options.shared:
            tc.variables["MVK_HIDE_VULKAN_SYMBOLS"] = self.options.hide_vulkan_symbols
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["MoltenVK"]
        self.cpp_info.frameworks = ["Metal", "Foundation", "CoreFoundation", "QuartzCore", "IOSurface", "CoreGraphics"]
        if self.settings.os == "Mac":
            self.cpp_info.frameworks.extend(["AppKit", "IOKit"])
        elif self.settings.os in ["iOS", "tvOS"]:
            self.cpp_info.frameworks.append("UIKit")

        self.cpp_info.requires = [
            "cereal::cereal",
            "glslang::glslang-core",
            "glslang::spirv",
            "spirv-cross::spirv-cross-core",
            "spirv-cross::spirv-cross-msl",
            "spirv-cross::spirv-cross-reflect",
            "spirv-tools::spirv-tools-core",
            "vulkan-headers::vulkan-headers",
        ]

        if self.options.shared:
            moltenvk_icd_path = os.path.join(self.package_folder, "lib", "MoltenVK_icd.json")
            self.runenv_info.prepend_path("VK_DRIVER_FILES", moltenvk_icd_path)
            self.runenv_info.prepend_path("VK_ICD_FILENAMES", moltenvk_icd_path)
