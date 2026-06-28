import argparse
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from thirdparty._internal.cli.commands import build as build_command


def _args(*recipes: str) -> argparse.Namespace:
    return argparse.Namespace(
        recipe=list(recipes),
        build_type="Release",
        generate_only=False,
        resume=None,
        target_os=None,
        target_arch=None,
        dry_run=False,
        force=False,
        exact=False,
        fail_fast=False,
        jobs=None,
    )


class BuildCommandTests(unittest.TestCase):
    def test_build_all_ignores_directories_without_recipe_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            recipes_root = root / "recipes"
            (recipes_root / "kept").mkdir(parents=True)
            (recipes_root / "kept" / "recipe.py").write_text("class Recipe:\n    pass\n", encoding="utf-8")
            (recipes_root / "leftover-empty-dir").mkdir()

            prev_cwd = Path.cwd()
            os.chdir(root)
            try:
                with patch.object(build_command, "_build_ordered") as build_ordered:
                    build_command.build(_args())
            finally:
                os.chdir(prev_cwd)

        build_ordered.assert_called_once()
        self.assertEqual(build_ordered.call_args.args[2], ["kept"])

    def test_build_rejects_explicit_directory_without_recipe_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            recipes_root = root / "recipes"
            recipes_root.mkdir(parents=True)
            (recipes_root / "leftover-empty-dir").mkdir()

            prev_cwd = Path.cwd()
            os.chdir(root)
            try:
                stderr = io.StringIO()
                with redirect_stderr(stderr), self.assertRaises(SystemExit) as exc:
                    build_command.build(_args("leftover-empty-dir"))
            finally:
                os.chdir(prev_cwd)

        self.assertEqual(exc.exception.code, 1)
        self.assertIn("recipe not found: leftover-empty-dir", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
