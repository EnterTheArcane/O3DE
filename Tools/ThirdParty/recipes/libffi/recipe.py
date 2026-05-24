import glob
import os
import shutil

from thirdparty import RecipeBase
from thirdparty.tools.apple import fix_apple_shared_install_name
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_patches, copy, get, mkdir, rm, rmdir
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import check_min_vs, is_msvc, is_msvc_static_runtime, msvc_runtime_flag, unix_path
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "libffi"
    version = "3.5.2"
    license = "MIT"

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

    def build_requirements(self):
        if self.settings_build.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", default=False, check_type=str):
                self.tool_requires("msys2")
        if self.settings_build.os == "Windows" and self.settings.get_safe("compiler.runtime"):
            self.tool_requires("automake")

    def latest_version(self):
        repo = GithubRepository(self, "libffi/libffi")
        return Version(repo.latest_release.removeprefix("v"))

    def source(self):
        get(
            self,
            url="https://github.com/libffi/libffi/releases/download/v3.5.2/libffi-3.5.2.tar.gz",
            sha256="f3a3082a23b37c293a4fcd1053147b371f2ff91fa7ea1b2a52e335676bac82dc",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        virtual_build_env = VirtualBuildEnv(self)
        virtual_build_env.generate()

        yes_no = lambda v: "yes" if v else "no"
        tc = AutotoolsToolchain(self)
        tc.configure_args.extend([
            f"--enable-debug={yes_no(self.settings.build_type == 'Debug')}",
            "--enable-builddir=no",
            "--enable-docs=no",
            "--enable-shared" if self.options.shared else "--disable-shared",
            "--disable-static" if self.options.shared else "--enable-static",
        ])

        if self.settings_build.compiler == "apple-clang":
            tc.configure_args.append("--disable-multi-os-directory")

        if self.options.shared:
            tc.extra_defines.append("FFI_BUILDING_DLL")
        if not self.options.shared:
            tc.extra_defines.append("FFI_STATIC_BUILD")

        env = tc.environment()
        if self.settings_build.os == "Windows" and self.settings.get_safe("compiler.runtime"):

            if is_msvc(self) and check_min_vs(self, "180", raise_invalid=False):
                # https://github.com/conan-io/conan/issues/6514
                tc.extra_cflags.append("-FS")

            if is_msvc_static_runtime(self):
                tc.extra_defines.append("USE_STATIC_RTL")
            if "d" in msvc_runtime_flag(self):
                tc.extra_defines.append("USE_DEBUG_RTL")

            architecture_flag = ""
            if is_msvc(self):
                if self.settings.arch == "x86_64":
                    architecture_flag = "-m64"
                elif self.settings.arch == "x86":
                    architecture_flag = "-m32"
                elif self.settings.arch == "armv8":
                    architecture_flag = "-marm64"
            elif self.settings.compiler == "clang":
                architecture_flag = "-clang-cl"

            compile_wrapper = unix_path(self, os.path.join(self.source_folder, "msvcc.sh"))
            if architecture_flag:
                compile_wrapper = f"{compile_wrapper} {architecture_flag}"

            ar_wrapper = unix_path(self, self.dependencies.build["automake"].conf_info.get("user.automake:lib-wrapper"))
            env.define("CC", f"{compile_wrapper}")
            env.define("CXX", f"{compile_wrapper}")
            env.define("LD", "link -nologo")
            env.define("AR", f"{ar_wrapper} \"lib -nologo\"")
            env.define("NM", "dumpbin -symbols")
            env.define("OBJDUMP", ":")
            env.define("RANLIB", ":")
            env.define("STRIP", ":")
            env.define("CXXCPP", "cl -nologo -EP")
            env.define("CPP", "cl -nologo -EP")
            env.define("LIBTOOL", unix_path(self, os.path.join(self.source_folder, "ltmain.sh")))
            env.define("INSTALL", unix_path(self, os.path.join(self.source_folder, "install-sh")))
        tc.generate(env=env)

    def build(self):

        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        autotools = Autotools(self)
        autotools.install(args=[f"DESTDIR={unix_path(self, self.package_folder)}"])  # Need to specify the `DESTDIR` as a Unix path, aware of the subsystem
        fix_apple_shared_install_name(self)
        mkdir(self, os.path.join(self.package_folder, "bin"))
        for dll in glob.glob(os.path.join(self.package_folder, "lib", "*.dll")):
            shutil.move(dll, os.path.join(self.package_folder, "bin"))
        if is_msvc(self) and self.options.shared:
            for lib_path in glob.glob(os.path.join(self.package_folder, "lib", "*.dll.lib")):
                libname = os.path.basename(lib_path)[:-len(".dll.lib")]
                dst = os.path.join(self.package_folder, "lib", f"{libname}.lib")
                if os.path.isfile(dst):
                    os.remove(dst)
                shutil.move(lib_path, dst)
        elif is_msvc(self) and not self.options.shared:
            for a_path in glob.glob(os.path.join(self.package_folder, "lib", "*.a")):
                libname = os.path.basename(a_path)[:-2]  # strip .a
                dst = os.path.join(self.package_folder, "lib", f"{libname}.lib")
                if os.path.isfile(dst):
                    os.remove(dst)
                shutil.move(a_path, dst)
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        rm(self, "*.la", os.path.join(self.package_folder, "lib"), recursive=True)
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.libs = ["{}ffi".format("lib" if is_msvc(self) else "")]
        self.cpp_info.set_property("pkg_config_name", "libffi")
        if not self.options.shared:
            static_define = "FFI_STATIC_BUILD"
            self.cpp_info.defines = [static_define]
