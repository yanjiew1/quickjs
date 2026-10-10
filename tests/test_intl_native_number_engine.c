/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Authored fixtures; no execution receipt in this source packet. */
#include "intl/number-native.h"
#include "intl/number-unit.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Heap { size_t calls, fail_at, live; } Heap;
static void *allocate(void *opaque, size_t length)
{
    Heap *h = opaque;
    void *p;
    if (h->calls++ == h->fail_at) return NULL;
    p = malloc(length); if (p) h->live++; return p;
}
static void release(void *opaque, void *p)
{
    Heap *h = opaque;
    if (p) { assert(h->live); h->live--; free(p); }
}
static QJSIntlBytes slice(const char *s)
{
    QJSIntlBytes value = { s, strlen(s) }; return value;
}
static QJSIntlNumberOptions options(void)
{
    QJSIntlNumberOptions o;
    memset(&o, 0, sizeof(o));
    o.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    o.maximum_output_length = o.digits.maximum_output_length = 4096;
    o.digits.minimum_integer_digits = 1;
    o.digits.maximum_fraction_digits = 2;
    o.digits.minimum_significant_digits = 1;
    o.digits.maximum_significant_digits = 21;
    o.digits.rounding_increment = 1;
    o.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    return o;
}
static QJSIntlNumberData data(void)
{
    static const char *const symbols[] = { ".", ",", "+", "-", "%", "E", "INF", "NaN", ".", "," };
    static const QJSIntlNumberScalarRange classes[] = {
        {0x20, 0x20, 0}, {0x24, 0x24, 0}, {0x30, 0x39, 6}, {0x41, 0x5a, 3}, {0x61, 0x7a, 3}
    };
    QJSIntlNumberData d;
    size_t i;
    memset(&d, 0, sizeof(d));
    for (i = 0; i < 10; i++) { d.digits[i] = (uint32_t)('0' + i); d.symbols[i] = slice(symbols[i]); }
    d.minimum_grouping_digits = 1;
    d.pattern.zero = slice("{number}"); d.pattern.negative = slice("{minusSign}{number}");
    d.pattern.positive = slice("{plusSign}{number}");
    d.pattern.primary_group = d.pattern.secondary_group = 3;
    d.currency_symbol = slice("$"); d.currency_narrow_symbol = slice("N");
    d.before_currency = d.after_currency = slice("\xc2\xa0");
    d.classes = classes; d.class_count = sizeof(classes) / sizeof(classes[0]);
    for (i = 0; i < 6; i++) {
        d.currency_names[i] = slice(i == QJS_INTL_PLURAL_ONE ? "dollar" : "dollars");
        d.currency_name_patterns[i] = slice("{number} {currency}");
        d.unit_patterns[i] = slice(i == QJS_INTL_PLURAL_ONE ? "{number} meter" : "{number} meters");
    }
    return d;
}
static void partition(const QJSIntlFormatted *f)
{
    size_t i, position = 0;
    assert(f->length && f->part_count);
    for (i = 0; i < f->part_count; i++) {
        assert(f->parts[i].start == position && f->parts[i].end > position);
        assert(f->parts[i].source == QJS_INTL_SOURCE_SINGLE);
        position = f->parts[i].end;
    }
    assert(position == f->length);
}
static void check(const QJSIntlNativeNumber *n, const char *value, const char *expected)
{
    QJSIntlMathematicalValue v = { QJS_INTL_FINITE, { NULL, 0 } };
    QJSIntlFormatted f = { 0 };
    size_t i;
    v.decimal = slice(value);
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    assert(f.length == strlen(expected));
    for (i = 0; i < f.length; i++) assert(f.text[i] == (unsigned char)expected[i]);
    partition(&f);
    qjs_intl_native_number_result_clear(n, &f);
}
static void basics(const QJSIntlAllocator *a)
{
    QJSIntlNumberOptions o = options();
    QJSIntlNumberData d = data();
    QJSIntlNativeNumber *n = NULL;
    QJSIntlFormatted f = { 0 };
    QJSIntlMathematicalValue v = { QJS_INTL_NAN, { NULL, 0 } };
    QJSIntlDecimal rounded;
    QJSIntlDecimalResult raw = { 0 };
    size_t i;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1234567.895", "1,234,567.9");
    check(n, "-0", "-0"); check(n, "-0.001", "-0");
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    assert(f.part_count == 1 && f.parts[0].type == QJS_INTL_PART_NAN);
    qjs_intl_native_number_result_clear(n, &f);
    assert(qjs_intl_native_number_format_range(n, &v, &v, &f) == QJS_INTL_INVALID_ARGUMENT);
    v.kind = QJS_INTL_POSITIVE_INFINITY;
    assert(qjs_intl_native_number_format_range(n, &v, &v, &f) == QJS_INTL_UNSUPPORTED);
    rounded.digits = (char *)"1234"; rounded.length = 4; rounded.exponent = -2; rounded.negative = 1;
    raw.rounded = rounded; raw.text = (char *)"12.3400"; raw.length = 7;
    assert(qjs_intl_native_number_format_rounded(n, &raw, &f) == QJS_INTL_OK);
    assert(f.length == 8 && f.text[0] == '-' && f.text[7] == '0');
    qjs_intl_native_number_result_clear(n, &f);
    qjs_intl_native_number_close(n);
    o.sign_display = QJS_INTL_NUMBER_SIGN_NEGATIVE;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "-0.001", "0"); check(n, "-1", "-1"); qjs_intl_native_number_close(n);
    o.sign_display = QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "0", "0"); check(n, "1", "+1"); qjs_intl_native_number_close(n);
    o.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "-0", "-0"); check(n, "0", "+0"); qjs_intl_native_number_close(n);
    o = options(); d.pattern.secondary_group = 2;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1234567", "12,34,567"); qjs_intl_native_number_close(n);
    o.grouping = QJS_INTL_NUMBER_GROUP_MIN2;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1234", "1234"); check(n, "12345", "12,345"); qjs_intl_native_number_close(n);
    o = options(); d = data(); o.notation = QJS_INTL_NUMBER_SCIENTIFIC; o.digits.maximum_fraction_digits = 1;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "999.95", "1E3"); check(n, "0.01234", "1.2E-2"); qjs_intl_native_number_close(n);
    o.notation = QJS_INTL_NUMBER_ENGINEERING;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "0.00099", "990E-6"); qjs_intl_native_number_close(n);
    o = options(); o.style = QJS_INTL_NUMBER_PERCENT;
    d.pattern.zero = slice("{number}{percentSign}");
    d.pattern.negative = slice("{minusSign}{number}{percentSign}");
    d.pattern.positive = slice("{plusSign}{number}{percentSign}");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "0.12345", "12.35%"); qjs_intl_native_number_close(n);
    o = options(); d = data(); for (i = 0; i < 10; i++) d.digits[i] = 0x1d7e2 + (uint32_t)i;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    v.kind = QJS_INTL_FINITE; v.decimal = slice("12.3");
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK && f.length == 7);
    assert(f.parts[0].type == QJS_INTL_PART_INTEGER && f.parts[0].end == 4);
    partition(&f); qjs_intl_native_number_result_clear(n, &f); qjs_intl_native_number_close(n);
    o.notation = QJS_INTL_NUMBER_COMPACT;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_UNSUPPORTED && !n);
    o = options(); d = data(); d.symbols[0] = slice("\xc0\xaf");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_DATA_ERROR && !n);
    d = data(); o.maximum_output_length = 3;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    v.decimal = slice("12345");
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OVERFLOW && !f.text && !f.parts);
    qjs_intl_native_number_close(n);
}
static void labels(const QJSIntlAllocator *a)
{
    static const QJSIntlPluralRule rules[] = {
        {QJS_INTL_PLURAL_ONE, {"i = 1 and v = 0", 15}}, {QJS_INTL_PLURAL_OTHER, {"", 0}}
    };
    QJSIntlPluralRulesData pd = { rules, 2, NULL, 0 };
    QJSIntlPluralOptions po;
    QJSIntlNativePlural *plural = NULL;
    QJSIntlNativeNumber *n = NULL;
    QJSIntlNumberOptions o = options();
    QJSIntlNumberData d = data();
    QJSIntlFormatted f = { 0 };
    QJSIntlMathematicalValue v = { QJS_INTL_FINITE, {"1", 1} };
    memset(&po, 0, sizeof(po)); po.digits = o.digits;
    assert(qjs_intl_native_plural_open(a, &po, &pd, &plural) == QJS_INTL_OK);
    d.cardinal = plural; o.style = QJS_INTL_NUMBER_UNIT; o.unit = slice("meter");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1", "1 meter"); check(n, "-2", "-2 meters");
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    assert(f.parts[2].type == QJS_INTL_PART_UNIT); qjs_intl_native_number_result_clear(n, &f);
    qjs_intl_native_number_close(n);
    o.unit = slice("meter-per-second");
    for (size_t unit_index = 0; unit_index < 6; unit_index++)
        d.unit_patterns[unit_index] = slice(unit_index == QJS_INTL_PLURAL_ONE ?
            "{number} meter per second" : "{number} meters per second");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1", "1 meter per second"); check(n, "-2", "-2 meters per second");
    qjs_intl_native_number_close(n);
    assert(qjs_intl_number_unit_validate(slice("mile-scandinavian-per-fluid-ounce")));
    assert(!qjs_intl_number_unit_validate(slice("meter-per-second-per-hour")));
    assert(!qjs_intl_number_unit_validate(slice("bogus-per-second")));
    o.unit = slice("meter-per-second-per-hour");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_INVALID_ARGUMENT && !n);
    o = options(); o.style = QJS_INTL_NUMBER_CURRENCY; o.currency = slice("USD");
    o.currency_display = QJS_INTL_CURRENCY_NAME;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1", "1 dollar"); check(n, "2", "2 dollars"); qjs_intl_native_number_close(n);
    o.digits.minimum_fraction_digits = o.digits.maximum_fraction_digits = 2;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1", "1.00 dollars"); qjs_intl_native_number_close(n);
    o.currency_display = QJS_INTL_CURRENCY_SYMBOL;
    d.pattern.zero = slice("{currency}{number}"); d.pattern.negative = slice("({currency}{number})");
    d.pattern.positive = slice("{plusSign}{currency}{number}");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "-1234.5", "($1,234.50)"); qjs_intl_native_number_close(n);
    o.currency_display = QJS_INTL_CURRENCY_CODE;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    assert(f.text[3] == 0xa0 && f.parts[0].type == QJS_INTL_PART_CURRENCY);
    partition(&f); qjs_intl_native_number_result_clear(n, &f); qjs_intl_native_number_close(n);
    qjs_intl_native_plural_close(plural);
}
static void allocation_failures(Heap *heap, const QJSIntlAllocator *a)
{
    QJSIntlNumberOptions o = options();
    QJSIntlNumberData d = data();
    QJSIntlNativeNumber *n = NULL;
    QJSIntlMathematicalValue v = { QJS_INTL_FINITE, {"1234567.895", 11} };
    QJSIntlFormatted f = { 0 };
    size_t i, calls, baseline;
    heap->calls = 0; heap->fail_at = SIZE_MAX;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    calls = heap->calls; qjs_intl_native_number_close(n); assert(!heap->live);
    for (i = 0; i < calls; i++) {
        heap->calls = 0; heap->fail_at = i;
        assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_NO_MEMORY && !n);
        assert(!heap->live);
    }
    heap->calls = 0; heap->fail_at = SIZE_MAX;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    baseline = heap->live; heap->calls = 0;
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    calls = heap->calls; qjs_intl_native_number_result_clear(n, &f);
    for (i = 0; i < calls; i++) {
        heap->calls = 0; heap->fail_at = i;
        assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_NO_MEMORY);
        assert(!f.text && !f.parts && !f.length && !f.part_count && heap->live == baseline);
    }
    heap->fail_at = SIZE_MAX; qjs_intl_native_number_close(n); assert(!heap->live);
}
static void compact(const QJSIntlAllocator *a)
{
    static const QJSIntlPluralRule rules[] = {
        {QJS_INTL_PLURAL_ONE, {"i = 1 and v = 0", 15}}, {QJS_INTL_PLURAL_OTHER, {"", 0}}
    };
    QJSIntlPluralRulesData pd = { rules, 2, NULL, 0 };
    QJSIntlPluralOptions po;
    QJSIntlNativePlural *plural = NULL;
    QJSIntlNumberOptions o = options();
    QJSIntlNumberData d = data();
    QJSIntlNumberCompact rows[4];
    QJSIntlNativeNumber *n = NULL;
    QJSIntlFormatted f = { 0 };
    QJSIntlMathematicalValue v = { QJS_INTL_FINITE, {"1200", 4} };
    size_t i, j, calls, baseline;
    Heap *heap = a->opaque;
    memset(&po, 0, sizeof(po)); po.digits = o.digits;
    assert(qjs_intl_native_plural_open(a, &po, &pd, &plural) == QJS_INTL_OK);
    memset(rows, 0, sizeof(rows));
    for (i = 0; i < 4; i++) {
        rows[i].magnitude = i == 3 ? 6 : (int32_t)i + 3;
        rows[i].exponent = i == 3 ? 6 : 3;
        for (j = 0; j < 6; j++) rows[i].patterns[j] = slice(i == 3 ? "{number}M" : "{number}K");
    }
    d.cardinal = plural; d.compact = rows; d.compact_count = 4;
    o.notation = QJS_INTL_NUMBER_COMPACT; o.grouping = QJS_INTL_NUMBER_GROUP_MIN2;
    o.digits.maximum_fraction_digits = 1;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "12", "12"); check(n, "1200", "1.2K");
    check(n, "999.95", "1K"); check(n, "999950", "1M");
    check(n, "-999950", "-1M"); check(n, "1e9", "1000M");
    check(n, "-0", "-0");
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    assert(f.parts[f.part_count - 1].type == QJS_INTL_PART_COMPACT);
    qjs_intl_native_number_result_clear(n, &f); qjs_intl_native_number_close(n);
    /* Visible zeros govern cardinal agreement; exact count="1" wins. */
    rows[0].patterns[QJS_INTL_PLURAL_ONE] = slice("{number} thousand");
    rows[0].patterns[QJS_INTL_PLURAL_OTHER] = slice("{number} thousands");
    rows[0].exact_one = slice("mille");
    o.digits.minimum_fraction_digits = o.digits.maximum_fraction_digits = 2;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1000", "mille"); check(n, "1200", "1.20 thousands");
    qjs_intl_native_number_close(n);
    rows[0].exact_one = slice("");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1000", "1.00 thousands"); qjs_intl_native_number_close(n);
    /* ComputeExponent previews the absolute value under signed rounding. */
    o.digits.minimum_fraction_digits = 0; o.digits.maximum_fraction_digits = 1;
    o.digits.rounding_mode = QJS_INTL_ROUND_CEIL;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "-999950", "-0.9M"); qjs_intl_native_number_close(n);
    rows[0].patterns[0] = slice("{currency}{number}K");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_DATA_ERROR && !n);
    rows[0].patterns[0] = slice("{number}K");
    baseline = heap->live; heap->calls = 0;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    calls = heap->calls; qjs_intl_native_number_close(n);
    for (i = 0; i < calls; i++) {
        heap->calls = 0; heap->fail_at = i;
        assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_NO_MEMORY && !n);
        assert(heap->live == baseline);
    }
    heap->fail_at = SIZE_MAX; heap->calls = 0;
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    baseline = heap->live; heap->calls = 0;
    assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_OK);
    calls = heap->calls; qjs_intl_native_number_result_clear(n, &f);
    for (i = 0; i < calls; i++) {
        heap->calls = 0; heap->fail_at = i;
        assert(qjs_intl_native_number_format(n, &v, &f) == QJS_INTL_NO_MEMORY);
        assert(!f.text && !f.parts && heap->live == baseline);
    }
    heap->fail_at = SIZE_MAX; qjs_intl_native_number_close(n);
    qjs_intl_native_plural_close(plural);
}
static void ranges(const QJSIntlAllocator *a)
{
    QJSIntlNumberOptions o = options();
    QJSIntlNumberData d = data();
    QJSIntlNativeNumber *n = NULL;
    QJSIntlMathematicalValue x = {QJS_INTL_FINITE, {"1.234", 5}};
    QJSIntlMathematicalValue y = {QJS_INTL_FINITE, {"1.233", 5}};
    QJSIntlFormatted f = {0};
    size_t i, calls, baseline;
    Heap *heap = a->opaque;
    d.approximately = slice("~{0}"); d.range = slice("{0}..{1}");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 5 && f.text[0] == '~' && f.text[4] == '3');
    assert(f.parts[0].type == QJS_INTL_PART_APPROXIMATELY_SIGN);
    for (i = 0; i < f.part_count; i++) assert(f.parts[i].source == QJS_INTL_SOURCE_SHARED);
    qjs_intl_native_number_result_clear(n, &f);
    x.decimal = slice("3"); y.decimal = slice("1");
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 4 && f.part_count == 3 && f.text[0] == '3' && f.text[3] == '1');
    assert(f.parts[0].source == QJS_INTL_SOURCE_START_RANGE);
    assert(f.parts[1].source == QJS_INTL_SOURCE_SHARED && f.parts[1].type == QJS_INTL_PART_LITERAL);
    assert(f.parts[2].source == QJS_INTL_SOURCE_END_RANGE);
    qjs_intl_native_number_result_clear(n, &f);
    x.kind = QJS_INTL_NEGATIVE_ZERO; y.decimal = slice("0");
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 5 && f.text[0] == '-' && f.text[4] == '0');
    qjs_intl_native_number_result_clear(n, &f);
    x.kind = y.kind = QJS_INTL_POSITIVE_INFINITY;
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 4 && f.text[0] == '~');
    for (i = 0; i < f.part_count; i++) assert(f.parts[i].source == QJS_INTL_SOURCE_SHARED);
    qjs_intl_native_number_result_clear(n, &f);
    x.kind = QJS_INTL_NAN;
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_INVALID_ARGUMENT && !f.text);
    qjs_intl_native_number_close(n);
    o.sign_display = QJS_INTL_NUMBER_SIGN_NEVER;
    d.approximately = slice("{0} ~");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    x.kind = QJS_INTL_NEGATIVE_ZERO; y.kind = QJS_INTL_FINITE; y.decimal = slice("0");
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 3 && f.text[2] == '~' && f.parts[1].type == QJS_INTL_PART_LITERAL);
    qjs_intl_native_number_result_clear(n, &f); qjs_intl_native_number_close(n);
    d.range = slice("{1}..{0}");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    x.kind = y.kind = QJS_INTL_FINITE; x.decimal = slice("1"); y.decimal = slice("3");
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 4 && f.text[0] == '3' && f.text[3] == '1');
    assert(f.parts[0].source == QJS_INTL_SOURCE_END_RANGE && f.parts[2].source == QJS_INTL_SOURCE_START_RANGE);
    qjs_intl_native_number_result_clear(n, &f);
    /* Identity compares rendered text, including signDisplay suppression. */
    x.decimal = slice("-1"); y.decimal = slice("1");
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    assert(f.length == 3 && f.text[0] == '1' && f.text[2] == '~');
    for (i = 0; i < f.part_count; i++) assert(f.parts[i].source == QJS_INTL_SOURCE_SHARED);
    qjs_intl_native_number_result_clear(n, &f); qjs_intl_native_number_close(n);
    d.range = slice("{0}{1}");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_DATA_ERROR && !n);
    d.range = slice("{0}..{1}");
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    baseline = heap->live; heap->calls = 0; x.decimal = slice("2");
    assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_OK);
    calls = heap->calls; qjs_intl_native_number_result_clear(n, &f);
    for (i = 0; i < calls; i++) {
        heap->calls = 0; heap->fail_at = i;
        assert(qjs_intl_native_number_format_range(n, &x, &y, &f) == QJS_INTL_NO_MEMORY);
        assert(!f.text && !f.parts && heap->live == baseline);
    }
    heap->fail_at = SIZE_MAX; qjs_intl_native_number_close(n);
}
static void unit_composition(Heap *heap, const QJSIntlAllocator *a)
{
    QJSIntlBytes numerator[6], out[6];
    QJSIntlNumberData d = data();
    QJSIntlNumberOptions o = options();
    QJSIntlPluralRulesData pd;
    QJSIntlPluralOptions po;
    QJSIntlNativePlural *plural = NULL;
    QJSIntlNativeNumber *n = NULL;
    static const QJSIntlPluralRule rules[] = {
        {QJS_INTL_PLURAL_ONE, {"i = 1 and v = 0", 15}}, {QJS_INTL_PLURAL_OTHER, {"", 0}}
    };
    size_t i, baseline;
    for (i = 0; i < 6; i++) numerator[i] = slice(i == QJS_INTL_PLURAL_ONE ? "meter" : "{number} meters");
    assert(qjs_intl_number_unit_compose(a, numerator, slice("{number} per second"), slice("second"),
        slice("{numerator} per {denominator}"), 256, out) == QJS_INTL_OK);
    assert(out[QJS_INTL_PLURAL_ONE].length == strlen("meter per second"));
    qjs_intl_number_unit_composed_clear(a, out);
    assert(qjs_intl_number_unit_compose(a, numerator, slice(""), slice("second"),
        slice("{denominator}/{numerator}"), 256, out) == QJS_INTL_OK);
    assert(out[QJS_INTL_PLURAL_ONE].length == strlen("second/meter"));
    qjs_intl_number_unit_composed_clear(a, out);
    baseline = heap->live;
    for (i = 0; i < 6; i++) {
        heap->calls = 0; heap->fail_at = i;
        assert(qjs_intl_number_unit_compose(a, numerator, slice("{number}/s"), slice("second"),
            slice("{numerator}/{denominator}"), 256, out) == QJS_INTL_NO_MEMORY);
        assert(heap->live == baseline);
        for (size_t j = 0; j < 6; j++) assert(!out[j].data && !out[j].length);
    }
    heap->fail_at = SIZE_MAX;
    assert(qjs_intl_number_unit_compose(a, numerator, slice("{number}/s"), slice("second"),
        slice("{numerator}/{denominator}"), 2, out) == QJS_INTL_OVERFLOW);
    memset(&po, 0, sizeof(po)); po.digits = o.digits;
    pd.rules = rules; pd.rule_count = 2; pd.ranges = NULL; pd.range_count = 0;
    assert(qjs_intl_native_plural_open(a, &po, &pd, &plural) == QJS_INTL_OK);
    d.cardinal = plural; o.style = QJS_INTL_NUMBER_UNIT; o.unit = slice("meter");
    for (i = 0; i < 6; i++) d.unit_patterns[i] = numerator[i];
    assert(qjs_intl_native_number_open(a, &o, &d, &n) == QJS_INTL_OK);
    check(n, "1", "meter"); check(n, "-1", "-meter"); check(n, "2", "2 meters");
    qjs_intl_native_number_close(n); qjs_intl_native_plural_close(plural);
}
int main(void)
{
    Heap heap = {0, SIZE_MAX, 0};
    QJSIntlAllocator a = {&heap, allocate, NULL, release};
    basics(&a); labels(&a); compact(&a); ranges(&a); unit_composition(&heap, &a);
    assert(!heap.live); allocation_failures(&heap, &a);
    return 0;
}
