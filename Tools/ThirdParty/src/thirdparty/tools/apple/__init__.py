from __future__ import annotations

import subprocess
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty.internal.model.recipe import RecipeBase


def is_apple_os(recipe: RecipeBase) -> bool:
    """Return True when the host platform is macOS (or any Apple OS)."""
    return recipe.is_macos


def fix_apple_shared_install_name(recipe: RecipeBase) -> None:
    """Rewrite absolute install names in .dylib files to use ``@rpath``.

    On non-macOS platforms this is a no-op.  On macOS it runs
    ``install_name_tool -id @rpath/<lib>`` on every ``.dylib`` found under
    ``<package_folder>``.
    """
    if not recipe.is_macos:
        return

    pkg = Path(recipe.package_folder)
    for dylib in pkg.rglob("*.dylib"):
        rel = dylib.relative_to(pkg)
        new_id = f"@rpath/{dylib.name}"
        subprocess.run(
            ["install_name_tool", "-id", new_id, str(dylib)],
            check=True,
        )
        print(f"[thirdparty] install_name_tool -id {new_id}  ({rel})")
