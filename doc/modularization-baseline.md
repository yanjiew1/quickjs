# Monolithic Baseline

Baseline revision: `04be246`, QuickJS version `2026-06-04`.
All baseline sources and binaries are retained in
`/tmp/quickjs3-validation/baseline/{gcc,gcc-lto,clang,clang-lto}`.

## Environment And Toolchains

- Ubuntu 22.04.5, x86_64, glibc 2.35.
- AMD Ryzen Threadripper PRO 3995WX; benchmarks pinned to CPU 0.
- GCC 11.4.0 (Ubuntu), GNU ar/gcc-ar, system linker.
- Official LLVM 23.1.1, commit
  `6dfe1677ab8dffbc6ec13d53a1e0215d75147689`, installed at
  `/home/yanjie/opt/llvm-23.1.1`. No ROCm tools are used.
- Archive: https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.1/LLVM-23.1.1-Linux-X64.tar.xz
- Archive retained at `~/opt/LLVM-23.1.1-Linux-X64.tar.xz`.
- SHA-256: `832aeb58d105de1cabc7b982dd2c65de0610f7377df48ae8fc2dd8e97420a15c`.
- The published digest matches. `gh attestation verify` succeeds using the
  release's `.jsonl` bundle and `--repo llvm/llvm-project`.
- Native, LTO, ASan/UBSan and MSan compiler/linker smoke checks pass.
  LeakSanitizer must run outside the restricted process sandbox.

Build with the original Makefile's flags and `CONFIG_WERROR=y`; add
`CONFIG_LTO=y` for LTO. Clang additionally uses `CONFIG_CLANG=y`,
`AR=/home/yanjie/opt/llvm-23.1.1/bin/llvm-ar`, and the following wrapper as
both `CC` and `HOST_CC`. The wrapper is also embedded in qjsc, so its generated
executables use the same compiler and linker:

```sh
#!/bin/sh
for arg do
    case "$arg" in
        -c|-S|-E) exec /home/yanjie/opt/llvm-23.1.1/bin/clang "$@" ;;
    esac
done
exec /home/yanjie/opt/llvm-23.1.1/bin/clang \
    --ld-path=/home/yanjie/opt/llvm-23.1.1/bin/ld.lld "$@"
```

The C++ wrapper substitutes `clang++`. Baseline and candidate must use identical
toolchains and flags. ROCm binaries must not enter the Clang configuration.

## Correctness

All four configurations pass clean builds, the existing `make test` suite, and
the new `tests/test_embed.c` public API harness. C++ embedding also passes with
G++ and official Clang++. Serialized script/module fixtures are byte-for-byte
identical across all configurations and execute successfully.

The isolated Test262 checkout is revision
`5c8206929d81b2d3d727ca6aac56c18358c8d790`, with `tests/test262.patch` applied.
All four configurations have an identical actual failure set:
**58 / 83,558 executions, 3,356 excluded, 6,000 skipped**.
The complete failure record is in `modularization-test262-baseline.txt`.
The runner's exit status is 1 because of those existing failures.

```sh
cp test262_errors.txt failures.txt
./run-test262 -c test262.conf -a -T 8 -u -e failures.txt -r none
diff failures.txt /path/to/baseline/failures.txt
```

The custom allocator harness exposes an existing failure at allocation budget
11: `JS_NewContext` fails before `ctx->loaded_modules` is initialized, then
`JS_FreeRuntime` reaches `JS_MarkContext`/`gc_decref_child` through cycle
collection and dereferences an invalid list entry. The original GCC build
crashes. This structural refactor must preserve initialization/failure ordering;
fixing that bug is separate work. The default harness covers budgets 0-10 and a
successful initialization followed by forced evaluation OOM. Individual budgets
can be compared in isolated processes with `test_embed oom N`.

## Size And Measurements

GNU `size qjs`, bytes:

| Configuration | Text | Data | BSS |
|---|---:|---:|---:|
| GCC | 1028539 | 29848 | 336 |
| GCC LTO | 1069559 | 29880 | 272 |
| Clang | 1103870 | 29496 | 1824 |
| Clang LTO | 1167013 | 29688 | 4024 |

Measurements use `taskset -c 0`, one warmup and seven measured runs. The existing
`tests/microbench.js` suite reports nanoseconds per operation. The additional
`tests/bench_workloads.c` measures complete runtime/context startup, compilation,
and promise chains with queue draining. Compile that harness at `-O2 -fwrapv`,
adding `-flto` and linking `libquickjs.lto.a` for the LTO configurations.

Raw measurements are retained beside each baseline executable as
`microbench-{0..7}.json` and `workloads-{0..7}.json`. Checked-in medians are in
`modularization-baseline.json`. Final comparisons must alternate retained
baseline/candidate runs and use `tests/compare_bench.js` to check the approved
3% geometric-mean and 5% individual slowdown gates, separately for GCC non-LTO
and LTO. Clang comparisons are reported separately.
