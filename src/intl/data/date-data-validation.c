/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-data-validation.h"
#include "intl/date-pattern.h"
#include <string.h>

QJSIntlDataStatus qjs_intl_date_data_i64(const QJSIntlDataSection *s,
    uint32_t row, uint32_t offset, int64_t *out)
{
    uint32_t lo, hi;
    uint64_t bits;
    if (!out) return QJS_INTL_DATA_INVALID_ARGUMENT;
    *out = 0;
    if (offset > UINT32_MAX - 4 ||
        qjs_intl_data_record_u32(s, row, offset, &lo) ||
        qjs_intl_data_record_u32(s, row, offset + 4, &hi)) return QJS_INTL_DATA_INVALID;
    bits = ((uint64_t)hi << 32) | lo;
    *out = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
    return QJS_INTL_DATA_OK;
}
static int compare(QJSIntlDataSlice a, QJSIntlDataSlice b)
{
    size_t n = a.length < b.length ? a.length : b.length;
    int r = n ? memcmp(a.data, b.data, n) : 0;
    return r ? r : a.length < b.length ? -1 : a.length > b.length ? 1 : 0;
}
static int string(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                  uint32_t row, uint32_t offset, QJSIntlDataSlice *out)
{
    return qjs_intl_data_record_string(v, s, row, offset, out) == QJS_INTL_DATA_OK;
}
static int identifier(QJSIntlDataSlice s, int calendar)
{
    size_t i;
    if (!s.length) return 0;
    for (i = 0; i < s.length; i++) {
        unsigned char c = s.data[i];
        if (calendar) {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
        } else if (c < 0x21 || c > 0x7e) return 0;
    }
    return 1;
}
static int word(QJSIntlDataSlice s, const char *value)
{
    size_t n = strlen(value);
    return s.length == n && (!n || !memcmp(s.data, value, n));
}
static int lunar(QJSIntlDataSlice calendar)
{
    return word(calendar, "chinese") || word(calendar, "dangi");
}
static int template_valid(QJSIntlDataSlice s, unsigned int expected, int quoted)
{
    size_t i;
    unsigned int seen = 0;
    int quote = 0;
    if (!s.length) return 0;
    for (i = 0; i < s.length; i++) {
        unsigned char c = s.data[i];
        if (quoted && c == '\'') {
            if (i + 1 < s.length && s.data[i + 1] == '\'') { i++; continue; }
            quote = !quote;
        }
        if (!quote && c == '{') {
            unsigned int bit;
            if (i + 2 >= s.length || s.data[i + 2] != '}' || s.data[i + 1] < '0' || s.data[i + 1] > '1') return 0;
            bit = 1u << (s.data[i + 1] - '0');
            if (!(expected & bit) || (seen & bit)) return 0;
            seen |= bit; i += 2;
        } else if (!quote && c == '}') return 0;
    }
    return !quote && seen == expected;
}
static int hour_valid(QJSIntlDataSlice s)
{
    size_t i = 0;
    unsigned int h = 0, m = 0;
    while (i < s.length && s.data[i] != 'H') i++;
    while (i < s.length && s.data[i] == 'H') { i++; h++; }
    while (i < s.length && s.data[i] != 'm') {
        if (s.data[i] == 'H') return 0;
        i++;
    }
    while (i < s.length && s.data[i] == 'm') { i++; m++; }
    while (i < s.length) {
        if (s.data[i] == 'H' || s.data[i] == 'm') return 0;
        i++;
    }
    return h >= 1 && h <= 2 && m == 2;
}
static int common(const QJSIntlDataSection *s, uint32_t row, uint32_t locales,
                   uint32_t *locale, QJSIntlDataSlice *bytes)
{
    return qjs_intl_data_record_u32(s, row, 0, locale) == QJS_INTL_DATA_OK &&
           *locale < locales && qjs_intl_data_record(s, row, bytes) == QJS_INTL_DATA_OK;
}
static QJSIntlStatus year_kind(void *opaque, QJSIntlBytes literal,
                              unsigned int symbol, unsigned int count)
{
    unsigned int *present = opaque;
    (void)literal; (void)count;
    if (symbol == 'r' || symbol == 'U') *present = 1;
    return QJS_INTL_OK;
}
static int patterns(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                     uint32_t locales)
{
    uint32_t i, locale, previous_locale = 0;
    QJSIntlDataSlice row, calendar, skeleton, pattern, previous_calendar = { NULL, 0 }, previous_skeleton = { NULL, 0 };
    unsigned int previous_kind = 0, previous_style = 0, previous_family = 0;
    for (i = 0; i < s->record_count; i++) {
        unsigned int kind, style, family, parsed;
        int fields[QJS_DATE_FIELD_COUNT], c;
        QJSIntlBytes p;
        if (!common(s, i, locales, &locale, &row) || row.length != 32 ||
            !string(v, s, i, 4, &calendar) || !identifier(calendar, 1) ||
            !string(v, s, i, 16, &skeleton) || !string(v, s, i, 24, &pattern) || !pattern.length) return 0;
        kind = row.data[12]; style = row.data[13]; family = row.data[14];
        if (kind > 3 || (kind ? style > 3 : style != 255) || family > 2 || row.data[15]) return 0;
        if (kind == 3) {
            if (family || skeleton.length || !template_valid(pattern, 3, 1)) return 0;
        } else {
            unsigned int cyclic = 0;
            p.data = (const char *)pattern.data; p.length = pattern.length;
            if (qjs_intl_date_pattern_fields(p, fields, &parsed) || parsed != family) return 0;
            if (qjs_intl_date_pattern_visit(p, year_kind, &cyclic) || (cyclic && !lunar(calendar))) return 0;
            if (kind == 0 && !skeleton.length) return 0;
            if (kind == 1 && family) return 0;
            if (kind == 2 && !family) return 0;
        }
        c = locale < previous_locale ? -1 : locale > previous_locale ? 1 : compare(calendar, previous_calendar);
        if (!c) c = kind < previous_kind ? -1 : kind > previous_kind ? 1 : style < previous_style ? -1 :
                    style > previous_style ? 1 : family < previous_family ? -1 : family > previous_family ? 1 : compare(skeleton, previous_skeleton);
        if (i && c <= 0) return 0;
        previous_locale = locale; previous_calendar = calendar; previous_kind = kind;
        previous_style = style; previous_family = family; previous_skeleton = skeleton;
    }
    return 1;
}
static int names(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                  uint32_t locales)
{
    uint32_t i, locale, index, previous_locale = 0, previous_index = 0;
    QJSIntlDataSlice row, calendar, text, previous_calendar = { NULL, 0 };
    unsigned int previous_field = 0, previous_context = 0, previous_width = 0;
    for (i = 0; i < s->record_count; i++) {
        unsigned int field, context, width;
        int c;
        if (!common(s, i, locales, &locale, &row) || row.length != 28 ||
            !string(v, s, i, 4, &calendar) || !identifier(calendar, 1) ||
            !string(v, s, i, 20, &text) || !text.length ||
            qjs_intl_data_record_u32(s, i, 16, &index)) return 0;
        field = row.data[12]; context = row.data[13]; width = row.data[14];
        if (field > 5 || context > 1 || width > 3 || row.data[15] ||
            (field == 0 && context) || (width == 3 && field != 2) ||
            (field == 1 && (index < 1 || index > 14 ||
                (index == 14 && !word(calendar, "hebrew")))) ||
            (field == 2 && (index < 1 || index > 7)) || (field == 3 && index >= 12) ||
            (field == 4 && (!lunar(calendar) || context || index < 1 || index > 60)) ||
            (field == 5 && (!lunar(calendar) || index > 1 ||
                (!index && (context || width)) || !template_valid(text, 1, 0)))) return 0;
        c = locale < previous_locale ? -1 : locale > previous_locale ? 1 : compare(calendar, previous_calendar);
        if (!c) c = field < previous_field ? -1 : field > previous_field ? 1 : context < previous_context ? -1 :
                    context > previous_context ? 1 : width < previous_width ? -1 : width > previous_width ? 1 :
                    index < previous_index ? -1 : index > previous_index ? 1 : 0;
        if (i && c <= 0) return 0;
        previous_locale = locale; previous_calendar = calendar; previous_field = field;
        previous_context = context; previous_width = width; previous_index = index;
    }
    return 1;
}
static int symbols(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                    uint32_t locales, const QJSIntlDataSection *numbering)
{
    uint32_t i, locale, n, previous_locale = 0, previous_n = 0;
    QJSIntlDataSlice row, text, numeric;
    for (i = 0; i < s->record_count; i++) {
        unsigned int j;
        if (!common(s, i, locales, &locale, &row) || row.length != 48 ||
            qjs_intl_data_record_u32(s, i, 4, &n) || n >= numbering->record_count ||
            qjs_intl_data_record(numbering, n, &numeric) || numeric.length != 52 ||
            numeric.data[8] != 10 || numeric.data[9]) return 0;
        if (i && (locale < previous_locale || (locale == previous_locale && n <= previous_n))) return 0;
        for (j = 0; j < 5; j++) {
            if (!string(v, s, i, 8 + j * 8, &text) || !text.length) return 0;
            if (j == 1 && !template_valid(text, 1, 0)) return 0;
            if ((j == 3 || j == 4) && !hour_valid(text)) return 0;
        }
        previous_locale = locale; previous_n = n;
    }
    return 1;
}
static int contains(uint32_t from, uint32_t before, uint32_t second)
{
    return from < before ? second >= from && second < before : second >= from || second < before;
}
static int rules(const QJSIntlDataSection *s, uint32_t locales)
{
    uint32_t i, locale, from, before, start = 0, previous_locale = 0, previous_from = 0, previous_before = 0;
    unsigned int previous_period = 0, previous_exact = 0;
    QJSIntlDataSlice row;
    for (i = 0; i < s->record_count; i++) {
        unsigned int period, exact;
        int c;
        if (!common(s, i, locales, &locale, &row) || row.length != 16 ||
            qjs_intl_data_record_u32(s, i, 8, &from) || qjs_intl_data_record_u32(s, i, 12, &before)) return 0;
        period = row.data[4]; exact = row.data[5];
        if (period >= 12 || exact > 1 || row.data[6] || row.data[7] || from >= 86400 || before > 86400 ||
            (exact ? from != before : from == before)) return 0;
        c = locale < previous_locale ? -1 : locale > previous_locale ? 1 : period < previous_period ? -1 :
            period > previous_period ? 1 : exact < previous_exact ? -1 : exact > previous_exact ? 1 :
            from < previous_from ? -1 : from > previous_from ? 1 : before < previous_before ? -1 : before > previous_before ? 1 : 0;
        if (i && c <= 0) return 0;
        if (i && locale != previous_locale) start = i;
        {
            uint32_t j;
            for (j = start; j < i; j++) {
                QJSIntlDataSlice prior;
                uint32_t other_from, other_before;
                if (qjs_intl_data_record(s, j, &prior) || qjs_intl_data_record_u32(s, j, 8, &other_from) ||
                    qjs_intl_data_record_u32(s, j, 12, &other_before)) return 0;
                if (exact && prior.data[5] && from == other_from) return 0;
                if (!exact && !prior.data[5] &&
                    (contains(from, before, other_from) || contains(other_from, other_before, from))) return 0;
            }
        }
        previous_locale = locale; previous_period = period; previous_exact = exact;
        previous_from = from; previous_before = before;
    }
    return 1;
}
static int zones(const QJSIntlDataView *v, const QJSIntlDataSection *s, uint32_t locales)
{
    uint32_t i, locale, previous_locale = 0;
    unsigned int previous_kind = 0;
    QJSIntlDataSlice row, key, text, previous_key = { NULL, 0 };
    for (i = 0; i < s->record_count; i++) {
        unsigned int kind, j;
        int c;
        if (!common(s, i, locales, &locale, &row) || row.length != 72 ||
            !string(v, s, i, 8, &key) || !identifier(key, 0)) return 0;
        kind = row.data[4];
        if (kind > 1 || row.data[5] || row.data[6] || row.data[7]) return 0;
        for (j = 0; j < 7; j++)
            if (!string(v, s, i, 16 + j * 8, &text) || (kind && j == 6 && text.length)) return 0;
        c = locale < previous_locale ? -1 : locale > previous_locale ? 1 : kind < previous_kind ? -1 :
            kind > previous_kind ? 1 : compare(key, previous_key);
        if (i && c <= 0) return 0;
        previous_locale = locale; previous_kind = kind; previous_key = key;
    }
    return 1;
}
static int metazones(const QJSIntlDataView *v, const QJSIntlDataSection *s)
{
    uint32_t i;
    QJSIntlDataSlice zone, meta, previous_zone = { NULL, 0 };
    int64_t from, before, previous_before = 0;
    for (i = 0; i < s->record_count; i++) {
        int c;
        if (!string(v, s, i, 0, &zone) || !identifier(zone, 0) ||
            !string(v, s, i, 8, &meta) || !identifier(meta, 0) ||
            qjs_intl_date_data_i64(s, i, 16, &from) || qjs_intl_date_data_i64(s, i, 24, &before) || from >= before) return 0;
        c = compare(zone, previous_zone);
        if (i && (c < 0 || (!c && from < previous_before))) return 0;
        previous_zone = zone; previous_before = before;
    }
    return 1;
}
static int aliases(const QJSIntlDataView *v, const QJSIntlDataSection *s)
{
    uint32_t i;
    QJSIntlDataSlice key, target, previous = { NULL, 0 };
    for (i = 0; i < s->record_count; i++) {
        if (!string(v, s, i, 0, &key) || !identifier(key, 0) ||
            !string(v, s, i, 8, &target) || !identifier(target, 0) ||
            (i && compare(key, previous) <= 0)) return 0;
        previous = key;
    }
    return 1;
}
static int range_fallbacks(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                           uint32_t locales)
{
    uint32_t i, locale, previous_locale = 0;
    QJSIntlDataSlice row, calendar, fallback, previous_calendar = { NULL, 0 };
    for (i = 0; i < s->record_count; i++) {
        QJSIntlBytes pattern;
        int c;
        if (!common(s, i, locales, &locale, &row) || row.length != 20 ||
            !string(v, s, i, 4, &calendar) || !identifier(calendar, 1) ||
            !string(v, s, i, 12, &fallback)) return 0;
        pattern.data = (const char *)fallback.data; pattern.length = fallback.length;
        if (qjs_intl_date_range_template_visit(pattern, NULL, NULL)) return 0;
        c = locale < previous_locale ? -1 : locale > previous_locale ? 1 : compare(calendar, previous_calendar);
        if (i && c <= 0) return 0;
        previous_locale = locale; previous_calendar = calendar;
    }
    return 1;
}
static int zone_formats(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                         uint32_t locales)
{
    uint32_t i, locale, previous_locale = 0;
    QJSIntlDataSlice row, zone, meta, location, pattern;
    QJSIntlDataSlice previous_zone = { NULL, 0 }, previous_meta = { NULL, 0 };
    for (i = 0; i < s->record_count; i++) {
        int c;
        if (!common(s, i, locales, &locale, &row) || row.length != 36 ||
            !string(v, s, i, 4, &zone) || !identifier(zone, 0) ||
            !string(v, s, i, 12, &meta) || (meta.length && !identifier(meta, 0)) ||
            !string(v, s, i, 20, &location) ||
            !string(v, s, i, 28, &pattern) ||
            (pattern.length && (!meta.length || !template_valid(pattern, 1, 0)))) return 0;
        c = locale < previous_locale ? -1 : locale > previous_locale ? 1 : compare(zone, previous_zone);
        if (!c) c = compare(meta, previous_meta);
        if (i && c <= 0) return 0;
        previous_locale = locale; previous_zone = zone; previous_meta = meta;
    }
    return 1;
}
QJSIntlDataStatus qjs_intl_date_data_validate(const QJSIntlDataView *v)
{
    static const uint32_t widths[] = { 32, 28, 48, 16, 72, 32, 16, 20, 36 };
    QJSIntlDataSection s[9], locales, numbering;
    QJSIntlDataStatus r;
    unsigned int i, mask = 0;
    if (!v) return QJS_INTL_DATA_INVALID_ARGUMENT;
    memset(s, 0, sizeof(s));
    for (i = 0; i < 9; i++) {
        r = qjs_intl_data_section(v, 100 + i, &s[i]);
        if (r == QJS_INTL_DATA_NOT_FOUND) continue;
        if (r || s[i].record_width != widths[i]) return QJS_INTL_DATA_INVALID;
        mask |= 1u << i;
    }
    if (!mask) return QJS_INTL_DATA_OK;
    if ((mask & 7) != 7 || !s[0].record_count || !s[1].record_count || !s[2].record_count ||
        qjs_intl_data_section(v, 16, &locales) || qjs_intl_data_section(v, 17, &numbering)) return QJS_INTL_DATA_INVALID;
    if (!patterns(v, &s[0], locales.record_count) || !names(v, &s[1], locales.record_count) ||
        !symbols(v, &s[2], locales.record_count, &numbering) ||
        ((mask & 8) && !rules(&s[3], locales.record_count)) ||
        ((mask & 16) && !zones(v, &s[4], locales.record_count)) ||
        ((mask & 32) && !metazones(v, &s[5])) || ((mask & 64) && !aliases(v, &s[6])) ||
        ((mask & 128) && !range_fallbacks(v, &s[7], locales.record_count)) ||
        ((mask & 256) && !zone_formats(v, &s[8], locales.record_count))) return QJS_INTL_DATA_INVALID;
    return QJS_INTL_DATA_OK;
}
