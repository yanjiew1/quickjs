# Optional reference ICU SDK

This offline builder accepts the pinned ICU4C 78.3 archive, with stock UCD 17.0,
CLDR 48.0, and tzdata 2026a. It applies a private exact plural operand patch and
the Hebrew epoch and Indian negative Julian date corrections, followed by
the ICU-owned Chinese PMO, coherence and cross-range repairs. The complete
ten-file source identity, local attribution, Unicode-3.0 license, and patch
scope are recorded in `PROVENANCE.json` and `NOTICE.txt`. No public ICU headers,
or class layouts are changed. Exported symbol and binary ABI equality remain
pending root runtime qualification. These are local patches;
upstream merge acceptance is not asserted.

Obtain `icu4c-78.3-sources.tgz` separately from the immutable release URL in
`PROVENANCE.json`. The builder verifies its SHA-256 and every packaged input
before extracting into a fresh output directory. It performs no downloads.
It requires Python 3.12 or newer for safe tar extraction.

```sh
python3 tools/build-reference-icu.py \
  --archive /absolute/path/icu4c-78.3-sources.tgz \
  --output /absolute/path/icu-reference-shared \
  --cc /absolute/path/gcc --cxx /absolute/path/g++ \
  --as /usr/bin/as --ld /usr/bin/ld --ldd /usr/bin/ldd \
  --profile shared --runner-identity '<immutable runner and toolchain image digest>' \
  --runtime-library-path /absolute/path/compiler/lib64 --check
```

The supported builder profile is native Linux GCC with an external GNU
assembler and GNU BFD linker. It rejects Clang, integrated assemblers, cross
compiler targets, and unsupported linker selections. A private `-B` directory
and `-fuse-ld=bfd` direct both GCC drivers through traced wrappers that execute
the exact `--as` and `--ld` files. The manifest retains those paths, resolved
paths and hashes, driver selection queries, verbose compile/link probes,
actual assembler/linker invocation traces, and unchanged tool identity checks.
`LD` alone does not establish the driver-selected linker. All inputs are
explicitly recorded; the wrappers use the declared and hashed shell.

The builder compiles and runs the unchanged plural, Hebrew and Indian public
ICU C API witnesses. In shared mode it retains `ldd` logs for all three and
requires `libicui18n`, `libicuuc`, and `libicudata` to resolve to the just-installed
prefix with exact installed file hashes. Missing, external or unexpected ICU
libraries fail qualification. Other resolved dependencies are recorded with
paths and hashes. Use `--profile static` with a new directory for a static SDK;
that profile links the three exact installed archive paths.

Run under the invoking process owner with an explicit timeout. Child commands
remain in its session. Root owns local SDK builds and runtime gates and must
apply the existing execution lease, cpuset, ownership journal, cleanup and
restoration rules. Preserve failed outputs and rerun in a fresh directory.

Set `PKG_CONFIG_LIBDIR` exclusively to `install/lib/pkgconfig` and clear
`PKG_CONFIG_PATH` for QuickJS. For shared execution, include `install/lib` and
the compiler runtime directory in `LD_LIBRARY_PATH`. Preserve the effective
`.obj/intl-build-config` and verify actual engine linkage. Use `CONFIG_INTL=y
CONFIG_TEMPORAL=y CONFIG_INTL_BACKEND=icu`; use `ICU_STATIC=y` for static mode.
Run complete `make test`, precision tests, metadata, API and official normal
vectors against the exact frontend source map with original assertions.
Dependency or SDK witness acceptance does not accept the frontend. System and
unmodified ICU qualification remain separate; native CLDR 48.2/tzdata 2026e
data are separate from these stock ICU data.

A sanitizer SDK needs a separate output and `--sanitizer address` or
`--sanitizer undefined`, followed by actual ICU internal instrumentation and
qualification gates. An instrumented frontend linked to a normal SDK does not
establish ICU internal instrumentation. Hosted ICU sanitizer qualification and
new builder runtime qualification remain pending. Do not add MSan acceptance
without complete dependency instrumentation.

The manifest includes archive/revision, patches and packaged inputs, builder,
compiler/binutils/shell/ldd hashes, sanitizer flags, configure arguments,
architecture/libc, source and installed manifests, linkage and test logs.
Supply an immutable runner/libc/toolchain/runtime image identity; compiler
binary hashes alone do not pin mutable compiler or runtime libraries. A cache
restore must compare the complete identity and installed manifest and rerun
all capability and shared linkage checks. No release-only or fallback keys.
The builder records build evidence and supplies no automatic cache service.

The final PMO source is applied as one exact `chinese-pmo.patch` after the two
unchanged parent patches. The builder verifies the complete patched ICU source
manifest and byte equality of all original public Unicode headers before
configuration. The private `source/i18n/astro.h` is included in the exact
source delta and is not a public installed Unicode header.

The unchanged PMO factual notice and original provenance are packaged as
`chinese_pmo_notice.txt` and `PMO-PROVENANCE.json`. The published Chinese window,
1899 approximation prefix, derived 2101 closing boundary, original HKO
differences and unchanged Dangi separation remain documented. The cross-range
repair uses the existing astronomical model outside the published table; no
new ephemeris accuracy is claimed. Root must retain all original PMO, coherence,
cross-range, extreme, outside, Dangi, full ICU and Intl/Temporal witnesses.

Only native Linux GCC SDK construction is supported. A GCC-built SDK may be
linked to an independently qualified Clang engine, but this does not accept a
Clang or cross compiler SDK builder. Final SDK/engine receipts, exported symbol
equality, existing ABI client acceptance and publication approval are pending.
