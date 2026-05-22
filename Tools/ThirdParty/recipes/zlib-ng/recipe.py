from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import copy, get, rmdir
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "zlib-ng"
    version = "2.3.3"
    license = "Zlib"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "zlib_compat": [True, False],
        "with_gzfileop": [True, False],
        "with_optim": [True, False],
        "with_new_strategies": [True, False],
        "with_native_instructions": [True, False],
        "with_reduced_mem": [True, False],
        "with_runtime_cpu_detection": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "zlib_compat": False,
        "with_gzfileop": True,
        "with_optim": True,
        "with_new_strategies": True,
        "with_native_instructions": False,
        "with_reduced_mem": False,
        "with_runtime_cpu_detection": True,
    }

    def source(self):
        get(
            url="https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.3.3.tar.gz",
            dest=self.source_folder,
            sha256="f9c65aa9c852eb8255b636fd9f07ce1c406f061ec19a2e7d508b318ca0c907d1",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        if Version(self.version) >= "2.3.1":
            tc.cache_variables["BUILD_TESTING"] = False
        else:
            tc.cache_variables["ZLIB_ENABLE_TESTS"] = False
            tc.cache_variables["ZLIBNG_ENABLE_TESTS"] = False

        tc.variables["ZLIB_COMPAT"] = self.options.zlib_compat
        tc.variables["WITH_GZFILEOP"] = self.options.with_gzfileop
        tc.variables["WITH_OPTIM"] = self.options.with_optim
        tc.variables["WITH_NEW_STRATEGIES"] = self.options.with_new_strategies
        tc.variables["WITH_NATIVE_INSTRUCTIONS"] = self.options.with_native_instructions
        tc.variables["WITH_REDUCED_MEM"] = self.options.with_reduced_mem
        tc.variables["WITH_RUNTIME_CPU_DETECTION"] = (
            self.options.with_runtime_cpu_detection
        )
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        license_folder = os.path.join(self.package_folder, "licenses")
        copy("LICENSE.md", src=self.source_folder, dst=license_folder)
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
        # upstream CMakeLists intentionally hardcodes install_name with full
        # install path (to match autootools behavior), instead of @rpath
        fix_apple_shared_install_name(self)
