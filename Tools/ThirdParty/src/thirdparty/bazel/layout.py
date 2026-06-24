from __future__ import annotations

import os

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase


def bazel_layout(recipe: RecipeBase, src_folder=".", build_folder=".", target_folder=None):
    """Bazel layout is so limited. It does not allow to create its special symlinks in other
    folder. See more information in https://bazel.build/remote/output-directories"""
    subproject = recipe.folders.subproject
    recipe.folders._source = src_folder if not subproject else os.path.join(subproject, src_folder)
    # Bazel always builds the whole project in the root folder, but consumer can put another one
    recipe.folders._build = build_folder if not subproject else os.path.join(subproject, build_folder)
    generators_folder = recipe.folders._generators or "recipe"
    recipe.folders._generators = os.path.join(recipe.folders._build, generators_folder)
    bindirs = os.path.join(recipe.folders._build, "bazel-bin")
    libdirs = os.path.join(recipe.folders._build, "bazel-bin")
    # Target folder is useful for working on editable mode
    if target_folder:
        bindirs = os.path.join(bindirs, target_folder)
        libdirs = os.path.join(libdirs, target_folder)
    recipe.infos.build.bindirs = [bindirs]
    recipe.infos.build.libdirs = [libdirs]
