from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_patches, copy, get, rmdir, save, replace_in_file
from thirdparty.tools.scm import Version
import os
import textwrap

class Recipe(RecipeBase):
    name = "openjpeg"
    version = "2.5.4"
    license = "BSD-2-Clause"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_codec": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_codec": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def source(self):
        get(
            self,
            url="https://github.com/uclouvain/openjpeg/archive/refs/tags/v2.5.4.tar.gz",
            sha256="a695fbe19c0165f295a8531b1e4e855cd94d0875d2f88ec4b61080677e27188a",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP"] = True
        tc.variables["BUILD_DOC"] = False
        tc.variables["BUILD_STATIC_LIBS"] = not self.options.shared
        tc.variables["BUILD_LUTS_GENERATOR"] = False
        tc.variables["BUILD_CODEC"] = False
        tc.variables["BUILD_JPIP"] = False
        tc.variables["BUILD_VIEWER"] = False
        tc.variables["BUILD_JAVA"] = False
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_PKGCONFIG_FILES"] = False
        tc.variables["OPJ_DISABLE_TPSOT_FIX"] = False
        tc.variables["OPJ_USE_THREAD"] = True
        tc.generate()

    def _patch_sources(self):
        apply_patches(self)
        # The finite-math-only optimization has no effect and can cause linking errors
        # when linked against glibc >= 2.31
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                        "-ffast-math", "-ffast-math;-fno-finite-math-only")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", self._openjpeg_subdir))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake", self._openjpeg_subdir))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        self._create_cmake_module_variables(
            os.path.join(self.package_folder, self._module_vars_rel_path)
        )
        self._create_cmake_module_alias_targets(
            os.path.join(self.package_folder, self._module_target_rel_path),
            {"openjp2": "OpenJPEG::OpenJPEG"}
        )

    def _create_cmake_module_variables(self, module_file):
        content = textwrap.dedent(f"""\
            set(OPENJPEG_FOUND TRUE)
            if(DEFINED OpenJPEG_INCLUDE_DIRS)
                set(OPENJPEG_INCLUDE_DIRS ${{OpenJPEG_INCLUDE_DIRS}})
            endif()
            if(DEFINED OpenJPEG_LIBRARIES)
                set(OPENJPEG_LIBRARIES ${{OpenJPEG_LIBRARIES}})
            endif()
            set(OPENJPEG_MAJOR_VERSION "{Version(self.version).major}")
            set(OPENJPEG_MINOR_VERSION "{Version(self.version).minor}")
            set(OPENJPEG_BUILD_VERSION "{Version(self.version).patch}")
            set(OPENJPEG_BUILD_SHARED_LIBS {"TRUE" if self.options.shared else "FALSE"})
        """)
        save(self, module_file, content)

    def _create_cmake_module_alias_targets(self, module_file, targets):
        content = ""
        for alias, aliased in targets.items():
            content += textwrap.dedent(f"""\
                if(TARGET {aliased} AND NOT TARGET {alias})
                    add_library({alias} INTERFACE IMPORTED)
                    set_property(TARGET {alias} PROPERTY INTERFACE_LINK_LIBRARIES {aliased})
                endif()
            """)
        save(self, module_file, content)

    @property
    def _module_vars_rel_path(self):
        return os.path.join("lib", "cmake", f"conan-official-{self.name}-variables.cmake")

    @property
    def _module_target_rel_path(self):
        return os.path.join("lib", "cmake", f"conan-official-{self.name}-targets.cmake")

    @property
    def _openjpeg_subdir(self):
        openjpeg_version = Version(self.version)
        return f"openjpeg-{openjpeg_version.major}.{openjpeg_version.minor}"

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenJPEG")
        self.cpp_info.set_property("cmake_target_name", "openjp2")
        self.cpp_info.set_property("cmake_build_modules", [self._module_vars_rel_path])
        self.cpp_info.set_property("pkg_config_name", "libopenjp2")
        self.cpp_info.includedirs.append(os.path.join("include", self._openjpeg_subdir))
        self.cpp_info.builddirs.append(os.path.join("lib", "cmake"))
        self.cpp_info.libs = ["openjp2"]
        if self.settings.os == "Windows" and not self.options.shared:
            self.cpp_info.defines.append("OPJ_STATIC")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread", "m"]
        elif self.settings.os == "Android":
            self.cpp_info.system_libs = ["m"]

