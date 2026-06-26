import os
from pathlib import Path

from thirdparty.errors import RecipeException


def _folder_path(base_folder: str | None, relative_folder: str = "") -> Path:
    if base_folder is None:
        raise RecipeException("Base folder is not set, cannot compute the final folder path")   
    if not relative_folder:
        return Path(os.path.normpath(base_folder))
    return Path(os.path.normpath(os.path.join(base_folder, relative_folder)))


class Folders:
    def __init__(self):
        self._base_source: str | None = None
        self._base_build: str | None = None
        self._base_package: str | None = None
        self._base_generators: str | None = None

        self._base_export: str | None = None
        self._base_export_sources: str | None = None

        self._immutable_package_folder: Path | None = None

        self._source: str = ""
        self._build: str = ""
        self._package: str = ""
        self._generators: str = ""
        # Relative location of the project root, if the recipe is not in that project root, but
        # in a subfolder: e.g: If the recipe is in a subfolder then self.root = ".."
        self.root: str | None = None
        # The relative location with respect to the project root of the subproject containing the
        # recipe.py, that makes most of the output folders defined in layouts (cmake_layout, etc)
        # start from the subproject again
        self.subproject: str | None = None
        self.build_folder_vars: list[str] | None = None

    def set_base_folders(self, recipe_folder: str, output_folder: str | None):
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
        base_folder = recipe_folder if self.root is None else os.path.normpath(os.path.join(recipe_folder, self.root))

        self._base_source = base_folder
        self._base_build = output_folder or base_folder
        self._base_generators = output_folder or base_folder
        self._base_export_sources = output_folder or base_folder

    @property
    def source(self) -> Path:
        return _folder_path(self._base_source, self._source)

    @property
    def base_source(self) -> str | None:
        return self._base_source

    def set_base_source(self, folder: str | None):
        self._base_source = folder

    @property
    def build(self) -> Path:
        return _folder_path(self._base_build, self._build)

    @property
    def base_build(self) -> str | None:
        return self._base_build

    def set_base_build(self, folder: str | None):
        self._base_build = folder

    @property
    def base_package(self) -> str | None:
        return self._base_package

    def set_base_package(self, folder: str | None):
        self._base_package = folder

    @property
    def package(self) -> Path:
        """For the cache, the package folder is only the base"""
        return _folder_path(self._base_package)

    def set_finalize_folder(self, folder: str | None):
        self._immutable_package_folder = self.package
        self.set_base_package(folder)

    @property
    def immutable_package(self) -> Path:
        return self._immutable_package_folder or self.package

    @property
    def generators(self) -> Path:
        return _folder_path(self._base_generators, self._generators)

    def set_base_generators(self, folder: str | None):
        self._base_generators = folder

    @property
    def base_export(self) -> str | None:
        return self._base_export

    def set_base_export(self, folder: str | None):
        self._base_export = folder

    @property
    def base_export_sources(self) -> str | None:
        return self._base_export_sources

    def set_base_export_sources(self, folder: str | None):
        self._base_export_sources = folder

    @property
    def export(self) -> Path:
        return _folder_path(self._base_export)

    @property
    def export_sources(self) -> Path:
        return _folder_path(self._base_export_sources)
