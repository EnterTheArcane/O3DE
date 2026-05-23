import argparse
import importlib
import pkgutil
import sys
from collections.abc import Callable
from typing import cast
from types import ModuleType

import thirdparty.cli.commands as _commands_pkg
from thirdparty.cli.command import CommandFn, is_command

_SetupParserFn = Callable[[argparse.ArgumentParser], None]
_CommandEntry = tuple[CommandFn, _SetupParserFn | None]


def _discover_commands() -> dict[str, _CommandEntry]:
    commands: dict[str, _CommandEntry] = {}
    for module_info in pkgutil.iter_modules(list(_commands_pkg.__path__)):
        module: ModuleType = importlib.import_module(
            f"{_commands_pkg.__name__}.{module_info.name}"
        )
        for attr_name in dir(module):
            obj = getattr(module, attr_name)
            if is_command(obj):
                setup_raw = getattr(module, "setup_parser", None)
                setup = (cast("_SetupParserFn", setup_raw) if callable(setup_raw) else None)
                commands[obj.__name__] = (obj, setup)
    return commands


def main() -> None:
    commands = _discover_commands()

    parser = argparse.ArgumentParser(prog="thirdparty")
    subparsers = parser.add_subparsers(dest="command", metavar="<command>")
    subparsers.required = True

    for name, (fn, setup) in commands.items():
        sub = subparsers.add_parser(name, help=fn.__doc__)
        if setup is not None:
            setup(sub)

    args = parser.parse_args()

    entry = commands.get(args.command)
    if entry is None:
        parser.print_help()
        sys.exit(1)

    fn, _ = entry
    fn(args)
