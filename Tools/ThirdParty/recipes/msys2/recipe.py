import ctypes
import errno
import fnmatch
import os
import shutil
import subprocess
from typing import Any

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.errors import RecipeException, RecipeInvalidConfiguration
from thirdparty.files import chdir, get, replace_in_file, copy, rm


# ``ctypes.windll`` / ``ctypes.WinError`` only exist on Windows; OpLock is Windows-only and the
# cross-platform stubs don't expose them, so reach them dynamically as ``Any``.
_windll: Any = getattr(ctypes, "windll", None)
_win_error: Any = getattr(ctypes, "WinError", None)


class OpLock:
    def __init__(self):
        self.handle = _windll.kernel32.CreateMutexA(None, 0, "Global\\RecipeMSYS2".encode())
        if not self.handle:
            raise _win_error()

    def __enter__(self):
        status = _windll.kernel32.WaitForSingleObject(self.handle, 0xFFFFFFFF)
        if status not in [0, 0x80]:
            raise _win_error()

    def __exit__(
        self,
        exc_type: object,
        exc_val: object,
        exc_tb: object):
        status = _windll.kernel32.ReleaseMutex(self.handle)
        if not status:
            raise _win_error()

    def close(self):
        _windll.kernel32.CloseHandle(self.handle)

    __del__ = close


class _Options(RecipeOptions):
    exclude_files: str = "*/link.exe"
    packages: Any
    additional_packages: str | None = None
    no_kill: bool = False


class Recipe(RecipeBase[_Options]):
    name = "msys2"
    version = "latest"
    license = "MSYS license"

    def configure(self):
        default_packages = "base-devel,binutils,gcc,mingw-w64-cross-mingwarm64-gcc"
        self.options.packages = default_packages

    def validate(self):
        if self.settings.os != "Windows":
            raise RecipeInvalidConfiguration("msys2 is only supported on Windows")

    def source(self):
        get(
            self,
            url="https://github.com/msys2/msys2-installer/releases/download/2025-12-13/msys2-base-x86_64-20251213.tar.xz",
            sha256="999f63c2fc7525af5cd41b55e9ea704471a4f9d0278a257fff3b0d1183c441b9",
            destination=self.folders.source,
            strip_root=False)  # Preserve tarball root dir (msys64/)

    def build(self):
        with OpLock():
            self._do_build()

    def package(self):
        excludes = None
        if self.options.exclude_files:
            excludes = tuple(str(self.options.exclude_files).split(","))
        rm(self, "mtab", self.folders.source, recursive=True)
        for exclude in (excludes or ()):
            for root, _, filenames in os.walk(self._msys_dir):
                for filename in filenames:
                    fullname = os.path.join(root, filename)
                    if fnmatch.fnmatch(fullname, exclude):
                        os.unlink(fullname)
        # See https://github.com/recipe-io/recipe-center-index/blob/master/docs/error_knowledge_base.md#kb-h013-default-package-layout
        copy(self, "*", dst=self.folders.package / "bin" / "msys64", src=self._msys_dir, excludes=excludes)
        shutil.copytree(
            self._msys_dir / "usr" / "share" / "licenses",
            self.folders.package / "licenses")

    def package_info(self):
        self.info.libdirs = []
        self.info.includedirs = []

        msys_root = self.folders.package / "bin" / "msys64"
        msys_bin = msys_root / "usr" / "bin"
        self.info.bindirs.append(msys_bin)

        self.buildenv_info.define_path("MSYS_ROOT", msys_root)
        self.buildenv_info.define_path("MSYS_BIN", msys_bin)

        self.conf_info.define("tools.microsoft.bash:path", msys_bin / "bash.exe")

    def compatibility(self):
        if self.settings.arch == "ARM":
            # Fallback on x86_64 package when natively on Windows arm64
            return [{"settings": [("arch", "X64")]}]

    def _update_pacman(self):
        with chdir(self, self._msys_dir / "usr" / "bin"):
            try:
                self._kill_pacman()

                # https://www.msys2.org/docs/ci/
                self.run('bash -l -c "pacman --debug --noconfirm --ask 20 -Syuu"')  # Core update (in case any core packages are outdated)
                self._kill_pacman()
                self.run('bash -l -c "pacman --debug --noconfirm --ask 20 -Syuu"')  # Normal update
                self._kill_pacman()
                self.run('bash -l -c "pacman --debug -Rc dash --noconfirm"')
            except RecipeException:
                self.run('bash -l -c "cat /var/log/pacman.log || echo nolog"')
                self._kill_pacman()
                raise

    # https://github.com/msys2/MSYS2-packages/issues/1966
    def _kill_pacman(self):
        if self.options.no_kill:
            return
        if (self.settings.os == "Windows"):
            taskkill_exe: str = os.path.join(os.environ.get("SystemRoot", ""), "system32", "taskkill.exe")

            log_out = True
            if log_out:
                out = subprocess.PIPE
                err = subprocess.STDOUT
            else:
                out = open(os.devnull, "w", encoding="UTF-8")
                err = subprocess.PIPE

            if os.path.exists(taskkill_exe):
                taskkill_cmds = [
                    f"{taskkill_exe} /f /t /im pacman.exe",
                    f"{taskkill_exe} /f /im gpg-agent.exe",
                    f"{taskkill_exe} /f /im dirmngr.exe",
                    f'{taskkill_exe} /fi "MODULES eq msys-2.0.dll"',
                ]
                for taskkill_cmd in taskkill_cmds:
                    try:
                        proc = subprocess.Popen(taskkill_cmd, stdout=out, stderr=err, bufsize=1)
                        proc.wait()
                    except OSError as e:
                        if e.errno == errno.ENOENT:
                            raise RecipeException("Cannot kill pacman") from e

    @property
    def _msys_dir(self):
        subdir = "msys64"  # top-level directoy in tarball
        return self.folders.source / subdir

    def _do_build(self):
        packages: list[str] = []
        if self.options.packages:
            packages.extend(str(self.options.packages).split(","))
        if self.options.additional_packages:
            packages.extend(str(self.options.additional_packages).split(","))

        self._update_pacman()

        with chdir(self, self._msys_dir / "usr" / "bin"):
            for package in packages:
                self.run(f'bash -l -c "pacman -S {package} --noconfirm"')
            for package in ["pkgconf"]:
                if self.run(f'bash -l -c "pacman -Qq {package}"', ignore_errors=True, quiet=True) == 0:
                    self.run(f'bash -l -c "pacman -Rs -d -d {package} --noconfirm"')
            self.run(f'bash -l -c "pacman -Scc --noconfirm"')

        self._kill_pacman()

        # create /tmp dir in order to avoid
        # bash.exe: warning: could not find /tmp, please create!
        tmp_dir = self._msys_dir / "tmp"
        if not os.path.isdir(tmp_dir):
            os.makedirs(tmp_dir)
        tmp_name = tmp_dir / "dummy"
        with open(tmp_name, "a", encoding="UTF-8"):
            os.utime(tmp_name, None)

        # Prepend the PKG_CONFIG_PATH environment variable with an eventual PKG_CONFIG_PATH environment variable
        # Note: this is no longer needed when we exclusively support Recipe 2 integrations
        replace_in_file(
            self, self._msys_dir / "etc" / "profile",
            'PKG_CONFIG_PATH="', 'PKG_CONFIG_PATH="${PKG_CONFIG_PATH:+${PKG_CONFIG_PATH}:}')
