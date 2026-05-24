import os

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "alembic"
    version = "1.8.11"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_hdf5": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_hdf5": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("imath", transitive_headers=True)
        if self.options.with_hdf5:
            self.requires("hdf5")

    def latest_version(self):
        repo = GithubRepository(self, "alembic/alembic")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/alembic/alembic/archive/refs/tags/1.8.11.tar.gz",
            sha256="ab299bb4b1894a6675c73fa29940522b54c81a91b1d691ca3470d86b7345ffce",
            destination=self.source_folder,
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["USE_ARNOLD"] = False
        tc.variables["USE_MAYA"] = False
        tc.variables["USE_PRMAN"] = False
        tc.variables["USE_PYALEMBIC"] = False
        tc.variables["USE_BINARIES"] = False
        tc.variables["USE_EXAMPLES"] = False
        tc.variables["USE_HDF5"] = self.options.with_hdf5
        tc.variables["USE_TESTS"] = False
        tc.variables["ALEMBIC_BUILD_LIBS"] = True
        tc.variables["ALEMBIC_ILMBASE_LINK_STATIC"] = True # for -DOPENEXR_DLL, handled by OpenEXR package
        tc.variables["ALEMBIC_SHARED_LIBS"] = self.options.shared
        tc.variables["ALEMBIC_USING_IMATH_3"] = False
        tc.variables["ALEMBIC_ILMBASE_FOUND"] = 1
        tc.variables["ALEMBIC_DEBUG_WARNINGS_AS_ERRORS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Alembic")
        self.cpp_info.set_property("cmake_target_name", "Alembic::Alembic")
        self.cpp_info.libs = ["Alembic"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread"])

