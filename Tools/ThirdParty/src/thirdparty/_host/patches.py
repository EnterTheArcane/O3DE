import logging
import os

import patch_ng


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


def apply_patches(conanfile):
    patches_dir = os.path.join(conanfile.recipe_folder, "patches")
    if not os.path.isdir(patches_dir):
        return
    for pf in sorted(f for f in os.listdir(patches_dir) if f.endswith(".patch")):
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
