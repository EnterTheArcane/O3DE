from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import apply_conandata_patches, get, copy, rm, rmdir, replace_in_file, collect_libs
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime, VCVars
from thirdparty.tools.scm import Version
import os
import shutil

class Recipe(RecipeBase):
    name = "mimalloc"
    version = "3.3.2"
    license = "MIT"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "secure": [True, False],
        "override": [True, False],
        "inject": [True, False],
        "single_object": [True, False],
        "guarded": [True, False],
        "win_redirect": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "secure": False,
        "override": False,
        "inject": False,
        "single_object": False,
        "guarded": False,
        "win_redirect": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        else:
            del self.options.win_redirect

        # single_object and inject are options
        # only when overriding on Unix-like platforms:
        if is_msvc(self):
            del self.options.single_object
            del self.options.inject
        if Version(self.version) < "2.1.9":
            del self.options.guarded

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

            # single_object is valid only for static
            # override:
            self.options.rm_safe("single_object")

        # inject is valid only for Unix-like dynamic override:
        if not self.options.shared:
            self.options.rm_safe("inject")

        # single_object and inject are valid only when
        # overriding on Unix-like platforms:
        if not self.options.override:
            self.options.rm_safe("single_object")
            self.options.rm_safe("inject")

    def build_requirements(self):
        self.tool_requires("cmake")

    def source(self):
        get(self, url="https://github.com/microsoft/mimalloc/archive/v3.3.2.tar.gz", sha256="ca02384e007f46950598500dfaebde5ff9948c1d231f5a81b058799afa64bbbb", destination=self.source_folder, strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MI_BUILD_TESTS"] = "OFF"
        tc.variables["MI_BUILD_SHARED"] = self.options.shared
        tc.variables["MI_BUILD_STATIC"] = not self.options.shared
        tc.variables["MI_BUILD_OBJECT"] = self.options.get_safe("single_object", False)
        tc.variables["MI_OVERRIDE"] = "ON" if self.options.override else "OFF"
        tc.variables["MI_SECURE"] = "ON" if self.options.secure else "OFF"
        tc.variables["MI_WIN_REDIRECT"] = "ON" if self.options.get_safe("win_redirect") else "OFF"
        tc.variables["MI_INSTALL_TOPLEVEL"] = "ON"
        tc.variables["MI_GUARDED"] = self.options.get_safe("guarded", False)
        tc.generate()
        venv = VirtualBuildEnv(self)
        venv.generate(scope="build")

        if is_msvc(self):
            vcvars = VCVars(self)
            vcvars.generate()

    def build(self):
        apply_conandata_patches(self)
        if is_msvc(self) and self.settings.arch == "x86" and self.options.shared:
            replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                            "mimalloc-redirect.lib",
                            "mimalloc-redirect32.lib")
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        cmake = CMake(self)
        cmake.install()

        rmdir(self, os.path.join(self.package_folder, "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

        if self.options.get_safe("single_object"):
            rm(self, "*.a", os.path.join(self.package_folder, "lib"))
            shutil.copy(os.path.join(self.package_folder, "lib", self._obj_name + ".o"),
                        os.path.join(self.package_folder, "lib", self._obj_name))

        if self.settings.os == "Windows" and self.options.shared:
            if self.settings.arch == "x86_64":
                copy(self, "mimalloc-redirect.dll",
                    src=os.path.join(self.source_folder, "bin"),
                    dst=os.path.join(self.package_folder, "bin"))
                copy(self, "minject.exe",
                    src=os.path.join(self.source_folder, "bin"),
                    dst=os.path.join(self.package_folder, "bin"))
            elif self.settings.arch == "x86":
                copy(self, "mimalloc-redirect32.dll",
                    src=os.path.join(self.source_folder, "bin"),
                    dst=os.path.join(self.package_folder, "bin"))
                copy(self, "minject32.exe",
                    src=os.path.join(self.source_folder, "bin"),
                    dst=os.path.join(self.package_folder, "bin"))

        rmdir(self, os.path.join(self.package_folder, "share"))

    @property
    def _obj_name(self):
        name = "mimalloc"
        if self.options.secure:
            name += "-secure"
        if self.settings.build_type not in ("Release", "RelWithDebInfo", "MinSizeRel"):
            name += "-{}".format(str(self.settings.build_type).lower())
        return name

    @property
    def _lib_name(self):
        name = "mimalloc" if self.settings.os == "Windows" else "libmimalloc"

        if self.settings.os == "Windows" and not self.options.shared:
            name += "-static"
        if self.options.secure:
            name += "-secure"
        if self.settings.build_type not in ("Release", "RelWithDebInfo", "MinSizeRel"):
            name += "-{}".format(str(self.settings.build_type).lower())
        return name

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "mimalloc")
        self.cpp_info.set_property("cmake_target_name", "mimalloc" if self.options.shared else "mimalloc-static")

        if self.options.get_safe("inject"):
            self.cpp_info.includedirs = []
            self.cpp_info.libdirs = []
            self.cpp_info.resdirs = []
            return

        if self.options.get_safe("single_object"):
            obj_ext = "o"
            obj_file = "{}.{}".format(self._obj_name, obj_ext)
            obj_path = os.path.join(self.package_folder, "lib", obj_file)
            self.cpp_info.exelinkflags = [obj_path]
            self.cpp_info.sharedlinkflags = [obj_path]
            self.cpp_info.libdirs = []
            self.cpp_info.bindirs = []
        else:
            self.cpp_info.libs = collect_libs(self)

        if self.settings.os == "Linux":
            self.cpp_info.system_libs.append("pthread")
        if not self.options.shared:
            if self.settings.os == "Windows":
                self.cpp_info.system_libs.extend(["psapi", "shell32", "user32", "bcrypt"])
            elif self.settings.os == "Linux":
                self.cpp_info.system_libs.append("rt")
