import os

from thirdparty import RecipeBase
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get, rmdir
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "alembic"
    version = "1.8.11"
    license = "BSD-3-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def requirements(self):
        self.requires("hdf5")
        self.requires("imath", transitive_headers=True)

    def latest_version(self):
        repo = GithubRepository(self, "alembic/alembic")
        return Version(repo.latest_release)

    def source(self):
        get(
            self,
            url="https://github.com/alembic/alembic/archive/refs/tags/1.8.11.tar.gz",
            sha256="ab299bb4b1894a6675c73fa29940522b54c81a91b1d691ca3470d86b7345ffce",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ALEMBIC_BUILD_LIBS"] = True
        tc.variables["ALEMBIC_DEBUG_WARNINGS_AS_ERRORS"] = False
        tc.variables["ALEMBIC_ILMBASE_FOUND"] = 1
        tc.variables["ALEMBIC_ILMBASE_LINK_STATIC"] = True  # for -DOPENEXR_DLL, handled by OpenEXR package
        tc.variables["ALEMBIC_SHARED_LIBS"] = self.options.shared
        tc.variables["ALEMBIC_USING_IMATH_3"] = False
        tc.variables["USE_ARNOLD"] = False
        tc.variables["USE_BINARIES"] = False
        tc.variables["USE_EXAMPLES"] = False
        tc.variables["USE_HDF5"] = True
        tc.variables["USE_MAYA"] = False
        tc.variables["USE_PRMAN"] = False
        tc.variables["USE_PYALEMBIC"] = False
        tc.variables["USE_TESTS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.folders.source, dst=os.path.join(self.folders.package, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.folders.package, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Alembic")
        self.cpp_info.set_property("cmake_target_name", "Alembic::Alembic")
        self.cpp_info.libs = ["Alembic"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread"])
