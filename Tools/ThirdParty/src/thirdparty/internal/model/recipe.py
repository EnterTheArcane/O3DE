from __future__ import annotations

import platform
import sys
from dataclasses import dataclass
from typing import Any


# ---------------------------------------------------------------------------
# Options accessor
# ---------------------------------------------------------------------------

class _OptionsAccessor:
    """Proxy that supports both attribute-style and dict-style option access.

    This lets ported recipes use either ``self.options["shared"]`` or the
    Conan-compatible ``self.options.shared`` without modification.
    """

    def __init__(self, values: dict[str, Any]) -> None:
        object.__setattr__(self, "_values", values)

    def __getattr__(self, name: str) -> Any:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        if name in values:
            return values[name]
        raise AttributeError(f"No option {name!r}")

    def __setattr__(self, name: str, value: Any) -> None:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        values[name] = value

    def __getitem__(self, key: str) -> Any:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        return values[key]

    def __setitem__(self, key: str, value: Any) -> None:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        values[key] = value

    def get(self, key: str, default: Any = None) -> Any:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        return values.get(key, default)

    def get_safe(self, key: str, default: Any = None) -> Any:
        """Conan-compatible alias for :meth:`get`."""
        return self.get(key, default)

    def rm_safe(self, key: str) -> None:
        """Remove option *key* if it exists (Conan-compatible no-op wrapper)."""
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        values.pop(key, None)

    def __bool__(self) -> bool:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        return bool(values)

    def __repr__(self) -> str:
        values: dict[str, Any] = object.__getattribute__(self, "_values")
        return f"_OptionsAccessor({values!r})"


# ---------------------------------------------------------------------------
# Settings stub (for ported recipes that still reference self.settings.*)
# ---------------------------------------------------------------------------

class _CompilerAccessor:
    """Minimal stub for self.settings.compiler.*"""

    def __getattr__(self, name: str) -> Any:
        return None

    def __eq__(self, other: object) -> bool:
        return False

    def get_safe(self, name: str, default: Any = None) -> Any:
        return default

    def __bool__(self) -> bool:
        return False


class _SettingsAccessor:
    """Stub for self.settings — forwards common attributes to RecipeBase helpers."""

    def __init__(self, recipe: "RecipeBase") -> None:
        self._recipe = recipe
        self.compiler: _CompilerAccessor = _CompilerAccessor()

    @property
    def os(self) -> str:
        if self._recipe.is_windows:
            return "Windows"
        if self._recipe.is_macos:
            return "Macos"
        return "Linux"

    @property
    def arch(self) -> str:
        return self._recipe.arch

    @property
    def build_type(self) -> str:
        return self._recipe.build_type

    def get_safe(self, name: str, default: Any = None) -> Any:
        try:
            return getattr(self, name)
        except AttributeError:
            return default

    def rm_safe(self, name: str) -> None:
        pass  # no-op


# ---------------------------------------------------------------------------
# Dependency info
# ---------------------------------------------------------------------------

@dataclass
class DepInfo:
    """Information about a built dependency exposed via ``recipe.dependencies``."""
    package_folder: str


# ---------------------------------------------------------------------------
# RecipeBase
# ---------------------------------------------------------------------------

class RecipeBase:
    """Base class for all ThirdParty build recipes."""

    # Override in subclasses using class body:
    name: str | None = None
    version: str = ""
    license: str | None = None
    default_options: dict[str, Any] = {}

    def __init__(self) -> None:
        # Injected by the build runner before lifecycle methods are called:
        self.version: str = ""
        self.recipe_folder: str = ""
        self.source_folder: str = ""
        self.build_folder: str = ""
        self.package_folder: str = ""
        self.build_type: str = "Release"
        # Package folder paths for resolved dependencies (set by runner):
        self.dep_package_paths: list[str] = []
        # Rich dependency info keyed by recipe name (set by runner):
        self.dependencies: dict[str, DepInfo] = {}
        # Active option values initialised from class-level default_options:
        self.options: _OptionsAccessor = _OptionsAccessor(dict(type(self).default_options))
        # Settings stub for ported recipes that still reference self.settings.*:
        self.settings: _SettingsAccessor = _SettingsAccessor(self)

    # --- Platform helpers ---

    @property
    def is_windows(self) -> bool:
        return sys.platform == "win32"

    @property
    def is_linux(self) -> bool:
        return sys.platform.startswith("linux")

    @property
    def is_macos(self) -> bool:
        return sys.platform == "darwin"

    @property
    def arch(self) -> str:
        machine = platform.machine().lower()
        return "arm64" if machine in ("arm64", "aarch64") else "x86_64"

    # --- Lifecycle methods (override in subclasses) ---

    def requirements(self) -> list[str]:
        """Return list of recipe names this recipe depends on."""
        return []

    def source(self) -> None:
        """Download and extract source code."""

    def generate(self) -> None:
        """Configure the build system (write CMake toolchain file, etc.)."""

    def build(self) -> None:
        """Compile the library."""

    def package(self) -> None:
        """Install build artifacts to the package folder."""
