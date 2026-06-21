import logging
import os
import re
import shutil

import patch_ng
import yaml

from thirdparty.errors import RecipeException
from thirdparty._internal.paths import DATA_YML
from thirdparty._internal.util.files import mkdir, load, save


class PatchLogHandler(logging.Handler):
    def __init__(self, scoped_output, patch_file):
        logging.Handler.__init__(self, logging.DEBUG)
        self._scoped_output = scoped_output
        self.patchname = patch_file or "patch_ng"

    def emit(self, record):
        logstr = self.format(record)
        if record.levelno == logging.WARN:
            self._scoped_output.warning("%s: %s" % (self.patchname, logstr))
        else:
            self._scoped_output.info("%s: %s" % (self.patchname, logstr))


def _patch_applies_to_version(filename: str, version: str) -> bool:
    """Match version-prefixed patch names like ``1.2.3-0001-fix.patch``."""
    match = re.match(r"^([\d][^\s/\\]*?)-\d{4}-", filename)
    if match:
        return match.group(1) == version
    return True


def patch(recipe, base_path=None, patch_file=None, patch_string=None, strip=0, fuzz=False, **kwargs):
    """
    Applies a diff from file (patch_file) or string (patch_string) in the recipe.source_folder
    directory. The folder containing the sources can be customized with the self.folders attribute
    in the layout(self) method.

    :param recipe: the current recipe, always pass 'self'
    :param base_path: The path is a relative path to recipe.export_sources_folder unless an
           absolute path is provided.
    :param patch_file: Patch file that should be applied. The path is relative to the
           recipe.source_folder unless an absolute path is provided.
    :param patch_string: Patch string that should be applied.
    :param strip: Number of folders to be stripped from the path.
    :param fuzz: Should accept fuzzy patches.
    :param kwargs: Extra parameters that can be added and will contribute to output information
    """

    patch_type = kwargs.get('patch_type') or ("file" if patch_file else "string")
    patch_description = kwargs.get('patch_description')

    if patch_type or patch_description:
        patch_type_str = ' ({})'.format(patch_type) if patch_type else ''
        patch_description_str = ': {}'.format(patch_description) if patch_description else ''
        recipe.output.info('Apply patch{}{}'.format(patch_type_str, patch_description_str))

    patchlog = logging.getLogger("patch_ng")
    patchlog.handlers = []
    patchlog.addHandler(PatchLogHandler(recipe.output, patch_file))

    if patch_file:
        # trick *1: patch_file path could be absolute (e.g. recipe.build_folder), in that case
        # the join does nothing and works.
        patch_path = os.path.join(recipe.export_sources_folder, patch_file)
        patchset = patch_ng.fromfile(patch_path)
    else:
        patchset = patch_ng.fromstring(patch_string.encode())

    if not patchset:
        raise RecipeException("Failed to parse patch: %s" % (patch_file if patch_file else "string"))

    # trick *1
    root = os.path.join(recipe.source_folder, base_path) if base_path else recipe.source_folder
    if not patchset.apply(strip=strip, root=root, fuzz=fuzz):
        raise RecipeException("Failed to apply patch: %s" % patch_file)


def apply_patches(recipe):
    """Apply all recipe-local ``patches/*.patch`` files for the active version."""
    patches_dir = os.path.join(recipe.recipe_folder, "patches")
    if not os.path.isdir(patches_dir):
        return

    version = str(getattr(recipe, "version", None) or "")
    for patch_name in sorted(name for name in os.listdir(patches_dir) if name.endswith(".patch")):
        if not _patch_applies_to_version(patch_name, version):
            recipe.output.info(f"Skip patch (wrong version): {patch_name}")
            continue

        patch_path = os.path.join(patches_dir, patch_name)
        recipe.output.info(f"Apply patch: {patch_name}")
        patchlog = logging.getLogger("patch_ng")
        patchlog.handlers = []
        patchlog.addHandler(PatchLogHandler(recipe.output, patch_name))
        patchset = patch_ng.fromfile(patch_path)
        if not patchset:
            raise RecipeException(f"Failed to parse patch: {patch_name}")
        if not patchset.apply(root=recipe.source_folder):
            raise RecipeException(f"Failed to apply patch: {patch_name}")


def apply_recipe_data_patches(recipe):
    """
    Applies patches stored in ``recipe.recipe_data`` (read from ``recipe_data.yml`` file).
    It will apply all the patches under ``patches`` entry that matches the given
    ``recipe.version``. If versions are not defined in ``recipe_data.yml`` it will apply all the
    patches directly under ``patches`` keyword.

    The key entries will be passed as kwargs to the ``patch`` function.
    """
    if recipe.recipe_data is None:
        raise RecipeException("recipe_data.yml not defined")

    patches = recipe.recipe_data.get('patches')
    if patches is None:
        recipe.output.info("apply_recipe_data_patches(): No patches defined in recipe_data")
        return

    if isinstance(patches, dict):
        assert recipe.version, "Can only be applied if recipe.version is already defined"
        entries = patches.get(str(recipe.version), [])
        if entries is None:
            recipe.output.warning(f"apply_recipe_data_patches(): No patches defined for version {recipe.version} in recipe_data.yml")
            return
    elif isinstance(patches, list):
        entries = patches
    else:
        raise RecipeException("recipe_data.yml 'patches' should be a list or a dict {version: list}")
    for it in entries:
        if "patch_file" in it:
            # The patch files are located in the root src
            entry = it.copy()
            patch_file = entry.pop("patch_file")
            patch_file_path = os.path.join(recipe.export_sources_folder, patch_file)
            if "patch_description" not in entry:
                entry["patch_description"] = patch_file
            patch(recipe, patch_file=patch_file_path, **entry)
        elif "patch_string" in it:
            patch(recipe, **it)
        elif "patch_user" in it:
            pass  # This will be managed directly by users, can be a command or a script execution
        else:
            raise RecipeException("The 'recipe_data.yml' file needs a 'patch_file' or 'patch_string'"
                                 " entry for every patch to be applied")


def export_recipe_data_patches(recipe):
    """
    Exports patches stored in 'recipe.recipe_data' (read from 'recipe_data.yml' file). It will export
    all the patches under 'patches' entry that matches the given 'recipe.version'. If versions are
    not defined in 'recipe_data.yml' it will export all the patches directly under 'patches' keyword.
    """
    if recipe.recipe_data is None:
        raise RecipeException("recipe_data.yml not defined")

    recipe_patches = recipe.recipe_data.get('patches')

    def _handle_patches(patches, patches_folder):
        if patches is None:
            recipe.output.info("export_recipe_data_patches(): No patches defined in recipe_data")
            return

        if isinstance(patches, dict):
            assert recipe.version, "Can only be exported if recipe.version is already defined"
            entries = patches.get(recipe.version, [])
            if entries is None:
                recipe.output.warning("export_recipe_data_patches(): No patches defined for "
                                         f"version {recipe.version} in recipe_data.yml")
                return
        elif isinstance(patches, list):
            entries = patches
        else:
            raise RecipeException("recipe_data.yml 'patches' should be a list or a dict "
                                 "{version: list}")
        for it in entries:
            patch_file = it.get("patch_file")
            if patch_file:
                src = os.path.join(patches_folder, patch_file)
                dst = os.path.join(recipe.export_sources_folder, patch_file)
                if not os.path.exists(src):
                    raise RecipeException(f"Patch file does not exist: '{src}'")
                mkdir(os.path.dirname(dst))
                shutil.copy2(src, dst)
        return entries

    _handle_patches(recipe_patches, recipe.recipe_folder)

    extra_path = recipe.conf.get("core.sources.patch:extra_path")
    if extra_path:
        if not os.path.isdir(extra_path):
            raise RecipeException(f"Patches extra path '{extra_path}' does not exist")
        pkg_path = os.path.join(extra_path, recipe.name)
        if not os.path.isdir(pkg_path):
            return
        data_path = os.path.join(pkg_path, DATA_YML)
        try:
            data = yaml.safe_load(load(data_path))
        except Exception as e:
            raise RecipeException("Invalid yml format at {}: {}".format(data_path, e))
        data = data or {}
        recipe.output.info(f"Applying extra patches 'core.sources.patch:extra_path': {data_path}")
        new_patches = _handle_patches(data.get('patches'), pkg_path)

        # Update the RECIPEDATA.YML
        recipe_patches = recipe_patches or {}
        recipe_patches.setdefault(recipe.version, []).extend(new_patches)

        recipe.recipe_data['patches'] = recipe_patches
        # Saving in the EXPORT folder
        recipe_data_path = os.path.join(recipe.export_folder, DATA_YML)
        new_recipe_data_yml = yaml.safe_dump(recipe.recipe_data, default_flow_style=False)
        save(recipe_data_path, new_recipe_data_yml)

