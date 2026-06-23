import os
import subprocess
from typing import Any

from thirdparty._internal.output import Output, Color, LEVEL_QUIET
from thirdparty._internal.subsystems import command_env_wrapper
from thirdparty.errors import RecipeException
from thirdparty._internal.model.cpp_info import MockInfoProperty
from thirdparty._internal.model.conf import Conf
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty._internal.model.layout import Folders, Infos, Layouts
from thirdparty._internal.model.options import Options
from thirdparty._internal.model.requires import Requirements
from thirdparty._internal.model.settings import Settings


class RecipeBase:
    """
    The base class for all package recipes
    """

    # Reference
    name: str | None = None
    version: str | None = None  # Any str, can be "1.1" or whatever

    # Metadata
    license: str | tuple[str, ...] | None = None

    # Binary model: Settings and Options
    # NOTE: ``settings``/``options``/the ``*_requires`` are intentionally ``Any``: recipe authors
    # may set them as a tuple/dict (e.g. ``options = {...}``) while at runtime they hold model
    # objects (``Settings``, ``Options``, ``Requirements``).  ``Any`` keeps both valid.
    settings: Any = None  # set to a Settings object by the build driver (host/target)
    settings_build: Any = None  # Settings for the build machine (tools)
    settings_target: Any = None  # Settings of what a tool_require will build for
    options: Any = None
    default_options: dict[str, Any] | None = None
    default_build_options: dict[str, Any] | None = None

    win_bash: bool | None = None
    win_bash_run: bool | None = None  # For run scope

    _is_consumer_recipe: bool = False

    # #### Requirements
    requires: Any = None
    tool_requires: Any = None
    build_requires: Any = None
    test_requires: Any = None
    tested_reference_str: str | None = None

    recipe_folder: str | None = None

    # Tools implied by the build-system helpers a recipe imports (e.g. CMake -> "cmake").
    # Populated at load time by the recipe loader from the recipe module's direct imports.
    _implicit_tool_requires: frozenset[str] = frozenset()

    # Package information
    cpp: "Infos | None" = None
    buildenv_info: Any = None  # Environment
    runenv_info: Any = None  # Environment
    conf_info: "Conf | None" = None
    conf: "Conf | None" = None

    def __init__(self, display_name=""):
        self.display_name: str = display_name
        # something that can run commands, as os.sytem

        self._recipe_runtime: Any = None
        from thirdparty.env import Environment
        self.buildenv_info = Environment()
        self.runenv_info = Environment()
        # At the moment only for build_requires, others will be ignored
        self.conf_info = Conf()
        self.info: Any = None
        self._recipe_buildenv: Any = None  # The profile buildenv, will be assigned initialize()
        self._recipe_runenv: Any = None
        self._recipe_node: Any = None  # access to container Node object, to access info, context, deps...

        if isinstance(self.settings, str):
            self.settings = [self.settings]
        self.requires = Requirements(self.requires, self.build_requires, self.test_requires,
                                     self.tool_requires)
        if self.build_requires:
            self.output.warning(
                "build_requires is deprecated, prefer to use tool_requires with correct traits",
                warn_tag="deprecated")

        self.options = Options(self.options or {}, self.default_options)

        # user declared variables
        self.user_info = MockInfoProperty("user_info")
        self.env_info = MockInfoProperty("env_info")
        self._recipe_dependencies: "RecipeDependencies | None" = None

        if not hasattr(self, "virtualbuildenv"):  # Allow the user to override it with True or False
            self.virtualbuildenv = True
        if not hasattr(self, "virtualrunenv"):  # Allow the user to override it with True or False
            self.virtualrunenv = True

        self.env_scripts = {}  # Accumulate the env scripts generated in order

        # layout() method related variables:
        self.folders = Folders()
        self.cpp = Infos()
        self.layouts = Layouts()

    @property
    def output(self):
        # an output stream (writeln, info, warn error)
        scope = self.display_name
        if not scope:
            scope = self.ref if self._recipe_node else ""
        return Output(scope=scope)

    @property
    def context(self):
        return self._recipe_node.context

    @property
    def dependencies(self):
        # Caching it, this object is requested many times
        if self._recipe_dependencies is None:
            self._recipe_dependencies = RecipeDependencies.from_node(self._recipe_node)
        return self._recipe_dependencies

    @property
    def ref(self):
        return self._recipe_node.ref

    @property
    def buildenv(self):
        # Lazy computation of the package buildenv based on the profileone
        from thirdparty.env import Environment
        if not isinstance(self._recipe_buildenv, Environment):
            self._recipe_buildenv = self._recipe_buildenv.get_profile_env(self.ref,
                                                                        self._is_consumer_recipe)
        return self._recipe_buildenv

    @property
    def runenv(self):
        # Lazy computation of the package runenv based on the profile one
        from thirdparty.env import Environment
        if not isinstance(self._recipe_runenv, Environment):
            self._recipe_runenv = self._recipe_runenv.get_profile_env(self.ref,
                                                                    self._is_consumer_recipe)
        return self._recipe_runenv

    @property
    def cpp_info(self):
        """
        Same as using ``self.cpp.package`` in the ``layout()`` method. Use it if you need to read
        the ``package_folder`` to locate the already located artifacts.
        """
        return self.cpp.package

    @cpp_info.setter
    def cpp_info(self, value):
        self.cpp.package = value

    def run(self, command: str, stdout=None, cwd=None, ignore_errors=False, env="", quiet=False,
            shell=True, scope="build", stderr=None):
        """ Run a command in the current package context.

        :parameter command: The command to run.
        :parameter stdout: The output stream to write the command output. If ``None``, it defaults to
            the standard output stream.
        :parameter stderr: The error output stream to write the command error output. If ``None``,
            it defaults to the standard error stream.
        :parameter cwd: The current working directory to run the command in.
        :parameter ignore_errors: If ``True``, do not raise an error if the command returns a
            non-zero exit code.
        :parameter env: The environment file to use. If empty, it defaults to ``"env_build"`` for
            when ``scope`` is ``build`` or ``"env_run"`` for ``run`` (the aggregated environment
            files produced by ``generate_aggregated_env``, which include vcvars on Windows).
            If set to ``None`` explicitly, no environment file will be applied,
            which is useful for commands that do not require any environment.
        :parameter quiet: If ``True``, suppress the output of the command.
        :parameter shell: If ``True``, run the command in a shell. This is passed to the
            underlying ``Popen`` function.
        :parameter scope: The scope of the command, either ``"build"`` or ``"run"``.
        """
        if env == "":  # This default allows not breaking for users with ``env=None`` indicating
            # they don't want any env-file applied
            env = "env_build" if scope == "build" else "env_run"

        env = [env] if env and isinstance(env, str) else (env or [])
        assert isinstance(env, list), "env argument to RecipeBase.run() should be a list"
        envfiles_folder = self.folders.generators or os.getcwd()
        wrapped_cmd = command_env_wrapper(self, command, env, envfiles_folder=envfiles_folder,
                                          scope=scope)
        from thirdparty._internal.util.runners import run_command
        if not quiet:
            self.output.info(f"RUN: {command}", fg=Color.BRIGHT_BLUE)
        self.output.debug(f"Full command: {wrapped_cmd}")
        if quiet or Output.get_output_level() == LEVEL_QUIET:
            stdout = subprocess.DEVNULL if stdout is None else stdout
            stderr = subprocess.DEVNULL if stderr is None else stderr
        retcode = run_command(wrapped_cmd, cwd=cwd, stdout=stdout, stderr=stderr, shell=shell)
        if not quiet:
            self.output.writeln("")

        if not ignore_errors and retcode != 0:
            raise RecipeException("Error %d while executing" % retcode)

        return retcode

    def __repr__(self):
        return self.display_name
