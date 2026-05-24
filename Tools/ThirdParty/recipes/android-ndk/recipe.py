import os
import re
import zipfile

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, download, rm, unzip
from thirdparty.tools.scm import Version
from thirdparty.tools.scm.github import GithubRepository


_SOURCES = {
    "r29": {
        "Windows": {
            "x86_64": {
                "url": "https://dl.google.com/android/repository/android-ndk-r29-windows.zip",
                "sha256": "4f83a1a87ea0d33ae2b43812ce27b768be949bc78acf90b955134d19e3068f1c",
            },
        },
        "Linux": {
            "x86_64": {
                "url": "https://dl.google.com/android/repository/android-ndk-r29-linux.zip",
                "sha256": "4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf",
            },
        },
        "Macos": {
            "x86_64": {
                "url": "https://dl.google.com/android/repository/android-ndk-r29-darwin.zip",
                "sha256": "ce5e4b100ec5fe5be4eb3edcb2c02528824ff9cda3860f5304619be6c3da34d3",
            },
        },
    },
}


class Recipe(RecipeBase):
    name = "android-ndk"
    version = "r29"
    license = "Apache-2.0"

    def latest_version(self):
        repo = GithubRepository(self, "android/ndk")
        return Version(repo.latest_release)

    @property
    def _ndk_major_minor(self):
        match = re.search(r"r(\d+)(\w?)", self.version)
        assert match
        major, minor = match.groups()
        return int(major), minor if minor else "a"

    @property
    def _ndk_version_major(self):
        return self._ndk_major_minor[0]

    @property
    def _platform(self):
        return {
            "Linux": "linux",
            "Macos": "darwin",
            "Windows": "windows",
        }.get(str(self.settings.os))

    @property
    def _arch(self):
        if self.settings.os == "Macos":
            return "x86_64"
        return str(self.settings.arch)

    @property
    def _host(self):
        return f"{self._platform}-{self._arch}"

    @property
    def _ndk_root_rel_path(self):
        return os.path.join("bin", "toolchains", "llvm", "prebuilt", self._host)

    @property
    def _ndk_root(self):
        return os.path.join(self.package_folder, self._ndk_root_rel_path)

    @property
    def _android_abi(self):
        return {
            "armv7": "armeabi-v7a",
            "armv8": "arm64-v8a",
            "x86": "x86",
            "x86_64": "x86_64",
        }.get(str(self.settings_target.arch))

    @property
    def _llvm_triplet(self):
        arch = {
            "armv7": "arm",
            "armv8": "aarch64",
            "x86": "i686",
            "x86_64": "x86_64",
        }.get(str(self.settings_target.arch))
        abi = "androideabi" if self.settings_target.arch == "armv7" else "android"
        return f"{arch}-linux-{abi}"

    @property
    def _clang_triplet(self):
        arch = {
            "armv7": "armv7a",
            "armv8": "aarch64",
            "x86": "i686",
            "x86_64": "x86_64",
        }.get(str(self.settings_target.arch))
        abi = "androideabi" if self.settings_target.arch == "armv7" else "android"
        return f"{arch}-linux-{abi}"

    def _wrap_executable(self, tool):
        suffix = ".exe" if self.settings.os == "Windows" else ""
        return f"{tool}{suffix}"

    def _tool_name(self, tool, bare=False):
        if "clang" in tool:
            suffix = ".cmd" if self.settings.os == "Windows" else ""
            prefix = "llvm" if bare else f"{self._clang_triplet}{self.settings_target.os.api_level}"
            return f"{prefix}-{tool}{suffix}"
        else:
            prefix = "llvm" if bare else f"{self._llvm_triplet}"
            return self._wrap_executable(f"{prefix}-{tool}")

    def _define_tool_var(self, name, value, bare=False):
        ndk_bin = os.path.join(self._ndk_root, "bin")
        path = os.path.join(ndk_bin, self._tool_name(value, bare))
        if not os.path.isfile(path):
            self.output.error(f"Environment variable {name} could not be set: '{path}' not found")
            return "UNKNOWN"
        return path

    def _define_tool_var_naked(self, name, value):
        ndk_bin = os.path.join(self._ndk_root, "bin")
        path = os.path.join(ndk_bin, self._wrap_executable(value))
        if not os.path.isfile(path):
            self.output.error(f"Environment variable {name} could not be set: '{path}' not found")
            return "UNKNOWN"
        return path

    @staticmethod
    def _chmod_plus_x(filename):
        if os.name == "posix":
            os.chmod(filename, os.stat(filename).st_mode | 0o111)

    def _fix_permissions(self):
        if os.name != "posix":
            return
        for root, _, files in os.walk(os.path.join(self.package_folder, "bin")):
            for filename in files:
                filepath = os.path.join(root, filename)
                with open(filepath, "rb") as f:
                    sig = list(f.read(4))
                if len(sig) > 2 and sig[0] == 0x23 and sig[1] == 0x21:
                    self._chmod_plus_x(filepath)
                elif sig == [0x7F, 0x45, 0x4C, 0x46]:
                    self._chmod_plus_x(filepath)
                elif sig[:4] in (
                    [0xCA, 0xFE, 0xBA, 0xBE], [0xBE, 0xBA, 0xFE, 0xCA],
                    [0xFE, 0xED, 0xFA, 0xCF], [0xCF, 0xFA, 0xED, 0xFE],
                    [0xFE, 0xEF, 0xFA, 0xCE], [0xCE, 0xFA, 0xED, 0xFE],
                ):
                    self._chmod_plus_x(filepath)

    def _unzip_fix_symlinks(self, url, target_folder, sha256):
        filename = "android_sdk.zip"
        download(self, url, filename, sha256=sha256)
        unzip(self, filename, destination=target_folder, strip_root=True)

        def _is_symlink_zipinfo(zi):
            return (zi.external_attr >> 28) == 0xA

        with zipfile.ZipFile(filename, "r") as z:
            zip_info = z.infolist()
            names = [n.replace("\\", "/") for n in z.namelist()]
            common_folder = os.path.commonprefix(names).split("/", 1)[0]
            for file_ in zip_info:
                if not _is_symlink_zipinfo(file_):
                    continue
                rel_name = file_.filename.replace("\\", "/").removeprefix(common_folder + "/")
                link_target = z.read(file_).decode("utf-8")
                link_path = os.path.normpath(os.path.join(target_folder, rel_name))
                os.remove(link_path)
                os.symlink(link_target, link_path)

    def source(self):
        pass

    def build(self):
        data = _SOURCES[self.version][str(self.settings.os)][self._arch]
        self._unzip_fix_symlinks(
            url=data["url"],
            target_folder=self.source_folder,
            sha256=data["sha256"])

    def package(self):
        copy(self, "*", src=self.source_folder, dst=os.path.join(self.package_folder, "bin"))
        copy(self, "*NOTICE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*NOTICE.toolchain", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "cmake-wrapper.cmd", src=os.path.join(self.source_folder, os.pardir), dst=os.path.join(self.package_folder, "bin"))
        copy(self, "cmake-wrapper", src=os.path.join(self.source_folder, os.pardir), dst=os.path.join(self.package_folder, "bin"))
        self._fix_permissions()
        rm(self, "*Config.cmake", os.path.join(self.package_folder, "bin"), recursive=True)
        rm(self, "*-config.cmake", os.path.join(self.package_folder, "bin"), recursive=True)
        rm(self, "Find*.cmake", os.path.join(self.package_folder, "bin"), recursive=True)

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []

        self.buildenv_info.define_path("ANDROID_NDK_ROOT", os.path.join(self.package_folder, "bin"))
        self.buildenv_info.define_path("ANDROID_NDK_HOME", os.path.join(self.package_folder, "bin"))

        if not hasattr(self, "settings_target") or self.settings_target is None:
            return
        if self.settings_target.os != "Android":
            return

        self.cpp_info.bindirs.append(os.path.join(self._ndk_root_rel_path, "bin"))
        self.buildenv_info.define_path("NDK_ROOT", self._ndk_root)
        self.buildenv_info.define("CHOST", self._llvm_triplet)

        ndk_sysroot = os.path.join(self._ndk_root, "sysroot")
        self.conf_info.define("tools.build:sysroot", ndk_sysroot)
        self.buildenv_info.define_path("SYSROOT", ndk_sysroot)
        self.buildenv_info.define("ANDROID_NATIVE_API_LEVEL", str(self.settings_target.os.api_level))
        self.conf_info.define("tools.android:ndk_path", os.path.join(self.package_folder, "bin"))

        compiler_executables = {
            "c": self._define_tool_var("CC", "clang"),
            "cpp": self._define_tool_var("CXX", "clang++"),
        }
        self.conf_info.update("tools.build:compiler_executables", compiler_executables)
        self.buildenv_info.define_path("CC", compiler_executables["c"])
        self.buildenv_info.define_path("CXX", compiler_executables["cpp"])

        bare = self._ndk_version_major >= 23
        self.buildenv_info.define_path("AR", self._define_tool_var("AR", "ar", bare))
        self.buildenv_info.define_path("RANLIB", self._define_tool_var("RANLIB", "ranlib", bare))
        self.buildenv_info.define_path("STRIP", self._define_tool_var("STRIP", "strip", bare))
        self.buildenv_info.define_path("ADDR2LINE", self._define_tool_var("ADDR2LINE", "addr2line", bare))
        self.buildenv_info.define_path("NM", self._define_tool_var("NM", "nm", bare))
        self.buildenv_info.define_path("OBJCOPY", self._define_tool_var("OBJCOPY", "objcopy", bare))
        self.buildenv_info.define_path("OBJDUMP", self._define_tool_var("OBJDUMP", "objdump", bare))
        self.buildenv_info.define_path("READELF", self._define_tool_var("READELF", "readelf", bare))

        self.buildenv_info.define("ANDROID_PLATFORM", f"android-{self.settings_target.os.api_level}")
        self.buildenv_info.define("ANDROID_TOOLCHAIN", "clang")
        self.buildenv_info.define("ANDROID_ABI", self._android_abi)
        libcxx_str = str(self.settings_target.compiler.libcxx)
        self.buildenv_info.define("ANDROID_STL", libcxx_str if libcxx_str.startswith("c++_") else "c++_shared")
