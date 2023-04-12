"""Top-level package for k2eg cli."""
# k2egcli/__init__.py

__app_name__ = "k2egcli"
__version__ = "0.1.0"

(
    SUCCESS,
    DIR_ERROR,
) = range(2)

ERRORS = {
    DIR_ERROR: "config directory error",
}