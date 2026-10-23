"""Atomic, deterministic candidate outputs shared by maintenance commands."""
import json
import os
from pathlib import Path
import tempfile


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode('utf-8')


def write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=str(path.parent), delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        os.replace(str(temporary), str(path))
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink()
