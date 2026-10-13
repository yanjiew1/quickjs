#!/bin/sh
# Exercise the object recipes without requiring every supported compiler.
set -eu
build_make=${1:-make}
build_tmp=$(mktemp -d)
trap 'rm -rf "$build_tmp"' EXIT HUP INT TERM
cat > "$build_tmp/compiler" <<'COMPILER'
#!/bin/sh
set -eu
output= depfile= mmd=no check=no
while [ "$#" -gt 0 ]; do
    case "$1" in
        -o) output=$2; shift ;;
        -MF) depfile=$2; shift ;;
        -MMD) mmd=yes ;;
        -DCONFIG_CHECK_JSVALUE) check=yes ;;
    esac
    shift
done
if [ "$BUILD_EXPECT_DEPS" = yes ]; then
    [ "$mmd" = yes ] && [ "$depfile" = "$output.d" ] || exit 1
    : > "$depfile"
else
    [ "$mmd" = no ] && [ -z "$depfile" ] || exit 1
fi
if [ "${BUILD_EXPECT_CHECK:-no}" = yes ]; then
    [ "$check" = yes ] || exit 1
fi
[ -n "$output" ]
: > "$output"
COMPILER
chmod +x "$build_tmp/compiler"
check_recipes() {
    build_mode=$1
    BUILD_EXPECT_DEPS=$2
    export BUILD_EXPECT_DEPS
    shift 2
    build_obj="$build_tmp/$build_mode"
    set -- "$@" \
        "$build_obj/src/cutils/cutils.o" \
        "$build_obj/src/cutils/cutils.host.o" \
        "$build_obj/src/cutils/cutils.pic.o" \
        "$build_obj/src/cutils/cutils.nolto.o" \
        "$build_obj/src/cutils/cutils.debug.o" \
        "$build_obj/src/cutils/cutils.fuzz.o" \
        "$build_obj/src/cutils/cutils.check.o" \
        "$build_obj/tools/unicode_gen.test.host.o" \
        "$build_obj/src/unicode/libunicode.test.host.o" \
        "$build_obj/src/quickjs/serialization/reader.trace.o" \
        "$build_obj/src/quickjs/number.o" \
        "$build_obj/src/quickjs/builtins/number.o"
    # Upstream source releases omit the optional fuzz sources.
    if [ -f fuzz/fuzz_eval.c ]; then
        set -- "$@" "$build_obj/fuzz/fuzz_eval.o"
    fi
    "$build_make" --no-print-directory -s CONFIG_CLANG= CONFIG_COSMO= \
        OBJDIR="$build_obj" CC="$build_tmp/compiler" \
        HOST_CC="$build_tmp/compiler" "$@"
}
check_recipes gcc yes
check_recipes clang yes CONFIG_CLANG=y
check_recipes cosmo yes CONFIG_COSMO=y
check_recipes clang-cosmo yes CONFIG_CLANG=y CONFIG_COSMO=y

check_core_coverage() {
    build_mode=$1
    shift
    build_obj="$build_tmp/coverage-$build_mode"
    BUILD_EXPECT_DEPS=yes
    BUILD_EXPECT_CHECK=yes
    export BUILD_EXPECT_DEPS BUILD_EXPECT_CHECK
    "$build_make" --no-print-directory -s CONFIG_CLANG= CONFIG_COSMO= \
        CONFIG_ASAN= CONFIG_UBSAN= PROGS= OBJDIR="$build_obj" \
        CC="$build_tmp/compiler" HOST_CC="$build_tmp/compiler" "$@" all
    # Compare source paths, not only counts: equal basenames have distinct owners.
    find src/quickjs -name '*.c' | sed 's/\.c$/.check.o/' | sort \
        > "$build_tmp/expected-checks"
    if [ ! -d "$build_obj/src/quickjs" ]; then
        echo "all omitted the core CONFIG_CHECK_JSVALUE checks" >&2
        exit 1
    fi
    find "$build_obj/src/quickjs" -name '*.check.o' \
        | sed "s|^$build_obj/||" | sort > "$build_tmp/actual-checks"
    diff -u "$build_tmp/expected-checks" "$build_tmp/actual-checks"
}
check_core_coverage gcc
check_core_coverage clang CONFIG_CLANG=y
check_core_coverage gcc-asan CONFIG_ASAN=y
check_core_coverage clang-asan CONFIG_CLANG=y CONFIG_ASAN=y
check_core_coverage gcc-ubsan CONFIG_UBSAN=y
check_core_coverage clang-ubsan CONFIG_CLANG=y CONFIG_UBSAN=y
