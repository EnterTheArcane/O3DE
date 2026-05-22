from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "alembic"
    version = "1.8.8"
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

    def requirements(self) -> list[str]:
        return ["imath", "openexr"]

    def source(self):
        get(
            url="https://github.com/alembic/alembic/archive/refs/tags/1.8.8.tar.gz",
            dest=self.source_folder,
            sha256="ba1f34544608ef7d3f68cafea946ec9cc84792ddf9cda3e8d5590821df71f6c6",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["USE_ARNOLD"] = False
        tc.variables["USE_MAYA"] = False
        tc.variables["USE_PRMAN"] = False
        tc.variables["USE_PYALEMBIC"] = False
        tc.variables["USE_BINARIES"] = False
        tc.variables["USE_EXAMPLES"] = False
        tc.variables["USE_HDF5"] = False
        tc.variables["USE_TESTS"] = False
        tc.variables["ALEMBIC_BUILD_LIBS"] = True
        tc.variables["ALEMBIC_SHARED_LIBS"] = self.options.shared
        tc.variables["ALEMBIC_USING_IMATH_3"] = True
        if Version(self.version) >= "1.8.4":
            tc.variables["ALEMBIC_DEBUG_WARNINGS_AS_ERRORS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
