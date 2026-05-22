from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty.internal.model.recipe import RecipeBase


def is_msvc(recipe: RecipeBase) -> bool:
    """Return True when the host platform is Windows (MSVC assumed).

    On Linux/macOS this always returns False.
    """
    return recipe.is_windows


def is_msvc_static_runtime(recipe: RecipeBase) -> bool:
    """Return True when MSVC is using the static (/MT) runtime.

    Stub: always returns False (dynamic runtime assumed).
    """
    return False


def check_min_vs(recipe: RecipeBase, version: str, raise_invalid: bool = True) -> None:
    """Verify minimum Visual Studio version.

    Stub: no-op — version checking against the installed compiler is not
    implemented in the ThirdParty tool.
    """


def msvc_runtime_flag(recipe: RecipeBase) -> str:
    """Return the MSVC runtime flag string.

    Stub: always returns 'MD' (dynamic release runtime).
    """
    return "MD"


def unix_path(recipe_or_path, path: str | None = None) -> str:
    """Convert a Windows path to a forward-slash path (Unix-style).

    Accepts both ``unix_path(path)`` and Conan-style ``unix_path(self, path)``.
    """
    target = path if path is not None else str(recipe_or_path)
    return target.replace("\\", "/")


class VCVars:
    """Stub for Conan's VCVars environment helper. No-op in ThirdParty."""

    def __init__(self, recipe: RecipeBase) -> None:
        pass

    def generate(self) -> None:
        pass


class NMakeDeps:
    """Stub for Conan's NMakeDeps. No-op in ThirdParty."""

    def __init__(self, recipe) -> None:
        pass

    def generate(self) -> None:
        pass


class NMakeToolchain:
    """Stub for Conan's NMakeToolchain. No-op in ThirdParty."""

    def __init__(self, recipe) -> None:
        pass

    def generate(self) -> None:
        pass


class MSBuildToolchain:
    """Stub for Conan's MSBuildToolchain. No-op in ThirdParty."""

    filename = "conantoolchain.props"

    def __init__(self, recipe: RecipeBase) -> None:
        self._recipe = recipe
        self.toolset: str = "v143"
        self.configuration: str = recipe.build_type

    def generate(self) -> None:
        pass


class MSBuild:
    """Stub for Conan's MSBuild helper. Not used on Windows CMake builds."""

    def __init__(self, recipe: RecipeBase) -> None:
        self._recipe = recipe
        self.build_type: str = recipe.build_type
        self.platform: str = "x64"

    def build(self, solution: str, targets: list[str] | None = None) -> None:
        raise RuntimeError("MSBuild direct invocation is not implemented; use CMake generator instead")
