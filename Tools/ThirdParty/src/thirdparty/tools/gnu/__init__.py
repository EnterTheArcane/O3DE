from __future__ import annotations

import os
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty.internal.model.recipe import RecipeBase


class AutotoolsDeps:
    """Stub — AutotoolsDeps is not supported on Windows in this build system."""

    def __init__(self, recipe) -> None:
        self._recipe = recipe

    def generate(self) -> None:
        pass


class AutotoolsToolchain:
    """Stub — Autotools is not supported on Windows in this build system."""

    def __init__(self, recipe) -> None:
        self._recipe = recipe
        self.configure_args: list[str] = []
        self.extra_cflags: list[str] = []
        self.extra_cxxflags: list[str] = []
        self.extra_defines: list[str] = []
        self.update_configure_args = lambda d: self.configure_args

    def environment(self) -> "Environment":
        """Return a stub environment object."""
        return Environment()

    def generate(self, env=None) -> None:
        pass


class Environment:
    """Stub environment descriptor used by Autotools-based recipes."""

    def __init__(self) -> None:
        self._vars: dict[str, str] = {}

    def define(self, name: str, value: str) -> None:
        self._vars[name] = value

    def vars(self, recipe) -> "Environment":
        return self

    def save_script(self, name: str) -> None:
        pass


class Autotools:
    """Stub — Autotools is not supported on Windows in this build system."""

    def __init__(self, recipe) -> None:
        self._recipe = recipe

    def configure(self) -> None:
        raise RuntimeError("Autotools builds are not supported on Windows")

    def make(self) -> None:
        raise RuntimeError("Autotools builds are not supported on Windows")

    def install(self) -> None:
        raise RuntimeError("Autotools builds are not supported on Windows")


class PkgConfigDeps:
    """Collects ``lib/pkgconfig`` directories from resolved dependencies and
    exposes them via ``PKG_CONFIG_PATH`` so that Meson (or other build systems)
    can discover those packages through pkg-config.

    The primary dependency-discovery mechanism for Meson builds in this system
    is the cmake method via ``CMAKE_PREFIX_PATH`` (written by ``MesonToolchain``).
    This class provides a secondary fallback for packages that ship ``.pc`` files.
    """

    def __init__(self, recipe: "RecipeBase") -> None:
        self._recipe = recipe

    def generate(self) -> None:
        pc_dirs: list[str] = []
        for dep_path in self._recipe.dep_package_paths:
            pc_dir = os.path.join(dep_path, "lib", "pkgconfig")
            if os.path.isdir(pc_dir):
                pc_dirs.append(pc_dir)

        if not pc_dirs:
            return

        existing = os.environ.get("PKG_CONFIG_PATH", "")
        new_paths = os.pathsep.join(pc_dirs)
        os.environ["PKG_CONFIG_PATH"] = (
            new_paths + os.pathsep + existing if existing else new_paths
        )
        print(f"[thirdparty] PKG_CONFIG_PATH = {os.environ['PKG_CONFIG_PATH']}")
