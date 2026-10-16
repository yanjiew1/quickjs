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
contain the generator's UCD and emoji inputs. `-Dfuzzing=true` enables the Clang
libFuzzer applications; select sanitizers separately with `b_sanitize`.

## Cross compilation

Use Meson's `--cross-file` and define its `exe_wrapper` when target executables
need an emulator. Native bytecode and Unicode generators are built with the
build-machine compiler. Script-driven tests consume the same wrapper through
Meson's `MESON_EXE_WRAPPER` environment variable. Target compiler flags must be
in the cross file, not in native compiler settings.
