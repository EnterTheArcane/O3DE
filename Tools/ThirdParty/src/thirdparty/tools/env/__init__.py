"""Stubs for Conan environment helpers (VirtualBuildEnv, VirtualRunEnv, Environment)."""
from __future__ import annotations

from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from thirdparty.internal.model.recipe import RecipeBase


class _EnvVars:
    """No-op env vars object."""
    def get(self, name: str, default: Any = None) -> Any:
        return default

    def save_script(self, name: str) -> None:
        pass


class VirtualBuildEnv:
    """Stub for Conan's VirtualBuildEnv. No-op in ThirdParty."""

    def __init__(self, recipe: RecipeBase) -> None:
        pass

    def vars(self) -> _EnvVars:
        return _EnvVars()

    def generate(self, scope: str = "build") -> None:
        pass


class VirtualRunEnv:
    """Stub for Conan's VirtualRunEnv. No-op in ThirdParty."""

    def __init__(self, recipe: RecipeBase) -> None:
        pass

    def vars(self) -> _EnvVars:
        return _EnvVars()

    def generate(self, scope: str = "run") -> None:
        pass


class Environment:
    """Stub for Conan's Environment. No-op in ThirdParty."""

    def define(self, name: str, value: Any) -> None:
        pass

    def define_path(self, name: str, value: str) -> None:
        pass

    def unset(self, name: str) -> None:
        pass

    def append(self, name: str, value: Any, separator: str = " ") -> None:
        pass

    def prepend_path(self, name: str, value: str) -> None:
        pass

    def vars(self, recipe: Any = None, scope: str = "build") -> _EnvVars:
        return _EnvVars()
