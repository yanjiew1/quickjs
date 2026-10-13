/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "duration-native.h"
#include <string.h>

static const char *const unit_names[10] = {
    "year", "month", "week", "day", "hour", "minute", "second",
    "millisecond", "microsecond", "nanosecond"
};
struct QJSIntlNativeDuration {
    QJSIntlAllocator a;
    QJSIntlDurationOptions options;
    QJSIntlDurationNumberFactory factory;
    QJSIntlNativeNumber *numbers[10][2]; /* first sign auto; following never */
    QJSIntlNativeList *list;
    QJSIntlUTF16 separators[2];
};
static int add_size(size_t a, size_t b, size_t *out)
{
    if (b > SIZE_MAX - a) return 0;
    *out = a + b; return 1;
}
static int numeric(QJSIntlDurationUnitStyle s)
{
    return s == QJS_INTL_DURATION_NUMERIC || s == QJS_INTL_DURATION_TWO_DIGIT;
}
static int zero(QJSIntlBytes s) { return s.length == 1 && s.data[0] == '0'; }
static QJSIntlBytes unit_name(size_t i)
{
    QJSIntlBytes s = { unit_names[i], strlen(unit_names[i]) }; return s;
}
void qjs_intl_native_duration_result_clear(const QJSIntlAllocator *a,
                                          QJSIntlFormatted *out)
{
    if (!out) return;
    if (a && a->free) {
        if (out->text) a->free(a->opaque, out->text);
        if (out->parts) a->free(a->opaque, out->parts);
    }
    memset(out, 0, sizeof(*out));
}
void qjs_intl_native_duration_close(QJSIntlNativeDuration *d)
{
    size_t i, j;
    QJSIntlAllocator a;
    if (!d) return;
    a = d->a;
    for (i = 0; i < 10; i++) for (j = 0; j < 2; j++)
        if (d->numbers[i][j]) d->factory.close(d->factory.close_opaque, d->numbers[i][j]);
    qjs_intl_native_list_close(d->list);
    for (i = 0; i < 2; i++) if (d->separators[i].data)
        a.free(a.opaque, (void *)d->separators[i].data);
    a.free(a.opaque, d);
}
static QJSIntlStatus options_validate(const QJSIntlDurationOptions *o)
{
    size_t i;
    QJSIntlDurationUnitStyle prev = QJS_INTL_DURATION_UNIT_LONG;
    if (!o || (unsigned)o->style > QJS_INTL_DURATION_DIGITAL ||
        o->fractional_digits < -1 || o->fractional_digits > 9 ||
        !o->maximum_output_length) return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < 10; i++) {
        QJSIntlDurationUnitStyle s = o->units[i].style;
        if ((unsigned)s > QJS_INTL_DURATION_FRACTIONAL || o->units[i].always > 1 ||
            (i < 4 && (unsigned)s > QJS_INTL_DURATION_UNIT_NARROW) ||
            (i >= 7 && numeric(s)) || (i < 7 && s == QJS_INTL_DURATION_FRACTIONAL) ||
            (s == QJS_INTL_DURATION_FRACTIONAL && o->units[i].always))
            return QJS_INTL_INVALID_ARGUMENT;
        if (i >= 5 && ((prev == QJS_INTL_DURATION_FRACTIONAL && s != prev) ||
            (numeric(prev) && !numeric(s) && s != QJS_INTL_DURATION_FRACTIONAL)))
            return QJS_INTL_INVALID_ARGUMENT;
        /* Resolved GetDurationUnitOptions propagates 2-digit to m/s. */
        if ((i == 5 || i == 6) && numeric(prev) && s != QJS_INTL_DURATION_TWO_DIGIT)
            return QJS_INTL_INVALID_ARGUMENT;
        prev = s;
    }
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_duration_open(const QJSIntlAllocator *a,
    const QJSIntlDurationOptions *o, const QJSIntlDurationData *data,
    QJSIntlNativeDuration **out)
{
    QJSIntlNativeDuration *d;
    QJSIntlStatus s;
    size_t i, j;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    s = options_validate(o);
    if (s != QJS_INTL_OK) return s;
    if (!a || !a->malloc || !a->free || !data || !data->number.open ||
        !data->number.close) return QJS_INTL_INVALID_ARGUMENT;
    d = a->malloc(a->opaque, sizeof(*d));
    if (!d) return QJS_INTL_NO_MEMORY;
    memset(d, 0, sizeof(*d));
    d->a = *a; d->options = *o; d->factory = data->number;
    if (data->open_list)
        s = data->open_list(data->list_open_opaque, a,
            o->style == QJS_INTL_DURATION_DIGITAL ? QJS_INTL_LIST_SHORT : (QJSIntlListStyle)o->style, &d->list);
    else s = qjs_intl_native_list_open(a, &data->list, &d->list);
    if (s != QJS_INTL_OK) goto fail;
    if (!d->list) { s = QJS_INTL_DATA_ERROR; goto fail; }
    for (i = 0; i < 2; i++) {
        QJSIntlUTF16 v = i ? data->minute_second_separator : data->hour_minute_separator;
        uint16_t *copy;
        if (!v.data || !v.length) { s = QJS_INTL_DATA_ERROR; goto fail; }
        if (v.length > o->maximum_output_length || v.length > SIZE_MAX / sizeof(*copy)) {
            s = QJS_INTL_OVERFLOW; goto fail;
        }
        copy = a->malloc(a->opaque, v.length * sizeof(*copy));
        if (!copy) { s = QJS_INTL_NO_MEMORY; goto fail; }
        memcpy(copy, v.data, v.length * sizeof(*copy));
        d->separators[i].data = copy; d->separators[i].length = v.length;
    }
    for (i = 0; i < 10; i++) {
        QJSIntlNumberOptions n;
        int combined = i >= 6 && i < 9 &&
            o->units[i+1].style == QJS_INTL_DURATION_FRACTIONAL;
        if (o->units[i].style == QJS_INTL_DURATION_FRACTIONAL) continue;
        memset(&n, 0, sizeof(n));
        n.notation = QJS_INTL_NUMBER_STANDARD;
        n.style = numeric(o->units[i].style) ? QJS_INTL_NUMBER_DECIMAL : QJS_INTL_NUMBER_UNIT;
        n.grouping = n.style == QJS_INTL_NUMBER_DECIMAL ? QJS_INTL_NUMBER_GROUP_OFF : QJS_INTL_NUMBER_GROUP_AUTO;
        if (n.style == QJS_INTL_NUMBER_UNIT) {
            n.unit = unit_name(i);
            n.unit_display = (QJSIntlUnitDisplay)o->units[i].style;
        }
        n.digits.minimum_integer_digits = o->units[i].style == QJS_INTL_DURATION_TWO_DIGIT ? 2 : 1;
        n.digits.minimum_significant_digits = 1; n.digits.maximum_significant_digits = 21;
        n.digits.rounding_increment = 1;
        n.digits.rounding_type = QJS_INTL_ROUND_FRACTION;
        n.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
        if (combined || (i == 6 && numeric(o->units[i].style))) {
            n.digits.minimum_fraction_digits = o->fractional_digits < 0 ? 0 : (uint8_t)o->fractional_digits;
            n.digits.maximum_fraction_digits = o->fractional_digits < 0 ? 9 : (uint8_t)o->fractional_digits;
            n.digits.rounding_mode = QJS_INTL_ROUND_TRUNC;
        } else n.digits.maximum_fraction_digits = 3; /* NF standard defaults */
        n.digits.maximum_output_length = o->maximum_output_length;
        n.maximum_output_length = o->maximum_output_length;
        for (j = 0; j < 2; j++) {
            n.sign_display = j ? QJS_INTL_NUMBER_SIGN_NEVER : QJS_INTL_NUMBER_SIGN_AUTO;
            s = d->factory.open(d->factory.open_opaque, a, &n, &d->numbers[i][j]);
            if (s != QJS_INTL_OK) goto fail;
            if (!d->numbers[i][j]) { s = QJS_INTL_DATA_ERROR; goto fail; }
        }
    }
    *out = d; return QJS_INTL_OK;
fail:
    qjs_intl_native_duration_close(d); return s;
}
QJSIntlStatus qjs_intl_native_duration_resolved_options(const QJSIntlNativeDuration *d,
                                                      QJSIntlDurationOptions *out)
{
    if (!d || !out) return QJS_INTL_INVALID_ARGUMENT;
    *out = d->options; return QJS_INTL_OK;
}
/* Duration-specific power-of-ten addition, not rounding. Each fractional
 * unit's INTEGER magnitude is shifted by 3 additional decimal places. This
 * also handles unbalanced values, e.g. 1001ms -> 1.001s, without binary float.
 * Shared NumberFormat alone rounds/truncates this exact mathematical value.
 */
static QJSIntlStatus exact_value(const QJSIntlNativeDuration *d,
    const QJSIntlDurationRecord *r, size_t first, size_t last, char **out,
    size_t *length, int *nonzero)
{
    size_t scale = 3 * (last-first), width = 1, n, i, j, begin, len;
    char *sum, *text;
    *out = NULL; *length = 0; *nonzero = 0;
    for (i = first; i <= last; i++) {
        if (!add_size(r->magnitudes[i].length, 3 * (last-i), &n) ||
            !add_size(n, 1, &n)) return QJS_INTL_OVERFLOW;
        if (n > width) width = n;
    }
    if (width > d->options.maximum_output_length &&
        width - d->options.maximum_output_length > 11) return QJS_INTL_OVERFLOW;
    sum = d->a.malloc(d->a.opaque, width);
    if (!sum) return QJS_INTL_NO_MEMORY;
    memset(sum, '0', width);
    for (i = first; i <= last; i++) {
        unsigned carry = 0;
        size_t pos = width - 3 * (last-i);
        QJSIntlBytes v = r->magnitudes[i];
        for (j = v.length; j > 0 || carry; ) {
            unsigned digit = j ? (unsigned)(v.data[--j] - '0') : 0;
            unsigned total = digit + (unsigned)(sum[--pos] - '0') + carry;
            sum[pos] = (char)('0' + total % 10); carry = total / 10;
        }
    }
    begin = 0;
    while (begin + 1 < width && sum[begin] == '0') begin++;
    *nonzero = !(begin + 1 == width && sum[begin] == '0');
    /* At least one integer digit before the decimal separator. */
    if (width - begin <= scale) begin = width - scale - 1;
    len = width - begin;
    if (!add_size(len, (scale ? 1 : 0) + (r->sign < 0 ? 1 : 0), &len) ||
        !add_size(len, 1, &n)) { d->a.free(d->a.opaque, sum); return QJS_INTL_OVERFLOW; }
    text = d->a.malloc(d->a.opaque, n);
    if (!text) { d->a.free(d->a.opaque, sum); return QJS_INTL_NO_MEMORY; }
    j = 0;
    if (r->sign < 0) text[j++] = '-';
    for (i = begin; i < width; i++) {
        if (scale && i == width - scale) text[j++] = '.';
        text[j++] = sum[i];
    }
    text[j] = 0;
    d->a.free(d->a.opaque, sum);
    *out = text; *length = len; return QJS_INTL_OK;
}
static QJSIntlStatus render(const QJSIntlNativeDuration *d, size_t unit,
    const char *text, size_t length, int sign_available, QJSIntlFormatted *out)
{
    QJSIntlMathematicalValue v;
    QJSIntlStatus s;
    size_t i;
    v.kind = QJS_INTL_FINITE; v.decimal.data = text; v.decimal.length = length;
    s = qjs_intl_native_number_format(d->numbers[unit][sign_available ? 0 : 1], &v, out);
    if (s == QJS_INTL_OK) for (i = 0; i < out->part_count; i++) out->parts[i].unit = unit_name(unit);
    return s;
}
/* Merge borrowed component views into one duration-owned result. */
static QJSIntlStatus merge(const QJSIntlNativeDuration *d,
    const QJSIntlFormatted *const *views, size_t count, QJSIntlFormatted *out)
{
    size_t i, j, text_len = 0, parts = 0, cursor = 0, index = 0;
    memset(out, 0, sizeof(*out));
    for (i = 0; i < count; i++) {
        if (!add_size(text_len, views[i]->length, &text_len) ||
            !add_size(parts, views[i]->part_count, &parts)) return QJS_INTL_OVERFLOW;
    }
    if (text_len > d->options.maximum_output_length || text_len > SIZE_MAX / sizeof(*out->text) ||
        parts > SIZE_MAX / sizeof(*out->parts)) return QJS_INTL_OVERFLOW;
    if (text_len) {
        out->text = d->a.malloc(d->a.opaque, text_len * sizeof(*out->text));
        if (!out->text) return QJS_INTL_NO_MEMORY;
    }
    if (parts) {
        out->parts = d->a.malloc(d->a.opaque, parts * sizeof(*out->parts));
        if (!out->parts) { qjs_intl_native_duration_result_clear(&d->a, out); return QJS_INTL_NO_MEMORY; }
    }
    for (i = 0; i < count; i++) {
        if (views[i]->length) memcpy(out->text + cursor, views[i]->text,
                                    views[i]->length * sizeof(*out->text));
        for (j = 0; j < views[i]->part_count; j++) {
            out->parts[index] = views[i]->parts[j];
            out->parts[index].start += cursor; out->parts[index].end += cursor; index++;
        }
        cursor += views[i]->length;
    }
    out->length = text_len; out->part_count = parts; return QJS_INTL_OK;
}
static QJSIntlFormatted separator_view(QJSIntlUTF16 s, QJSIntlPart *p)
{
    QJSIntlFormatted v;
    memset(p, 0, sizeof(*p)); p->end = s.length; p->type = QJS_INTL_PART_LITERAL;
    v.text = (uint16_t *)s.data; v.length = s.length; v.parts = p; v.part_count = 1;
    return v;
}
QJSIntlStatus qjs_intl_native_duration_format(const QJSIntlNativeDuration *d,
    const QJSIntlDurationRecord *r, QJSIntlFormatted *out)
{
    QJSIntlFormatted pieces[10], chunks[10], joined, seps[2];
    QJSIntlPart sep_parts[2];
    QJSIntlUTF16 items[10];
    const QJSIntlFormatted **views = NULL;
    QJSIntlFormatted *literal_views = NULL;
    size_t i, j, count = 0, first_numeric = 10, last, index;
    size_t lengths[10] = {0};
    char *values[10] = {0};
    int nonzero[10] = {0}, selected[10] = {0}, sign_available = 1, any = 0;
    QJSIntlStatus s = QJS_INTL_OK;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!d || !r || r->sign < -1 || r->sign > 1) return QJS_INTL_INVALID_ARGUMENT;
    memset(pieces, 0, sizeof(pieces)); memset(chunks, 0, sizeof(chunks));
    memset(&joined, 0, sizeof(joined));
    for (i = 0; i < 10; i++) {
        QJSIntlBytes v = r->magnitudes[i];
        if (!v.data || !v.length || (v.length > 1 && v.data[0] == '0')) return QJS_INTL_INVALID_ARGUMENT;
        if (v.length > d->options.maximum_output_length) return QJS_INTL_OVERFLOW;
        for (j = 0; j < v.length; j++) if (v.data[j] < '0' || v.data[j] > '9') return QJS_INTL_INVALID_ARGUMENT;
        if (!zero(v)) any = 1;
    }
    if ((r->sign == 0) != !any) return QJS_INTL_INVALID_ARGUMENT;
    seps[0] = separator_view(d->separators[0], &sep_parts[0]);
    seps[1] = separator_view(d->separators[1], &sep_parts[1]);
    for (i = 0; i < 10; i++) {
        if (numeric(d->options.units[i].style)) { first_numeric = i; break; }
        if (d->options.units[i].style == QJS_INTL_DURATION_FRACTIONAL) break;
        last = i;
        while (last < 9 && d->options.units[last+1].style == QJS_INTL_DURATION_FRACTIONAL) last++;
        s = exact_value(d, r, i, last, &values[i], &lengths[i], &nonzero[i]);
        if (s != QJS_INTL_OK) goto done;
        if (nonzero[i] || d->options.units[i].always) {
            s = render(d, i, values[i], lengths[i], sign_available, &pieces[i]);
            if (s != QJS_INTL_OK) goto done;
            sign_available = 0; chunks[count] = pieces[i]; memset(&pieces[i], 0, sizeof(pieces[i])); count++;
        }
        if (last != i) break;
    }
    if (first_numeric < 10) {
        const QJSIntlFormatted *clock[5];
        size_t clock_count = 0;
        for (i = first_numeric; i <= 6; i++) {
            last = i;
            if (i == 6) while (last < 9 && d->options.units[last+1].style == QJS_INTL_DURATION_FRACTIONAL) last++;
            s = exact_value(d, r, i, last, &values[i], &lengths[i], &nonzero[i]);
            if (s != QJS_INTL_OK) goto done;
            selected[i] = nonzero[i] || d->options.units[i].always;
        }
        if (first_numeric == 4 && selected[4] && selected[6]) selected[5] = 1;
        for (i = first_numeric; i <= 6; i++) if (selected[i]) {
            s = render(d, i, values[i], lengths[i], sign_available, &pieces[i]);
            if (s != QJS_INTL_OK) goto done;
            sign_available = 0;
            if (i == 5 && selected[4]) clock[clock_count++] = &seps[0];
            if (i == 6 && selected[5]) clock[clock_count++] = &seps[1];
            clock[clock_count++] = &pieces[i];
        }
        if (clock_count) {
            s = merge(d, clock, clock_count, &chunks[count]);
            if (s != QJS_INTL_OK) goto done;
            count++;
        }
    }
    for (i = 0; i < count; i++) { items[i].data = chunks[i].text; items[i].length = chunks[i].length; }
    s = qjs_intl_native_list_format(d->list, items, count, &joined);
    if (s != QJS_INTL_OK) goto done;
    if (joined.part_count > SIZE_MAX / sizeof(*views) || joined.part_count > SIZE_MAX / sizeof(*literal_views)) {
        s = QJS_INTL_OVERFLOW; goto done;
    }
    if (joined.part_count) {
        views = d->a.malloc(d->a.opaque, joined.part_count * sizeof(*views));
        literal_views = d->a.malloc(d->a.opaque, joined.part_count * sizeof(*literal_views));
        if (!views || !literal_views) { s = QJS_INTL_NO_MEMORY; goto done; }
    }
    index = 0;
    for (i = 0; i < joined.part_count; i++) {
        QJSIntlPart *p = &joined.parts[i];
        if (p->type == QJS_INTL_PART_ELEMENT) {
            if (index >= count) { s = QJS_INTL_DATA_ERROR; goto done; }
            views[i] = &chunks[index++];
        } else {
            if (p->type != QJS_INTL_PART_LITERAL || p->end < p->start || p->end > joined.length) {
                s = QJS_INTL_DATA_ERROR; goto done;
            }
            literal_views[i].text = joined.text + p->start;
            literal_views[i].length = p->end - p->start;
            literal_views[i].parts = p; literal_views[i].part_count = 1;
            /* merge expects component-relative offsets. */
            p->end -= p->start; p->start = 0;
            views[i] = &literal_views[i];
        }
    }
    if (index != count) { s = QJS_INTL_DATA_ERROR; goto done; }
    s = merge(d, views, joined.part_count, out);
done:
    for (i = 0; i < 10; i++) {
        if (values[i]) d->a.free(d->a.opaque, values[i]);
        if (pieces[i].text || pieces[i].parts)
            qjs_intl_native_number_result_clear(d->numbers[i][0], &pieces[i]);
        qjs_intl_native_duration_result_clear(&d->a, &chunks[i]);
    }
    qjs_intl_native_list_result_clear(&d->a, &joined);
    if (views) d->a.free(d->a.opaque, views);
    if (literal_views) d->a.free(d->a.opaque, literal_views);
    if (s != QJS_INTL_OK) qjs_intl_native_duration_result_clear(&d->a, out);
    return s;
}
