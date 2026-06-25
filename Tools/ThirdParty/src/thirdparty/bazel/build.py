import os
import platform
from typing import TYPE_CHECKING

from thirdparty.bazel.toolchain import BazelToolchain

if TYPE_CHECKING:
    from thirdparty._internal.model.recipe import RecipeBase


class Bazel:
    _recipe: RecipeBase
    _recipe_bazelrc: str
    _use_recipe_config: bool
    _startup_opts: str

    def __init__(self, recipe: RecipeBase):
        """
        :param recipe: ``< RecipeBase object >`` The current recipe object. Always use ``self``.
        """
        self._recipe = recipe
        # Use BazelToolchain generated file if exists
        self._recipe_bazelrc = os.path.join(self._recipe.folders.generators, BazelToolchain.bazelrc_name)
        self._use_recipe_config = os.path.exists(self._recipe_bazelrc)
        self._startup_opts = self._get_startup_command_options()

    def _safe_run_command(self, command: str):
        """
        Windows is having problems stopping bazel processes, so it ends up locking
        some files if something goes wrong. Better to shut down the Bazel server after running
        each command.
        """
        try:
            self._recipe.run(command)
        finally:
            if platform.system() == "Windows":
                self._recipe.run("bazel" + self._startup_opts + " shutdown")

    def _get_startup_command_options(self) -> str:
        bazelrc_paths = []
        if self._use_recipe_config:
            bazelrc_paths.append(self._recipe_bazelrc)
        # User bazelrc paths have more prio than Recipe one
        # See more info in https://bazel.build/run/bazelrc
        bazelrc_paths.extend(
            self._recipe.conf.get(
                "tools.google.bazel:bazelrc_path", default=[], check_type=list))
        opts = " ".join(["--bazelrc=" + rc.replace("\\", "/") for rc in bazelrc_paths])
        return f" {opts}" if opts else ""

    def build(self, args=None, target: str = "//...", clean: bool = True):
        """
        Runs "bazel <rcpaths> build <configs> <args> <targets>" command where:

        * ``rcpaths``: adds ``--bazelrc=xxxx`` per rc-file path. It listens to ``BazelToolchain``
          (``--bazelrc=recipe_bzl.rc``), and ``tools.google.bazel:bazelrc_path`` conf.
        * ``configs``: adds ``--config=xxxx`` per bazel-build configuration.
          It listens to ``BazelToolchain`` (``--config=recipe-config``), and
          ``tools.google.bazel:configs`` conf.
        * ``args``: they are any extra arguments to add to the ``bazel build`` execution.
        * ``targets``: all the target labels.

        :param target: It is the target label. By default, it's "//..." which runs all the targets.
        :param args: list of extra arguments to pass to the CLI.
        :param clean: boolean that indicates to run a "bazel clean" before running the "bazel build".
                      Notice that this is important to ensure a fresh bazel cache every
        """
        # Note: In case of error like this: ... https://bcr.bazel.build/: PKIX path building failed
        # Check this comment: https://github.com/bazelbuild/bazel/issues/3915#issuecomment-1120894057
        bazelrc_build_configs = []
        if self._use_recipe_config:
            bazelrc_build_configs.append(BazelToolchain.bazelrc_config)
        command = "bazel" + self._startup_opts + " build"
        bazelrc_build_configs.extend(
            self._recipe.conf.get(
                "tools.google.bazel:configs", default=[], check_type=list))
        for config in bazelrc_build_configs:
            command += f" --config={config}"
        if args:
            command += " ".join(f" {arg}" for arg in args)
        command += f" {target}"
        if clean:
            self._safe_run_command("bazel" + self._startup_opts + " clean")
        self._safe_run_command(command)

    def test(self, target=None):
        """
        Runs "bazel test <targets>" command.
        """
        if self._recipe.conf.get("tools.build:skip_test", check_type=bool) or target is None:
            return
        self._safe_run_command("bazel" + self._startup_opts + f" test {target}")
