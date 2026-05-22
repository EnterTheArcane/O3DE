# Custom OpenSSL builder — Windows (Perl + nmake)
import os
import subprocess
from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rm, rmdir


class Recipe(RecipeBase):
    name = "openssl"
    version = "3.5.6"
    license = "Apache-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "no_asm": [True, False],
        "no_tests": [True, False],
        "no_deprecated": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "no_asm": False,
        "no_tests": True,
        "no_deprecated": False,
    }

    def requirements(self) -> list[str]:
        return []  # zlib opt-in not needed for our usage

    def source(self):
        get(
            url="https://github.com/openssl/openssl/releases/download/openssl-3.5.6/openssl-3.5.6.tar.gz",
            dest=self.source_folder,
            sha256="deae7c80cba99c4b4f940ecadb3c3338b13cb77418409238e57d7f31f2a3b736",
        )

    def _find_vcvarsall(self) -> str:
        """Locate vcvarsall.bat via vswhere.exe."""
        vswhere = (
            r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
        )
        result = subprocess.run(
            [vswhere, "-latest", "-property", "installationPath"],
            capture_output=True,
            text=True,
        )
        vs_path = result.stdout.strip()
        vcvarsall = os.path.join(vs_path, "VC", "Auxiliary", "Build", "vcvarsall.bat")
        if not os.path.isfile(vcvarsall):
            raise RuntimeError(f"vcvarsall.bat not found at: {vcvarsall}")
        return vcvarsall

    def _run(self, cmd, cwd: str) -> None:
        vcvarsall = self._find_vcvarsall()
        if isinstance(cmd, list):
            cmd_str = subprocess.list2cmdline(cmd)
        else:
            cmd_str = cmd
        # Wrap in vcvarsall to get MSVC environment (nmake, cl.exe, etc.)
        wrapped = f'call "{vcvarsall}" x64 && {cmd_str}'
        result = subprocess.run(wrapped, cwd=cwd, check=False, shell=True)
        if result.returncode != 0:
            raise RuntimeError(f"Command failed (exit {result.returncode}): {cmd_str}")

    def _configure_args(self) -> list[str]:
        args = ["VC-WIN64A"]
        prefix = self.package_folder.replace("\\", "/")
        args.append(f"--prefix={prefix}")
        args.append(f"--openssldir={prefix}/ssl")
        if self.options.no_asm:
            args.append("no-asm")
        if self.options.no_tests:
            args.append("no-tests")
        if self.options.no_deprecated:
            args.append("no-deprecated")
        if not self.options.shared:
            args.append("no-shared")
        args += ["no-docs", "no-unit-test"]
        return args

    def build(self):
        Path(self.build_folder).mkdir(parents=True, exist_ok=True)
        args = self._configure_args()
        src_rel = os.path.relpath(self.source_folder, self.build_folder).replace(
            "\\", "/"
        )
        configure_cmd = ["perl", f"{src_rel}/Configure"] + args
        self._run(configure_cmd, cwd=self.build_folder)
        self._run(["nmake"], cwd=self.build_folder)

    def package(self):
        self._run(["nmake", "install_sw"], cwd=self.build_folder)
        copy(
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        rm(pattern="*.pdb", folder=os.path.join(self.package_folder, "lib"))
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
