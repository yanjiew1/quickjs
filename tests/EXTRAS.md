# Upstream extras benchmarks

These assets come from the official QuickJS extras release dated 2026-06-04:

- Release page: https://bellard.org/quickjs/
- Archive: https://bellard.org/quickjs/quickjs-extras-2026-06-04.tar.xz
- Download reviewed: 2026-10-07
- Archive SHA-256: `11549a45b25b055946eeac2a0064399297dcf80062c6c07b644e0bc5eb329817`

The files in `bench-v8/` and the JavaScript files in `octane/` retain their
upstream content. `extras-upstream.json` records each original file checksum
and the source and checksum of the additional license texts. These are performance workloads with their own result
checks. They are separate from the repository's unit tests and Test262.

## V8 version 7

`bench-v8/` contains the original workload sources, framework, documentation,
runner, Makefile, and combined script. The existing top-level `make bench-v8`
target runs this suite. Performance measurements can use the separate sources with a dedicated
runner so that each measured case has a fresh JavaScript process.

The archive's `combined.js` lacks the `"use strip";` line that is present in
`base.js`. Rebuilding the combined file with the upstream Makefile therefore
adds that line. Both files are preserved as distributed by upstream.

## Octane

`octane/run.js` can run a selected workload. For example, from the repository
root after building `qjs`:

```sh
./qjs tests/octane/run.js raytrace
./qjs tests/octane/run.js regexp
```

Some workloads use supporting files that the runner loads with the same name
prefix, such as `typescript` and `gbemu`. Running the script without a workload
argument runs the complete upstream suite, including zlib. The project does
not run that complete suite automatically; zlib remains outside the selected
performance measurements.

The upstream Octane runner reports benchmark errors through its output and
omits the final score on failure. An automated caller must check that output,
in addition to the process exit status.

## Copyright and licenses

The imported benchmarks are third-party assets. Their original copyright and
license notices remain in the individual files; the repository's QuickJS MIT
license does not replace those notices.

- V8 and most Octane benchmark files include their BSD-style license terms.
- `octane/pdfjs.js` and `octane/gbemu-part*.js` include GPL version 2 notices.
  The complete license is included as `octane/LICENSE-GPL-2.0.txt`, downloaded
  from https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
- The TypeScript workload includes Microsoft's Apache License, version 2.0
  notice in `octane/typescript.js` and its embedded source input. The complete
  license is included as `octane/LICENSE-APACHE-2.0.txt`, downloaded from
  https://www.apache.org/licenses/LICENSE-2.0.txt.
- Box2D, zlib, and other embedded code retain the notices in their source files.

## Optional Web Tooling Benchmark

The extras archive also contains `tests/cli/cli.js`. Despite its directory
name, it is a generated Webpack bundle of Web Tooling Benchmark version 0.5.3,
with Babel, Prepack, TypeScript, Uglify, and other third-party tooling bundled
inside it. It is 32,702,851 bytes and is not a QuickJS command-line unit test.
It is not vendored here. Its original embedded notices and licenses apply.

To retrieve the exact upstream bundle outside the repository, the following
optional recipe verifies both the archive and the selected file. It writes
only the selected regular file and does not extract other archive members.

```sh
python3 - <<'PYTHON'
import hashlib
import io
from pathlib import Path
import tarfile
import urllib.request

url = "https://bellard.org/quickjs/quickjs-extras-2026-06-04.tar.xz"
archive_sha = "11549a45b25b055946eeac2a0064399297dcf80062c6c07b644e0bc5eb329817"
file_sha = "a7a227840538deb2d44dcadbf3da6b12187b29961f2e134db2269b785f7d9931"
with urllib.request.urlopen(url, timeout=60) as response:
    archive = response.read()
if hashlib.sha256(archive).hexdigest() != archive_sha:
    raise RuntimeError("Upstream extras archive checksum mismatch")
with tarfile.open(fileobj=io.BytesIO(archive), mode="r:xz") as source:
    member = source.getmember("quickjs-2026-06-04/tests/cli/cli.js")
    if not member.isfile():
        raise RuntimeError("Expected a regular CLI benchmark file")
    with source.extractfile(member) as stream:
        data = stream.read()
if len(data) != 32702851 or hashlib.sha256(data).hexdigest() != file_sha:
    raise RuntimeError("Upstream CLI benchmark checksum mismatch")
output = Path("/srv/data/work/quickjs-tmp/quickjs-extra-cli/cli.js")
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(data)
print(output)
PYTHON
```

A developer can then run that optional workload with:

```sh
./qjs /srv/data/work/quickjs-tmp/quickjs-extra-cli/cli.js
```
