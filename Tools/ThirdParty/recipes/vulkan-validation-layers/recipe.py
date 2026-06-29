import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rename, rm, replace_in_file
from thirdparty.pkgconfig import PkgConfigDeps
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    with_wsi_xcb: bool = True
    with_wsi_xlib: bool = True
    with_wsi_wayland: bool = True


class Recipe(RecipeBase[_Options]):
    name = "vulkan-validation-layers"
    version = "1.4.350.1"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "KhronosGroup/Vulkan-ValidationLayers")
        return Version(repo.latest_tag("vulkan-sdk-").removeprefix("vulkan-sdk-"))

    def configure(self):
        if not self._has_wsi_options:
            self.options.with_wsi_xcb = False
            self.options.with_wsi_xlib = False
            self.options.with_wsi_wayland = False

    def requirements(self):
        self.requires("robin-hood-hashing")
        self.requires("spirv-headers")
        self.requires("spirv-tools")
        self.requires("vulkan-headers")
        self.requires("vulkan-utility-libraries")

        if self.options.with_wsi_xcb or self.options.with_wsi_xlib:
            self.requires("xorg")
        if self.options.with_wsi_wayland:
            self.requires("wayland")
        if self._needs_pkg_config and not self.conf.get("tools.gnu:pkg_config", check_type=str):
            self.requires_tool("pkgconf")
        self.requires_tool("cmake")

    def source(self):
        get(
            self,
            url="https://github.com/KhronosGroup/Vulkan-ValidationLayers/archive/refs/tags/vulkan-sdk-1.4.350.1.tar.gz",
            sha256="1c6e33f60bb0cfed62cdb02a82845a7cf2681f92e07c5255eacd4b418db0def5",
            destination=self.folders.source,
            strip_root=True)
        for text in ["set(CMAKE_CXX_STANDARD 17)", "set(CMAKE_CXX_STANDARD_REQUIRED ON)"]:
            replace_in_file(self, self.folders.source / "CMakeLists.txt", text, "")

        spirv_helper_h = self.folders.source / "layers" / "vulkan" / "generated" / "spirv_grammar_helper.h"
        spirv_helper_cpp = self.folders.source / "layers" / "vulkan" / "generated" / "spirv_grammar_helper.cpp"
        for helper_file in [spirv_helper_h, spirv_helper_cpp]:
            replace_in_file(self, helper_file, "OpControlBarrierArriveEXT", "OpControlBarrierArriveINTEL")
            replace_in_file(self, helper_file, "OpControlBarrierWaitEXT", "OpControlBarrierWaitINTEL")
        replace_in_file(self, spirv_helper_cpp, "RayFlagsForceOpacityMicromap2StateKHRMask", "RayFlagsForceOpacityMicromap2StateEXTMask")

        replace_in_file(self, spirv_helper_h, "        case spv::OpPredicatedLoadINTEL:\n", "")
        replace_in_file(
            self, spirv_helper_cpp,
            '        case spv::OpPredicatedLoadINTEL:\n'
            '            return "OpPredicatedLoadINTEL";\n'
            '        case spv::OpPredicatedStoreINTEL:\n'
            '            return "OpPredicatedStoreINTEL";\n',
            "")
        replace_in_file(
            self, spirv_helper_cpp,
            "        {spv::OpPredicatedLoadINTEL, {{OperandKind::Id, OperandKind::Id, OperandKind::Id, OperandKind::BitEnum}}},\n"
            "        {spv::OpPredicatedStoreINTEL, {{OperandKind::Id, OperandKind::Id, OperandKind::Id, OperandKind::BitEnum}}},\n",
            "")
        replace_in_file(
            self, spirv_helper_cpp,
            '        case spv::ExecutionModeOpacityMicromapIdKHR:\n'
            '            return "OpacityMicromapIdKHR";\n',
            "")

        spirv_validation_cpp = self.folders.source / "layers" / "vulkan" / "generated" / "spirv_validation_helper.cpp"
        replace_in_file(self, spirv_validation_cpp, "CapabilityRayTracingOpacityMicromapKHR", "CapabilityRayTracingOpacityMicromapEXT")
        replace_in_file(self, spirv_validation_cpp, "CapabilitySplitBarrierEXT", "CapabilitySplitBarrierINTEL")
        replace_in_file(self, spirv_validation_cpp, "        {spv::CapabilityRayTracingOpacityMicromapExecutionModeKHR, {0, &DeviceFeatures::micromap, nullptr, \"\"}},\n", "")
        replace_in_file(self, spirv_validation_cpp, "        {spv::CapabilityCooperativeMatrixDecodeVectorNV, {0, &DeviceFeatures::cooperativeMatrixDecodeVector, nullptr, \"\"}},\n", "")
        replace_in_file(
            self, spirv_validation_cpp,
            '        case spv::CapabilityWeakLinkageAMD:\n'
            '            return "WeakLinkageAMD";\n',
            "")
        replace_in_file(
            self, spirv_validation_cpp,
            '        case spv::CapabilityRayTracingOpacityMicromapExecutionModeKHR:\n'
            '            return "RayTracingOpacityMicromapExecutionModeKHR";\n',
            "")
        replace_in_file(
            self, spirv_validation_cpp,
            '        case spv::CapabilityCooperativeMatrixDecodeVectorNV:\n'
            '            return "CooperativeMatrixDecodeVectorNV";\n',
            "")
        replace_in_file(
            self, spirv_validation_cpp,
            '        case spv::CapabilityPredicatedIOINTEL:\n'
            '            return "PredicatedIOINTEL";\n'
            '        case spv::CapabilityRoundedDivideSqrtINTEL:\n'
            '            return "RoundedDivideSqrtINTEL";\n',
            "")
        replace_in_file(self, spirv_validation_cpp, '    {spv::CapabilityRayTracingOpacityMicromapExecutionModeKHR, "VkPhysicalDeviceOpacityMicromapFeaturesKHR::micromap"},\n', "")
        replace_in_file(
            self, spirv_validation_cpp,
            '    {spv::CapabilityCooperativeMatrixDecodeVectorNV, "VkPhysicalDeviceCooperativeMatrixDecodeVectorFeaturesNV::cooperativeMatrixDecodeVector"},\n',
            "")

    def generate(self):
        tc = CMakeToolchain(self)
        if self._has_wsi_options:
            tc.cache_variables["BUILD_WSI_XCB_SUPPORT"] = self.options.with_wsi_xcb
            tc.cache_variables["BUILD_WSI_XLIB_SUPPORT"] = self.options.with_wsi_xlib
            tc.cache_variables["BUILD_WSI_WAYLAND_SUPPORT"] = self.options.with_wsi_wayland
        tc.cache_variables["BUILD_WERROR"] = False
        tc.cache_variables["BUILD_TESTS"] = False
        tc.cache_variables["UPDATE_DEPS"] = False
        tc.generate()

        deps = CMakeDeps(self)
        # Recipe provides both under the same name, upstream only uses this one
        deps.set_property("spirv-tools", "cmake_file_name", "SPIRV-Tools-opt")
        deps.generate()

        if self._needs_pkg_config:
            deps = PkgConfigDeps(self)
            deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()
        rm(self, "*.pdb", self.folders.package / "bin")
        if not self.settings.os == "Windows":
            # Move json files to res, but keep in mind to preserve relative
            # path between module library and manifest json file
            rename(self, self.folders.package / "share", self.folders.package / "res")
        # There is no need to use fix_apple_shared_install_name(self) as the .dylib created
        # is a BUNDLE. Running otool -hv libVkLayer_khronos_validation.dylib shows filetype=BUNDLE

    def package_info(self):
        # Libs variable is empty as this is a shared library loaded exclusively on the runtime
        # context (VirtualRunEnv):
        # - Linux and Macos only need to have the folder libdirs=[lib] defined (LD_LIBRARY_PATH, DYLD_LIBRARY_PATH)
        # - Windows will set the bindirs=[bin] on the PATH env variable
        # More info: https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/main/layers/CMakeLists.txt#L632-L636
        self.info.libs = []
        self.info.includedirs = []

        # We need to expose this VK_LAYER_PATH explicitly on the runtime environment
        manifest_subfolder = "bin" if self.settings.os == "Windows" else os.path.join("res", "vulkan", "explicit_layer.d")
        vk_layer_path = self.folders.package / manifest_subfolder
        self.info.runenv.prepend_path("VK_LAYER_PATH", vk_layer_path)

        if self.settings.os == "Android":
            self.info.system_libs.extend(["android", "log"])

    @property
    def _has_wsi_options(self):
        return self.settings.os in ["Linux", "FreeBSD"]

    @property
    def _needs_pkg_config(self):
        return self.options.with_wsi_xcb or \
            self.options.with_wsi_xlib or \
            self.options.with_wsi_wayland
