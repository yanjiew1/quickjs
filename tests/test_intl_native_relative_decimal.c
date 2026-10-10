/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Authored source fixtures; execution belongs to the integrating root. */
#include "intl/number-decimal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Heap { size_t calls, fail_at, live; } Heap;
typedef QJSIntlStatus (*Convert)(const QJSIntlAllocator *, double,
                                QJSIntlNumberDecimal *);
static void *allocate(void *opaque, size_t n)
{
    Heap *h = opaque;
    void *p;
    if (h->calls++ == h->fail_at) return NULL;
    p = malloc(n);
    if (p) h->live++;
    return p;
}
static void release(void *opaque, void *p)
{
    Heap *h = opaque;
    if (p) { assert(h->live); h->live--; free(p); }
}
static double from_bits(uint64_t bits)
{
    double number;
    assert(sizeof(number) == sizeof(bits));
    memcpy(&number, &bits, sizeof(number));
    return number;
}
static void empty(const QJSIntlNumberDecimal *r)
{
    assert(!r->text && !r->decimal.digits && !r->decimal.length);
    assert(!r->decimal.exponent && !r->decimal.negative);
    assert(!r->value.decimal.data && !r->value.decimal.length);
    assert(r->value.kind == QJS_INTL_FINITE && !r->number_negative);
}
static void text_is(const QJSIntlNumberDecimal *r, const char *expected)
{
    assert(r->text && !strcmp(r->text, expected));
    assert(r->value.decimal.data == r->text);
    assert(r->value.decimal.length == strlen(expected));
    assert(r->decimal.digits && r->decimal.digits[r->decimal.length] == '\0');
}
static void check(const QJSIntlAllocator *a, Convert convert, uint64_t bits,
                  const char *expected, QJSIntlMathematicalKind kind)
{
    QJSIntlNumberDecimal r = {0};
    assert(convert(a, from_bits(bits), &r) == QJS_INTL_OK);
    text_is(&r, expected);
    assert(r.value.kind == kind && r.number_negative == (bits >> 63));
    assert(r.decimal.negative == (expected[0] == '-'));
    qjs_intl_number_decimal_clear(a, &r);
    empty(&r);
    qjs_intl_number_decimal_clear(a, &r); /* repeat clear is harmless */
}
static void distinct_rounding(const QJSIntlAllocator *a)
{
    QJSIntlNumberDecimal shortest = {0}, real = {0};
    QJSIntlDecimalResult s = {0}, r = {0};
    double number = from_bits(UINT64_C(0x4005666666666666)); /* 2.675 */
    assert(qjs_intl_number_to_intl_decimal(a, number, &shortest) == QJS_INTL_OK);
    assert(qjs_intl_number_to_real_decimal(a, number, &real) == QJS_INTL_OK);
    text_is(&shortest, "2.675");
    text_is(&real, "267499999999999982236431605997495353221893310546875e-50");
    assert(qjs_intl_decimal_to_raw_fixed(a, &shortest.decimal, 2, 2, 1,
        QJS_INTL_ROUND_HALF_EXPAND, 16, &s) == QJS_INTL_OK);
    assert(qjs_intl_decimal_to_raw_fixed(a, &real.decimal, 2, 2, 1,
        QJS_INTL_ROUND_HALF_EXPAND, 16, &r) == QJS_INTL_OK);
    assert(!strcmp(s.text, "2.68") && !strcmp(r.text, "2.67"));
    qjs_intl_decimal_result_clear(a, &s);
    qjs_intl_decimal_result_clear(a, &r);
    qjs_intl_number_decimal_clear(a, &shortest);
    qjs_intl_number_decimal_clear(a, &real);
}
static void extremes(const QJSIntlAllocator *a)
{
    static const char maximum[] =
        "179769313486231570814527423731704356798070567525844996598917476803"
        "157260780028538760589558632766878171540458953514382464234321326889"
        "464182768467546703537516986049910576551282076245490090389328944075"
        "868508455133942304583236903222948165808559332123348274797826204144"
        "723168738177180919299881250404026184124858368";
    unsigned int sign, step;
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(0x7fefffffffffffff),
          "1.7976931348623157e+308", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_real_decimal, UINT64_C(0x7fefffffffffffff),
          maximum, QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(1),
          "5e-324", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(0x8000000000000001),
          "-5e-324", QJS_INTL_FINITE);
    /* The exact smallest subnormal is 5^1074 * 10^-1074. Verify the
     * coefficient by division, independent of the converter's multiply. */
    for (sign = 0; sign < 2; sign++) {
        uint64_t bits = UINT64_C(1) | ((uint64_t)sign << 63);
        QJSIntlNumberDecimal r = {0};
        char quotient[800];
        size_t length, begin = 0, i;
        assert(qjs_intl_number_to_real_decimal(a, from_bits(bits), &r) == QJS_INTL_OK);
        assert(r.value.kind == QJS_INTL_FINITE && r.number_negative == sign);
        assert(r.decimal.negative == sign && r.decimal.exponent == -1074);
        assert(r.decimal.length == 751);
        assert(!memcmp(r.decimal.digits, "494065645841246544", 18));
        length = r.decimal.length;
        memcpy(quotient, r.decimal.digits, length);
        for (step = 0; step < 1074; step++) {
            unsigned int remainder = 0;
            for (i = begin; i < length; i++) {
                unsigned int current = remainder * 10 + (unsigned int)(quotient[i] - '0');
                quotient[i] = (char)('0' + current / 5);
                remainder = current % 5;
            }
            assert(!remainder);
            while (begin + 1 < length && quotient[begin] == '0') begin++;
        }
        assert(begin + 1 == length && quotient[begin] == '1');
        assert(r.text[0] == (sign ? '-' : '4'));
        assert(r.value.decimal.length == 757 + sign);
        assert(!strcmp(r.text + r.value.decimal.length - 6, "e-1074"));
        qjs_intl_number_decimal_clear(a, &r);
    }
}
static void special_values(const QJSIntlAllocator *a)
{
    static const uint64_t nonfinite[] = {
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000000001), UINT64_C(0xfff8000000000001)
    };
    static const QJSIntlMathematicalKind kinds[] = {
        QJS_INTL_POSITIVE_INFINITY, QJS_INTL_NEGATIVE_INFINITY,
        QJS_INTL_NAN, QJS_INTL_NAN
    };
    size_t i;
    check(a, qjs_intl_number_to_intl_decimal, 0, "0", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(0x8000000000000000),
          "-0", QJS_INTL_NEGATIVE_ZERO);
    check(a, qjs_intl_number_to_real_decimal, 0, "0", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_real_decimal, UINT64_C(0x8000000000000000),
          "0", QJS_INTL_FINITE);
    for (i = 0; i < sizeof(nonfinite) / sizeof(*nonfinite); i++) {
        QJSIntlNumberDecimal r = {0};
        assert(qjs_intl_number_to_intl_decimal(a, from_bits(nonfinite[i]), &r) == QJS_INTL_OK);
        assert(r.value.kind == kinds[i] && r.number_negative == (nonfinite[i] >> 63));
        assert(!r.text && !r.decimal.digits && !r.value.decimal.data);
        qjs_intl_number_decimal_clear(a, &r);
        assert(qjs_intl_number_to_real_decimal(a, from_bits(nonfinite[i]), &r) ==
               QJS_INTL_INVALID_ARGUMENT);
        empty(&r);
    }
}
static void failures(void)
{
    Convert converters[] = {
        qjs_intl_number_to_intl_decimal, qjs_intl_number_to_real_decimal
    };
    size_t route, fail;
    for (route = 0; route < 2; route++) {
        for (fail = 0; fail < 3; fail++) {
            Heap h = {0, fail, 0};
            QJSIntlAllocator a = {&h, allocate, NULL, release};
            QJSIntlNumberDecimal r = {0};
            QJSIntlStatus status = converters[route](&a,
                from_bits(UINT64_C(0x4005666666666666)), &r);
            assert(status == (fail < 2 ? QJS_INTL_NO_MEMORY : QJS_INTL_OK));
            if (status != QJS_INTL_OK) empty(&r);
            qjs_intl_number_decimal_clear(&a, &r);
            assert(h.live == 0);
            assert(converters[route](NULL, 1.0, &r) == QJS_INTL_INVALID_ARGUMENT);
            empty(&r);
            assert(converters[route](&a, 1.0, NULL) == QJS_INTL_INVALID_ARGUMENT);
        }
    }
    /* Classification has no allocation dependency, even with OOM armed. */
    {
        Heap h = {0, 0, 0};
        QJSIntlAllocator a = {&h, allocate, NULL, release};
        QJSIntlNumberDecimal r = {0};
        assert(qjs_intl_number_to_intl_decimal(&a,
            from_bits(UINT64_C(0x7ff0000000000000)), &r) == QJS_INTL_OK);
        qjs_intl_number_decimal_clear(&a, &r);
        assert(h.calls == 0 && h.live == 0);
    }
}
int main(void)
{
    Heap h = {0, SIZE_MAX, 0};
    QJSIntlAllocator a = {&h, allocate, NULL, release};
    distinct_rounding(&a); extremes(&a); special_values(&a); failures();
    assert(h.live == 0);
    return 0;
}
