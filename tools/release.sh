#!/bin/sh
# Keep the release entry point; all builds use separate Meson directories.
set -eu
release_tools=$(CDPATH= cd "$(dirname "$0")" && pwd)
exec "${PYTHON:-python3}" "$release_tools/release.py" "$@"
