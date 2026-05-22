# CPython builder — Windows (PCbuild/build.bat wrapper)
import os
import subprocess
from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.tools.files import copy, get, rmdir


class Recipe(RecipeBase):
    name = "cpython"
    version = "3.12.7"
    license = "PSF-2.0"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "optimizations": [True, False],
        "with_ssl": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "optimizations": False,
        "with_ssl": False,
    }

    def requirements(self) -> list[str]:
        return ["openssl"] if self.options.with_ssl else []

    def source(self):
        get(
            url="https://www.python.org/ftp/python/3.12.7/Python-3.12.7.tgz",
            dest=self.source_folder,
            sha256="73ac8fe780227bf371add8373c3079f42a0dc62deff8d612cd15a618082ab623",
        )

    def _run(self, cmd, cwd: str) -> None:
        if isinstance(cmd, str):
            result = subprocess.run(cmd, cwd=cwd, check=False, shell=True)
        else:
            result = subprocess.run(cmd, cwd=cwd, check=False)
        if result.returncode != 0:
            raise RuntimeError(f"Command failed (exit {result.returncode}): {cmd}")

    def build(self):
        build_bat = os.path.join(self.source_folder, "PCbuild", "build.bat")
        # build.bat flags: -c Release/Debug -p x64 --no-tkinter --no-ssl
        build_type = "Debug" if self.build_type == "Debug" else "Release"
        cmd = [
            build_bat,
            "-c",
            build_type,
            "-p",
            "x64",
            "--no-tkinter",
        ]
        if not self.options.with_ssl:
            cmd.append("--no-ssl")
        elif self.options.with_ssl and "openssl" in self.dependencies:
            ssl_dir = self.dependencies["openssl"].package_folder.replace("\\", "/")
            cmd += [f"--ssl={ssl_dir}"]
        if self.options.optimizations:
            cmd.append("--pgo")
        self._run(cmd, cwd=self.source_folder)

    def package(self):
        build_type = "Debug" if self.build_type == "Debug" else "Release"
        artifacts_dir = os.path.join(self.source_folder, "PCbuild", "amd64")

        # Copy python executable and DLLs
        bin_dir = os.path.join(self.package_folder, "bin")
        Path(bin_dir).mkdir(parents=True, exist_ok=True)
        copy("python*.exe", src=artifacts_dir, dst=bin_dir)
        copy("python*.dll", src=artifacts_dir, dst=bin_dir)
        copy("python*.pyd", src=artifacts_dir, dst=bin_dir)
        copy("*.dll", src=artifacts_dir, dst=bin_dir)

        # Copy import libraries
        lib_dir = os.path.join(self.package_folder, "lib")
        Path(lib_dir).mkdir(parents=True, exist_ok=True)
        copy("python*.lib", src=artifacts_dir, dst=lib_dir)

        # Copy headers
        include_src = os.path.join(self.source_folder, "Include")
        include_dst = os.path.join(self.package_folder, "include", "python")
        Path(include_dst).mkdir(parents=True, exist_ok=True)
        copy("*.h", src=include_src, dst=include_dst)
        # PC directory has pyconfig.h for Windows
        pc_dir = os.path.join(self.source_folder, "PC")
        copy("pyconfig.h", src=pc_dir, dst=include_dst)

        # Copy stdlib
        lib_src = os.path.join(self.source_folder, "Lib")
        lib_dst = os.path.join(self.package_folder, "lib", "python")
        copy("*", src=lib_src, dst=lib_dst)

        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
