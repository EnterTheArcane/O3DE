from thirdparty import RecipeBase
from thirdparty.tools.env import VirtualBuildEnv
from thirdparty.tools.files import apply_conandata_patches, chdir, copy, get, replace_in_file, rmdir
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import NMakeToolchain, is_msvc
import os
import shutil

class Recipe(RecipeBase):
    name = "nasm"
    version = "3.01"
    license = "BSD-2-Clause"

 
    @property
    def _nasm(self):
        suffix = "w.exe" if is_msvc(self) else ""
        return os.path.join(self.package_folder, "bin", f"nasm{suffix}")

    @property
    def _ndisasm(self):
        suffix = "w.exe" if is_msvc(self) else ""
        return os.path.join(self.package_folder, "bin", f"ndisasm{suffix}")

    def _chmod_plus_x(self, filename):
        if os.name == "posix":
            os.chmod(filename, os.stat(filename).st_mode | 0o111)

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("strawberryperl")
            if not is_msvc(self):
                self.win_bash = True
                if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                    self.tool_requires("msys2")

    def source(self):
        get(
            self,
            url="https://www.nasm.us/pub/nasm/releasebuilds/3.01/nasm-3.01.tar.xz",
            sha256="b7324cbe86e767b65f26f467ed8b12ad80e124e3ccb89076855c98e43a9eddd4",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        if is_msvc(self):
            tc = NMakeToolchain(self)
            tc.generate()
        else:
            tc = AutotoolsToolchain(self)
            if self.settings.arch == "x86":
                tc.extra_cflags.append("-m32")
            elif self.settings.arch == "x86_64":
                tc.extra_cflags.append("-m64")
            tc.generate()

    def build(self):
        apply_conandata_patches(self)
        if is_msvc(self):
            with chdir(self, self.source_folder):
                self.run(f'nmake /f {os.path.join("Mkfiles", "msvc.mak")}')
        else:
            with chdir(self, self.source_folder):
                autotools = Autotools(self)
                autotools.configure()

                # GCC9 - "pure" attribute on function returning "void"
                replace_in_file(self, "Makefile", "-Werror=attributes", "")
                autotools.make()

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        if is_msvc(self):
            copy(self, pattern="*.exe", src=self.source_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
            with chdir(self, os.path.join(self.package_folder, "bin")):
                shutil.copy2("nasm.exe", "nasmw.exe")
                shutil.copy2("ndisasm.exe", "ndisasmw.exe")
        else:
            with chdir(self, self.source_folder):
                autotools = Autotools(self)
                autotools.install()
            rmdir(self, os.path.join(self.package_folder, "share"))
        self._chmod_plus_x(self._nasm)
        self._chmod_plus_x(self._ndisasm)

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []

        compiler_executables = {"asm": self._nasm}
        self.conf_info.update("tools.build:compiler_executables", compiler_executables)
        self.buildenv_info.define_path("NASM", self._nasm)
        self.buildenv_info.define_path("NDISASM", self._ndisasm)
        self.buildenv_info.define_path("AS", self._nasm)

        # TODO: Legacy, to be removed on Conan 2.0
