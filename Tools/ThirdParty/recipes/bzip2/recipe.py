from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, save
from thirdparty.tools.scm import Version
import os
import textwrap


class Recipe(RecipeBase):
    name = "bzip2"
    version = "1.0.8"
    license = "bzip2-1.0.6"  # SPDX license identifier for version 1.0.6 or newer
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_executable": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_executable": True,
    }

    def source(self):
        get(
            url="https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz",
            dest=self.source_folder,
            sha256="ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269",
        )
        # Copy the wrapper CMakeLists.txt (exported by the original CCI recipe) to the build root
        import shutil

        shutil.copy2(
            os.path.join(self.recipe_folder, "CMakeLists.txt"),
            os.path.join(self.source_folder, os.pardir, "CMakeLists.txt"),
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BZ2_BUILD_EXE"] = self.options.build_executable
        tc.variables["BZ2_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.variables["BZ2_VERSION_MAJOR"] = Version(self.version).major
        tc.variables["BZ2_VERSION_STRING"] = self.version
        tc.generate()

    def build(self):
        apply_patches(self)
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        self._create_cmake_module_variables(
            os.path.join(self.package_folder, self._module_file_rel_path)
        )

    def _create_cmake_module_variables(self, module_file):
        content = textwrap.dedent(
            f"""\
            set(BZIP2_NEED_PREFIX TRUE)
            set(BZIP2_FOUND TRUE)
            if(NOT DEFINED BZIP2_INCLUDE_DIRS AND DEFINED BZip2_INCLUDE_DIRS)
                set(BZIP2_INCLUDE_DIRS ${{BZip2_INCLUDE_DIRS}})
            endif()
            if(NOT DEFINED BZIP2_INCLUDE_DIR AND DEFINED BZip2_INCLUDE_DIR)
                set(BZIP2_INCLUDE_DIR ${{BZip2_INCLUDE_DIR}})
            endif()
            if(NOT DEFINED BZIP2_LIBRARIES AND DEFINED BZip2_LIBRARIES)
                set(BZIP2_LIBRARIES ${{BZip2_LIBRARIES}})
            endif()
            set(BZIP2_VERSION_STRING "{self.version}")
        """
        )
        save(module_file, content)

    @property
    def _module_file_rel_path(self):
        return os.path.join(
            "lib", "cmake", f"conan-official-{self.name}-variables.cmake"
        )
