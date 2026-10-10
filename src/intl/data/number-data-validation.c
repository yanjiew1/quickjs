/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-data-validation.h"
#include "../number-native.h"
#include "../number-range.h"
#include <string.h>

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int zeros(const unsigned char *p, size_t length)
{
    size_t i; for (i = 0; i < length; i++) if (p[i]) return 0;
    return 1;
}
static int utf8(QJSIntlBytes s, int empty)
{
    size_t i = 0;
    if (!s.length) return empty;
    if (!s.data) return 0;
    while (i < s.length) {
        uint32_t c = (unsigned char)s.data[i++];
        unsigned int n, j;
        if (c < 0x80) n = 0;
        else if (c >= 0xc2 && c <= 0xdf) { c &= 31; n = 1; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; n = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; n = 3; }
        else return 0;
        if (n > s.length - i) return 0;
        for (j = 0; j < n; j++) {
            unsigned int b = (unsigned char)s.data[i++];
            if ((b & 0xc0) != 0x80) return 0;
            c = c << 6 | (b & 63);
        }
        if (!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff) ||
            (n == 1 && c < 0x80) || (n == 2 && c < 0x800) || (n == 3 && c < 0x10000)) return 0;
    }
    return 1;
}
static int string(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                  uint32_t row, uint32_t offset, QJSIntlBytes *out)
{
    QJSIntlDataSlice text;
    if (qjs_intl_data_record_string(v, s, row, offset, &text) != QJS_INTL_DATA_OK) return 0;
    out->data = (const char *)text.data; out->length = text.length;
    return 1;
}
static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t n = a.length < b.length ? a.length : b.length;
    int c = n ? memcmp(a.data, b.data, n) : 0;
    return c ? c : a.length < b.length ? -1 : a.length > b.length;
}
static int code(QJSIntlBytes s, int default_allowed)
{
    size_t i;
    if (default_allowed && s.length == 7 && !memcmp(s.data, "DEFAULT", 7)) return 1;
    if (s.length != 3) return 0;
    for (i = 0; i < 3; i++) if (s.data[i] < 'A' || s.data[i] > 'Z') return 0;
    return 1;
}
static int unit(QJSIntlBytes s)
{
    static const char *const names[] = {
        "acre", "bit", "byte", "celsius", "centimeter", "day", "degree", "fahrenheit",
        "fluid-ounce", "foot", "gallon", "gigabit", "gigabyte", "gram", "hectare", "hour",
        "inch", "kilobit", "kilobyte", "kilogram", "kilometer", "liter", "megabit", "megabyte",
        "meter", "microsecond", "mile", "mile-scandinavian", "milliliter", "millimeter",
        "millisecond", "minute", "month", "nanosecond", "ounce", "percent", "petabyte",
        "pound", "second", "stone", "terabit", "terabyte", "week", "yard", "year"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (s.length == strlen(names[i]) && !memcmp(s.data, names[i], s.length)) return 1;
    return 0;
}
static unsigned int occurrences(QJSIntlBytes s, const char *needle)
{
    size_t i, length = strlen(needle);
    unsigned int count = 0;
    for (i = 0; i + length <= s.length; i++)
        if (!memcmp(s.data + i, needle, length)) count++;
    return count;
}
static int raw_template(QJSIntlBytes s, int range)
{
    size_t i;
    unsigned int counts[2] = { 0, 0 };
    if (!utf8(s, 0)) return 0;
    for (i = 0; i < s.length; i++) {
        if (s.data[i] == '}') return 0;
        if (s.data[i] != '{') continue;
        if (i + 3 > s.length || s.data[i + 2] != '}' ||
            (s.data[i + 1] != '0' && !(range && s.data[i + 1] == '1'))) return 0;
        if (++counts[s.data[i + 1] - '0'] != 1) return 0;
        i += 2;
    }
    return counts[0] == 1 && counts[1] == (unsigned int)range;
}
static int pair_exists(const QJSIntlDataSection *s, uint32_t locale, uint32_t numbering)
{
    uint32_t lo = 0, hi = s->record_count, a, b;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (qjs_intl_data_record_u32(s, mid, 0, &a) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(s, mid, 4, &b) != QJS_INTL_DATA_OK) return 0;
        if (a < locale || (a == locale && b < numbering)) lo = mid + 1;
        else hi = mid;
    }
    return lo < s->record_count &&
        qjs_intl_data_record_u32(s, lo, 0, &a) == QJS_INTL_DATA_OK &&
        qjs_intl_data_record_u32(s, lo, 4, &b) == QJS_INTL_DATA_OK && a == locale && b == numbering;
}
QJSIntlDataStatus qjs_intl_number_data_validate(const QJSIntlDataView *v)
{
    static const uint32_t widths[9] = { 96, 40, 76, 64, 56, 12, 24, 12, 24 };
    QJSIntlDataSection sections[9], locales, systems;
    unsigned int present[9] = { 0 }, any = 0, k;
    uint32_t i, j, a = 0, b = 0, previous_a = 0, previous_b = 0;
    unsigned int previous_style = 0, previous_variant = 0;
    QJSIntlDataSlice row, system;
    QJSIntlBytes text, key = { NULL, 0 }, previous_key = { NULL, 0 };
    if (!v || !v->data) return QJS_INTL_DATA_INVALID_ARGUMENT;
    for (k = 0; k < 9; k++) {
        QJSIntlDataStatus r = qjs_intl_data_section(v, 80 + k, &sections[k]);
        if (r == QJS_INTL_DATA_NOT_FOUND) continue;
        if (r != QJS_INTL_DATA_OK || sections[k].record_width != widths[k]) return QJS_INTL_DATA_INVALID;
        present[k] = 1; any = 1;
    }
    if (!any) return QJS_INTL_DATA_OK;
    if (v->length < 64 || v->data[10] < 3 || v->data[11] ||
        le32(v->data + 36) != (18u << 16) ||
        !QJS_INTL_CLDR_VERSION_SUPPORTED(le32(v->data + 40)) ||
        present[0] != present[1] || ((present[2] || present[8]) && !present[7]) ||
        qjs_intl_data_section(v, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &systems) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_INVALID;
    for (k = 0; k < 9; k++) {
        QJSIntlDataSection *s = &sections[k];
        int has_default = 0;
        if (!present[k]) continue;
        previous_key.data = NULL; previous_key.length = 0;
        previous_a = previous_b = previous_style = previous_variant = 0;
        for (i = 0; i < s->record_count; i++) {
            unsigned int style = 0, variant = 0;
            if (qjs_intl_data_record(s, i, &row) != QJS_INTL_DATA_OK || row.length != widths[k])
                return QJS_INTL_DATA_INVALID;
            if (k == 7) {
                a = le32(row.data); b = le32(row.data + 4); j = le32(row.data + 8);
                if (a > b || b > 0x10ffff || j > 7 || j == 2 ||
                    (a <= 0xdfff && b >= 0xd800) ||
                    (i && (a <= previous_b || (a == previous_b + 1 && j == previous_style))))
                    return QJS_INTL_DATA_INVALID;
                previous_b = b; previous_style = j; continue;
            }
            if (k == 5) {
                if (!string(v, s, i, 0, &key) || !code(key, 1) ||
                    row.data[8] > 100 || !zeros(row.data + 9, 3) ||
                    (i && compare(previous_key, key) >= 0)) return QJS_INTL_DATA_INVALID;
                if (key.length == 7) {
                    if (row.data[8] != 2) return QJS_INTL_DATA_INVALID;
                    has_default = 1;
                }
                previous_key = key; continue;
            }
            a = le32(row.data);
            if (a >= locales.record_count) return QJS_INTL_DATA_INVALID;
            if (k == 2 || k == 3) {
                if (!string(v, s, i, 4, &key) || (k == 2 ? !code(key, 0) : !unit(key)))
                    return QJS_INTL_DATA_INVALID;
                style = k == 3 ? row.data[12] : 0;
                if (k == 3 && (style > 2 || !zeros(row.data + 13, 3))) return QJS_INTL_DATA_INVALID;
                if (i && (a < previous_a || (a == previous_a &&
                    (compare(previous_key, key) > 0 ||
                     (!compare(previous_key, key) && style <= previous_style))))) return QJS_INTL_DATA_INVALID;
                if (k == 2) {
                    for (j = 0; j < 8; j++)
                        if (!string(v, s, i, 12 + j * 8, &text) || !utf8(text, j < 2))
                            return QJS_INTL_DATA_INVALID;
                } else for (j = 0; j < 6; j++)
                    if (!string(v, s, i, 16 + j * 8, &text) ||
                        !qjs_intl_number_template_validate(text, 3)) return QJS_INTL_DATA_INVALID;
                previous_key = key; previous_a = a; previous_style = style; continue;
            }
            b = le32(row.data + 4);
            if (b >= systems.record_count || qjs_intl_data_record(&systems, b, &system) != QJS_INTL_DATA_OK ||
                system.length != 52 || system.data[8] != 10 || system.data[9]) return QJS_INTL_DATA_INVALID;
            if (k == 1) { style = row.data[8]; variant = row.data[9]; }
            if (i && (a < previous_a || (a == previous_a &&
                (b < previous_b || (b == previous_b && (k != 1 || style < previous_style ||
                 (style == previous_style && variant <= previous_variant))))))) return QJS_INTL_DATA_INVALID;
            if (k != 0 && (!present[0] || !pair_exists(&sections[0], a, b))) return QJS_INTL_DATA_INVALID;
            if (k == 0) {
                if (row.data[8] < 1 || row.data[8] > 9 || !zeros(row.data + 9, 7) ||
                    !pair_exists(&sections[1], a, b)) return QJS_INTL_DATA_INVALID;
                for (j = 0; j < 10; j++)
                    if (!string(v, s, i, 16 + j * 8, &text) || !utf8(text, 0)) return QJS_INTL_DATA_INVALID;
            } else if (k == 1) {
                if (style > 3 || variant > 1 || (variant && style < 2) || !zeros(row.data + 10, 2) ||
                    !zeros(row.data + 14, 2) || row.data[12] > 9 || row.data[13] > 9 ||
                    (!!row.data[12] != !!row.data[13]) ||
                    (variant && (!i || a != previous_a || b != previous_b ||
                                 style != previous_style || previous_variant))) return QJS_INTL_DATA_INVALID;
                for (j = 0; j < 3; j++) {
                    if (!string(v, s, i, 16 + j * 8, &text) ||
                        !qjs_intl_number_template_validate(text, 0) ||
                        occurrences(text, "{currency}") != (unsigned int)(style >= 2) ||
                        occurrences(text, "{percentSign}") != (unsigned int)(style == 1) ||
                        (!j && (occurrences(text, "{minusSign}") || occurrences(text, "{plusSign}"))) ||
                        (j == 1 && occurrences(text, "{plusSign}")) ||
                        (j == 2 && (!occurrences(text, "{plusSign}") || occurrences(text, "{minusSign}"))))
                        return QJS_INTL_DATA_INVALID;
                }
            } else if (k == 4) {
                for (j = 0; j < 6; j++)
                    if (!string(v, s, i, 8 + j * 8, &text) ||
                        !qjs_intl_number_template_validate(text, 2)) return QJS_INTL_DATA_INVALID;
            } else if (k == 6) {
                QJSIntlNumberRangeSlices pieces;
                if (!string(v, s, i, 8, &text) || !raw_template(text, 0) ||
                    !qjs_intl_number_range_slices(text, 0, &pieces) ||
                    !string(v, s, i, 16, &text) || !raw_template(text, 1) ||
                    !qjs_intl_number_range_slices(text, 1, &pieces)) return QJS_INTL_DATA_INVALID;
            } else if (k == 8) {
                if (!string(v, s, i, 8, &text) || !utf8(text, 1) ||
                    !string(v, s, i, 16, &text) || !utf8(text, 1)) return QJS_INTL_DATA_INVALID;
            }
            previous_a = a; previous_b = b; previous_style = style; previous_variant = variant;
        }
        if (k == 5 && !has_default) return QJS_INTL_DATA_INVALID;
    }
    return QJS_INTL_DATA_OK;
}
