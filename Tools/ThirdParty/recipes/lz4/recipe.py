import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.scm.github import GithubRepository
from thirdparty.tools.microsoft import is_msvc
from thirdparty.tools.scm import Version


class Recipe(RecipeBase):
    name = "lz4"
    version = "1.10.0"
    license = "BSD-2-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def latest_version(self):
        repo = GithubRepository(self, "lz4/lz4")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/lz4/lz4/archive/v1.10.0.tar.gz",
            sha256="537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b",
            destination=self.source_folder,
            strip_root=True)
        apply_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LZ4_BUILD_CLI"] = False
        tc.variables["LZ4_BUNDLED_MODE"] = False
        tc.variables["LZ4_POSITION_INDEPENDENT_LIB"] = self.options.get_safe("fPIC", True)
        # Generate a relocatable shared lib on Macos
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0042"] = "NEW"
        # Honor BUILD_SHARED_LIBS (see https://github.com/conan-io/conan/issues/11840)
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        tc.generate()

    @property
    def _cmakelists_folder(self):
        subfolder = os.path.join("build", "cmake")
        return os.path.join(self.source_folder, subfolder)

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self._cmakelists_folder)
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=os.path.join(self.source_folder, "lib"), dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    @property
    def _lz4_target(self):
        return f"LZ4::{'lz4_shared' if self.options.shared else 'lz4_static'}"

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "lz4")
        self.cpp_info.set_property("cmake_target_name", self._lz4_target)
        self.cpp_info.set_property("cmake_target_aliases", ["lz4::lz4"]) # old unofficial target in CCI for lz4, kept for the moment to not break consumers
        self.cpp_info.set_property("pkg_config_name", "liblz4")
        self.cpp_info.libs = ["lz4"]
        if is_msvc(self) and self.options.shared:
            self.cpp_info.defines.append("LZ4_DLL_IMPORT=1")
