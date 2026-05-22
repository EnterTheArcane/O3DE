# Ported from conan-center-index/icu by port_recipe.py
# REVIEW: verify all transforms are correct before building

import glob
import hashlib
import os
import shutil

from thirdparty import RecipeBase
from thirdparty.tools.env import VirtualBuildEnv, VirtualRunEnv
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.files import apply_patches, copy, get, mkdir, rename, replace_in_file, rm, rmdir, save
from thirdparty.tools.gnu import Autotools, AutotoolsToolchain
from thirdparty.tools.microsoft import check_min_vs, is_msvc, unix_path
from thirdparty.tools.scm import Version

class Recipe(RecipeBase):
    name = "icu"
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
        "shared": False,
        "fPIC": True,
        "data_packaging": "archive",
        "with_dyload": True,
        "dat_package_file": None,
        "with_icuio": True,
        "with_extras": False,
    }

    @property
    def _min_cppstd(self):
        return 17

    @property
    def _compilers_minimum_version(self):
        return {
            "gcc": "8",
            "clang": "7",
            "apple-clang": "12",
            "Visual Studio": "16",
            "msvc": "192",
        }

    @property
    def _settings_build(self):
        return getattr(self, "settings_build", self.settings)

    @property
    def _enable_icu_tools(self):
        return self.settings.os not in ["iOS", "tvOS", "watchOS", "Emscripten"]

    @staticmethod
    def _sha256sum(file_path):
        m = hashlib.sha256()
        with open(file_path, "rb") as fh:
            for data in iter(lambda: fh.read(8192), b""):
                m.update(data)
        return m.hexdigest()

    def source(self):
        get(url=self.thirdparty_data["versions"][self.version]["url"], dest=self.source_folder, sha256=self.thirdparty_data["versions"][self.version]["sha256"])

    def _source_dir(self):
        """ICU tarball extracts to icu/ then source/ — returns the source/ path."""
        return os.path.join(self.source_folder, "source")

    def generate(self):
        pass  # No CMake toolchain needed; Windows uses msbuild, others use autotools

    def _patch_sources(self):
        apply_patches(self)

        replace_in_file(
                self,
                os.path.join(self.source_folder, "source", "configure"),
                "if test -z \"$PYTHON\"",
                "if true",
        )

        if self._settings_build.os == "Windows":
            # https://unicode-org.atlassian.net/projects/ICU/issues/ICU-20545
            makeconv_cpp = os.path.join(self.source_folder, "source", "tools", "makeconv", "makeconv.cpp")
            replace_in_file(makeconv_cpp,
                            "pathBuf.appendPathPart(arg, localError);",
                            "pathBuf.append(\"/\", localError); pathBuf.append(arg, localError);")

        # relocatable shared libs on macOS
        mh_darwin = os.path.join(self.source_folder, "source", "config", "mh-darwin")
        replace_in_file(mh_darwin, "-install_name $(libdir)/$(notdir", "-install_name @rpath/$(notdir")
        replace_in_file(mh_darwin,
            "-install_name $(notdir $(MIDDLE_SO_TARGET)) $(PKGDATA_TRAILING_SPACE)",
            "-install_name @rpath/$(notdir $(MIDDLE_SO_TARGET))",
        )

        # workaround for https://unicode-org.atlassian.net/browse/ICU-20531
        mkdir(self, os.path.join(self.build_folder, "data", "out", "tmp"))

        # workaround for "No rule to make target 'out/tmp/dirs.timestamp'"
        save(os.path.join(self.build_folder, "data", "out", "tmp", "dirs.timestamp"), "")

    def _find_msbuild(self):
        """Locate MSBuild.exe via vswhere.exe."""
        import subprocess as _sp
        vswhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
        result = _sp.run(
            [vswhere, "-latest", "-requires", "Microsoft.Component.MSBuild",
             "-find", r"MSBuild\**\Bin\MSBuild.exe"],
            capture_output=True, text=True
        )
        for line in result.stdout.strip().splitlines():
            if os.path.isfile(line.strip()):
                return line.strip()
        raise RuntimeError("MSBuild.exe not found. Run from a VS Developer prompt or install Build Tools.")

    def build(self):
        if self.is_windows:
            import subprocess
            msbuild = self._find_msbuild()
            sln = os.path.join(self._source_dir(), "allinone", "allinone.sln")
            subprocess.run([
                msbuild, sln,
                "/p:Configuration=Release",
                "/p:Platform=x64",
                "/p:SkipUWP=TRUE",
                "/m",
                "/nologo",
                "/v:minimal",
            ], cwd=self._source_dir(), check=True)
        else:
            self._patch_sources()
            autotools = Autotools(self)
            autotools.configure(build_script_folder=self._source_dir())
            autotools.make()

    def package(self):
        copy("LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        if self.is_windows:
            src = self._source_dir()
            # Headers from each module go to include/unicode/
            for hdr_dir in ["common", "i18n", "io"]:
                copy("*.h", src=os.path.join(src, hdr_dir, "unicode"),
                     dst=os.path.join(self.package_folder, "include", "unicode"),
                     keep_path=False)
            # DLLs + import libs
            vtag = Version(self.version).major
            bin64 = os.path.join(src, "bin64")
            lib64 = os.path.join(src, "lib64")
            for f in glob.glob(os.path.join(bin64, f"icu*{vtag}.dll")):
                copy(os.path.basename(f), src=bin64,
                     dst=os.path.join(self.package_folder, "bin"), keep_path=False)
            for f in glob.glob(os.path.join(lib64, f"icu*.lib")):
                copy(os.path.basename(f), src=lib64,
                     dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        else:
            autotools = Autotools(self)
            autotools.install()
            rmdir(os.path.join(self.package_folder, "lib", "icu"))
            rmdir(os.path.join(self.package_folder, "lib", "man"))
            rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
            rmdir(os.path.join(self.package_folder, "share"))
