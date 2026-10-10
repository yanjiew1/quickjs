/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-extra-validation.h"
#include "../number-native.h"
#include <string.h>

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int text(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                uint32_t row, uint32_t offset, QJSIntlBytes *out)
{
    QJSIntlDataSlice value;
    if (qjs_intl_data_record_string(v, s, row, offset, &value) != QJS_INTL_DATA_OK) return 0;
    out->data = (const char *)value.data; out->length = value.length;
    return 1;
}
static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t n = a.length < b.length ? a.length : b.length;
    int c = n ? memcmp(a.data, b.data, n) : 0;
    return c ? c : a.length < b.length ? -1 : a.length > b.length;
}
static int pair(const QJSIntlDataSection *s, uint32_t a, uint32_t b)
{
    uint32_t lo = 0, hi = s->record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, x, y;
        if (qjs_intl_data_record_u32(s, mid, 0, &x) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(s, mid, 4, &y) != QJS_INTL_DATA_OK) return 0;
        if (x < a || (x == a && y < b)) lo = mid + 1; else hi = mid;
    }
    if (lo < s->record_count) {
        uint32_t x, y;
        return qjs_intl_data_record_u32(s, lo, 0, &x) == QJS_INTL_DATA_OK &&
            qjs_intl_data_record_u32(s, lo, 4, &y) == QJS_INTL_DATA_OK && x == a && y == b;
    }
    return 0;
}
static int per_pattern(QJSIntlBytes s)
{
    size_t i = 0;
    unsigned int first = 0, second = 0;
    if (!qjs_intl_number_compact_template_validate(s, 1)) {
        /* That gate rejects named compound tokens; validate scalar text by
         * replacing only recognized tokens conceptually, without allocating. */
        QJSIntlBytes literal;
        while (i < s.length) {
            size_t start = i;
            while (i < s.length && s.data[i] != '{' && s.data[i] != '}') i++;
            literal.data = s.data + start; literal.length = i - start;
            if (literal.length && !qjs_intl_number_compact_template_validate(literal, 1)) return 0;
            if (i == s.length) break;
            if (s.data[i] == '}') return 0;
            if (s.length - i >= 11 && !memcmp(s.data + i, "{numerator}", 11)) {
                if (++first > 1) return 0;
                i += 11;
            } else if (s.length - i >= 13 && !memcmp(s.data + i, "{denominator}", 13)) {
                if (++second > 1) return 0;
                i += 13;
            } else return 0;
        }
    }
    return first == 1 && second == 1;
}
static int unit_exists(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                       uint32_t locale, QJSIntlBytes unit, unsigned int display)
{
    uint32_t i, lo = 0, hi = s->record_count;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2, value;
        if (qjs_intl_data_record_u32(s, middle, 0, &value) != QJS_INTL_DATA_OK) return 0;
        if (value < locale) lo = middle + 1; else hi = middle;
    }
    for (i = lo; i < s->record_count; i++) {
        QJSIntlDataSlice row;
        QJSIntlBytes key;
        uint32_t a;
        if (qjs_intl_data_record_u32(s, i, 0, &a) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(s, i, &row) != QJS_INTL_DATA_OK || !text(v, s, i, 4, &key)) return 0;
        if (a > locale) break;
        if (a == locale && row.data[12] == display && !compare(key, unit)) return 1;
    }
    return 0;
}
static int compound_exists(const QJSIntlDataSection *s, uint32_t locale, unsigned int display)
{
    uint32_t lo = 0, hi = s->record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlDataSlice row;
        if (qjs_intl_data_record(s, mid, &row) != QJS_INTL_DATA_OK) return 0;
        if (le32(row.data) < locale || (le32(row.data) == locale && row.data[4] < display)) lo = mid + 1;
        else hi = mid;
    }
    if (lo < s->record_count) {
        QJSIntlDataSlice row;
        return qjs_intl_data_record(s, lo, &row) == QJS_INTL_DATA_OK && le32(row.data) == locale && row.data[4] == display;
    }
    return 0;
}
QJSIntlDataStatus qjs_intl_number_extra_data_validate(const QJSIntlDataView *v)
{
    static const uint32_t ids[] = {120, 121, 122}, widths[] = {76, 32, 16};
    QJSIntlDataSection sections[3], symbols, units, locales;
    unsigned int present[3] = {0}, k, any = 0;
    uint32_t i;
    QJSIntlDataStatus status = qjs_intl_number_data_validate(v);
    if (status != QJS_INTL_DATA_OK) return status;
    for (k = 0; k < 3; k++) {
        status = qjs_intl_data_section(v, ids[k], &sections[k]);
        if (status == QJS_INTL_DATA_NOT_FOUND) continue;
        if (status != QJS_INTL_DATA_OK || sections[k].record_width != widths[k]) return QJS_INTL_DATA_INVALID;
        present[k] = 1; any = 1;
    }
    if (!any) return QJS_INTL_DATA_OK;
    if (v->length < 64 || v->data[10] < 3 || v->data[11] ||
        le32(v->data + 36) != (18u << 16) ||
        !QJS_INTL_CLDR_VERSION_SUPPORTED(le32(v->data + 40)) ||
        present[1] != present[2] ||
        qjs_intl_data_section(v, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_SYMBOL, &symbols) != QJS_INTL_DATA_OK ||
        (present[1] && qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_UNIT, &units) != QJS_INTL_DATA_OK))
        return QJS_INTL_DATA_INVALID;
    for (k = 0; k < 3; k++) {
        uint32_t previous_a = 0, previous_b = 0, previous_m = 0;
        unsigned int previous_display = 0;
        QJSIntlBytes previous_key = {NULL, 0};
        if (!present[k]) continue;
        for (i = 0; i < sections[k].record_count; i++) {
            QJSIntlDataSlice row;
            QJSIntlBytes key = {NULL, 0}, value;
            uint32_t a, b = 0, magnitude = 0, exponent;
            unsigned int display, j;
            if (qjs_intl_data_record(&sections[k], i, &row) != QJS_INTL_DATA_OK || row.length != widths[k])
                return QJS_INTL_DATA_INVALID;
            a = le32(row.data); if (a >= locales.record_count) return QJS_INTL_DATA_INVALID;
            display = row.data[k == 2 ? 4 : k == 1 ? 12 : 8];
            if (display > (k == 0 ? 1u : 2u)) return QJS_INTL_DATA_INVALID;
            if (k == 0) {
                b = le32(row.data + 4); magnitude = le32(row.data + 12); exponent = le32(row.data + 16);
                if (row.data[9] || row.data[10] || row.data[11] || magnitude > 1000000000 || exponent > magnitude ||
                    !pair(&symbols, a, b)) return QJS_INTL_DATA_INVALID;
                for (j = 0; j < 6; j++)
                    if (!text(v, &sections[k], i, 20 + j * 8, &value) ||
                        !qjs_intl_number_compact_template_validate(value, (int32_t)exponent)) return QJS_INTL_DATA_INVALID;
                if (!text(v, &sections[k], i, 68, &value) || (value.length &&
                    !qjs_intl_number_compact_template_validate(value, (int32_t)exponent))) return QJS_INTL_DATA_INVALID;
                if (i && (a < previous_a || (a == previous_a && (b < previous_b ||
                    (b == previous_b && (display < previous_display || (display == previous_display && magnitude <= previous_m)))))))
                    return QJS_INTL_DATA_INVALID;
            } else if (k == 1) {
                if (row.data[13] || row.data[14] || row.data[15] || !text(v, &sections[k], i, 4, &key) ||
                    !unit_exists(v, &units, a, key, display) || !compound_exists(&sections[2], a, display))
                    return QJS_INTL_DATA_INVALID;
                if (!text(v, &sections[k], i, 16, &value) ||
                    (value.length && !qjs_intl_number_template_validate(value, 1))) return QJS_INTL_DATA_INVALID;
                if (!text(v, &sections[k], i, 24, &value) ||
                    !qjs_intl_number_compact_template_validate(value, 1) ||
                    memchr(value.data, '{', value.length) || memchr(value.data, '}', value.length)) return QJS_INTL_DATA_INVALID;
                if (i && (a < previous_a || (a == previous_a && (compare(key, previous_key) < 0 ||
                    (!compare(key, previous_key) && display <= previous_display))))) return QJS_INTL_DATA_INVALID;
            } else {
                if (row.data[5] || row.data[6] || row.data[7] || !text(v, &sections[k], i, 8, &value) ||
                    !per_pattern(value) || (i && (a < previous_a || (a == previous_a && display <= previous_display))))
                    return QJS_INTL_DATA_INVALID;
            }
            previous_a = a; previous_b = b; previous_m = magnitude; previous_display = display; previous_key = key;
        }
    }
    return QJS_INTL_DATA_OK;
}
