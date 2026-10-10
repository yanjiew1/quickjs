/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd: ToRawFixed,
 * ToRawPrecision, FormatNumericToString, GetUnsignedRoundingMode.
 */
#include "decimal.h"
#include <string.h>

static int allocator_valid(const QJSIntlAllocator *a)
{
    return a && a->malloc && a->free;
}
static int digit(char c) { return c >= '0' && c <= '9'; }
static int zero(const QJSIntlDecimal *d)
{
    return d->length == 1 && d->digits[0] == '0';
}
static int decimal_valid(const QJSIntlDecimal *d)
{
    size_t i;
    if (!d || !d->digits || !d->length || d->length > INT32_MAX ||
        d->negative > 1 || (d->length > 1 && d->digits[0] == '0') ||
        d->exponent < -QJS_INTL_DECIMAL_EXPONENT_LIMIT ||
        d->exponent > QJS_INTL_DECIMAL_EXPONENT_LIMIT) return 0;
    for (i = 0; i < d->length; i++) if (!digit(d->digits[i])) return 0;
    return 1;
}
void qjs_intl_decimal_clear(const QJSIntlAllocator *a, QJSIntlDecimal *d)
{
    if (!d) return;
    if (d->digits && allocator_valid(a)) a->free(a->opaque, d->digits);
    memset(d, 0, sizeof(*d));
}
void qjs_intl_decimal_result_clear(const QJSIntlAllocator *a, QJSIntlDecimalResult *r)
{
    if (!r) return;
    qjs_intl_decimal_clear(a, &r->rounded);
    if (r->text && allocator_valid(a)) a->free(a->opaque, r->text);
    memset(r, 0, sizeof(*r));
}
QJSIntlStatus qjs_intl_decimal_parse(const QJSIntlAllocator *a, QJSIntlBytes s,
                                    QJSIntlDecimal *out)
{
    size_t at = 0, begin, end, point = SIZE_MAX, digits = 0, first, n;
    int negative = 0, exp_negative = 0;
    int64_t exp = 0, fraction;
    char *p;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!allocator_valid(a) || !s.data || !s.length) return QJS_INTL_INVALID_ARGUMENT;
    if (s.length > INT32_MAX) return QJS_INTL_OVERFLOW;
    if (s.data[at] == '+' || s.data[at] == '-') negative = s.data[at++] == '-';
    begin = at;
    while (at < s.length) {
        if (digit(s.data[at])) { digits++; at++; }
        else if (s.data[at] == '.' && point == SIZE_MAX) point = at++;
        else break;
    }
    end = at;
    if (!digits) return QJS_INTL_INVALID_ARGUMENT;
    if (at < s.length && (s.data[at] == 'e' || s.data[at] == 'E')) {
        at++;
        if (at < s.length && (s.data[at] == '+' || s.data[at] == '-'))
            exp_negative = s.data[at++] == '-';
        if (at == s.length || !digit(s.data[at])) return QJS_INTL_INVALID_ARGUMENT;
        while (at < s.length && digit(s.data[at])) {
            unsigned int d = (unsigned int)(s.data[at++] - '0');
            if (exp > (QJS_INTL_DECIMAL_EXPONENT_LIMIT - (int64_t)d) / 10)
                return QJS_INTL_OVERFLOW;
            exp = exp * 10 + d;
        }
    }
    if (at != s.length) return QJS_INTL_INVALID_ARGUMENT;
    if (exp_negative) exp = -exp;
    fraction = point == SIZE_MAX ? 0 : (int64_t)(end - point - 1);
    exp -= fraction;
    if (exp < -QJS_INTL_DECIMAL_EXPONENT_LIMIT ||
        exp > QJS_INTL_DECIMAL_EXPONENT_LIMIT) return QJS_INTL_OVERFLOW;
    first = begin;
    while (first < end && (s.data[first] == '0' || s.data[first] == '.')) first++;
    n = 0;
    for (at = first; at < end; at++) if (s.data[at] != '.') n++;
    if (!n) n = 1;
    p = a->malloc(a->opaque, n + 1);
    if (!p) return QJS_INTL_NO_MEMORY;
    if (first == end) { p[0] = '0'; exp = 0; }
    else {
        n = 0;
        for (at = first; at < end; at++) if (s.data[at] != '.') p[n++] = s.data[at];
    }
    p[n] = '\0';
    out->digits = p; out->length = n; out->exponent = (int32_t)exp;
    out->negative = (uint8_t)negative;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_decimal_from_value(const QJSIntlAllocator *a,
                               const QJSIntlMathematicalValue *value,
                               QJSIntlDecimal *out)
{
    QJSIntlBytes s = { "-0", 2 };
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!value) return QJS_INTL_INVALID_ARGUMENT;
    if (value->kind == QJS_INTL_NEGATIVE_ZERO) return qjs_intl_decimal_parse(a, s, out);
    if (value->kind != QJS_INTL_FINITE) return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_decimal_parse(a, value->decimal, out);
}
static int increment_valid(unsigned int increment)
{
    switch (increment) {
    case 1: case 2: case 5: case 10: case 20: case 25: case 50: case 100:
    case 200: case 250: case 500: case 1000: case 2000: case 2500: case 5000:
        return 1;
    default: return 0;
    }
}
QJSIntlStatus qjs_intl_decimal_options_validate(const QJSIntlDecimalOptions *o)
{
    int fixed, significant;
    if (!o || !o->maximum_output_length ||
        o->minimum_integer_digits < 1 || o->minimum_integer_digits > 21 ||
        (unsigned int)o->rounding_mode > QJS_INTL_ROUND_HALF_EVEN ||
        (unsigned int)o->rounding_type > QJS_INTL_ROUND_LESS_PRECISION ||
        (unsigned int)o->trailing_zero_display > QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER ||
        !increment_valid(o->rounding_increment)) return QJS_INTL_INVALID_ARGUMENT;
    fixed = o->rounding_type != QJS_INTL_ROUND_SIGNIFICANT;
    significant = o->rounding_type != QJS_INTL_ROUND_FRACTION;
    if (fixed && (o->minimum_fraction_digits > o->maximum_fraction_digits ||
                  o->maximum_fraction_digits > 100)) return QJS_INTL_INVALID_ARGUMENT;
    if (significant && (o->minimum_significant_digits < 1 ||
        o->minimum_significant_digits > o->maximum_significant_digits ||
        o->maximum_significant_digits > 21)) return QJS_INTL_INVALID_ARGUMENT;
    if (o->rounding_increment != 1 &&
        (o->rounding_type != QJS_INTL_ROUND_FRACTION ||
         o->minimum_fraction_digits != o->maximum_fraction_digits))
        return QJS_INTL_INVALID_ARGUMENT;
    return QJS_INTL_OK;
}

/* Fractional tail of x / 10^quantum compared with 0.5. Integer prefix length
 * may be negative: zeros are implicit and never expanded just to compare.
 */
static void tail(const QJSIntlDecimal *d, int64_t prefix,
                 int *nonzero, int *half_relation)
{
    size_t first, i;
    char leading;
    *nonzero = 0; *half_relation = -1;
    if (prefix >= (int64_t)d->length) return;
    first = prefix <= 0 ? 0 : (size_t)prefix;
    for (i = first; i < d->length; i++) if (d->digits[i] != '0') *nonzero = 1;
    leading = prefix < 0 ? '0' : d->digits[first];
    if (leading != '5') { *half_relation = leading < '5' ? -1 : 1; return; }
    *half_relation = 0;
    for (i = first + 1; i < d->length; i++)
        if (d->digits[i] != '0') { *half_relation = 1; break; }
}
static int round_up(QJSIntlRoundingMode mode, int negative, int half_relation,
                    int odd)
{
    switch (mode) {
    case QJS_INTL_ROUND_CEIL: return !negative;
    case QJS_INTL_ROUND_FLOOR: return negative;
    case QJS_INTL_ROUND_EXPAND: return 1;
    case QJS_INTL_ROUND_TRUNC: return 0;
    default: break;
    }
    if (half_relation) return half_relation > 0;
    switch (mode) {
    case QJS_INTL_ROUND_HALF_CEIL: return !negative;
    case QJS_INTL_ROUND_HALF_FLOOR: return negative;
    case QJS_INTL_ROUND_HALF_EXPAND: return 1;
    case QJS_INTL_ROUND_HALF_TRUNC: return 0;
    case QJS_INTL_ROUND_HALF_EVEN: return odd;
    default: return 0;
    }
}

/* Round to increment * 10^quantum. Long division uses a small integer
 * remainder, never converting the coefficient to a machine integer.
 * Capacity reserves enough for quotient carry and small multiplication.
 */
static QJSIntlStatus round_quantum(const QJSIntlAllocator *a, const QJSIntlDecimal *d,
    int32_t quantum, unsigned int increment, QJSIntlRoundingMode mode,
    size_t limit, QJSIntlDecimal *out)
{
    int64_t prefix = zero(d) ? 0 : (int64_t)d->length + d->exponent - quantum;
    size_t n = prefix > 0 ? (size_t)prefix : 1, i, first;
    unsigned int remainder = 0, carry;
    int nonzero, half, relation, up;
    char *p;
    if (n > limit || n > INT32_MAX || n > SIZE_MAX - 8) return QJS_INTL_OVERFLOW;
    p = a->malloc(a->opaque, n + 8);
    if (!p) return QJS_INTL_NO_MEMORY;
    if (prefix <= 0) p[0] = '0';
    else for (i = 0; i < n; i++) {
        unsigned int current = i < d->length ? (unsigned int)(d->digits[i] - '0') : 0;
        remainder = remainder * 10 + current;
        p[i] = (char)('0' + remainder / increment);
        remainder %= increment;
    }
    tail(d, zero(d) ? (int64_t)d->length : prefix, &nonzero, &half);
    relation = 2 * remainder < increment ? -1 : 2 * remainder > increment ? 1 : 0;
    if (!relation && nonzero) relation = 1;
    else if (2 * remainder + 1 == increment) relation = half;
    up = (remainder || nonzero) &&
        round_up(mode, d->negative, relation, (p[n - 1] - '0') & 1);
    if (up) {
        i = n;
        while (i && p[i - 1] == '9') p[--i] = '0';
        if (i) p[i - 1]++;
        else { memmove(p + 1, p, n); p[0] = '1'; n++; }
    }
    carry = 0;
    i = n;
    while (i) {
        unsigned int current = (unsigned int)(p[--i] - '0') * increment + carry;
        p[i] = (char)('0' + current % 10);
        carry = current / 10;
    }
    while (carry) {
        memmove(p + 1, p, n); p[0] = (char)('0' + carry % 10);
        carry /= 10; n++;
    }
    first = 0;
    while (first + 1 < n && p[first] == '0') first++;
    if (first) { n -= first; memmove(p, p + first, n); }
    if (n > limit || n > INT32_MAX) { a->free(a->opaque, p); return QJS_INTL_OVERFLOW; }
    p[n] = '\0';
    out->digits = p; out->length = n; out->exponent = quantum;
    out->negative = d->negative;
    return QJS_INTL_OK;
}

/* Render unsigned coefficient/exponent with a bounded amount of padding,
 * then trim only the permitted number of trailing fractional zeros.
 */
static QJSIntlStatus render(const QJSIntlAllocator *a, QJSIntlDecimalResult *r,
                            size_t cut, size_t limit)
{
    const QJSIntlDecimal *d = &r->rounded;
    int64_t point = (int64_t)d->length + d->exponent;
    size_t integer = point > 0 ? (size_t)point : 1;
    size_t fraction = d->exponent < 0 ? (size_t)(-(int64_t)d->exponent) : 0;
    size_t n, i, at = 0;
    char *p;
    if (integer > limit || fraction > limit - integer) return QJS_INTL_OVERFLOW;
    n = integer + fraction;
    if (fraction) {
        if (n == limit || n == SIZE_MAX) return QJS_INTL_OVERFLOW;
        n++;
    }
    if (n == SIZE_MAX) return QJS_INTL_OVERFLOW;
    p = a->malloc(a->opaque, n + 1);
    if (!p) return QJS_INTL_NO_MEMORY;
    for (i = 0; i < integer; i++)
        p[at++] = point <= 0 ? '0' : i < d->length ? d->digits[i] : '0';
    if (fraction) {
        p[at++] = '.';
        for (i = 0; i < fraction; i++) {
            int64_t source = point + (int64_t)i;
            p[at++] = source >= 0 && source < (int64_t)d->length ?
                d->digits[(size_t)source] : '0';
        }
        while (cut && at && p[at - 1] == '0') { at--; cut--; }
        if (at && p[at - 1] == '.') at--;
    }
    p[at] = '\0';
    r->text = p; r->length = at; r->integer_digits = integer;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_decimal_to_raw_fixed(const QJSIntlAllocator *a,
    const QJSIntlDecimal *d, unsigned int min, unsigned int max,
    unsigned int increment, QJSIntlRoundingMode mode, size_t limit,
    QJSIntlDecimalResult *out)
{
    QJSIntlStatus s;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!allocator_valid(a) || !decimal_valid(d) || min > max || max > 100 ||
        !increment_valid(increment) || (unsigned int)mode > QJS_INTL_ROUND_HALF_EVEN ||
        !limit) return QJS_INTL_INVALID_ARGUMENT;
    out->rounding_magnitude = -(int32_t)max;
    s = round_quantum(a, d, out->rounding_magnitude, increment, mode, limit, &out->rounded);
    if (s == QJS_INTL_OK) s = render(a, out, max - min, limit);
    if (s != QJS_INTL_OK) qjs_intl_decimal_result_clear(a, out);
    return s;
}
QJSIntlStatus qjs_intl_decimal_to_raw_precision(const QJSIntlAllocator *a,
    const QJSIntlDecimal *d, unsigned int min, unsigned int max,
    QJSIntlRoundingMode mode, size_t limit, QJSIntlDecimalResult *out)
{
    QJSIntlStatus s;
    int64_t quantum;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!allocator_valid(a) || !decimal_valid(d) || min < 1 || min > max || max > 21 ||
        (unsigned int)mode > QJS_INTL_ROUND_HALF_EVEN || !limit)
        return QJS_INTL_INVALID_ARGUMENT;
    quantum = zero(d) ? 1 - (int64_t)max :
        (int64_t)d->length + d->exponent - (int64_t)max;
    if (quantum < -QJS_INTL_DECIMAL_EXPONENT_LIMIT ||
        quantum > QJS_INTL_DECIMAL_EXPONENT_LIMIT) return QJS_INTL_OVERFLOW;
    s = round_quantum(a, d, (int32_t)quantum, 1, mode, limit, &out->rounded);
    if (s == QJS_INTL_OK && out->rounded.length > max) {
        /* Carry crosses a power of ten. The new exponent controls priority. */
        out->rounded.digits[--out->rounded.length] = '\0';
        out->rounded.exponent++;
        if (out->rounded.exponent > QJS_INTL_DECIMAL_EXPONENT_LIMIT) s = QJS_INTL_OVERFLOW;
    }
    out->rounding_magnitude = out->rounded.exponent;
    if (s == QJS_INTL_OK) s = render(a, out, max - min, limit);
    if (s != QJS_INTL_OK) qjs_intl_decimal_result_clear(a, out);
    return s;
}
QJSIntlStatus qjs_intl_decimal_format_numeric(const QJSIntlAllocator *a,
    const QJSIntlDecimal *d, const QJSIntlDecimalOptions *o, QJSIntlDecimalResult *out)
{
    QJSIntlDecimalResult fixed = {0}, significant = {0};
    QJSIntlStatus s;
    size_t point, pad, n;
    char *p;
    int take_fixed;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    s = qjs_intl_decimal_options_validate(o);
    if (s != QJS_INTL_OK || !allocator_valid(a) || !decimal_valid(d))
        return QJS_INTL_INVALID_ARGUMENT;
    if (o->rounding_type != QJS_INTL_ROUND_SIGNIFICANT) {
        s = qjs_intl_decimal_to_raw_fixed(a, d, o->minimum_fraction_digits,
            o->maximum_fraction_digits, o->rounding_increment, o->rounding_mode,
            o->maximum_output_length, &fixed);
        if (s != QJS_INTL_OK) goto fail;
    }
    if (o->rounding_type != QJS_INTL_ROUND_FRACTION) {
        s = qjs_intl_decimal_to_raw_precision(a, d, o->minimum_significant_digits,
            o->maximum_significant_digits, o->rounding_mode,
            o->maximum_output_length, &significant);
        if (s != QJS_INTL_OK) goto fail;
    }
    take_fixed = o->rounding_type == QJS_INTL_ROUND_FRACTION;
    if (o->rounding_type == QJS_INTL_ROUND_MORE_PRECISION)
        take_fixed = fixed.rounding_magnitude < significant.rounding_magnitude;
    if (o->rounding_type == QJS_INTL_ROUND_LESS_PRECISION)
        take_fixed = fixed.rounding_magnitude >= significant.rounding_magnitude;
    if (take_fixed) { *out = fixed; memset(&fixed, 0, sizeof(fixed)); }
    else { *out = significant; memset(&significant, 0, sizeof(significant)); }
    qjs_intl_decimal_result_clear(a, &fixed);
    qjs_intl_decimal_result_clear(a, &significant);
    if (o->trailing_zero_display == QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER) {
        point = 0;
        while (point < out->length && out->text[point] != '.') point++;
        n = point < out->length ? point + 1 : point;
        while (n < out->length && out->text[n] == '0') n++;
        if (n == out->length && point < out->length) {
            out->length = point; out->text[point] = '\0';
        }
    }
    if (out->integer_digits < o->minimum_integer_digits) {
        pad = o->minimum_integer_digits - out->integer_digits;
        if (pad > o->maximum_output_length - out->length ||
            out->length > SIZE_MAX - pad - 1) { s = QJS_INTL_OVERFLOW; goto fail; }
        n = pad + out->length;
        p = a->malloc(a->opaque, n + 1);
        if (!p) { s = QJS_INTL_NO_MEMORY; goto fail; }
        memset(p, '0', pad); memcpy(p + pad, out->text, out->length + 1);
        a->free(a->opaque, out->text); out->text = p; out->length = n;
    }
    return QJS_INTL_OK;
 fail:
    qjs_intl_decimal_result_clear(a, &fixed);
    qjs_intl_decimal_result_clear(a, &significant);
    qjs_intl_decimal_result_clear(a, out);
    return s;
}
