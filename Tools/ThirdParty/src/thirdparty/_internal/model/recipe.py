import os
import subprocess
import types
from abc import ABC
from typing import (
    IO, Any, ClassVar, Generic, Literal, TypeVar, Union, cast, get_args, get_origin,
    get_type_hints,
)

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


TOptions = TypeVar("TOptions", default=Any)


_ANY_OPTION_VALUE = "ANY"
_SCALAR_OPTION_TYPES = {str, int, float, Any}


class RecipeOptions:
    __defaults__: ClassVar[dict[str, Any]]
    __possible_values__: ClassVar[dict[str, list[Any]]]

    def get_safe(self, field: str, default: Any = None) -> Any:
        ...


def _is_none_type(annotation: Any) -> bool:
    return annotation is None or annotation is type(None)


def _append_unique(values: list[Any], value: Any) -> None:
    if value not in values:
        values.append(value)


def _derive_possible_values(name: str, annotation: Any) -> list[Any]:
    origin = get_origin(annotation)
    args = get_args(annotation)

    if _is_none_type(annotation):
        return [None]
    if annotation is bool:
        return [True, False]
    if origin is Literal:
        return list(args)
    if annotation in _SCALAR_OPTION_TYPES:
        return [_ANY_OPTION_VALUE]
    if origin in (types.UnionType, Union):
        result: list[Any] = []
        for arg in args:
            for value in _derive_possible_values(name, arg):
                _append_unique(result, value)
        if None in result:
            result.remove(None)
            result.insert(0, None)
        return result

    raise RecipeException(
        f"Unsupported typed option '{name}' annotation {annotation!r}. "
        "Supported annotations are bool, Literal, str, int, float, Any, "
        "and optional scalar forms like str | None")


def _typed_options_class(cls: type[Any]) -> type[Any] | None:
    for base in getattr(cls, "__orig_bases__", ()):
        if get_origin(base) is RecipeBase:
            args = get_args(base)
            if args and args[0] is not Any:
                return args[0]
    return None


def _derive_options(options_cls: type[Any]) -> tuple[dict[str, list[Any]], dict[str, Any]]:
    annotations = get_type_hints(options_cls, include_extras=True)
    explicit_defaults = getattr(options_cls, "__defaults__", {})
    explicit_possible_values = getattr(options_cls, "__possible_values__", {})
    options: dict[str, list[Any]] = {}
    defaults: dict[str, Any] = {}

    for name, annotation in annotations.items():
        if name.startswith("_"):
            continue
        options[name] = explicit_possible_values.get(name, _derive_possible_values(name, annotation))
        if name in options_cls.__dict__:
            defaults[name] = getattr(options_cls, name)
        elif name in explicit_defaults:
            defaults[name] = explicit_defaults[name]

    return options, defaults


class _Infos:
    def __init__(self):
        self.source: Info = Info()
        self.build: Info = Info()
        self.package: Info = Info(set_defaults=True)


class RecipeBase(ABC, Generic[TOptions]):
    name: str
    version: str
    license: str | tuple[str, ...]

    # Binary model: Settings and Options
    # ``settings`` is intentionally ``Any`` because the build driver assigns the settings model.
    # ``options`` is generic for recipe author typing, but still holds the runtime Options proxy.
    settings: Any = None  # set to a Settings object by the build driver (host/target)
    settings_build: Any = None  # Settings for the build machine (tools)
    settings_target: Any = None  # Settings of what a requires_tool will build for

    options: TOptions
    default_options: dict[str, Any] | None = None
    default_build_options: dict[str, Any] | None = None

    win_bash: bool | None = None
    win_bash_run: bool | None = None  # For run scope

    recipe_folder: str | None = None

    # Tools implied by the build-system helpers a recipe imports (e.g. CMake -> "cmake").
    # Populated at load time by the recipe loader from the recipe module's direct imports.
    _implicit_requires_tool: frozenset[str] = frozenset()

    folders: Folders
    infos: _Infos
    buildenv_info: Environment
    runenv_info: Environment
    conf_info: Conf
    conf: Conf

    def __init_subclass__(cls, **kwargs: Any):
        super().__init_subclass__(**kwargs)
        typed_options = _typed_options_class(cls)
        if typed_options is not None:
            if "options" in cls.__dict__ or "default_options" in cls.__dict__:
                raise RecipeException(
                    "Typed recipes must define options only in the RecipeBase[...] "
                    "options class, not with explicit options/default_options dictionaries")
            options, default_options = _derive_options(typed_options)
            recipe_cls = cast(Any, cls)
            recipe_cls.options = options
            recipe_cls.default_options = default_options

        license = cls.__dict__.get("license")
        if isinstance(license, str):
            cls.license = (license,)
        elif license is not None:
            cls.license = tuple(license)

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

        recipe_options = getattr(type(self), "options", None)
        cast(Any, self).options = Options(recipe_options or {}, self.default_options)
        self._recipe_dependencies: "RecipeDependencies | None" = None
        self.env_scripts = {}  # Accumulate the env scripts generated in order
        
        self._recipe_runtime = None
        self._recipe_buildenv = None  # The profile buildenv, will be assigned initialize()
        self._recipe_runenv = None
        self._recipe_node = None  # access to container Node object, to access info, context, deps...

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

    def latest_version(self) -> "str": ...
    def validate(self) -> None: ...
    def requirements(self) -> None: ...
    def source(self) -> None: ...
    def generate(self) -> None: ...
    def build(self) -> None: ...
    def package(self) -> None: ...
    def package_info(self) -> None: ...

    def __repr__(self):
        return self.name or ""
