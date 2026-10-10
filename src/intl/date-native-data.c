/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-native-data.h"
#include "date-native-bank.h"
#include "data/date-data-validation.h"
#include <string.h>

static QJSIntlStatus section(const QJSIntlDataView *v, uint32_t id,
                             uint32_t width, int required, QJSIntlDataSection *out)
{
    QJSIntlDataStatus r = qjs_intl_data_section(v, id, out);
    if (r == QJS_INTL_DATA_NOT_FOUND) {
        memset(out, 0, sizeof(*out));
        return required ? QJS_INTL_UNSUPPORTED : QJS_INTL_OK;
    }
    return !r && out->record_width == width ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}
static int text(const QJSIntlDataView *v, const QJSIntlDataSection *s,
                 uint32_t row, uint32_t offset, QJSIntlBytes *out)
{
    QJSIntlDataSlice slice;
    if (qjs_intl_data_record_string(v, s, row, offset, &slice)) return 0;
    out->data = (const char *)slice.data; out->length = slice.length;
    return 1;
}
static int same(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}
static int span(const QJSIntlDataSection *s, uint32_t locale,
                 uint32_t *first, uint32_t *before)
{
    uint32_t lo = 0, hi = s->record_count, key;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (qjs_intl_data_record_u32(s, middle, 0, &key)) return 0;
        if (key < locale) lo = middle + 1; else hi = middle;
    }
    *first = lo; hi = s->record_count;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (qjs_intl_data_record_u32(s, middle, 0, &key)) return 0;
        if (key <= locale) lo = middle + 1; else hi = middle;
    }
    *before = lo; return 1;
}
static void *allocate(const QJSIntlAllocator *a, size_t count, size_t width)
{
    if (!count || count > SIZE_MAX / width) return NULL;
    return a->malloc(a->opaque, count * width);
}
static QJSIntlStatus decode_open(const QJSIntlAllocator *a,
    const QJSIntlDataView *v, uint32_t locale, uint32_t numbering,
    const QJSIntlDateOptions *o, const QJSIntlDateEnvironment *e, QJSIntlNativeDate **out,
    const QJSIntlDateBankOptions *bank_options, QJSIntlNativeDateBank **bank_out)
{
    QJSIntlDataSection s[8], locales, digits;
    QJSIntlDataSlice row;
    QJSIntlDateData d;
    QJSIntlDatePattern *patterns = NULL;
    QJSIntlDateName *names = NULL;
    QJSIntlDatePeriodRule *periods = NULL;
    QJSIntlDateZoneName *zones = NULL;
    QJSIntlDateMetaPeriod *meta = NULL;
    QJSIntlStatus result;
    QJSIntlBytes calendar, key;
    QJSCalendarId calendar_id;
    const char *canonical;
    uint32_t begin[5], end[5], i, j;
    static const uint32_t widths[] = { 32, 28, 48, 16, 72, 32, 16, 20 };
    if (bank_options) {
        if (!bank_out) return QJS_INTL_INVALID_ARGUMENT;
        *bank_out = NULL;
    } else {
        if (!out) return QJS_INTL_INVALID_ARGUMENT;
        *out = NULL;
    }
    if (!a || !a->malloc || !a->free || !v || !o || !o->calendar.data ||
        !o->calendar.length || !o->time_zone.data || !o->time_zone.length) return QJS_INTL_INVALID_ARGUMENT;
    memset(&d, 0, sizeof(d));
    for (i = 0; i < 8; i++) {
        result = section(v, 100 + i, widths[i], i < 3, &s[i]);
        if (result) return result;
    }
    if (qjs_intl_data_section(v, 16, &locales) || qjs_intl_data_section(v, 17, &digits)) return QJS_INTL_DATA_ERROR;
    if (locale >= locales.record_count || numbering >= digits.record_count) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_data_record(&digits, numbering, &row) || row.length != 52 || row.data[8] != 10) return QJS_INTL_DATA_ERROR;
    if (row.data[9]) return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < 10; i++)
        if (qjs_intl_data_record_u32(&digits, numbering, 12 + i * 4, &d.digits[i])) return QJS_INTL_DATA_ERROR;
    if (qjs_calendar_from_identifier(&calendar_id, o->calendar.data, o->calendar.length)) return QJS_INTL_UNSUPPORTED;
    canonical = qjs_calendar_identifier(calendar_id);
    calendar.data = canonical; calendar.length = strlen(canonical);
    for (i = 0; i < 5; i++)
        if (!span(&s[i], locale, &begin[i], &end[i])) return QJS_INTL_DATA_ERROR;
    if (calendar_id == QJS_CAL_ISO8601) {
        int present = 0;
        for (i = begin[0]; i < end[0]; i++) {
            if (!text(v, &s[0], i, 4, &key)) return QJS_INTL_DATA_ERROR;
            if (same(key, calendar)) present = 1;
        }
        if (!present) { calendar.data = "gregory"; calendar.length = 7; }
    }
    for (i = begin[0]; i < end[0]; i++) {
        if (!text(v, &s[0], i, 4, &key)) return QJS_INTL_DATA_ERROR;
        if (same(key, calendar)) d.pattern_count++;
    }
    for (i = begin[1]; i < end[1]; i++) {
        if (!text(v, &s[1], i, 4, &key)) return QJS_INTL_DATA_ERROR;
        if (same(key, calendar)) d.name_count++;
    }
    if (!d.pattern_count) return QJS_INTL_UNSUPPORTED;
    {
        uint32_t first, before;
        if (!span(&s[7], locale, &first, &before)) return QJS_INTL_DATA_ERROR;
        for (i = first; i < before; i++) {
            if (!text(v, &s[7], i, 4, &key)) return QJS_INTL_DATA_ERROR;
            if (same(key, calendar)) {
                if (!text(v, &s[7], i, 12, &d.range_fallback)) return QJS_INTL_DATA_ERROR;
                break;
            }
        }
    }
    for (i = begin[2]; i < end[2]; i++) {
        uint32_t n;
        if (qjs_intl_data_record_u32(&s[2], i, 4, &n)) return QJS_INTL_DATA_ERROR;
        if (n == numbering) {
            if (!text(v, &s[2], i, 8, &d.decimal) || !text(v, &s[2], i, 16, &d.gmt_format) ||
                !text(v, &s[2], i, 24, &d.gmt_zero) || !text(v, &s[2], i, 32, &d.hour_positive) ||
                !text(v, &s[2], i, 40, &d.hour_negative)) return QJS_INTL_DATA_ERROR;
            break;
        }
    }
    if (!d.decimal.length) return QJS_INTL_UNSUPPORTED;
    d.period_count = end[3] - begin[3]; d.zone_name_count = end[4] - begin[4];
    d.meta_period_count = s[5].record_count; d.data_zone = o->time_zone;
    /* Alias targets are final CLDR keys, not recursive aliases. */
    for (i = 0; i < s[6].record_count; i++) {
        if (!text(v, &s[6], i, 0, &key)) return QJS_INTL_DATA_ERROR;
        if (same(key, o->time_zone)) {
            if (!text(v, &s[6], i, 8, &d.data_zone)) return QJS_INTL_DATA_ERROR;
            break;
        }
    }
    if (d.pattern_count > SIZE_MAX / sizeof(*patterns) || d.name_count > SIZE_MAX / sizeof(*names) ||
        d.period_count > SIZE_MAX / sizeof(*periods) || d.zone_name_count > SIZE_MAX / sizeof(*zones) ||
        d.meta_period_count > SIZE_MAX / sizeof(*meta)) return QJS_INTL_OVERFLOW;
    patterns = allocate(a, d.pattern_count, sizeof(*patterns));
    names = allocate(a, d.name_count, sizeof(*names)); periods = allocate(a, d.period_count, sizeof(*periods));
    zones = allocate(a, d.zone_name_count, sizeof(*zones)); meta = allocate(a, d.meta_period_count, sizeof(*meta));
    result = QJS_INTL_NO_MEMORY;
    if (!patterns || (d.name_count && !names) || (d.period_count && !periods) ||
        (d.zone_name_count && !zones) || (d.meta_period_count && !meta)) goto done;
    result = QJS_INTL_DATA_ERROR;
    j = 0;
    for (i = begin[0]; i < end[0]; i++) {
        if (!text(v, &s[0], i, 4, &key)) goto done;
        if (!same(key, calendar)) continue;
        if (qjs_intl_data_record(&s[0], i, &row) || row.length != 32) goto done;
        patterns[j].kind = row.data[12]; patterns[j].style = row.data[13]; patterns[j].family = row.data[14];
        if (!text(v, &s[0], i, 16, &patterns[j].skeleton) || !text(v, &s[0], i, 24, &patterns[j].pattern)) goto done;
        j++;
    }
    j = 0;
    for (i = begin[1]; i < end[1]; i++) {
        if (!text(v, &s[1], i, 4, &key)) goto done;
        if (!same(key, calendar)) continue;
        if (qjs_intl_data_record(&s[1], i, &row) || row.length != 28) goto done;
        names[j].field = row.data[12]; names[j].context = row.data[13]; names[j].width = row.data[14];
        {
            uint32_t index;
            if (qjs_intl_data_record_u32(&s[1], i, 16, &index)) goto done;
            names[j].index = index;
        }
        if (!text(v, &s[1], i, 20, &names[j].name)) goto done;
        j++;
    }
    for (i = begin[3], j = 0; i < end[3]; i++, j++) {
        uint32_t from, before;
        if (qjs_intl_data_record(&s[3], i, &row) || row.length != 16 ||
            qjs_intl_data_record_u32(&s[3], i, 8, &from) || qjs_intl_data_record_u32(&s[3], i, 12, &before)) goto done;
        periods[j].period = row.data[4]; periods[j].exact = row.data[5];
        periods[j].from_second = from; periods[j].before_second = before;
    }
    for (i = begin[4], j = 0; i < end[4]; i++, j++) {
        uint32_t n;
        if (qjs_intl_data_record(&s[4], i, &row) || row.length != 72) goto done;
        zones[j].metazone = row.data[4];
        if (!text(v, &s[4], i, 8, &zones[j].key) || !text(v, &s[4], i, 64, &zones[j].exemplar)) goto done;
        for (n = 0; n < 6; n++) if (!text(v, &s[4], i, 16 + n * 8, &zones[j].names[n])) goto done;
    }
    for (i = 0; i < s[5].record_count; i++)
        if (!text(v, &s[5], i, 0, &meta[i].zone) || !text(v, &s[5], i, 8, &meta[i].metazone) ||
            qjs_intl_date_data_i64(&s[5], i, 16, &meta[i].from_ms) ||
            qjs_intl_date_data_i64(&s[5], i, 24, &meta[i].before_ms)) goto done;
    d.patterns = patterns; d.names = names; d.periods = periods; d.zone_names = zones; d.meta_periods = meta;
    /* All seven handles copy the same decoded snapshot before release. */
    if (bank_options)
        result = qjs_intl_native_date_bank_open(a, &d, bank_options, e, bank_out);
    else
        result = qjs_intl_native_date_open(a, &d, o, e, out);
done:
    a->free(a->opaque, patterns); a->free(a->opaque, names); a->free(a->opaque, periods);
    a->free(a->opaque, zones); a->free(a->opaque, meta);
    return result;
}

QJSIntlStatus qjs_intl_native_date_open_data(const QJSIntlAllocator *a,
    const QJSIntlDataView *v, uint32_t locale, uint32_t numbering,
    const QJSIntlDateOptions *o, const QJSIntlDateEnvironment *e,
    QJSIntlNativeDate **out)
{
    return decode_open(a, v, locale, numbering, o, e, out, NULL, NULL);
}
QJSIntlStatus qjs_intl_native_date_bank_open_data(const QJSIntlAllocator *a,
    const QJSIntlDataView *v, uint32_t locale, uint32_t numbering,
    const QJSIntlDateBankOptions *o, const QJSIntlDateEnvironment *e,
    QJSIntlNativeDateBank **out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!o) return QJS_INTL_INVALID_ARGUMENT;
    return decode_open(a, v, locale, numbering, &o->requested, e, NULL, o, out);
}
