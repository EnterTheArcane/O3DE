import os
import subprocess
from abc import ABC
from collections import OrderedDict
from dataclasses import dataclass, field
from typing import (
    IO, Any, Generic, TypeVar, cast,
)

from thirdparty._internal.graph import CONTEXT_BUILD, CONTEXT_HOST
from thirdparty._internal.model.conf import Conf
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty._internal.model.info import Info
from thirdparty._internal.model.folders import Folders
from thirdparty._internal.model.options import Options
from thirdparty._internal.model.refs import RecipeReference
from thirdparty._internal.model.requires import Requirement
from thirdparty._internal.model.settings import Settings
from thirdparty._internal.model.version import Version
from thirdparty._internal.output import Output, Color, LEVEL_QUIET
from thirdparty._internal.subsystems import command_env_wrapper
from thirdparty._internal.util.detect import detect_settings
from thirdparty.errors import RecipeException


TOptions = TypeVar("TOptions", default=Any)

__all__ = ["RecipeBase", "RecipeState"]


@dataclass
class RecipeState:
    dependencies: RecipeDependencies
    build_context: bool
    settings: Settings
    settings_build: Settings
    conf: Conf


class RecipeBase(ABC, Generic[TOptions]):
    name: str
    version: str
    license: str | tuple[str, ...]

    options: TOptions

    win_bash: bool | None = None

    folders: Folders
    info: Info

    _state: RecipeState

    def __init_subclass__(cls, **kwargs: Any):
        super().__init_subclass__(**kwargs)

        Options.validate_recipe_class(cls)

        license = cls.__dict__.get("license")
        if isinstance(license, str):
            cls.license = (license,)
        elif license is not None:
            cls.license = tuple(license)

    def __init__(self):
        self.folders = Folders()
        self.info = Info(set_defaults=True)

        self._requires: list[Requirement] = []

        cast(Any, self).options = Options.from_recipe(type(self))
        settings = detect_settings()
        self._state = RecipeState(
            dependencies=RecipeDependencies(OrderedDict()),
            build_context=False,
            settings=settings,
            settings_build=settings,
            conf=Conf())
        self.env_scripts = {}  # Accumulate the env scripts generated in order

    def requires(self, ref: str, *, headers: bool = True, libs: bool = True, run: bool = False) -> None:
        self._add_requirement(Requirement(RecipeReference(ref), headers=headers, libs=libs, run=run))

    def requires_tool(self, ref: str, *, run: bool = True) -> None:
        self._add_requirement(Requirement(RecipeReference(ref), headers=False, libs=False, build=True, run=run))

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
    def settings(self) -> Settings:
        return self._state.settings

    @property
    def settings_build(self) -> Settings:
        return self._state.settings_build

    @property
    def conf(self) -> Conf:
        return self._state.conf

    @property
    def context(self) -> str:
        return CONTEXT_BUILD if self._state.build_context else CONTEXT_HOST

    @property
    def is_build_context(self) -> bool:
        return self._state.build_context

    @property
    def dependencies(self) -> RecipeDependencies:
        return self._state.dependencies

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
            raise RecipeException(f"Error {retcode} while executing")

        return retcode

    def latest_version(self) -> Version | None: ...
    def configure(self): ...
    def validate(self): ...
    def requirements(self): ...
    def source(self): ...
    def generate(self): ...
    def build(self): ...
    def package(self): ...
    def package_info(self): ...

    def __repr__(self):
        return self.name or ""
