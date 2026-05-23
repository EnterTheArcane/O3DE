from thirdparty import RecipeBase as ConanFile
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.build import cross_building
from thirdparty.tools.files import (
    apply_conandata_patches,
    copy,
    get,
    rename,
    rm,
    rmdir,
    replace_in_file
)
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import is_msvc, unix_path
import os

class Recipe(ConanFile):
    name = "libiconv"
    version = "1.18"
    license = "LGPL-2.1-or-later"

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }

    @property
    def _is_clang_cl(self):
        return self.settings.compiler == "clang" and self.settings.os == "Windows" and \
               self.settings.compiler.get_safe("runtime")

    @property
    def _msvc_tools(self):
        compilers = self.conf.get("tools.build:compiler_executables", default={})
        compiler = compilers.get("c") or compilers.get("cpp")
        return (compiler or "clang-cl", "llvm-lib", "lld-link") if self._is_clang_cl else ("cl", "lib", "link")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def build_requirements(self):
        if self.settings_build.os == "Windows":
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2/cci.latest")
            self.win_bash = True

    def source(self):
        get(self, url="https://ftpmirror.gnu.org/gnu/libiconv/libiconv-1.18.tar.gz", sha256="3b08f5f4f9b4eb82f151a7040bfd6fe6c6fb922efe4b1659c66ea933276965e8", destination=self.source_folder, strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        if cross_building(self) and is_msvc(self):
            triplet_arch_windows = {"x86_64": "x86_64", "x86": "i686", "armv8": "aarch64"}
            # ICU doesn't like GNU triplet of conan for msvc (see https://github.com/conan-io/conan/issues/12546)
            host_arch = triplet_arch_windows.get(str(self.settings.arch))
            build_arch = triplet_arch_windows.get(str(self.settings_build.arch))

            if host_arch and build_arch:
                host = f"{host_arch}-w64-mingw32"
                build = f"{build_arch}-w64-mingw32"
                tc.configure_args.extend([
                    f"--host={host}",
                    f"--build={build}",
                ])
        env = tc.environment()
        if is_msvc(self) or self._is_clang_cl:
            cc, lib, link = self._msvc_tools
            if cc.endswith("cl"):
                cc = f"{cc} -nologo"
            build_aux_path = os.path.join(self.source_folder, "build-aux")
            lt_compile = unix_path(self, os.path.join(build_aux_path, "compile"))
            lt_ar = unix_path(self, os.path.join(build_aux_path, "ar-lib"))
            env.define("CC", f"{lt_compile} {cc}")
            env.define("CXX", f"{lt_compile} {cc}")
            env.define("LD", link)
            env.define("STRIP", ":")
            env.define("AR", f"{lt_ar} {lib}")
            env.define("RANLIB", ":")
            env.define("NM", "dumpbin -symbols")
            env.define("win32_target", "_WIN32_WINNT_VISTA")
        tc.generate(env)

    def _apply_resource_patch(self):
        if self.settings.arch == "x86":
            windres_options_path = os.path.join(self.source_folder, "windows", "windres-options")
            self.output.info("Applying {} resource patch: {}".format(self.settings.arch, windres_options_path))
            replace_in_file(self, windres_options_path, '#   PACKAGE_VERSION_SUBMINOR', '#   PACKAGE_VERSION_SUBMINOR\necho "--target=pe-i386"', strict=True)

    def build(self): 
        apply_conandata_patches(self)
        self._apply_resource_patch()
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "COPYING.LIB", self.source_folder, os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        fix_apple_shared_install_name(self)
        if (is_msvc(self) or self._is_clang_cl) and self.options.shared:
            for import_lib in ["iconv", "charset"]:
                dst = os.path.join(self.package_folder, "lib", f"{import_lib}.lib")
                if os.path.isfile(dst):
                    os.remove(dst)
                rename(self, os.path.join(self.package_folder, "lib", f"{import_lib}.dll.lib"),
                             os.path.join(self.package_folder, "lib", f"{import_lib}.lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "Iconv")
        self.cpp_info.set_property("cmake_target_name", "Iconv::Iconv")
        self.cpp_info.libs = ["iconv", "charset"]
