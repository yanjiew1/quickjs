<!--
Copyright (c) 2017-2021 Fabrice Bellard
Copyright (c) 2017-2021 Charlie Gordon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

# Building QuickJS

Meson 0.58 or newer, Ninja, Python 3 and a C compiler are required. Keep each
configuration in a separate build directory; generated sources and test fixtures
stay there.

```sh
meson setup build -Dwerror=true
meson compile -C build
meson test -C build --print-errorlogs
meson install -C build
```

The default build uses optimization level 2 with debugging information and keeps
assertions enabled. Compiler selection follows Meson conventions (`CC` at setup,
or a native/cross file). Installation honors `--prefix`, `--libdir` and
`--includedir`; `pkg-config quickjs` describes the embedding library.

## Configurations

```sh
# The usual local correctness gate
meson setup build-sanitize -Db_sanitize=address,undefined -Dwerror=true
meson compile -C build-sanitize
meson test -C build-sanitize --print-errorlogs --num-processes 4

# Link-time optimization
meson setup build-lto -Db_lto=true

# Optional stress and allocation diagnostics
meson setup build-gc -Dgc_stress=true
meson test -C build-gc --suite gc-stress --print-errorlogs
meson setup build-iteration -Dmalloc_iter=true
```

LTO applies to engine code, including the VM, compiler and support algorithms.
Generated Unicode tables compile as ordinary objects with LTO disabled, then
join the same engine library. Their target architecture, sanitizer settings,
visibility and shared-library PIC settings still match that library.
Each build provides only its selected engine configuration. The installed
`libquickjs.a` uses that configuration's LTO setting; there is no second LTO
archive. `qjsc` applies the matching compiler/linker flags automatically when
generating an executable.

`--num-processes` controls concurrent repository tests. Use `--timeout-multiplier`
for slow emulators or instrumentation. The `threading` suite selects concurrency
tests; `--repeat` can repeat a focused test. Unit and integration tests use
separate Meson suites. Component tests share objects only when their source
identity and complete compilation settings match.

The non-runnable `CONFIG_CHECK_JSVALUE` type diagnostic is opt-in:

```sh
meson setup build-value-check -Dcheck_jsvalue=true
meson compile -C build-value-check quickjs-value-check
```

This separate representation is checked at compile time; it is never linked
into the interpreter used by runtime tests.

## Intl development profiles

Intl is currently incomplete and disabled by default. `-Dintl=minimal` and
`-Dintl=native` enable the native implementation with English/root or full
CLDR locale-data profiles. Both use the same algorithms and complete locale
identity metadata. They require no ICU, source-data download or runtime XML
parser. Prepared CLDR 49 beta 4 data is embedded in one compilation unit.

Current JavaScript APIs are `Intl.getCanonicalLocales()` and `Intl.Locale`
construction, getters, `toString()`, `maximize()`, `minimize()`, `getWeekInfo()`,
`getTextInfo()`, `getHourCycles()` and `getNumberingSystems()`. Other locale
preference methods and locale information methods are still being implemented.
The delivered formatting APIs are `Intl.ListFormat`, `Intl.PluralRules`, `Intl.NumberFormat`,
`Intl.RelativeTimeFormat`, `Intl.DurationFormat` and `Intl.DisplayNames`, including
their standard methods. Number and BigInt locale formatting uses the intrinsic
NumberFormat; Temporal.Duration locale formatting uses the intrinsic DurationFormat when
Temporal is enabled. DateTimeFormat, Collator and Segmenter are
still in development. Setup warns about this status; these options do not imply
complete Intl conformance.
Intl can be enabled independently of `-Dtemporal=false`. The embedding entry
point `JS_AddIntrinsicIntl()` publishes a realm's namespace in a raw context
after base objects are initialized. It is idempotent, reports exceptions on
failure and throws TypeError in a disabled build. Normal `JS_NewContext()`
contexts initialize the compiled feature automatically.

## Temporal

Temporal is enabled by default on this branch with all standard native calendars
and embedded IANA timezone rules. It has no ICU or installed timezone-data
dependency. Use `-Dcalendars=iso` to omit non-ISO calendar implementations and
their publication data. ISO arithmetic always remains in core; this option is
independent of the timezone backend selection.
Use `-Dtimezone_backend=minimal` for all Temporal classes with only core UTC
and explicit numeric offsets; that backend contains no additional named zones.
On Unix, the runtime discovers its default from `TZ`, including supported
names, an optional leading colon, and fixed POSIX offset forms such as
`UTC-5:30` (meaning `+05:30`). General POSIX DST expressions and zoneinfo paths
are not used as rule sources. On Windows, CLDR maps the stable Windows key and
the user's country to an IANA identifier. The first default lookup snapshots
the selected identifier for the runtime; Date and Temporal share it.
Embedders can override it with `JS_SetDefaultTimeZone()`.

`-Dsystem_timezone_policy=utc` (the default) uses UTC when discovery cannot
resolve a supported name. `-Dsystem_timezone_policy=host-offset` samples the
current host offset instead, falling back to UTC if it is not representable
as a minute-aligned timezone. This fixed offset has no DST or historical rules.
On Unix, that explicit fallback may use libc's installed host data; native
IANA calculations always use the embedded pack.

Use `-Dtemporal=false` to omit Temporal and retain Date's existing host policy.
Temporal embedding entry points remain callable and report TypeError when the
feature is disabled. `Temporal=auto` in the Test262 configuration follows the
runner's build setting. Calendar publication coverage, algorithm sources and
backend contracts are described in the
[native Temporal design](doc/temporal-design.md).

## Static and shared libraries

Use `-Ddefault_library=static` (the default), `shared`, or `both`. Shared builds
link the command-line tools to the shared engine. Only callable declarations in
`include/quickjs.h` and `include/quickjs-libc.h` are exported; internal functions
and table data remain hidden. Static archives retain the same public API.

On Windows, the runtime DLL installs into `bindir` beside the tools, and the
import library installs into `libdir`. On other platforms, the library
installs into `libdir/quickjs` and installed tools have the configured runtime
search path. Applications select their own runtime search path when linking.
`pkg-config --cflags --libs quickjs` supplies include and link flags;
`--static` also supplies support-library dependencies and the matching final-link
LTO flag when the installed archive contains bitcode. Such archives require a
compatible compiler/linker; ordinary shared-library consumers need no LTO flag.

In `shared` mode, `qjsc` executable output links the shared engine with a runtime
search path. In `static` and `both` modes, its executable output uses the static
archive, preserving standalone executable generation. Private white-box tests
use an internal static archive even in shared builds; it is not installed in
`shared` mode. The public embedding test and JavaScript suites exercise the
selected shared engine.

## External suites and developer tools

Full Test262 runs belong in CI. To prepare and run the pinned suite explicitly:

```sh
meson compile -C build test262-bootstrap
TEST262_TIMING_LOG=test262_timings.jsonl meson compile -C build test262-run
```

The runner accepts a `[requirements]` section in its configuration. Each entry
maps a file or directory path to whitespace-separated required features.
Paths are relative to the configuration file; directory paths end in `/`.
The longest matching path replaces a broader rule, and each test's own feature
metadata is still required. Declare every capability in `[features]`; an
unknown requirement is a configuration error. Capability skips are included in
the per-test timing report without loading the test's harness.

The runner uses all available processors by default. Set `TEST262_THREADS` to
limit it. `TEST262_PROGRESS_LOG` records progress alongside optional per-test
JSONL timings. `test262-default`, `test262-check`, `test262-update` and
`test262-legacy` provide the other runner modes. Bootstrap resets the Test262
checkout to the repository pin and applies its patches.

`stats`, `microbench` and `bench-v8` are explicit developer targets. Native
`unicode_gen` and `unicode_gen_test` are optional build targets. Configure
`-Dunicode_data=/path/to/ucd` to regenerate both Unicode table files inside the
build directory instead of using the checked-in tables. The directory must
contain the generator's UCD and emoji inputs. The `Unicode table regeneration`
CI job downloads the version pinned in `tools/unicode_download.sh`, verifies
`tools/unicode-data.sha256`, and compares both generated files byte for byte
with the repository. Update the downloader, checksums and generated tables
together when changing Unicode versions.

`-Dfuzzing=true` enables the Clang libFuzzer applications; select sanitizers
separately with `b_sanitize`.

The optional `docs` target builds HTML when `makeinfo` is installed. With
`texi2pdf`, `docs-pdf` also builds the PDF manual. Set `-Ddocumentation=enabled`
to require HTML tooling, or `disabled` to omit these targets. Documentation and
its generated version file remain in the build directory.

## Cross compilation

Use Meson's `--cross-file` and define its `exe_wrapper` when target executables
need an emulator. Native bytecode and Unicode generators are built with the
build-machine compiler. Script-driven tests consume the same wrapper through
Meson's `MESON_EXE_WRAPPER` environment variable. Target compiler flags must be
in the cross file, not in native compiler settings.

## Release archives

`tools/release.sh quickjs --output /path/to/releases` packages tracked sources,
including nested fixtures and build helpers. When documentation tools are
installed, it also builds and includes the manual. Untracked working notes and
build products are excluded. A source archive records its source revision and
whether the working tree contains changes.

The default release is the source archive. Explicit `binary`, `win_binary` and
`extras` selections require the corresponding toolchains or Unicode inputs.
All binary builds use temporary directories under the selected output path.
The optional `cosmo_binary` mode remains outside CI coverage.
