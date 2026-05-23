from thirdparty import RecipeBase
from thirdparty.tools.build import check_min_cppstd
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rm, rmdir
from thirdparty.tools.github import GithubRepository
from thirdparty.tools.scm import Version
import os

class Recipe(RecipeBase):
    name = "draco"
    version = "1.5.7"
    license = "Apache-2.0"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "target": ["draco", "encode_and_decode", "encode_only", "decode_only"],
        "enable_point_cloud_compression": [True, False],
        "enable_mesh_compression": [True, False],
        "enable_standard_edgebreaker": [True, False],
        "enable_predictive_edgebreaker": [True, False],
        "enable_backwards_compatibility": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "target": "draco",
        "enable_point_cloud_compression": True,
        "enable_mesh_compression": True,
        "enable_standard_edgebreaker": True,
        "enable_predictive_edgebreaker": True,
        "enable_backwards_compatibility": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if not self.options.enable_mesh_compression:
            del self.options.enable_standard_edgebreaker
            del self.options.enable_predictive_edgebreaker

    def latest_version(self):
        repo = GithubRepository(self, "google/draco")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/google/draco/archive/refs/tags/1.5.7.tar.gz",
            sha256="bf6b105b79223eab2b86795363dfe5e5356050006a96521477973aba8f036fe1",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)

        tc.variables["DRACO_POINT_CLOUD_COMPRESSION"] = self.options.enable_point_cloud_compression
        tc.variables["DRACO_MESH_COMPRESSION"] = self.options.enable_mesh_compression
        if self.options.enable_mesh_compression:
            tc.variables["DRACO_STANDARD_EDGEBREAKER"] = self.options.enable_standard_edgebreaker
            tc.variables["DRACO_PREDICTIVE_EDGEBREAKER"] = self.options.enable_predictive_edgebreaker
        tc.variables["DRACO_ANIMATION_ENCODING"] = False
        tc.variables["DRACO_BACKWARDS_COMPATIBILITY"] = self.options.enable_backwards_compatibility
        tc.variables["DRACO_DECODER_ATTRIBUTE_DEDUPLICATION"] = False
        tc.variables["DRACO_FAST"] = False
        tc.variables["DRACO_GLTF"] = False
        tc.variables["DRACO_JS_GLUE"] = False
        tc.variables["DRACO_MAYA_PLUGIN"] = False
        tc.variables["DRACO_TESTS"] = False
        tc.variables["DRACO_UNITY_PLUGIN"] = False
        tc.variables["DRACO_WASM"] = False

        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        if self.options.shared:
            rm(self, "*draco.a", os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "draco")
        self.cpp_info.set_property("cmake_target_name", "draco::draco")
        self.cpp_info.set_property("pkg_config_name", "draco")
        self.cpp_info.libs = ["draco"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
