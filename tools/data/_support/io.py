"""Bounded byte transport and atomic outputs; source trust stays with callers."""
import os
from pathlib import Path
import tempfile
import time
import urllib.error
import urllib.request


def read_bytes(path, limit):
    # Bound the read itself, including a file that grows after a size check.
    with Path(path).open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('input exceeds byte limit: ' + str(path))
    return data


def download_bytes(url, limit, attempts=3, user_agent='QuickJS-data-generator'):
    if not url.startswith('https://'):
        raise ValueError('data sources must use HTTPS')
    deadline = time.monotonic() + 120
    for attempt in range(attempts):
        try:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError('data download exceeded time bound')
            request = urllib.request.Request(url, headers={'User-Agent': user_agent})
            with urllib.request.urlopen(request, timeout=min(30, remaining)) as response:
                if not response.geturl().startswith('https://'):
                    raise ValueError('data source redirected away from HTTPS')
                data = bytearray()
                while True:
                    if time.monotonic() > deadline:
                        raise TimeoutError('data download exceeded time bound')
                    # One underlying read lets progress remain observable to
                    # the overall deadline, even for a continuously slow peer.
                    chunk = response.read1(min(128 * 1024, limit + 1 - len(data)))
                    if time.monotonic() > deadline:
                        raise TimeoutError('data download exceeded time bound')
                    if not chunk:
                        return bytes(data)
                    data.extend(chunk)
                    if len(data) > limit:
                        raise ValueError('data download exceeded size bound')
        except (OSError, urllib.error.URLError):
            if attempt + 1 == attempts or time.monotonic() >= deadline:
                raise


def write_atomic(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    # Preserve Ninja restat without allocating a second whole embedded pack.
    if path.is_file():
        with path.open('rb') as previous:
            for at in range(0, len(data), 64 * 1024):
                if previous.read(64 * 1024) != data[at:at + 64 * 1024]:
                    break
            else:
                if not previous.read(1):
                    return
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=str(path.parent), delete=False) as output:
            temporary = output.name
            output.write(data)
        os.replace(temporary, str(path))
        temporary = None
    finally:
        if temporary is not None:
            os.unlink(temporary)
