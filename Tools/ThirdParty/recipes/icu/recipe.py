import glob
import hashlib
import os
import shutil

from thirdparty import RecipeBase
from thirdparty.apple import is_apple_os
from thirdparty.build import cross_building, stdcpp_library
from thirdparty.env import Environment, VirtualBuildEnv
from thirdparty.files import apply_patches, copy, get, mkdir, rename, replace_in_file, rm, rmdir, save
from thirdparty.gnu import Autotools, AutotoolsToolchain
from thirdparty.microsoft import check_min_vs, is_msvc, unix_path
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "icu"
    version = "78.3"
    license = "ICU"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "data_packaging": ["files", "archive", "library", "static"],
        "with_dyload": [True, False],
        "dat_package_file": [None, "ANY"],
        "with_icuio": [True, False],
        "with_extras": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "data_packaging": "archive",
        "with_dyload": True,
        "dat_package_file": None,
        "with_icuio": True,
        "with_extras": False,
    }
 
    @property
    def _enable_icu_tools(self):
        return self.settings.os not in ["iOS", "tvOS", "watchOS", "Emscripten"]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
            del self.options.data_packaging

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.license = "Unicode-3.0"

    @staticmethod
    def _sha256sum(file_path):
        m = hashlib.sha256()
        with open(file_path, "rb") as fh:
            for data in iter(lambda: fh.read(8192), b""):
                m.update(data)
        return m.hexdigest()

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2")

        if cross_building(self) and hasattr(self, "settings_build"):
            self.tool_requires(str(self.ref))

    def latest_version(self):
        repo = GithubRepository(self, "unicode-org/icu")
        return Version(repo.latest_release.removeprefix("release-"))

    def source(self):
        get(
            self,
            url="https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-sources.tgz",
            sha256="3a2e7a47604ba702f345878308e6fefeca612ee895cf4a5f222e7955fabfe0c0",
            destination=self.source_folder,
            strip_root=True)

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = AutotoolsToolchain(self)
        if check_min_vs(self, "180", raise_invalid=False):
            tc.extra_cflags.append("-FS")
            tc.extra_cxxflags.append("-FS")
        if not self.settings.compiler.cppstd and is_msvc(self):
            tc.extra_cxxflags.append(f"-std:c++17")
        if not self.options.shared:
            tc.extra_defines.append("U_STATIC_IMPLEMENTATION")
        if is_apple_os(self):
            tc.extra_defines.append("_DARWIN_C_SOURCE")
        yes_no = lambda v: "yes" if v else "no"
        tc.configure_args.extend([
            "--datarootdir=${prefix}/lib", # do not use share
            f"--enable-release={yes_no(self.settings.build_type != 'Debug')}",
            f"--enable-debug={yes_no(self.settings.build_type == 'Debug')}",
            f"--enable-dyload={yes_no(self.options.with_dyload)}",
            f"--enable-extras={yes_no(self.options.with_extras)}",
            f"--enable-icuio={yes_no(self.options.with_icuio)}",
            "--disable-layoutex",
            "--disable-layout",
            f"--enable-tools={yes_no(self._enable_icu_tools)}",
            "--disable-tests",
            "--disable-samples",
        ])
        if cross_building(self):
            base_path = unix_path(self, self.dependencies.build["icu"].package_folder)
            tc.configure_args.append(f"--with-cross-build={base_path}")
            if self.settings.os in ["iOS", "tvOS", "watchOS"]:
                # ICU build scripts interpret all Apple platforms as 'darwin'.
                # Since this can coincide with the `build` triple, we need to tweak
                # the build triple to avoid the collision and ensure the scripts
                # know we are cross-building.
                host_triplet = f"{str(self.settings.arch)}-apple-darwin"
                build_triplet = f"{str(self.settings.arch)}-apple"
                tc.update_configure_args({"--host": host_triplet,
                                          "--build": build_triplet})
        else:
            arch64 = ["x86_64", "sparcv9", "ppc64", "ppc64le", "armv8", "armv8.3", "mips64"]
            bits = "64" if self.settings.arch in arch64 else "32"
            tc.configure_args.append(f"--with-library-bits={bits}")
        if self.settings.os != "Windows":
            # http://userguide.icu-project.org/icudata
            # This is the only directly supported behavior on Windows builds.
            tc.configure_args.append(f"--with-data-packaging={self.options.data_packaging}")
        tc.generate()

        if is_msvc(self):
            env = Environment()
            env.define("CC", "cl -nologo")
            env.define("CXX", "cl -nologo")
            if cross_building(self):
                env.define("icu_cv_host_frag", "mh-msys-msvc")
            env.vars(self).save_script("conanbuild_icu_msvc")

    def _patch_sources(self):

        replace_in_file(
                self,
                os.path.join(self.source_folder, "source", "configure"),
                "if test -z \"$PYTHON\"",
                "if true",
        )

        if self.settings.os == "Windows":
            # https://unicode-org.atlassian.net/projects/ICU/issues/ICU-20545
            makeconv_cpp = os.path.join(self.source_folder, "source", "tools", "makeconv", "makeconv.cpp")
            replace_in_file(self, makeconv_cpp,
                            "pathBuf.appendPathPart(arg, localError);",
                            "pathBuf.append(\"/\", localError); pathBuf.append(arg, localError);")

        # relocatable shared libs on macOS
        mh_darwin = os.path.join(self.source_folder, "source", "config", "mh-darwin")
        replace_in_file(self, mh_darwin, "-install_name $(libdir)/$(notdir", "-install_name @rpath/$(notdir")
        replace_in_file(self,
            mh_darwin,
            "-install_name $(notdir $(MIDDLE_SO_TARGET)) $(PKGDATA_TRAILING_SPACE)",
            "-install_name @rpath/$(notdir $(MIDDLE_SO_TARGET))",
        )

        # workaround for https://unicode-org.atlassian.net/browse/ICU-20531
        mkdir(self, os.path.join(self.build_folder, "data", "out", "tmp"))

        # workaround for "No rule to make target 'out/tmp/dirs.timestamp'"
        save(self, os.path.join(self.build_folder, "data", "out", "tmp", "dirs.timestamp"), "")

    def build(self):
        self._patch_sources()

        if self.options.dat_package_file:
            dat_package_file = glob.glob(os.path.join(self.source_folder, "source", "data", "in", "*.dat"))
            if dat_package_file:
                shutil.copy(str(self.options.dat_package_file), dat_package_file[0])

        autotools = Autotools(self)
        autotools.configure(build_script_folder=os.path.join(self.source_folder, "source"))
        autotools.make()

    @property
    def _data_filename(self):
        vtag = Version(self.version).major
        arch = self.settings.get_safe("arch")
        suffix = "b" if arch in {"ppc32", "ppc64",
                                 "sparc", "sparcv9",
                                 "s390", "s390x",
                                 "mips", "mips64"} else "l"
        return f"icudt{vtag}{suffix}.dat"

    @property
    def _data_path(self):
        data_dir_name = "icu"
        if self.settings.os == "Windows" and self.settings.build_type == "Debug":
            data_dir_name += "d"
        data_dir = os.path.join(self.package_folder, "lib", data_dir_name, str(self.version))
        return os.path.join(data_dir, self._data_filename)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()

        dll_files = glob.glob(os.path.join(self.package_folder, "lib", "*.dll"))
        if dll_files:
            bin_dir = os.path.join(self.package_folder, "bin")
            mkdir(self, bin_dir)
            for dll in dll_files:
                dll_name = os.path.basename(dll)
                rm(self, dll_name, bin_dir)
                rename(self, src=dll, dst=os.path.join(bin_dir, dll_name))

        if self.settings.os != "Windows" and self.options.data_packaging in ["files", "archive"]:
            mkdir(self, os.path.join(self.package_folder, "res"))
            rename(self, src=self._data_path, dst=os.path.join(self.package_folder, "res", self._data_filename))

        # Copy some files required for cross-compiling
        config_dir = os.path.join(self.package_folder, "config")
        copy(self, "icucross.mk", src=os.path.join(self.build_folder, "config"), dst=config_dir)
        copy(self, "icucross.inc", src=os.path.join(self.build_folder, "config"), dst=config_dir)

        rmdir(self, os.path.join(self.package_folder, "lib", "icu"))
        rmdir(self, os.path.join(self.package_folder, "lib", "man"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "ICU")

        prefix = "s" if self.settings.os == "Windows" and not self.options.shared else ""
        suffix = "d" if self.settings.os == "Windows" and self.settings.build_type == "Debug" else ""

        # icudata
        self.cpp_info.components["icu-data"].set_property("cmake_target_name", "ICU::data")
        icudata_libname = "icudt" if self.settings.os == "Windows" else "icudata"
        self.cpp_info.components["icu-data"].libs = [f"{prefix}{icudata_libname}{suffix}"]
        if not self.options.shared:
            self.cpp_info.components["icu-data"].defines.append("U_STATIC_IMPLEMENTATION")
            # icu uses c++, so add the c++ runtime
            libcxx = stdcpp_library(self)
            if libcxx:
                self.cpp_info.components["icu-data"].system_libs.append(libcxx)

        # Alias of data CMake component
        self.cpp_info.components["icu-data-alias"].set_property("cmake_target_name", "ICU::dt")
        self.cpp_info.components["icu-data-alias"].requires = ["icu-data"]

        # icuuc
        self.cpp_info.components["icu-uc"].set_property("cmake_target_name", "ICU::uc")
        self.cpp_info.components["icu-uc"].set_property("pkg_config_name", "icu-uc")
        self.cpp_info.components["icu-uc"].libs = [f"{prefix}icuuc{suffix}"]
        self.cpp_info.components["icu-uc"].requires = ["icu-data"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["icu-uc"].system_libs = ["m", "pthread"]
            if self.options.with_dyload:
                self.cpp_info.components["icu-uc"].system_libs.append("dl")
        elif self.settings.os == "Windows":
            self.cpp_info.components["icu-uc"].system_libs = ["advapi32"]

        # icui18n
        self.cpp_info.components["icu-i18n"].set_property("cmake_target_name", "ICU::i18n")
        self.cpp_info.components["icu-i18n"].set_property("pkg_config_name", "icu-i18n")
        icui18n_libname = "icuin" if self.settings.os == "Windows" else "icui18n"
        self.cpp_info.components["icu-i18n"].libs = [f"{prefix}{icui18n_libname}{suffix}"]
        self.cpp_info.components["icu-i18n"].requires = ["icu-uc"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["icu-i18n"].system_libs = ["m"]

        # Alias of i18n CMake component
        self.cpp_info.components["icu-i18n-alias"].set_property("cmake_target_name", "ICU::in")
        self.cpp_info.components["icu-i18n-alias"].requires = ["icu-i18n"]

        # icuio
        if self.options.with_icuio:
            self.cpp_info.components["icu-io"].set_property("cmake_target_name", "ICU::io")
            self.cpp_info.components["icu-io"].set_property("pkg_config_name", "icu-io")
            self.cpp_info.components["icu-io"].libs = [f"{prefix}icuio{suffix}"]
            self.cpp_info.components["icu-io"].requires = ["icu-i18n", "icu-uc"]

        if self.settings.os != "Windows" and self.options.data_packaging in ["files", "archive"]:
            self.cpp_info.components["icu-data"].resdirs = ["res"]
            data_path = os.path.join(self.package_folder, "res", self._data_filename).replace("\\", "/")
            self.runenv_info.prepend_path("ICU_DATA", data_path)
            if self._enable_icu_tools or self.options.with_extras:
                self.buildenv_info.prepend_path("ICU_DATA", data_path)

        if self._enable_icu_tools:
            # icutu
            self.cpp_info.components["icu-tu"].set_property("cmake_target_name", "ICU::tu")
            self.cpp_info.components["icu-tu"].libs = [f"{prefix}icutu{suffix}"]
            self.cpp_info.components["icu-tu"].requires = ["icu-i18n", "icu-uc"]
            if self.settings.os in ["Linux", "FreeBSD"]:
                self.cpp_info.components["icu-tu"].system_libs = ["pthread"]

            # icutest
            self.cpp_info.components["icu-test"].set_property("cmake_target_name", "ICU::test")
            self.cpp_info.components["icu-test"].libs = [f"{prefix}icutest{suffix}"]
            self.cpp_info.components["icu-test"].requires = ["icu-tu", "icu-uc"]
