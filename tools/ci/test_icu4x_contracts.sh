#!/bin/sh
# Copyright (c) 2026 Yan-Jie Wang
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

# Invoke from the repository root. Keep compilation separate from execution
# so CI reports build failures and assertion/sanitizer failures distinctly.
set -eu

compile_contract()
{
    name=$1
    shift
    echo "Compile ICU4X $name contract"
    ${CC:-cc} -std=c11 -g -O1 -Wall -Wextra -Werror -I. \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        "$@" "$bridge" -lgcc_s -lutil -lrt -lpthread -lm -ldl \
        -o "$output/$name"
}

# RelativeTimeFormat consumes this NumberFormat provider too. Keep its exact
# implementation list in one place so every contract links the same owners.
compile_number_contract()
{
    contract_name=$1
    shift
    compile_contract "$contract_name" \
        data/intl/icu4x-number.c \
        src/intl/number/icu4x.c \
        src/intl/number/affix.c \
        src/intl/number/body.c \
        src/intl/number/exponent.c \
        src/intl/number/round.c \
        src/intl/plural/rules.c \
        src/intl/currency/boundary-icu4x.c \
        src/intl/icu4x/common.c \
        src/intl/common/decimal.c \
        src/intl/common/format-builder.c \
        src/intl/common/text.c \
        src/intl/common/pair-pattern.c \
        src/intl/common/unary-pattern.c \
        src/intl/common/range-compose.c \
        "$output/cutils.o" \
        data/intl/icu4x-unit.c \
        src/intl/number/unit-icu4x.c \
        src/intl/number/unit.c \
        src/intl/native/data.c \
        src/intl/native/pack.c \
        src/intl/common/locale-grammar.c \
        src/intl/common/source-locale.c \
        src/intl/unit/data.c \
        src/intl/unit/grammar-data.c \
        src/intl/unit/identifier.c \
        src/intl/unit/pattern.c \
        src/intl/unit/format.c \
        "$@"
}

case ${1:-} in
compile)
    test "$#" = 3
    bridge=$2
    output=$3
    mkdir -p "$output"
    # Match cutils' established warning policy for upstream callback/signed
    # arithmetic conventions, scoped to that object rather than new providers.
    ${CC:-cc} -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -I. \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -c src/cutils/cutils.c -o "$output/cutils.o"
    # dtoa uses the same established signed/unsigned arithmetic conventions;
    # isolate that policy from the new provider sources compiled below.
    ${CC:-cc} -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-sign-compare -Wno-unused-parameter -I. -Isrc/cutils \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -c src/dtoa/dtoa.c -o "$output/dtoa.o"
    compile_contract collation-bridge tests/icu4x/collation-bridge.c
    compile_contract collation \
        data/intl/icu4x-locale.c \
        src/intl/collation/icu4x.c \
        src/intl/icu4x/common.c \
        tests/icu4c/helpers.c \
        tests/icu4c/collation.c
    compile_contract collation-inventory \
        data/intl/icu4x-locale.c \
        src/intl/collation/icu4x.c \
        src/intl/icu4x/common.c \
        tests/icu4c/helpers.c \
        tests/icu4x/collation.c
    compile_contract unit-bridge src/intl/unit/identifier.c tests/icu4x/unit-bridge.c
    compile_contract unit-compose \
        data/intl/icu4x-unit.c \
        src/intl/number/unit-icu4x.c \
        src/intl/number/unit.c \
        src/intl/native/data.c \
        src/intl/native/pack.c \
        src/intl/common/locale-grammar.c \
        src/intl/common/source-locale.c \
        src/intl/common/pair-pattern.c \
        src/intl/common/unary-pattern.c \
        src/intl/common/text.c \
        src/intl/common/format-builder.c \
        src/intl/unit/data.c \
        src/intl/unit/grammar-data.c \
        src/intl/unit/identifier.c \
        src/intl/unit/pattern.c \
        src/intl/unit/format.c \
        src/intl/plural/rules.c \
        src/intl/icu4x/common.c \
        "$output/cutils.o" \
        tests/icu4c/helpers.c \
        tests/icu4x/unit-compose.c
    compile_contract unit-data \
        data/intl/icu4x-unit.c \
        src/intl/native/data.c \
        src/intl/native/pack.c \
        src/intl/common/locale-grammar.c \
        src/intl/common/source-locale.c \
        src/intl/unit/data.c \
        src/intl/unit/grammar-data.c \
        src/intl/unit/identifier.c \
        tests/icu4c/helpers.c \
        tests/icu4x/unit-data.c
    # The pure duration/epoch arithmetic TUs also contain unrelated calendar
    # operations. Discard those unused sections in this isolated contract;
    # the complete engine links their normal selected calendar dependencies.
    compile_number_contract duration -Isrc/cutils -ffunction-sections -fdata-sections -Wl,--gc-sections \
        src/intl/list/icu4x.c \
        src/intl/duration/provider.c \
        src/intl/duration/icu4x.c \
        src/intl/duration/composition.c \
        src/intl/duration/digital.c \
        src/intl/duration/options.c \
        src/intl/duration/decimal.c \
        src/intl/duration/format.c \
        src/temporal/duration.c \
        src/temporal/epoch.c \
        src/temporal/options.c \
        tests/icu4c/helpers.c \
        tests/icu4c/duration.c
    compile_contract duration-bridge \
        src/intl/duration/icu4x.c \
        src/intl/duration/digital.c \
        src/intl/common/format-builder.c \
        src/intl/common/text.c \
        src/intl/icu4x/common.c \
        "$output/cutils.o" \
        tests/icu4c/helpers.c \
        tests/icu4x/duration-bridge.c
    compile_contract relative-bridge tests/icu4x/relative-bridge.c
    compile_contract currency tests/icu4x/currency.c
    compile_contract currency-boundary -DQJS_TEST_ICU4X \
        src/intl/icu4x/common.c \
        src/intl/currency/boundary-icu4x.c \
        tests/test_intl_currency_boundary.c
    compile_number_contract number \
        tests/icu4c/helpers.c \
        tests/icu4x/number.c
    compile_number_contract relative -Isrc/cutils -Isrc/dtoa \
        tests/icu4c/helpers.c \
        data/intl/icu4x-locale.c \
        src/intl/relative/icu4x.c \
        src/intl/relative/format.c \
        src/intl/plural/icu4x.c \
        src/intl/common/number-decimal.c \
        "$output/dtoa.o" \
        tests/icu4c/relative.c
    compile_number_contract relative-extra -Isrc/cutils -Isrc/dtoa \
        tests/icu4c/helpers.c \
        data/intl/icu4x-locale.c \
        src/intl/relative/icu4x.c \
        src/intl/relative/format.c \
        src/intl/plural/icu4x.c \
        src/intl/common/number-decimal.c \
        "$output/dtoa.o" \
        tests/icu4x/relative.c
    compile_contract currency-number tests/icu4x/currency-number.c
    compile_contract currency-format tests/icu4x/currency-format.c
    compile_contract compact tests/icu4x/compact.c
    compile_contract decimal \
        src/intl/number/body.c \
        tests/icu4c/helpers.c \
        tests/icu4x/decimal.c
    compile_contract names tests/icu4x/names.c
    compile_contract display \
        data/intl/icu4x-display.c \
        src/intl/display/icu4x.c \
        src/intl/icu4x/common.c \
        src/intl/common/locale-grammar.c \
        tests/icu4c/helpers.c \
        tests/icu4x/display.c
    compile_contract locale \
        data/intl/icu4x-locale.c \
        src/intl/icu4x/common.c \
        src/intl/common/locale-grammar.c \
        src/intl/locale/identifier.c \
        src/intl/locale/matching.c \
        src/intl/locale/preferences.c \
        src/intl/locale/icu4x.c \
        tests/icu4c/helpers.c \
        tests/icu4x/locale.c
    compile_contract segment \
        data/intl/icu4x-locale.c \
        src/intl/icu4x/common.c \
        src/intl/segment/icu4x.c \
        src/intl/segment/prepared.c \
        tests/icu4c/helpers.c \
        tests/icu4c/segment.c
    compile_contract plural-data \
        data/intl/icu4x-locale.c \
        src/intl/plural/rules.c \
        tests/icu4x/plural.c
    compile_contract plural-service \
        data/intl/icu4x-locale.c \
        src/intl/icu4x/common.c \
        src/intl/plural/icu4x.c \
        src/intl/plural/rules.c \
        src/intl/common/decimal.c \
        tests/icu4c/helpers.c \
        tests/icu4c/plural.c
    compile_contract list \
        src/intl/icu4x/common.c \
        src/intl/list/icu4x.c \
        tests/icu4c/helpers.c \
        tests/icu4c/list.c
    compile_contract casing \
        src/intl/icu4x/common.c \
        src/intl/common/locale-grammar.c \
        src/intl/locale/case-icu4x.c \
        tests/icu4c/helpers.c \
        tests/icu4x/case.c
    ;;
test)
    test "$#" = 2
    output=$2
    for name in locale segment plural-data plural-service list casing names currency display decimal currency-number currency-boundary currency-format compact number collation-bridge collation collation-inventory relative-bridge relative relative-extra unit-data unit-bridge unit-compose duration duration-bridge; do
        echo "Run ICU4X $name contract"
        timeout --kill-after=5s 60s "$output/$name"
    done
    ;;
*)
    echo "usage: $0 compile BRIDGE_ARCHIVE OUTPUT_DIR | test OUTPUT_DIR" >&2
    exit 2
    ;;
esac
