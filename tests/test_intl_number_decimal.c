/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/common/number-decimal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Heap {
    size_t calls, fail_at, live;
} Heap;
typedef QJSIntlStatus (*Convert)(const QJSIntlAllocator *, double,
                                 QJSIntlNumberDecimal *);
static void *allocate(void *opaque, size_t n)
{
    Heap *h = opaque;
    void *p;
    if (h->calls++ == h->fail_at)
        return NULL;
    p = malloc(n);
    if (p)
        h->live++;
    return p;
}
static void release(void *opaque, void *p)
{
    Heap *h = opaque;
    if (p) {
        assert(h->live);
        h->live--;
        free(p);
    }
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
    assert(!r->text);
    assert(!r->value.decimal.data && !r->value.decimal.length);
    assert(r->value.kind == QJS_INTL_FINITE);
}
static void text_is(const QJSIntlNumberDecimal *r, const char *expected)
{
    assert(r->text && !strcmp(r->text, expected));
    assert(r->value.decimal.data == r->text);
    assert(r->value.decimal.length == strlen(expected));
}
static void check(const QJSIntlAllocator *a, Convert convert, uint64_t bits,
                  const char *expected, QJSIntlMathematicalKind kind)
{
    QJSIntlNumberDecimal r = {0};
    assert(convert(a, from_bits(bits), &r) == QJS_INTL_OK);
    text_is(&r, expected);
    assert(r.value.kind == kind);
    qjs_intl_number_decimal_clear(a, &r);
    empty(&r);
    qjs_intl_number_decimal_clear(a, &r); /* repeat clear is harmless */
}
static void distinct_rounding(const QJSIntlAllocator *a)
{
    QJSIntlNumberDecimal shortest = {0}, real = {0};
    QJSIntlDecimalResult s = {0}, r = {0};
    QJSIntlDecimal shortest_decimal = {0}, real_decimal = {0};
    double number = from_bits(UINT64_C(0x4005666666666666)); /* 2.675 */
    assert(qjs_intl_number_to_intl_decimal(a, number, &shortest) == QJS_INTL_OK);
    assert(qjs_intl_number_to_real_decimal(a, number, &real) == QJS_INTL_OK);
    text_is(&shortest, "2.675");
    text_is(&real, "267499999999999982236431605997495353221893310546875e-50");
    assert(qjs_intl_decimal_from_value(a, &shortest.value, &shortest_decimal) == QJS_INTL_OK);
    assert(qjs_intl_decimal_from_value(a, &real.value, &real_decimal) == QJS_INTL_OK);
    assert(qjs_intl_decimal_to_raw_fixed(a, &shortest_decimal, 2, 2, 1,
                                         QJS_INTL_ROUND_HALF_EXPAND, 16,
                                         &s) == QJS_INTL_OK);
    assert(qjs_intl_decimal_to_raw_fixed(a, &real_decimal, 2, 2, 1,
                                         QJS_INTL_ROUND_HALF_EXPAND, 16,
                                         &r) == QJS_INTL_OK);
    assert(!strcmp(s.text, "2.68") && !strcmp(r.text, "2.67"));
    qjs_intl_decimal_result_clear(a, &s);
    qjs_intl_decimal_result_clear(a, &r);
    qjs_intl_decimal_clear(a, &shortest_decimal);
    qjs_intl_decimal_clear(a, &real_decimal);
    qjs_intl_number_decimal_clear(a, &shortest);
    qjs_intl_number_decimal_clear(a, &real);
}
static void extremes(const QJSIntlAllocator *a)
{
    static const struct {
        uint64_t bits, significand;
        int exponent;
    } powers_of_five[] = {
        {UINT64_C(1), UINT64_C(1), -1074},
        {UINT64_C(2), UINT64_C(1), -1073},
        {UINT64_C(3), UINT64_C(3), -1074},
        {UINT64_C(4), UINT64_C(1), -1072},
        {UINT64_C(0x0008000000000000), UINT64_C(1), -1023},
        {UINT64_C(0x000fffffffffffff), UINT64_C(4503599627370495), -1074},
        {UINT64_C(0x0010000000000000), UINT64_C(1), -1022},
        {UINT64_C(0x0010000000000001), UINT64_C(4503599627370497), -1074}};
    static const char maximum[] =
        "179769313486231570814527423731704356798070567525844996598917476803"
        "157260780028538760589558632766878171540458953514382464234321326889"
        "464182768467546703537516986049910576551282076245490090389328944075"
        "868508455133942304583236903222948165808559332123348274797826204144"
        "723168738177180919299881250404026184124858368";
    unsigned int sign, step;
    size_t case_index;
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(0x7fefffffffffffff),
          "1.7976931348623157e+308", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_real_decimal, UINT64_C(0x7fefffffffffffff), maximum,
          QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(1), "5e-324", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(0x8000000000000001), "-5e-324",
          QJS_INTL_FINITE);
    /* Verify coefficients by division, independent of the converter's
     * multiply, including cancellation across the subnormal boundary. */
    for (sign = 0; sign < 2; sign++) {
        for (case_index = 0; case_index < sizeof(powers_of_five) / sizeof(*powers_of_five);
             case_index++) {
            uint64_t bits = powers_of_five[case_index].bits | ((uint64_t)sign << 63);
            uint64_t quotient_value = 0;
            QJSIntlNumberDecimal r = {0};
            QJSIntlDecimal decimal = {0};
            char quotient[800];
            size_t length, begin = 0, i;
            assert(qjs_intl_number_to_real_decimal(a, from_bits(bits), &r) == QJS_INTL_OK);
            assert(r.value.kind == QJS_INTL_FINITE);
            assert(qjs_intl_decimal_from_value(a, &r.value, &decimal) == QJS_INTL_OK);
            assert(decimal.negative == sign);
            assert(decimal.exponent == powers_of_five[case_index].exponent);
            length = decimal.length;
            assert(length <= sizeof(quotient));
            memcpy(quotient, decimal.digits, length);
            for (step = 0; step < (unsigned int)-decimal.exponent; step++) {
                unsigned int remainder = 0;
                for (i = begin; i < length; i++) {
                    unsigned int current =
                        remainder * 10 + (unsigned int)(quotient[i] - '0');
                    quotient[i] = (char)('0' + current / 5);
                    remainder = current % 5;
                }
                assert(!remainder);
                while (begin + 1 < length && quotient[begin] == '0')
                    begin++;
            }
            for (i = begin; i < length; i++) {
                assert(quotient_value <=
                       (UINT64_MAX - (unsigned int)(quotient[i] - '0')) / 10);
                quotient_value = quotient_value * 10 + (unsigned int)(quotient[i] - '0');
            }
            assert(quotient_value == powers_of_five[case_index].significand);
            if (!case_index) {
                assert(decimal.length == 751);
                assert(!memcmp(decimal.digits, "494065645841246544", 18));
                assert(r.text[0] == (sign ? '-' : '4'));
                assert(r.value.decimal.length == 757 + sign);
                assert(!strcmp(r.text + r.value.decimal.length - 6, "e-1074"));
            }
            qjs_intl_decimal_clear(a, &decimal);
            qjs_intl_number_decimal_clear(a, &r);
        }
    }
}
static void binary_factors(const QJSIntlAllocator *a)
{
    static const struct {
        uint64_t bits;
        const char *text;
    } cases[] = {
        {UINT64_C(0x3fdfffffffffffff),
         "499999999999999944488848768742172978818416595458984375e-54"},
        {UINT64_C(0x3fe0000000000000), "5e-1"},
        {UINT64_C(0x3fe0000000000001),
         "50000000000000011102230246251565404236316680908203125e-53"},
        {UINT64_C(0x3ff0000000000000), "1"},
        {UINT64_C(0x3ff0000000000001),
         "10000000000000002220446049250313080847263336181640625e-52"},
        {UINT64_C(0x3ff8000000000000), "15e-1"},
        {UINT64_C(0x4000000000000000), "2"},
        {UINT64_C(0x40f86a0000000000), "1e5"},
        /* Stop at exponent zero even though the remaining significand is even. */
        {UINT64_C(0x4320000000000000), "2251799813685248"},
        {UINT64_C(0x4320000000000001), "22517998136852485e-1"},
        {UINT64_C(0x4340000000000000), "9007199254740992"}};
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++)
        check(a, qjs_intl_number_to_real_decimal, cases[i].bits, cases[i].text,
              QJS_INTL_FINITE);
}
static void special_values(const QJSIntlAllocator *a)
{
    static const uint64_t nonfinite[] = {
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000000001), UINT64_C(0xfff8000000000001)};
    static const QJSIntlMathematicalKind kinds[] = {QJS_INTL_POSITIVE_INFINITY,
                                                    QJS_INTL_NEGATIVE_INFINITY,
                                                    QJS_INTL_NAN, QJS_INTL_NAN};
    size_t i;
    check(a, qjs_intl_number_to_real_decimal, UINT64_C(0x3ff0000000000000),
          "1", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_real_decimal, UINT64_C(0xbff8000000000000),
          "-15e-1", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, 0, "0", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_intl_decimal, UINT64_C(0x8000000000000000), "-0",
          QJS_INTL_NEGATIVE_ZERO);
    check(a, qjs_intl_number_to_real_decimal, 0, "0", QJS_INTL_FINITE);
    check(a, qjs_intl_number_to_real_decimal, UINT64_C(0x8000000000000000), "0",
          QJS_INTL_FINITE);
    for (i = 0; i < sizeof(nonfinite) / sizeof(*nonfinite); i++) {
        QJSIntlNumberDecimal r = {0};
        assert(qjs_intl_number_to_intl_decimal(a, from_bits(nonfinite[i]), &r) ==
               QJS_INTL_OK);
        assert(r.value.kind == kinds[i]);
        assert(!r.text && !r.value.decimal.data);
        qjs_intl_number_decimal_clear(a, &r);
        assert(qjs_intl_number_to_real_decimal(a, from_bits(nonfinite[i]), &r) ==
               QJS_INTL_INVALID_ARGUMENT);
        empty(&r);
    }
}
static void failures(void)
{
    Convert converters[] = {qjs_intl_number_to_intl_decimal,
                            qjs_intl_number_to_real_decimal};
    size_t route, fail;
    for (route = 0; route < 2; route++) {
        for (fail = 0; fail < 2; fail++) {
            Heap h = {0, fail, 0};
            QJSIntlAllocator a = {&h, allocate, NULL, release};
            QJSIntlNumberDecimal r = {0};
            QJSIntlStatus status =
                converters[route](&a, from_bits(UINT64_C(0x4005666666666666)), &r);
            assert(status == (fail == 0 ? QJS_INTL_NO_MEMORY : QJS_INTL_OK));
            assert(h.calls == 1);
            if (status != QJS_INTL_OK)
                empty(&r);
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
        assert(qjs_intl_number_to_intl_decimal(
                   &a, from_bits(UINT64_C(0x7ff0000000000000)), &r) == QJS_INTL_OK);
        qjs_intl_number_decimal_clear(&a, &r);
        assert(h.calls == 0 && h.live == 0);
    }
}
int main(void)
{
    Heap h = {0, SIZE_MAX, 0};
    QJSIntlAllocator a = {&h, allocate, NULL, release};
    distinct_rounding(&a);
    extremes(&a);
    binary_factors(&a);
    special_values(&a);
    failures();
    assert(h.live == 0);
    return 0;
}
