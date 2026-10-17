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

`--num-processes` controls concurrent repository tests. Use `--timeout-multiplier`
for slow emulators or instrumentation. The `threading` suite selects concurrency
tests; `--repeat` can repeat a focused test. Compile-only `CONFIG_CHECK_JSVALUE`
checks are included in every default build.

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
`--static` also supplies support-library dependencies.

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

## Experimental JSValue representations

Select `-Dvalue_representation=legacy` (default), `wide`, or `compact` in a
separate build directory. Legacy retains the original platform choice: an
8-byte NaN box on 32-bit targets and a 16-byte union/tag on 64-bit targets.
Wide uses the union/tag implementation on both. Compact uses a common 8-byte,
Q=1 NaN-boxed format with a reference-count flag, major tags, and pointer
three-bit subtags. Compact reference tag numbers match these encoded subtype
bits and differ from the legacy tag numbers. Use symbolic `JS_TAG_*` constants.
Immediate values do not use these low-bit subtags.
It requires binary64 doubles, pointers below 2^49, and at least
8-byte alignment for boxed pointers. Unsupported addresses fail fast rather
than being truncated, including in release builds. Pointer metadata such as
HWASan/MTE tags is not supported: select wide for those configurations.

Compact inline BigInts hold signed 48-bit values on 64-bit platforms and signed
32-bit values on 32-bit platforms. Heap limb width remains unchanged (64 bits
on 64-bit targets with `__int128`). Numeric NaNs are canonicalized; raw
ArrayBuffer bytes are preserved. Serialization encodes logical values, not the
in-memory JSValue layout.

Changing modes changes the public ABI. Rebuild applications, native modules,
and generated executable output with matching headers and libraries. The
installed `quickjs-config.h` records the mode; build-tree Meson dependencies
propagate it too. Direct source-header users must supply the matching
`-DJS_VALUE_MODE=0`, `1`, or `2`; do not mix a source default header with a
compact library. `qjsc` executable generation uses its matching configured
headers. The original representation remains available for unsupported
platforms and sanitizer configurations.

### Experimental VM stack cache

Enable the experimental cache with `-Dvm_stack_cache=true`. It is off by
default because the measured workloads do not show an overall improvement.
Use separate build directories to compare both modes. This option does not
change bytecode or the public ABI.

The top operand is kept in a local value. Constants, unchecked local/argument
loads and numeric arithmetic produce cached results. Unchecked local/argument
stores can consume them directly; other instructions materialize the top before
entry. A push publishes the former top before it becomes a lower stack slot.
Slow helpers, calls, exception handling and suspension retain the physical-stack
contract. Both computed-goto and switch dispatch use the same handler bodies;
`CFLAGS=-DDIRECT_DISPATCH=0` selects the switch path for validation.
