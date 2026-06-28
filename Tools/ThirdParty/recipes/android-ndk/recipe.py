import os
from pathlib import Path
import zipfile

from thirdparty import RecipeBase
from thirdparty.files import copy, download, rm, unzip
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository

_SOURCES = {
    "Linux": {
        "X64": {
            "url": "https://dl.google.com/android/repository/android-ndk-r29-linux.zip",
            "sha256": "4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf",
        },
    },
    "Mac": {
        "X64": {
            "url": "https://dl.google.com/android/repository/android-ndk-r29-darwin.zip",
            "sha256": "ce5e4b100ec5fe5be4eb3edcb2c02528824ff9cda3860f5304619be6c3da34d3",
        },
    },
    "Windows": {
        "X64": {
            "url": "https://dl.google.com/android/repository/android-ndk-r29-windows.zip",
            "sha256": "4f83a1a87ea0d33ae2b43812ce27b768be949bc78acf90b955134d19e3068f1c",
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
    def _arch(self):
        if self.settings.os == "Mac":
            return "X64"
        return str(self.settings.arch)

    @property
    def _host_tag(self):
        host_os = {
            "Linux": "linux",
            "Mac": "darwin",
            "Windows": "windows",
        }.get(str(self.settings.os))
        return f"{host_os}-x86_64"

    @property
    def _toolchain_bin(self):
        return self.folders.package / "bin" / "toolchains" / "llvm" / "prebuilt" / self._host_tag / "bin"

    def _tool_exe(self, name: str):
        suffix = ".exe" if self.settings.os == "Windows" else ""
        return self._toolchain_bin / f"{name}{suffix}"

    def _fix_permissions(self):
        if os.name != "posix":
            return
        for root, _, files in os.walk(self.folders.package / "bin"):
            for filename in files:
                filepath = os.path.join(root, filename)
                with open(filepath, "rb") as f:
                    sig = list(f.read(4))
                if len(sig) > 2 and sig[0] == 0x23 and sig[1] == 0x21:
                    _chmod_plus_x(filepath)
                elif sig == [0x7F, 0x45, 0x4C, 0x46]:
                    _chmod_plus_x(filepath)
                elif sig[:4] in (
                        [0xCA, 0xFE, 0xBA, 0xBE], [0xBE, 0xBA, 0xFE, 0xCA],
                        [0xFE, 0xED, 0xFA, 0xCF], [0xCF, 0xFA, 0xED, 0xFE],
                        [0xFE, 0xEF, 0xFA, 0xCE], [0xCE, 0xFA, 0xED, 0xFE],
                ):
                    _chmod_plus_x(filepath)

    def _unzip_fix_symlinks(
        self,
        url: str,
        target_folder: Path,
        sha256: str):
        filename = "android_sdk.zip"
        download(self, url, filename, sha256=sha256)
        unzip(self, filename, destination=target_folder, strip_root=True)

        def _is_symlink_zipinfo(zi: zipfile.ZipInfo) -> bool:
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
        data = _SOURCES[str(self.settings.os)][self._arch]
        self._unzip_fix_symlinks(
            url=data["url"],
            target_folder=self.folders.source,
            sha256=data["sha256"])

    def package(self):
        copy(self, "*", src=self.folders.source, dst=self.folders.package / "bin")
        copy(self, "*NOTICE", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "*NOTICE.toolchain", src=self.folders.source, dst=self.folders.package / "licenses")
        self._fix_permissions()
        rm(self, "*Config.cmake", self.folders.package / "bin", recursive=True)
        rm(self, "*-config.cmake", self.folders.package / "bin", recursive=True)
        rm(self, "Find*.cmake", self.folders.package / "bin", recursive=True)

    def package_info(self):
        self.info.includedirs = []
        self.info.libdirs = []

        ndk_root = self.folders.package / "bin"
        self.buildenv_info.define_path("ANDROID_NDK_ROOT", ndk_root)
        self.buildenv_info.define_path("ANDROID_NDK_HOME", ndk_root)
        self.buildenv_info.define_path("NDK_ROOT", ndk_root)
        self.conf_info.define("tools.android:ndk_path", ndk_root)
        self.buildenv_info.define_path("AR", self._tool_exe("llvm-ar"))
        self.buildenv_info.define_path("RANLIB", self._tool_exe("llvm-ranlib"))
        self.buildenv_info.define_path("STRIP", self._tool_exe("llvm-strip"))
        self.buildenv_info.define_path("ADDR2LINE", self._tool_exe("llvm-addr2line"))
        self.buildenv_info.define_path("NM", self._tool_exe("llvm-nm"))
        self.buildenv_info.define_path("OBJCOPY", self._tool_exe("llvm-objcopy"))
        self.buildenv_info.define_path("OBJDUMP", self._tool_exe("llvm-objdump"))
        self.buildenv_info.define_path("READELF", self._tool_exe("llvm-readelf"))
        self.buildenv_info.define_path("ELFEDIT", self._tool_exe("llvm-elfedit"))


def _chmod_plus_x(filename: str):
    if os.name == "posix":
        os.chmod(filename, os.stat(filename).st_mode | 0o111)
