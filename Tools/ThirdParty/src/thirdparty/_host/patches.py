import logging
import os
import re

import patch_ng

from thirdparty.internal.model.recipe import RecipeBase


class _PatchLogHandler(logging.Handler):
    def __init__(self, output, name):
        super().__init__(logging.DEBUG)
        self._out = output
        self._name = name

    def emit(self, record):
        msg = f"{self._name}: {self.format(record)}"
        if record.levelno == logging.WARNING:
            self._out.warning(msg)
        else:
            self._out.info(msg)


def _patch_applies_to_version(filename: str, version: str) -> bool:
    """Return True if the patch file should be applied to the given version.

    Patches named with a version prefix (e.g. "2.5.2-0001-foo.patch") are only applied when
    the recipe version exactly matches that prefix.  Patches without a version prefix (e.g.
    "0001-foo.patch") are always applied.
    """
    # Match an optional version prefix like "1.2.3-" or "cci.20200101-"
    m = re.match(r'^([\d][^\s/\\]*?)-\d{4}-', filename)
    if m:
        return m.group(1) == version
    return True


def apply_patches(conanfile: RecipeBase):
    patches_dir = os.path.join(conanfile.recipe_folder, "patches")
    if not os.path.isdir(patches_dir):
        return
    version = getattr(conanfile, "version", None) or ""
    for pf in sorted(f for f in os.listdir(patches_dir) if f.endswith(".patch")):
        if not _patch_applies_to_version(pf, version):
            conanfile.output.info(f"Skip patch (wrong version): {pf}")
            continue
        path = os.path.join(patches_dir, pf)
        conanfile.output.info(f"Apply patch: {pf}")
        log = logging.getLogger("patch_ng")
        log.handlers = [_PatchLogHandler(conanfile.output, pf)]
        patchset = patch_ng.fromfile(path)
        if not patchset:
            raise RuntimeError(f"Failed to parse patch: {pf}")
        if not patchset.apply(root=conanfile.source_folder):
            raise RuntimeError(f"Failed to apply patch: {pf}")


def export_conandata_patches(conanfile):
    pass
