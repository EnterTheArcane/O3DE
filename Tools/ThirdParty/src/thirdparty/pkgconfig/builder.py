from __future__ import annotations

import textwrap
from io import StringIO
from typing import TYPE_CHECKING, Any

from thirdparty.build import cmd_args_to_string
from thirdparty.env import Environment
from thirdparty.errors import RecipeException

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe_base import RecipeBase


class PkgConfig:

    _recipe: RecipeBase
    _library: str
    _info: dict[str, str]
    _pkg_config_path: str | None
    _variables: dict[str, str] | None

    def __init__(self, recipe: RecipeBase, library: str, pkg_config_path: str | None = None):
        """

        :param recipe: The current recipe object. Always use ``self``.
        :param library: The library which ``.pc`` file is to be parsed. It must exist in the pkg_config path.
        :param pkg_config_path:  If defined it will be prepended to ``PKG_CONFIG_PATH`` environment
               variable, so the execution finds the required files.
        """
        self._recipe = recipe
        self._library = library
        self._info = {}
        self._pkg_config_path = pkg_config_path
        self._variables = None

    def _parse_output(self, option: str) -> str:
        executable = self._recipe.conf.get("tools.gnu:pkg_config", default="pkg-config")
        command = cmd_args_to_string([executable, '--' + option, self._library, '--print-errors'])

        env = Environment()
        if self._pkg_config_path:
            env.prepend_path("PKG_CONFIG_PATH", self._pkg_config_path)
        with env.vars(self._recipe).apply():
            # This way we get the environment from RecipeBase, from profile (default buildenv)
            output, err = StringIO(), StringIO()
            ret = self._recipe.run(
                command, stdout=output, stderr=err, quiet=True,
                ignore_errors=True)
            if ret != 0:
                raise RecipeException(
                    f"PkgConfig failed. Command: {command}\n"
                    f"    stdout:\n{textwrap.indent(output.getvalue(), '    ')}\n"
                    f"    stderr:\n{textwrap.indent(err.getvalue(), '    ')}\n")
        value = output.getvalue().strip()
        return value

    def _get_option(self, option: str) -> str:
        if option not in self._info:
            self._info[option] = self._parse_output(option)
        return self._info[option]

    @property
    def includedirs(self) -> list[str]:
        return [include[2:] for include in self._get_option('cflags-only-I').split()]

    @property
    def cflags(self) -> list[str]:
        return [flag for flag in self._get_option('cflags-only-other').split()
                if not flag.startswith("-D")]

    @property
    def defines(self) -> list[str]:
        return [flag[2:] for flag in self._get_option('cflags-only-other').split()
                if flag.startswith("-D")]

    @property
    def libdirs(self) -> list[str]:
        return [lib[2:] for lib in self._get_option('libs-only-L').split()]

    @property
    def libs(self) -> list[str]:
        return [lib[2:] for lib in self._get_option('libs-only-l').split()]

    @property
    def linkflags(self) -> list[str]:
        return self._get_option('libs-only-other').split()

    @property
    def provides(self) -> str:
        return self._get_option('print-provides')

    @property
    def version(self) -> str:
        return self._get_option('modversion')

    @property
    def variables(self) -> dict[str, str]:
        if self._variables is None:
            variable_names = self._parse_output('print-variables').split()
            self._variables = {}
            for name in variable_names:
                self._variables[name] = self._parse_output('variable=%s' % name)
        return self._variables

    def fill_cpp_info(self, cpp_info: Any, is_system: bool = True,
                      system_libs: list[str] | None = None):
        """
        Method to fill a cpp_info object from the PkgConfig configuration

        :param cpp_info: Can be the global one (self.cpp_info) or a component one (self.components["foo"].cpp_info).
        :param is_system: If ``True``, all detected libraries will be assigned to ``cpp_info.system_libs``, and none to ``cpp_info.libs``.
        :param system_libs: If ``True``, all detected libraries will be assigned to ``cpp_info.system_libs``, and none to ``cpp_info.libs``.

        """
        if not self.provides:
            raise RecipeException(f"PkgConfig error, '{self._library}' files not available")
        self._recipe.output.verbose(f"PkgConfig fill cpp_info for {self._library}")
        if is_system:
            cpp_info.system_libs = self.libs
        else:
            system_libs = system_libs or []
            cpp_info.libs = [lib for lib in self.libs if lib not in system_libs]
            cpp_info.system_libs = [lib for lib in self.libs if lib in system_libs]
        cpp_info.libdirs = self.libdirs
        cpp_info.sharedlinkflags = self.linkflags
        cpp_info.exelinkflags = self.linkflags
        cpp_info.defines = self.defines
        cpp_info.includedirs = self.includedirs
        cpp_info.cflags = self.cflags
        cpp_info.cxxflags = self.cflags
