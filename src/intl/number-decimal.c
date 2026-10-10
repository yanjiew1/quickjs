/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-09 review:
 * ToIntlMathematicalValue, public PluralRules select/selectRange, and
 * PartitionRelativeTimePattern's direct mathematical Number consumers.
 */
#include "number-decimal.h"
#include "../dtoa/dtoa.h"
#include <float.h>
#include <limits.h>
#include <string.h>

/* A significand has at most 16 decimal digits. Multiplying it by 5^1074
 * produces at most 767 digits; positive binary exponents need <=309.
 * Reserve 800 coefficient digits, and 8 more for sign/exponent/NUL.
 */
#define COEFFICIENT_CAPACITY 800
#define TEXT_CAPACITY (COEFFICIENT_CAPACITY + 8)
#define SHORTEST_CAPACITY 32

static int allocator_valid(const QJSIntlAllocator *a)
{
    return a && a->malloc && a->free;
}

/* C does not guarantee IEEE storage or matching integer/double byte order.
 * Check the actual representation as well as its numeric parameters. No
 * pointer punning, union punning, mutable process state or floating rounding.
 */
static int binary64_supported(void)
{
#if FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && \
    DBL_MIN_EXP == -1021 && CHAR_BIT == 8
    uint64_t bits;
    double probe;
    if (sizeof(double) != sizeof(bits)) return 0;
    probe = 1.0; memcpy(&bits, &probe, sizeof(bits));
    if (bits != UINT64_C(0x3ff0000000000000)) return 0;
    probe = -0.0; memcpy(&bits, &probe, sizeof(bits));
    if (bits != UINT64_C(0x8000000000000000)) return 0;
    probe = DBL_MIN; memcpy(&bits, &probe, sizeof(bits));
    if (bits != UINT64_C(0x0010000000000000)) return 0;
    probe = DBL_MAX; memcpy(&bits, &probe, sizeof(bits));
    return bits == UINT64_C(0x7fefffffffffffff);
#else
    return 0;
#endif
}

void qjs_intl_number_decimal_clear(const QJSIntlAllocator *a,
                                    QJSIntlNumberDecimal *out)
{
    if (!out) return;
    qjs_intl_decimal_clear(a, &out->decimal);
    if (out->text && allocator_valid(a)) a->free(a->opaque, out->text);
    memset(out, 0, sizeof(*out));
}

static QJSIntlStatus prepare(const QJSIntlAllocator *a, double number,
                             QJSIntlNumberDecimal *out, uint64_t *bits)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!allocator_valid(a)) return QJS_INTL_INVALID_ARGUMENT;
    if (!binary64_supported()) return QJS_INTL_UNSUPPORTED;
    memcpy(bits, &number, sizeof(*bits));
    return QJS_INTL_OK;
}

/* Commit both owned representations only after the complete parse succeeds.
 * Nonfinite values bypass this helper and never allocate.
 */
static QJSIntlStatus finish(const QJSIntlAllocator *a, const char *text,
    size_t length, QJSIntlMathematicalKind kind, unsigned int negative,
    QJSIntlNumberDecimal *out)
{
    QJSIntlDecimal d;
    QJSIntlBytes s;
    QJSIntlStatus status;
    char *copy;
    if (length == SIZE_MAX) return QJS_INTL_OVERFLOW;
    copy = a->malloc(a->opaque, length + 1);
    if (!copy) return QJS_INTL_NO_MEMORY;
    memcpy(copy, text, length); copy[length] = '\0';
    s.data = copy; s.length = length;
    status = qjs_intl_decimal_parse(a, s, &d);
    if (status != QJS_INTL_OK) { a->free(a->opaque, copy); return status; }
    out->text = copy; out->decimal = d;
    out->value.kind = kind; out->value.decimal = s;
    out->number_negative = (uint8_t)negative;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_to_intl_decimal(const QJSIntlAllocator *a,
    double number, QJSIntlNumberDecimal *out)
{
    uint64_t bits, magnitude;
    unsigned int negative;
    int length;
    char text[SHORTEST_CAPACITY];
    JSDTOATempMem temporary;
    QJSIntlStatus status = prepare(a, number, out, &bits);
    if (status != QJS_INTL_OK) return status;
    negative = (unsigned int)(bits >> 63);
    magnitude = bits & UINT64_C(0x7fffffffffffffff);
    if ((magnitude >> 52) == 0x7ff) {
        out->number_negative = (uint8_t)negative;
        out->value.kind = (magnitude & UINT64_C(0x000fffffffffffff)) ?
            QJS_INTL_NAN : negative ? QJS_INTL_NEGATIVE_INFINITY :
            QJS_INTL_POSITIVE_INFINITY;
        return QJS_INTL_OK;
    }
    /* Free-format base10 requires at most 17 digits plus sign/dot/exponent;
     * js_dtoa_max_len bounds the string length. Reserve its terminator too.
     * The shortest text round-trips to the input finite nonzero Number, so
     * ToIntlMathematicalValue's RoundMVResult cannot overflow/underflow here.
     */
    if (js_dtoa_max_len(number, 10, 0,
        JS_DTOA_FORMAT_FREE | JS_DTOA_MINUS_ZERO) >= SHORTEST_CAPACITY)
        return QJS_INTL_OVERFLOW;
    length = js_dtoa(text, number, 10, 0,
        JS_DTOA_FORMAT_FREE | JS_DTOA_MINUS_ZERO, &temporary);
    if (length <= 0 || length >= SHORTEST_CAPACITY) return QJS_INTL_DATA_ERROR;
    return finish(a, text, (size_t)length,
        !magnitude && negative ? QJS_INTL_NEGATIVE_ZERO : QJS_INTL_FINITE,
        negative, out);
}

/* Decimal coefficient in little-endian base10. Each multiplier is 2 or 5,
 * so the largest temporary current value is 49 and carry is at most 4.
 */
static int multiply(unsigned char *digits, size_t *length, unsigned int factor)
{
    size_t i;
    unsigned int carry = 0;
    for (i = 0; i < *length; i++) {
        unsigned int current = digits[i] * factor + carry;
        digits[i] = (unsigned char)(current % 10);
        carry = current / 10;
    }
    while (carry) {
        if (*length == COEFFICIENT_CAPACITY) return 0;
        digits[(*length)++] = (unsigned char)(carry % 10);
        carry /= 10;
    }
    return 1;
}

QJSIntlStatus qjs_intl_number_to_real_decimal(const QJSIntlAllocator *a,
    double number, QJSIntlNumberDecimal *out)
{
    unsigned char digits[COEFFICIENT_CAPACITY];
    char text[TEXT_CAPACITY], exponent_digits[4];
    size_t length = 0, trailing = 0, at = 0, i, n = 0;
    uint64_t bits, significand, magnitude;
    unsigned int negative, field, exponent_magnitude;
    int binary_exponent, decimal_exponent = 0, steps;
    QJSIntlStatus status = prepare(a, number, out, &bits);
    if (status != QJS_INTL_OK) return status;
    negative = (unsigned int)(bits >> 63);
    magnitude = bits & UINT64_C(0x7fffffffffffffff);
    field = (unsigned int)(magnitude >> 52);
    if (field == 0x7ff) return QJS_INTL_INVALID_ARGUMENT;
    if (!magnitude) return finish(a, "0", 1, QJS_INTL_FINITE, negative, out);
    significand = magnitude & UINT64_C(0x000fffffffffffff);
    if (field) {
        significand |= UINT64_C(0x0010000000000000);
        binary_exponent = (int)field - 1023 - 52;
    } else binary_exponent = -1074;
    do {
        digits[length++] = (unsigned char)(significand % 10);
        significand /= 10;
    } while (significand);
    if (binary_exponent >= 0) {
        steps = binary_exponent;
        while (steps--) if (!multiply(digits, &length, 2)) return QJS_INTL_OVERFLOW;
    } else {
        decimal_exponent = binary_exponent;
        steps = -binary_exponent;
        /* m * 2^-k == (m * 5^k) * 10^-k, exactly. */
        while (steps--) if (!multiply(digits, &length, 5)) return QJS_INTL_OVERFLOW;
    }
    while (trailing + 1 < length && !digits[trailing]) trailing++;
    decimal_exponent += (int)trailing;
    if (negative) text[at++] = '-';
    for (i = length; i > trailing; i--) text[at++] = (char)('0' + digits[i - 1]);
    if (decimal_exponent) {
        text[at++] = 'e';
        if (decimal_exponent < 0) text[at++] = '-';
        exponent_magnitude = (unsigned int)(decimal_exponent < 0 ?
            -decimal_exponent : decimal_exponent);
        do {
            exponent_digits[n++] = (char)('0' + exponent_magnitude % 10);
            exponent_magnitude /= 10;
        } while (exponent_magnitude);
        while (n) text[at++] = exponent_digits[--n];
    }
    text[at] = '\0';
    return finish(a, text, at, QJS_INTL_FINITE, negative, out);
}
