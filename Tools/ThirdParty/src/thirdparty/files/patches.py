import logging
import os

import patch_ng

from thirdparty.errors import RecipeException


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


def patch(recipe, base_path=None, patch_file=None, patch_string=None, strip=0, fuzz=False, **kwargs):
    """
    Applies a diff from file (patch_file) or string (patch_string) in the recipe.folders.source
    directory. The folder containing the sources can be customized with the self.folders attribute
    in the layout(self) method.

    :param recipe: the current recipe, always pass 'self'
    :param base_path: The path is a relative path to recipe.folders.export_sources unless an
           absolute path is provided.
    :param patch_file: Patch file that should be applied. The path is relative to the
           recipe.folders.source unless an absolute path is provided.
    :param patch_string: Patch string that should be applied.
    :param strip: Number of folders to be stripped from the path.
    :param fuzz: Should accept fuzzy patches.
    :param kwargs: Extra parameters that can be added and will contribute to output information
    """

    patch_type = kwargs.get('patch_type') or ("file" if patch_file else "string")
    patch_description = kwargs.get('patch_description')

    if patch_type or patch_description:
        patch_type_str = f' ({patch_type})' if patch_type else ''
        patch_description_str = f': {patch_description}' if patch_description else ''
        recipe.output.info(f'Apply patch{patch_type_str}{patch_description_str}')

    patchlog = logging.getLogger("patch_ng")
    patchlog.handlers = []
    patchlog.addHandler(PatchLogHandler(recipe.output, patch_file))

    if patch_file:
        # trick *1: patch_file path could be absolute (e.g. recipe.folders.build), in that case
        # the join does nothing and works.
        patch_path = os.path.join(recipe.folders.export_sources, patch_file)
        patchset = patch_ng.fromfile(patch_path)
    else:
        patchset = patch_ng.fromstring(patch_string.encode())

    if not patchset:
        raise RecipeException("Failed to parse patch: %s" % (patch_file if patch_file else "string"))

    # trick *1
    root = os.path.join(recipe.folders.source, base_path) if base_path else recipe.folders.source
    if not patchset.apply(strip=strip, root=root, fuzz=fuzz):
        raise RecipeException("Failed to apply patch: %s" % patch_file)


def apply_patches(recipe):
    """Apply all recipe-local ``patches/*.patch`` files in alphabetical order."""
    patches_dir = os.path.join(recipe.recipe_folder, "patches")
    if not os.path.isdir(patches_dir):
        return

    for patch_name in sorted(name for name in os.listdir(patches_dir) if name.endswith(".patch")):
        patch_path = os.path.join(patches_dir, patch_name)
        recipe.output.info(f"Apply patch: {patch_name}")
        patchlog = logging.getLogger("patch_ng")
        patchlog.handlers = []
        patchlog.addHandler(PatchLogHandler(recipe.output, patch_name))
        patchset = patch_ng.fromfile(patch_path)
        if not patchset:
            raise RecipeException(f"Failed to parse patch: {patch_name}")
        if not patchset.apply(root=recipe.folders.source):
            raise RecipeException(f"Failed to apply patch: {patch_name}")
