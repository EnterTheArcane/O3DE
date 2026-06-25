import os
import subprocess
from typing import IO, Any

from thirdparty._internal.graph import CONTEXT_BUILD
from thirdparty._internal.model.conf import Conf
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty._internal.model.info import Info
from thirdparty._internal.model.layout import Folders
from thirdparty._internal.model.options import Options
from thirdparty._internal.model.refs import RecipeReference
from thirdparty._internal.model.requires import Requirement
from thirdparty._internal.output import Output, Color, LEVEL_QUIET
from thirdparty._internal.subsystems import command_env_wrapper
from thirdparty.env import Environment
from thirdparty.errors import RecipeException


class _Infos:
    def __init__(self):
        self.source: Info = Info()
        self.build: Info = Info()
        self.package: Info = Info(set_defaults=True)


class RecipeBase:
    """
    The base class for all package recipes
    """

    # Reference. Every concrete recipe declares ``name`` (it must match the recipe's directory);
    # the empty-string default only applies to the abstract base, so ``name`` is effectively a
    # required ``str`` and callers can read it directly instead of going through a reference.
    name: str = ""
    version: str | None = None  # Any str, can be "1.1" or whatever

    # Metadata
    license: str | tuple[str, ...] | None = None

    # Binary model: Settings and Options
    # NOTE: ``settings``/``options`` are intentionally ``Any``: recipe authors may set them as a
    # tuple/dict (e.g. ``options = {...}``) while at runtime they hold model objects
    # (``Settings``, ``Options``).  ``Any`` keeps both valid.
    settings: Any = None  # set to a Settings object by the build driver (host/target)
    settings_build: Any = None  # Settings for the build machine (tools)
    settings_target: Any = None  # Settings of what a requires_tool will build for
    options: Any = None
    default_options: dict[str, Any] | None = None
    default_build_options: dict[str, Any] | None = None

    win_bash: bool | None = None
    win_bash_run: bool | None = None  # For run scope

    _is_consumer_recipe: bool = False

    # #### Requirements
    # Dependencies are declared imperatively from requirements() via self.requires() /
    # self.requires_tool(); they accumulate in self._requires (set up in __init__).
    tested_reference_str: str | None = None

    recipe_folder: str | None = None

    # Tools implied by the build-system helpers a recipe imports (e.g. CMake -> "cmake").
    # Populated at load time by the recipe loader from the recipe module's direct imports.
    _implicit_requires_tool: frozenset[str] = frozenset()

    # Package information.
    folders: Folders
    infos: _Infos
    buildenv_info: Environment
    runenv_info: Environment
    conf_info: Conf
    conf: Conf

    _recipe_runtime: Any = None
    _recipe_buildenv: Any = None  # The profile buildenv, will be assigned initialize()
    _recipe_runenv: Any = None
    _recipe_node: Any = None  # access to container Node object, to access info, context, deps...

    def __init__(self):
        self.folders = Folders()
        self.infos = _Infos()
        self.buildenv_info = Environment()
        self.runenv_info = Environment()
        self.conf_info = Conf()

        # Requirements accumulate here as the recipe calls self.requires() /
        # self.requires_tool() from requirements(). Implicit tools (from imported build-system
        # helpers) are registered afterwards in run_configure_method, so explicit decls win.
        self._requires: list[Requirement] = []

        self.options = Options(self.options or {}, self.default_options)
        self._recipe_dependencies: "RecipeDependencies | None" = None
        self.env_scripts = {}  # Accumulate the env scripts generated in order

    def requires(self, ref: str, *, headers: bool = True, libs: bool = True,
                 run: bool = False) -> None:
        """Declare a regular (library) dependency. Call from requirements()."""
        self._add_requirement(
            Requirement(RecipeReference(ref), headers=headers, libs=libs, run=run))

    def requires_tool(self, ref: str, *, run: bool = True) -> None:
        """Declare a build tool dependency (e.g. cmake). Call from requirements()."""
        self._add_requirement(
            Requirement(RecipeReference(ref), headers=False, libs=False, build=True, run=run))

    def _add_requirement(self, req: Requirement) -> None:
        if any(r == req for r in self._requires):  # equality == (name, build)
            raise RecipeException(f"Duplicated requirement: {req.name}")
        self._requires.append(req)

    @property
    def output(self) -> Output:
        # an output stream (writeln, info, warn error)
        scope = self.name or ""
        return Output(scope=scope)

    @property
    def context(self):
        return self._recipe_node.context

    @property
    def is_build_context(self):
        return self.context == CONTEXT_BUILD

    @property
    def dependencies(self):
        # Caching it, this object is requested many times. The build driver assigns the real
        # dependency graph (see build.py); fall back to an empty set when none was provided.
        if self._recipe_dependencies is None:
            self._recipe_dependencies = RecipeDependencies({})
        return self._recipe_dependencies

    @property
    def recipe(self) -> "str | None":
        return self._recipe_node.recipe if self._recipe_node else None

    @property
    def buildenv(self):
        return self._recipe_buildenv

    @property
    def runenv(self):
        return self._recipe_runenv

    @property
    def info(self):
        """
        Same as using ``self.infos.package`` in the ``layout()`` method. Use it if you need to read
        the ``package_folder`` to locate the already located artifacts.
        """
        return self.infos.package

    @info.setter
    def info(self, value):
        self.infos.package = value

    def run(
        self,
        command: str,
        stdout: IO[Any] | None = None,
        cwd: str | None = None,
        ignore_errors: bool = False,
        env: str | list[str] | None = "",
        quiet: bool = False,
        shell: bool = True,
        scope: str = "build",
        stderr: IO[Any] | None = None) -> int:
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
        wrapped_cmd = command_env_wrapper(self, command, env, envfiles_folder=envfiles_folder, scope=scope)
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
        return self.name or ""
