"""This module provides the k2eg CLI."""
# k2egcli/cli.py

from typing import Optional

import typer

from k2egcli import __app_name__, __version__, command

app = typer.Typer()

app.add_typer(command.app, name="command")

def _version_callback(value: bool) -> None:
    if value:
        typer.echo(f"{__app_name__} v{__version__}")
        raise typer.Exit()

@app.callback()
def main(
    version: Optional[bool] = typer.Option(
        None,
        "--version",
        "-v",
        help="Show the application's version and exit.",
        callback=_version_callback,
        is_eager=True,
    )
) -> None:
    return