#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Compile one prepared profile's exact collation targets with the host tool."""
import argparse
import json
from pathlib import Path
import tempfile

import acquire
import collation_archive
import collation_bindings
import collation_ir
import collation_programs
import collation_root_input


def prepare(sources, profile, compiler, output):
    catalog = collation_programs.load(sources)
    bindings = collation_bindings.from_catalog(catalog, profile)
    targets = collation_bindings.program_targets(bindings)
    fingerprint = bytes.fromhex(sources.pin['manifest_sha256'])
    output.parent.mkdir(parents=True, exist_ok=True)

    # Keep large transient compiler inputs beside the requested destination;
    # normal engine builds use the resulting prepared bytes without downloads.
    with tempfile.TemporaryDirectory(dir=str(output.parent)) as temporary:
        root, programs = Path(temporary) / 'root.qjcr', Path(temporary) / 'programs.qjci'
        root.write_bytes(collation_root_input.prepare(sources))
        programs.write_bytes(collation_ir.encode_catalog(catalog, fingerprint, targets))
        collation_archive.compile_archive(compiler, root, programs, output, (fingerprint, bindings))

    return {'profile': profile, 'programs': len(targets), 'binding_rows': len(bindings),
            'bytes': output.stat().st_size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path,
                        help='explicit alternate source pin for release/Git comparison')
    parser.add_argument('--profile', choices=('minimal', 'native'), required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(acquire.Sources(args.sources, acquire.load_pin(args.source_pin)), args.profile, args.compiler, args.output),
                     sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
