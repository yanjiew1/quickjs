/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-native-data.h"
#include "plural-native-data.h"
#include "data/number-data-validation.h"
#include "data/number-extra-validation.h"
#include "number-unit.h"
#include <string.h>

static int text(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                uint32_t row, uint32_t offset, QJSIntlBytes *out)
{
    QJSIntlDataSlice value;
    if (qjs_intl_data_record_string(v, s, row, offset, &value) != QJS_INTL_DATA_OK)
        return 0;
    out->data = (const char *)value.data; out->length = value.length;
    return 1;
}
static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}
/* Validated sorted numeric keys. Two-field search is logarithmic; string
 * tables use their bounded locale range and exact byte identity. */
static int find_pair(const QJSIntlDataSection *s, uint32_t locale,
                     uint32_t numbering, uint32_t *index)
{
    uint32_t lo = 0, hi = s->record_count, a, b;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (qjs_intl_data_record_u32(s, middle, 0, &a) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(s, middle, 4, &b) != QJS_INTL_DATA_OK) return -1;
        if (a < locale || (a == locale && b < numbering)) lo = middle + 1;
        else hi = middle;
    }
    if (lo == s->record_count) return 0;
    if (qjs_intl_data_record_u32(s, lo, 0, &a) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record_u32(s, lo, 4, &b) != QJS_INTL_DATA_OK) return -1;
    if (a != locale || b != numbering) return 0;
    *index = lo; return 1;
}
static int find_string(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                       uint32_t locale, QJSIntlBytes key, unsigned int display,
                       uint32_t *index)
{
    uint32_t lo = 0, hi = s->record_count, value, i;
    QJSIntlBytes candidate;
    QJSIntlDataSlice row;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (qjs_intl_data_record_u32(s, middle, 0, &value) != QJS_INTL_DATA_OK) return -1;
        if (value < locale) lo = middle + 1; else hi = middle;
    }
    for (i = lo; i < s->record_count; i++) {
        if (qjs_intl_data_record_u32(s, i, 0, &value) != QJS_INTL_DATA_OK) return -1;
        if (value != locale) break;
        if (!text(v, s, i, 4, &candidate)) return -1;
        if (!equal(candidate, key)) continue;
        if (s->id == QJS_INTL_DATA_NUMBER_UNIT || s->id == QJS_INTL_DATA_NUMBER_DENOMINATOR) {
            if (qjs_intl_data_record(s, i, &row) != QJS_INTL_DATA_OK) return -1;
            if (row.data[12] != display) continue;
        }
        *index = i; return 1;
    }
    return 0;
}
QJSIntlStatus qjs_intl_native_number_currency_digits(const QJSIntlDataView *v,
                                                     QJSIntlBytes code,
                                                     unsigned int *out)
{
    QJSIntlDataSection s;
    QJSIntlDataSlice row;
    QJSIntlBytes key;
    QJSIntlDataStatus r;
    uint32_t i;
    unsigned int fallback = 0, found = 0, digits = 0;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = 0;
    if (!v || !code.data || code.length != 3) return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < 3; i++)
        if (code.data[i] < 'A' || code.data[i] > 'Z') return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_number_extra_data_validate(v) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_CURRENCY_DIGITS, &s);
    if (r == QJS_INTL_DATA_NOT_FOUND)
        return QJS_INTL_UNSUPPORTED;
    if (r != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < s.record_count; i++) {
        if (!text(v, &s, i, 0, &key) || qjs_intl_data_record(&s, i, &row) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (key.length == 7 && !memcmp(key.data, "DEFAULT", 7)) {
            fallback = row.data[8]; found |= 1;
        }
        if (equal(key, code)) { digits = row.data[8]; found |= 2; }
    }
    if (!(found & 1)) return QJS_INTL_DATA_ERROR;
    *out = found & 2 ? digits : fallback;
    return QJS_INTL_OK;
}
static QJSIntlStatus load_compact(const QJSIntlAllocator *a, const QJSIntlDataView *v,
    uint32_t locale, uint32_t numbering, unsigned int display,
    QJSIntlNumberCompact **out, size_t *count)
{
    QJSIntlDataSection s;
    QJSIntlDataSlice row;
    QJSIntlDataStatus r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_COMPACT, &s);
    uint32_t first, i, j, index = 0;
    size_t rows = 0;
    int found;
    *out = NULL; *count = 0;
    if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (r != QJS_INTL_DATA_OK || (found = find_pair(&s, locale, numbering, &first)) < 0)
        return QJS_INTL_DATA_ERROR;
    if (!found) return QJS_INTL_UNSUPPORTED;
    for (i = first; i < s.record_count; i++) {
        uint32_t x, y;
        if (qjs_intl_data_record_u32(&s, i, 0, &x) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&s, i, 4, &y) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&s, i, &row) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
        if (x != locale || y != numbering) break;
        if (row.data[8] == display) { if (!rows) index = i; rows++; }
    }
    if (!rows) return QJS_INTL_UNSUPPORTED;
    if (rows > SIZE_MAX / sizeof(**out)) return QJS_INTL_OVERFLOW;
    *out = a->malloc(a->opaque, rows * sizeof(**out)); if (!*out) return QJS_INTL_NO_MEMORY;
    memset(*out, 0, rows * sizeof(**out)); *count = rows;
    for (i = 0; i < rows; i++) {
        uint32_t magnitude, exponent;
        if (qjs_intl_data_record_u32(&s, index + i, 12, &magnitude) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&s, index + i, 16, &exponent) != QJS_INTL_DATA_OK) goto bad;
        (*out)[i].magnitude = (int32_t)magnitude; (*out)[i].exponent = (int32_t)exponent;
        for (j = 0; j < 6; j++)
            if (!text(v, &s, index + i, 20 + j * 8, &(*out)[i].patterns[j])) goto bad;
        if (!text(v, &s, index + i, 68, &(*out)[i].exact_one)) goto bad;
    }
    return QJS_INTL_OK;
bad:
    a->free(a->opaque, *out); *out = NULL; *count = 0;
    return QJS_INTL_DATA_ERROR;
}
static QJSIntlStatus compound_templates(const QJSIntlAllocator *a, const QJSIntlDataView *v,
    uint32_t locale, unsigned int display, QJSIntlBytes denominator,
    const QJSIntlBytes numerator[6], size_t maximum, QJSIntlBytes out[6])
{
    QJSIntlDataSection s;
    QJSIntlDataSlice row;
    QJSIntlBytes per_unit, name, pattern = {NULL, 0};
    QJSIntlDataStatus r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_DENOMINATOR, &s);
    uint32_t index, i;
    int found;
    if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (r != QJS_INTL_DATA_OK || (found = find_string(v, &s, locale, denominator, display, &index)) < 0)
        return QJS_INTL_DATA_ERROR;
    if (!found) return QJS_INTL_UNSUPPORTED;
    if (!text(v, &s, index, 16, &per_unit) || !text(v, &s, index, 24, &name)) return QJS_INTL_DATA_ERROR;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_COMPOUND_PER, &s) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < s.record_count; i++) {
        uint32_t value;
        if (qjs_intl_data_record_u32(&s, i, 0, &value) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&s, i, &row) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
        if (value > locale) break;
        if (value == locale && row.data[4] == display) {
            if (!text(v, &s, i, 8, &pattern)) return QJS_INTL_DATA_ERROR;
            break;
        }
    }
    if (!pattern.length) return QJS_INTL_DATA_ERROR;
    return qjs_intl_number_unit_compose(a, numerator, per_unit, name, pattern, maximum, out);
}
QJSIntlStatus qjs_intl_native_number_open_data(const QJSIntlAllocator *a,
    const QJSIntlDataView *v, uint32_t locale, uint32_t numbering,
    const QJSIntlNumberOptions *o, QJSIntlNativeNumber **out)
{
    QJSIntlDataSection s, locales, systems;
    QJSIntlDataSlice row;
    QJSIntlNumberData d;
    QJSIntlNumberScalarRange *ranges = NULL;
    QJSIntlNumberCompact *compact = NULL;
    QJSIntlBytes composed[6] = {{NULL, 0}}, numerator = {NULL, 0}, denominator = {NULL, 0};
    QJSIntlNativePlural *cardinal = NULL;
    QJSIntlPluralOptions po;
    QJSIntlStatus status = QJS_INTL_UNSUPPORTED;
    QJSIntlDataStatus r;
    uint32_t index, i, j, style;
    int found;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !v || !o) return QJS_INTL_INVALID_ARGUMENT;
    if ((unsigned int)o->style > QJS_INTL_NUMBER_UNIT ||
        (unsigned int)o->notation > QJS_INTL_NUMBER_COMPACT ||
        (unsigned int)o->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        (unsigned int)o->sign_display > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        (unsigned int)o->currency_display > QJS_INTL_CURRENCY_NAME ||
        (unsigned int)o->unit_display > QJS_INTL_UNIT_NARROW ||
        (unsigned int)o->compact_display > QJS_INTL_COMPACT_LONG ||
        o->currency_accounting > 1 || !o->maximum_output_length ||
        qjs_intl_decimal_options_validate(&o->digits) != QJS_INTL_OK)
        return QJS_INTL_INVALID_ARGUMENT;
    if (o->style == QJS_INTL_NUMBER_CURRENCY) {
        if (!o->currency.data || o->currency.length != 3) return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < 3; i++)
            if (o->currency.data[i] < 'A' || o->currency.data[i] > 'Z') return QJS_INTL_INVALID_ARGUMENT;
    }
    if (o->style == QJS_INTL_NUMBER_UNIT) {
        if (!qjs_intl_number_unit_validate(o->unit)) return QJS_INTL_INVALID_ARGUMENT;
        numerator = o->unit;
        for (i = 0; i + 5 <= o->unit.length; i++)
            if (!memcmp(o->unit.data + i, "-per-", 5)) {
                numerator.length = i; denominator.data = o->unit.data + i + 5;
                denominator.length = o->unit.length - i - 5; break;
            }
    }
    if (qjs_intl_number_extra_data_validate(v) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &systems) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (locale >= locales.record_count || numbering >= systems.record_count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_data_record(&systems, numbering, &row) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (row.data[8] != 10 || row.data[9]) return QJS_INTL_UNSUPPORTED;
    memset(&d, 0, sizeof(d));
    for (i = 0; i < 10; i++)
        if (qjs_intl_data_record_u32(&systems, numbering, 12 + i * 4, &d.digits[i]) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
    r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_SYMBOL, &s);
    if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (r != QJS_INTL_DATA_OK || (found = find_pair(&s, locale, numbering, &index)) < 0)
        return QJS_INTL_DATA_ERROR;
    if (!found) return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_record(&s, index, &row) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    d.minimum_grouping_digits = row.data[8];
    for (i = 0; i < QJS_INTL_NUMBER_SYMBOL_COUNT; i++)
        if (!text(v, &s, index, 16 + i * 8, &d.symbols[i])) return QJS_INTL_DATA_ERROR;
    style = o->style == QJS_INTL_NUMBER_PERCENT ? 1 :
        o->style == QJS_INTL_NUMBER_CURRENCY && o->currency_display != QJS_INTL_CURRENCY_NAME ?
            (o->currency_accounting ? 3 : 2) : 0;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_PATTERN, &s) != QJS_INTL_DATA_OK ||
        (found = find_pair(&s, locale, numbering, &index)) < 0) return QJS_INTL_DATA_ERROR;
    if (!found) return QJS_INTL_UNSUPPORTED;
    found = 0;
    for (i = index; i < s.record_count; i++) {
        uint32_t row_locale, row_system;
        if (qjs_intl_data_record_u32(&s, i, 0, &row_locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&s, i, 4, &row_system) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&s, i, &row) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
        if (row_locale != locale || row_system != numbering) break;
        if (row.data[8] == style) {
            QJSIntlNumberPattern *pattern = row.data[9] ? &d.alpha_pattern : &d.pattern;
            pattern->primary_group = row.data[12]; pattern->secondary_group = row.data[13];
            if (!text(v, &s, i, 16, &pattern->zero) || !text(v, &s, i, 24, &pattern->negative) ||
                !text(v, &s, i, 32, &pattern->positive)) return QJS_INTL_DATA_ERROR;
            if (!row.data[9]) found = 1;
        }
    }
    if (!found) return QJS_INTL_UNSUPPORTED;
    r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_MISC, &s);
    found = r == QJS_INTL_DATA_NOT_FOUND ? 0 : r != QJS_INTL_DATA_OK ? -1 :
        find_pair(&s, locale, numbering, &index);
    if (found < 0) return QJS_INTL_DATA_ERROR;
    if (found && (!text(v, &s, index, 8, &d.approximately) || !text(v, &s, index, 16, &d.range)))
        return QJS_INTL_DATA_ERROR;
    if (o->style == QJS_INTL_NUMBER_CURRENCY) {
        r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_CURRENCY, &s);
        found = r == QJS_INTL_DATA_NOT_FOUND ? 0 : r != QJS_INTL_DATA_OK ? -1 :
            find_string(v, &s, locale, o->currency, 0, &index);
        if (found < 0) return QJS_INTL_DATA_ERROR;
        if (found) {
            if (!text(v, &s, index, 12, &d.currency_symbol) ||
                !text(v, &s, index, 20, &d.currency_narrow_symbol)) return QJS_INTL_DATA_ERROR;
            for (i = 0; i < 6; i++)
                if (!text(v, &s, index, 28 + i * 8, &d.currency_names[i])) return QJS_INTL_DATA_ERROR;
        } else for (i = 0; i < 6; i++) d.currency_names[i] = o->currency;
        if (o->currency_display == QJS_INTL_CURRENCY_NAME) {
            r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_CURRENCY_NAME, &s);
            if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
            if (r != QJS_INTL_DATA_OK || (found = find_pair(&s, locale, numbering, &index)) < 0)
                return QJS_INTL_DATA_ERROR;
            if (!found) return QJS_INTL_UNSUPPORTED;
            for (i = 0; i < 6; i++)
                if (!text(v, &s, index, 8 + i * 8, &d.currency_name_patterns[i])) return QJS_INTL_DATA_ERROR;
        } else {
            r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_CURRENCY_SPACING, &s);
            if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
            if (r != QJS_INTL_DATA_OK || (found = find_pair(&s, locale, numbering, &index)) < 0)
                return QJS_INTL_DATA_ERROR;
            if (!found) return QJS_INTL_UNSUPPORTED;
            if (!text(v, &s, index, 8, &d.before_currency) || !text(v, &s, index, 16, &d.after_currency))
                return QJS_INTL_DATA_ERROR;
        }
    }
    if (o->style == QJS_INTL_NUMBER_UNIT) {
        r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_UNIT, &s);
        if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
        if (r != QJS_INTL_DATA_OK ||
            (found = find_string(v, &s, locale, numerator, o->unit_display, &index)) < 0)
            return QJS_INTL_DATA_ERROR;
        if (!found) return QJS_INTL_UNSUPPORTED;
        for (i = 0; i < 6; i++)
            if (!text(v, &s, index, 16 + i * 8, &d.unit_patterns[i])) return QJS_INTL_DATA_ERROR;
        if (denominator.length) {
            size_t maximum = o->maximum_output_length > SIZE_MAX / 4 ? SIZE_MAX : o->maximum_output_length * 4;
            status = compound_templates(a, v, locale, o->unit_display, denominator, d.unit_patterns, maximum, composed);
            if (status != QJS_INTL_OK) goto done;
            for (i = 0; i < 6; i++) d.unit_patterns[i] = composed[i];
        }
    }
    if (o->notation == QJS_INTL_NUMBER_COMPACT) {
        status = load_compact(a, v, locale, numbering, o->compact_display, &compact, &d.compact_count);
        if (status != QJS_INTL_OK) goto done;
        d.compact = compact;
    }
    if (o->style == QJS_INTL_NUMBER_UNIT ||
        o->notation == QJS_INTL_NUMBER_COMPACT ||
        (o->style == QJS_INTL_NUMBER_CURRENCY && o->currency_display == QJS_INTL_CURRENCY_NAME)) {
        memset(&po, 0, sizeof(po)); po.digits = o->digits;
        status = qjs_intl_native_plural_open_data(a, v, locale, &po, &cardinal);
        if (status != QJS_INTL_OK) goto done;
        d.cardinal = cardinal;
    }
    r = qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_UNICODE_CLASS, &s);
    if (r != QJS_INTL_DATA_OK && r != QJS_INTL_DATA_NOT_FOUND) { status = QJS_INTL_DATA_ERROR; goto done; }
    if (r == QJS_INTL_DATA_OK && s.record_count) {
        size_t range_count = s.record_count;
        if (range_count > SIZE_MAX / sizeof(*ranges)) { status = QJS_INTL_OVERFLOW; goto done; }
        ranges = a->malloc(a->opaque, range_count * sizeof(*ranges));
        if (!ranges) { status = QJS_INTL_NO_MEMORY; goto done; }
        for (i = 0; i < s.record_count; i++) {
            uint32_t *values[3] = { &ranges[i].first, &ranges[i].last, &ranges[i].flags };
            for (j = 0; j < 3; j++)
                if (qjs_intl_data_record_u32(&s, i, j * 4, values[j]) != QJS_INTL_DATA_OK) {
                    status = QJS_INTL_DATA_ERROR; goto done;
                }
        }
        d.classes = ranges; d.class_count = s.record_count;
    }
    status = qjs_intl_native_number_open(a, o, &d, out);
    if (status == QJS_INTL_OK && cardinal) {
        qjs_intl_native_number_take_cardinal(*out, cardinal); cardinal = NULL;
    }
done:
    qjs_intl_native_plural_close(cardinal);
    if (ranges) a->free(a->opaque, ranges);
    if (compact) a->free(a->opaque, compact);
    qjs_intl_number_unit_composed_clear(a, composed);
    return status;
}
