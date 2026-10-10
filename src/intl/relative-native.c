/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Original ECMA402 PartitionRelativeTimePattern / MakePartsList library.
 * Revision 7ae78cfdf8255468ffc8ebda33dafaea952808dd, review 2026-10-09.
 */
#include "relative-native.h"
#include <string.h>

typedef struct RelativePattern {
    uint16_t *text;
    size_t length, prefix, suffix;
} RelativePattern;
typedef struct RelativeLiteral {
    QJSIntlRelativeUnit unit;
    int32_t offset;
    RelativePattern value;
} RelativeLiteral;
struct QJSIntlNativeRelative {
    QJSIntlAllocator allocator;
    QJSIntlRelativeOptions options;
    QJSIntlRelativeNumberBridge number;
    QJSIntlNativePlural *plural;
    RelativePattern patterns[8][2][6];
    RelativeLiteral *literals;
    size_t literal_count;
};
static const char *const unit_names[] = {
    "second", "minute", "hour", "day", "week", "month", "quarter", "year"
};
static int valid_allocator(const QJSIntlAllocator *a)
{
    return a && a->malloc && a->free;
}
static int add_size(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) return 0;
    *out = a + b;
    return 1;
}
const char *qjs_intl_native_relative_unit_name(QJSIntlRelativeUnit unit)
{
    return (unsigned int)unit < 8 ? unit_names[unit] : NULL;
}
QJSIntlStatus qjs_intl_native_relative_unit(QJSIntlBytes value,
                                          QJSIntlRelativeUnit *out)
{
    unsigned int i;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = QJS_INTL_RELATIVE_SECOND;
    if (!value.data) return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < 8; i++) {
        size_t length = strlen(unit_names[i]);
        if ((value.length == length ||
             (value.length == length + 1 && value.data[length] == 's')) &&
            !memcmp(value.data, unit_names[i], length)) {
            *out = (QJSIntlRelativeUnit)i;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_INVALID_ARGUMENT;
}
void qjs_intl_native_relative_result_clear(const QJSIntlAllocator *a,
                                          QJSIntlFormatted *out)
{
    if (!out) return;
    if (valid_allocator(a)) {
        if (out->text) a->free(a->opaque, out->text);
        if (out->parts) a->free(a->opaque, out->parts);
    }
    memset(out, 0, sizeof(*out));
}
/* Strict scalar UTF8, decoded without locale functions. */
static QJSIntlStatus decode(const QJSIntlAllocator *a, QJSIntlBytes in,
                            int pattern, RelativePattern *out)
{
    size_t cursor = 0, length = 0, token = SIZE_MAX;
    if (!in.data || !in.length) return QJS_INTL_DATA_ERROR;
    if (in.length > SIZE_MAX / sizeof(uint16_t)) return QJS_INTL_OVERFLOW;
    out->text = a->malloc(a->opaque, in.length * sizeof(uint16_t));
    if (!out->text) return QJS_INTL_NO_MEMORY;
    while (cursor < in.length) {
        const unsigned char *p = (const unsigned char *)in.data;
        uint32_t c = p[cursor++], minimum = 0;
        size_t extra = 0;
        if (c < 0x80) { }
        else if (c >= 0xc2 && c <= 0xdf) { c &= 31; minimum = 0x80; extra = 1; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; minimum = 0x800; extra = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; minimum = 0x10000; extra = 3; }
        else return QJS_INTL_DATA_ERROR;
        if (extra > in.length - cursor) return QJS_INTL_DATA_ERROR;
        while (extra--) {
            if ((p[cursor] & 0xc0) != 0x80) return QJS_INTL_DATA_ERROR;
            c = (c << 6) | (p[cursor++] & 63);
        }
        if (!c || c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return QJS_INTL_DATA_ERROR;
        if (c >= 0x10000) {
            c -= 0x10000;
            out->text[length++] = (uint16_t)(0xd800 + (c >> 10));
            out->text[length++] = (uint16_t)(0xdc00 + (c & 1023));
        } else out->text[length++] = (uint16_t)c;
    }
    out->length = length;
    if (!pattern) return QJS_INTL_OK;
    for (cursor = 0; cursor < length; cursor++) {
        if (out->text[cursor] == '{') {
            if (token != SIZE_MAX || length - cursor < 3 ||
                out->text[cursor + 1] != '0' || out->text[cursor + 2] != '}')
                return QJS_INTL_DATA_ERROR;
            token = cursor;
            cursor += 2;
        } else if (out->text[cursor] == '}') return QJS_INTL_DATA_ERROR;
    }
    /* CLDR implicit-number forms (Arabic dual, etc.) have no placeholder.
     * MakePartsList then emits only the complete literal pattern. */
    if (token == SIZE_MAX) { out->prefix = SIZE_MAX; return QJS_INTL_OK; }
    out->prefix = token;
    out->suffix = token + 3;
    return QJS_INTL_OK;
}
void qjs_intl_native_relative_close(QJSIntlNativeRelative *rt)
{
    unsigned int u, t, c;
    size_t i;
    QJSIntlAllocator a;
    if (!rt) return;
    a = rt->allocator;
    for (u = 0; u < 8; u++) for (t = 0; t < 2; t++) for (c = 0; c < 6; c++)
        if (rt->patterns[u][t][c].text) a.free(a.opaque, rt->patterns[u][t][c].text);
    for (i = 0; i < rt->literal_count; i++)
        if (rt->literals[i].value.text) a.free(a.opaque, rt->literals[i].value.text);
    if (rt->literals) a.free(a.opaque, rt->literals);
    qjs_intl_native_plural_close(rt->plural);
    a.free(a.opaque, rt);
}
QJSIntlStatus qjs_intl_native_relative_open(const QJSIntlAllocator *a,
    const QJSIntlRelativeOptions *options, const QJSIntlRelativeData *data,
    const QJSIntlPluralRulesData *rules, const QJSIntlRelativeNumberBridge *number,
    QJSIntlNativeRelative **out)
{
    QJSIntlNativeRelative *rt;
    QJSIntlPluralOptions plural_options;
    QJSIntlStatus status;
    unsigned int u, t, c;
    size_t i;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!valid_allocator(a) || !options || !data || !rules || !number ||
        !number->format_rounded || !number->clear ||
        (unsigned int)options->style > 2 || (unsigned int)options->numeric > 1 ||
        !options->maximum_output_length || (!data->literals && data->literal_count))
        return QJS_INTL_INVALID_ARGUMENT;
    if (data->literal_count > SIZE_MAX / sizeof(RelativeLiteral)) return QJS_INTL_OVERFLOW;
    rt = a->malloc(a->opaque, sizeof(*rt));
    if (!rt) return QJS_INTL_NO_MEMORY;
    memset(rt, 0, sizeof(*rt));
    rt->allocator = *a;
    rt->options = *options;
    rt->number = *number;
    if (data->literal_count) {
        rt->literals = a->malloc(a->opaque, data->literal_count * sizeof(*rt->literals));
        if (!rt->literals) { status = QJS_INTL_NO_MEMORY; goto fail; }
        memset(rt->literals, 0, data->literal_count * sizeof(*rt->literals));
        rt->literal_count = data->literal_count;
    }
    memset(&plural_options, 0, sizeof(plural_options));
    plural_options.type = QJS_INTL_PLURAL_CARDINAL;
    plural_options.digits.minimum_integer_digits = 1;
    plural_options.digits.maximum_fraction_digits = 3;
    plural_options.digits.minimum_significant_digits = 1;
    plural_options.digits.maximum_significant_digits = 21;
    plural_options.digits.rounding_increment = 1;
    plural_options.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    plural_options.digits.maximum_output_length = options->maximum_output_length;
    status = qjs_intl_native_plural_open(a, &plural_options, rules, &rt->plural);
    if (status != QJS_INTL_OK) goto fail;
    for (u = 0; u < 8; u++) for (t = 0; t < 2; t++) for (c = 0; c < 6; c++) {
        status = decode(a, data->patterns[u][t][c], 1, &rt->patterns[u][t][c]);
        if (status != QJS_INTL_OK) goto fail;
    }
    for (i = 0; i < data->literal_count; i++) {
        const QJSIntlRelativeLiteral *literal = &data->literals[i];
        if ((unsigned int)literal->unit >= 8 ||
            (i && (data->literals[i-1].unit > literal->unit ||
                   (data->literals[i-1].unit == literal->unit &&
                    data->literals[i-1].offset >= literal->offset)))) {
            status = QJS_INTL_DATA_ERROR; goto fail;
        }
        rt->literals[i].unit = literal->unit;
        rt->literals[i].offset = literal->offset;
        status = decode(a, literal->text, 0, &rt->literals[i].value);
        if (status != QJS_INTL_OK) goto fail;
    }
    *out = rt;
    return QJS_INTL_OK;
 fail:
    qjs_intl_native_relative_close(rt);
    return status;
}
QJSIntlStatus qjs_intl_native_relative_resolved_options(
    const QJSIntlNativeRelative *rt, QJSIntlRelativeOptions *out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!rt) return QJS_INTL_INVALID_ARGUMENT;
    *out = rt->options;
    return QJS_INTL_OK;
}
/* Compare mathematical value to a bounded CLDR integer key without rounding
 * or expanding exponent zeros. -0 and +0 map to key0 as Number ToString does. */
static int integer_offset(const QJSIntlDecimal *d, int32_t *out)
{
    size_t length = d->length, i;
    int64_t exponent = d->exponent, value = 0;
    if (length == 1 && d->digits[0] == '0') { *out = 0; return 1; }
    while (length > 1 && d->digits[length - 1] == '0') { length--; exponent++; }
    if (exponent < 0 || exponent > 10 || length > 10 || (int64_t)length + exponent > 10)
        return 0;
    for (i = 0; i < length; i++) value = value * 10 + d->digits[i] - '0';
    while (exponent--) value *= 10;
    if (d->negative) value = -value;
    if (value < INT32_MIN || value > INT32_MAX) return 0;
    *out = (int32_t)value;
    return 1;
}
static int number_valid(const QJSIntlFormatted *number)
{
    size_t i, cursor = 0;
    if (!number->length || !number->text || !number->parts || !number->part_count)
        return 0;
    for (i = 0; i < number->part_count; i++) {
        const QJSIntlPart *p = &number->parts[i];
        if (p->start != cursor || p->end <= p->start || p->end > number->length ||
            (p->type != QJS_INTL_PART_LITERAL && p->type != QJS_INTL_PART_INTEGER &&
             p->type != QJS_INTL_PART_FRACTION && p->type != QJS_INTL_PART_GROUP &&
             p->type != QJS_INTL_PART_DECIMAL &&
             p->type != QJS_INTL_PART_MINUS_SIGN)) return 0;
        cursor = p->end;
    }
    return cursor == number->length;
}
static QJSIntlStatus compose(const QJSIntlNativeRelative *rt,
    const RelativePattern *p, const QJSIntlFormatted *number,
    QJSIntlRelativeUnit unit, QJSIntlFormatted *out)
{
    const QJSIntlAllocator *a = &rt->allocator;
    size_t prefix = number ? p->prefix : p->length;
    size_t suffix = number ? p->length - p->suffix : 0;
    size_t length = prefix, count = prefix != 0, i, cursor = 0, index = 0;
    if (number && (!add_size(length, number->length, &length) ||
                   !add_size(count, number->part_count, &count))) return QJS_INTL_OVERFLOW;
    if (!add_size(length, suffix, &length) || !add_size(count, suffix != 0, &count) ||
        length > rt->options.maximum_output_length || length > SIZE_MAX / sizeof(uint16_t) ||
        count > SIZE_MAX / sizeof(QJSIntlPart)) return QJS_INTL_OVERFLOW;
    out->text = a->malloc(a->opaque, length * sizeof(*out->text));
    if (!out->text) return QJS_INTL_NO_MEMORY;
    out->parts = a->malloc(a->opaque, count * sizeof(*out->parts));
    if (!out->parts) return QJS_INTL_NO_MEMORY;
    memset(out->parts, 0, count * sizeof(*out->parts));
    if (prefix) {
        memcpy(out->text, p->text, prefix * sizeof(uint16_t));
        out->parts[index].end = prefix;
        index++;
        cursor = prefix;
    }
    if (number) {
        memcpy(out->text + cursor, number->text, number->length * sizeof(uint16_t));
        for (i = 0; i < number->part_count; i++) {
            QJSIntlPart *part = &out->parts[index++];
            part->start = cursor + number->parts[i].start;
            part->end = cursor + number->parts[i].end;
            part->type = number->parts[i].type;
            part->unit.data = unit_names[unit];
            part->unit.length = strlen(unit_names[unit]);
        }
        cursor += number->length;
    }
    if (suffix) {
        memcpy(out->text + cursor, p->text + p->suffix, suffix * sizeof(uint16_t));
        out->parts[index].start = cursor;
        out->parts[index].end = cursor + suffix;
    }
    out->length = length;
    out->part_count = count;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_relative_format(const QJSIntlNativeRelative *rt,
    const QJSIntlMathematicalValue *value, QJSIntlRelativeUnit unit,
    QJSIntlFormatted *out)
{
    QJSIntlDecimal d = {0};
    QJSIntlDecimalResult raw = {0};
    QJSIntlFormatted number = {0};
    QJSIntlPluralCategory category;
    JSIntlPluralOperands operands;
    QJSIntlStatus status;
    int32_t offset;
    unsigned int tense;
    size_t i;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!rt || !value || (unsigned int)unit >= 8 ||
        (value->kind != QJS_INTL_FINITE && value->kind != QJS_INTL_NEGATIVE_ZERO))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_decimal_from_value(&rt->allocator, value, &d);
    if (status != QJS_INTL_OK) goto done;
    if (rt->options.numeric == QJS_INTL_RELATIVE_AUTO && integer_offset(&d, &offset)) {
        for (i = 0; i < rt->literal_count; i++)
            if (rt->literals[i].unit == unit && rt->literals[i].offset == offset) {
                status = compose(rt, &rt->literals[i].value, NULL, unit, out);
                goto done;
            }
    }
    tense = d.negative ? 0 : 1;
    /* The pinned algorithm uses original -0 for past, then passes R(value)
     * to NumberFormat. R(-0) is mathematical0. Keep negative nonzero signs,
     * including when their magnitude subsequently rounds to zero. */
    if (d.length == 1 && d.digits[0] == '0') d.negative = 0;
    status = qjs_intl_decimal_to_raw_fixed(&rt->allocator, &d, 0, 3, 1,
        QJS_INTL_ROUND_HALF_EXPAND, rt->options.maximum_output_length, &raw);
    if (status != QJS_INTL_OK) goto done;
    /* Pinned ECMA402 supplies the signed Number to its default NumberFormat;
     * cardinal PluralRules has matching default fraction digits and uses
     * unsigned raw.text operands, preserving visible fractional zeros. */
    status = rt->number.format_rounded(rt->number.opaque, &raw, &number);
    if (status != QJS_INTL_OK) goto done;
    if (!number_valid(&number)) { status = QJS_INTL_DATA_ERROR; goto done; }
    operands.decimal = raw.text;
    operands.length = raw.length;
    operands.exponent = 0;
    status = qjs_intl_native_plural_select_operands(rt->plural, &operands, &category);
    if (status != QJS_INTL_OK) goto done;
    status = compose(rt, &rt->patterns[unit][tense][category],
        rt->patterns[unit][tense][category].prefix == SIZE_MAX ? NULL : &number, unit, out);
 done:
    rt->number.clear(rt->number.opaque, &number);
    qjs_intl_decimal_result_clear(&rt->allocator, &raw);
    qjs_intl_decimal_clear(&rt->allocator, &d);
    if (status != QJS_INTL_OK) qjs_intl_native_relative_result_clear(&rt->allocator, out);
    return status;
}
