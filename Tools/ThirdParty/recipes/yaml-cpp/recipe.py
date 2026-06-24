import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeToolchain
from thirdparty.files import apply_patches, collect_libs, copy, get, rmdir
from thirdparty.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "yaml-cpp"
    version = "0.9.0"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def latest_version(self):
        repo = GithubRepository(self, "jbeder/yaml-cpp")
        return Version(repo.latest_release.removeprefix("yaml-cpp-"))

    def source(self):
        get(
            self,
            url="https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.9.0.tar.gz",
            sha256="25cb043240f828a8c51beb830569634bc7ac603978e0f69d6b63558dadefd49a",
            destination=self.folders.source,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["YAML_CPP_BUILD_TESTS"] = False
        tc.variables["YAML_CPP_BUILD_CONTRIB"] = True
        tc.variables["YAML_CPP_BUILD_TOOLS"] = False
        tc.variables["YAML_CPP_INSTALL"] = True
        tc.variables["YAML_BUILD_SHARED_LIBS"] = self.options.shared
        if is_msvc(self):
            tc.variables["YAML_MSVC_SHARED_RT"] = not is_msvc_static_runtime(self)
            tc.preprocessor_definitions["_NOEXCEPT"] = "noexcept"
        tc.cache_variables["YAML_ENABLE_PIC"] = self.options.get_safe("fPIC", "OFF")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))
        rmdir(self, os.path.join(self.folders.package, "CMake"))
        rmdir(self, os.path.join(self.folders.package, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.folders.package, "share"))

    def package_info(self):
        self.info.set_property("cmake_file_name", "yaml-cpp")
        self.info.set_property("cmake_target_name", "yaml-cpp::yaml-cpp")
        self.info.set_property("cmake_target_aliases", ["yaml-cpp"])  # CMake imported target before 0.8.0
        self.info.set_property("pkg_config_name", "yaml-cpp")
        self.info.libs = collect_libs(self)
        if self.settings.os in ("Linux", "FreeBSD"):
            self.info.system_libs.append("m")
        if is_msvc(self):
            self.info.defines.append("_NOEXCEPT=noexcept")
        if not self.options.shared:
            self.info.defines.append("YAML_CPP_STATIC_DEFINE")
