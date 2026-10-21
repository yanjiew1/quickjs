#!/usr/bin/env python3
"""Exercise the compiled reader's public CLI on counted authored root bytes."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

from test_intl_collation_root_input import root, GROUPS, FINGERPRINT, wire


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    args = parser.parse_args()
    compiler = str(args.compiler.resolve())
    catalog = root()
    raw = wire.encode(catalog, GROUPS, FINGERPRINT)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'root.qjcr'
        path.write_bytes(raw)
        result = subprocess.run([compiler, '--audit-root', str(path)], check=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        report = json.loads(result.stdout.decode('utf-8'))
        assert report['mappings'] == len(catalog.data['rows'])
        assert report['han'] == len(catalog.han)
        assert report['primary_max'] == catalog.primary_max
        # Keep the header but truncate the sections, testing the root decoder
        # rather than command-line parsing or its initial minimum-size guard.
        path.write_bytes(raw[:wire.HEADER.size])
        result = subprocess.run([compiler, '--audit-root', str(path)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        assert result.returncode == 1 and b'invalid logical root' in result.stderr


if __name__ == '__main__':
    main()
