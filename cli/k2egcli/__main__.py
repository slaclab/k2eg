"""k2egclientry point script."""
# k2egcli/__main__.py

from k2egcli import cli, __app_name__

def main():
    cli.app(prog_name=__app_name__)

if __name__ == "__main__":
    main()