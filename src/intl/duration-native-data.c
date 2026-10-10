/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "duration-native-data.h"
#include "number-native-data.h"
#include "list-native-data.h"
#include "data/duration-data-validation.h"
#include <string.h>
QJSIntlStatus qjs_intl_native_duration_clock_data(const QJSIntlDataView *v,
    uint32_t locale, uint32_t system, uint8_t *two, QJSIntlBytes separators[2])
{
    QJSIntlDataSection section, locales, systems;
    QJSIntlDataSlice row, text;
    QJSIntlDataStatus status;
    uint32_t lo = 0, hi, a, b, i;
    if (two) *two = 0;
    if (separators) memset(separators, 0, 2 * sizeof(*separators));
    if (!v || !two || !separators) return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_data_section(v, QJS_INTL_DATA_DURATION_CLOCK, &section);
    if (status == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (status != QJS_INTL_DATA_OK || section.record_width != 32 ||
        qjs_intl_data_section(v, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &systems) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (locale >= locales.record_count || system >= systems.record_count)
        return QJS_INTL_INVALID_ARGUMENT;
    hi = section.record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi-lo) / 2;
        if (qjs_intl_data_record_u32(&section, mid, 0, &a) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&section, mid, 4, &b) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (a < locale || (a == locale && b < system)) lo = mid + 1;
        else hi = mid;
    }
    if (lo == section.record_count) return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_record_u32(&section, lo, 0, &a) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record_u32(&section, lo, 4, &b) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record(&section, lo, &row) != QJS_INTL_DATA_OK || row.length != 32)
        return QJS_INTL_DATA_ERROR;
    if (a != locale || b != system) return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < 2; i++) {
        if (qjs_intl_data_record_string(v, &section, lo, 16 + 8 * i, &text) != QJS_INTL_DATA_OK) {
            memset(separators, 0, 2 * sizeof(*separators)); return QJS_INTL_DATA_ERROR;
        }
        separators[i].data = (const char *)text.data; separators[i].length = text.length;
    }
    *two = row.data[8]; return QJS_INTL_OK;
}
typedef struct OpenContext { const QJSIntlDataView *view; uint32_t locale, system; } OpenContext;
static QJSIntlStatus open_number(void *opaque, const QJSIntlAllocator *a,
    const QJSIntlNumberOptions *options, QJSIntlNativeNumber **out)
{
    OpenContext *c = opaque;
    return qjs_intl_native_number_open_data(a, c->view, c->locale, c->system, options, out);
}
static void close_number(void *opaque, QJSIntlNativeNumber *n)
{
    (void)opaque; qjs_intl_native_number_close(n);
}
static QJSIntlStatus open_list(void *opaque, const QJSIntlAllocator *a,
    QJSIntlListStyle style, QJSIntlNativeList **out)
{
    OpenContext *c = opaque;
    return qjs_intl_native_list_open_data(a, c->view, c->locale, QJS_INTL_LIST_UNIT, style, out);
}
/* Scalar UTF8 decoding for the two clock literal slices, not localized digit
 * formatting. Owner gate has already validated bytes and scalar ranges. */
static QJSIntlStatus decode(const QJSIntlAllocator *a, QJSIntlBytes s,
                            size_t bound, QJSIntlUTF16 *out)
{
    size_t i = 0, count = 0;
    uint16_t *p;
    memset(out, 0, sizeof(*out));
    if (s.length > SIZE_MAX / sizeof(*p)) return QJS_INTL_OVERFLOW;
    p = a->malloc(a->opaque, s.length * sizeof(*p));
    if (!p) return QJS_INTL_NO_MEMORY;
    while (i < s.length) {
        uint32_t c = (unsigned char)s.data[i++]; unsigned n, j;
        if (c < 0x80) n = 0;
        else if (c < 0xe0) { c &= 31; n = 1; }
        else if (c < 0xf0) { c &= 15; n = 2; }
        else { c &= 7; n = 3; }
        for (j = 0; j < n; j++) c = c << 6 | ((unsigned char)s.data[i++] & 63);
        if (c < 0x10000) p[count++] = (uint16_t)c;
        else { c -= 0x10000; p[count++] = (uint16_t)(0xd800 + (c >> 10)); p[count++] = (uint16_t)(0xdc00 + (c & 1023)); }
    }
    if (count > bound) { a->free(a->opaque, p); return QJS_INTL_OVERFLOW; }
    out->data = p; out->length = count; return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_duration_open_data(const QJSIntlAllocator *a,
    const QJSIntlDataView *v, uint32_t locale, uint32_t system,
    const QJSIntlDurationOptions *o, QJSIntlNativeDuration **out)
{
    OpenContext c;
    QJSIntlDurationData data;
    QJSIntlBytes separators[2];
    QJSIntlUTF16 decoded[2] = {{0}};
    uint8_t two;
    QJSIntlStatus s;
    size_t i;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !v || !o) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_duration_data_validate(v) != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    s = qjs_intl_native_duration_clock_data(v, locale, system, &two, separators);
    if (s != QJS_INTL_OK) return s;
    if (two && o->units[4].style == QJS_INTL_DURATION_NUMERIC) return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < 2; i++) {
        s = decode(a, separators[i], o->maximum_output_length, &decoded[i]);
        if (s != QJS_INTL_OK) goto done;
    }
    c.view = v; c.locale = locale; c.system = system;
    memset(&data, 0, sizeof(data));
    data.number.open_opaque = &c; data.number.close_opaque = NULL;
    data.number.open = open_number; data.number.close = close_number;
    data.list_open_opaque = &c; data.open_list = open_list;
    data.hour_minute_separator = decoded[0]; data.minute_second_separator = decoded[1];
    s = qjs_intl_native_duration_open(a, o, &data, out);
done:
    for (i = 0; i < 2; i++) if (decoded[i].data) a->free(a->opaque, (void *)decoded[i].data);
    return s;
}
