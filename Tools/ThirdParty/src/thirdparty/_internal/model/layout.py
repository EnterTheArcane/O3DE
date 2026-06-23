import os
from pathlib import Path

from thirdparty._internal.model.cpp_info import CppInfo
from thirdparty._internal.model.conf import Conf


class Infos:
    def __init__(self):
        self.source = CppInfo()
        self.build = CppInfo()
        self.package = CppInfo(set_defaults=True)


class PartialLayout:
    def __init__(self):
        from thirdparty.env import Environment
        self.buildenv_info = Environment()
        self.runenv_info = Environment()
        self.conf_info = Conf()

    def set_relative_base_folder(self, folder):
        self.buildenv_info.set_relative_base_folder(folder)
        self.runenv_info.set_relative_base_folder(folder)
        self.conf_info.set_relative_base_folder(folder)


def _folder_path(base_folder, relative_folder=""):
    if base_folder is None:
        return None
    if not relative_folder:
        return Path(os.path.normpath(base_folder))
    return Path(os.path.normpath(os.path.join(base_folder, relative_folder)))


class Layouts:
    def __init__(self):
        self.source = PartialLayout()
        self.build = PartialLayout()
        self.package = PartialLayout()


class Folders:
    def __init__(self):
        self._base_source = None
        self._base_build = None
        self._base_package = None
        self._base_generators = None

        self._base_export = None
        self._base_export_sources = None

        self._base_recipe_metadata = None
        self._base_pkg_metadata = None
        self._immutable_package_folder = None

        self._source = ""
        self._build = ""
        self._package = ""
        self._generators = ""
        # Relative location of the project root, if the recipe is not in that project root, but
        # in a subfolder: e.g: If the recipe is in a subfolder then self.root = ".."
        self.root = None
        # The relative location with respect to the project root of the subproject containing the
        # recipe.py, that makes most of the output folders defined in layouts (cmake_layout, etc)
        # start from the subproject again
        self.subproject = None
        self.build_folder_vars = None

    def set_base_folders(self, recipe_folder, output_folder):
        """ this methods can be used for defining all the base folders in the
        local flow (recipe install, source, build), where only the current recipe location
        and the potential --output-folder user argument are the folders to take into account
        If the "layout()" method defines a self.folders.root = "xxx" it will be used to compute
        the base folder

        @param recipe_folder: the location where the current consumer recipe is
        @param output_folder: Can potentially be None (for export-pkg: TODO), in that case
        the recipe location is used
        """
        # This must be called only after ``layout()`` has been called
        base_folder = recipe_folder if self.root is None else \
            os.path.normpath(os.path.join(recipe_folder, self.root))

        self._base_source = base_folder
        self._base_build = output_folder or base_folder
        self._base_generators = output_folder or base_folder
        self._base_export_sources = output_folder or base_folder
        self._base_recipe_metadata = os.path.join(base_folder, "metadata")
        # TODO: It is likely that this base_pkg_metadata is not really used with this value
        self._base_pkg_metadata = output_folder or base_folder

    @property
    def source(self):
        return _folder_path(self._base_source, self._source)

    @property
    def base_source(self):
        return self._base_source

    def set_base_source(self, folder):
        self._base_source = folder

    @property
    def build(self):
        return _folder_path(self._base_build, self._build)

    @property
    def recipe_metadata(self):
        return self._base_recipe_metadata

    def set_base_recipe_metadata(self, folder):
        self._base_recipe_metadata = folder

    @property
    def package_metadata(self):
        return self._base_pkg_metadata

    def set_base_pkg_metadata(self, folder):
        self._base_pkg_metadata = folder

    @property
    def base_build(self):
        return self._base_build

    def set_base_build(self, folder):
        self._base_build = folder

    @property
    def base_package(self):
        return self._base_package

    def set_base_package(self, folder):
        self._base_package = folder

    @property
    def package(self):
        """For the cache, the package folder is only the base"""
        return _folder_path(self._base_package)

    def set_finalize_folder(self, folder):
        self._immutable_package_folder = self.package
        self.set_base_package(folder)

    @property
    def immutable_package(self):
        return self._immutable_package_folder or self.package

    @property
    def generators(self):
        return _folder_path(self._base_generators, self._generators)

    def set_base_generators(self, folder):
        self._base_generators = folder

    @property
    def base_export(self):
        return self._base_export

    def set_base_export(self, folder):
        self._base_export = folder

    @property
    def base_export_sources(self):
        return self._base_export_sources

    def set_base_export_sources(self, folder):
        self._base_export_sources = folder

    @property
    def export(self):
        return _folder_path(self._base_export)

    @property
    def export_sources(self):
        return _folder_path(self._base_export_sources)
