import argparse

from thirdparty._internal.cli.command import command


@command
def list(args: argparse.Namespace) -> None:
    """List available recipes."""
    print("List!")
