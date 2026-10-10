/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "duration-data-validation.h"
static int scalar_utf8(QJSIntlDataSlice s)
{
    size_t i = 0;
    if (!s.data || !s.length) return 0;
    while (i < s.length) {
        uint32_t c = s.data[i++]; unsigned n, j;
        if (c < 0x80) n = 0;
        else if (c >= 0xc2 && c <= 0xdf) { c &= 31; n = 1; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; n = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; n = 3; }
        else return 0;
        if (n > s.length - i) return 0;
        for (j = 0; j < n; j++) {
            unsigned b = s.data[i++];
            if ((b & 0xc0) != 0x80) return 0;
            c = c << 6 | (b & 63);
        }
        if (!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff) ||
            (n == 1 && c < 0x80) || (n == 2 && c < 0x800) || (n == 3 && c < 0x10000)) return 0;
    }
    return 1;
}
QJSIntlDataStatus qjs_intl_duration_data_validate(const QJSIntlDataView *view)
{
    QJSIntlDataSection clocks, locales, systems;
    QJSIntlDataSlice row, system, text;
    QJSIntlDataStatus s;
    uint32_t i, locale, numbering, prev_locale = 0, prev_numbering = 0;
    size_t j;
    if (!view) return QJS_INTL_DATA_INVALID_ARGUMENT;
    s = qjs_intl_data_section(view, QJS_INTL_DATA_DURATION_CLOCK, &clocks);
    if (s == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_DATA_OK;
    if (s != QJS_INTL_DATA_OK || clocks.record_width != 32 ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_NUMBERING, &systems) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_INVALID;
    for (i = 0; i < clocks.record_count; i++) {
        if (qjs_intl_data_record(&clocks, i, &row) != QJS_INTL_DATA_OK || row.length != 32 ||
            qjs_intl_data_record_u32(&clocks, i, 0, &locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&clocks, i, 4, &numbering) != QJS_INTL_DATA_OK ||
            locale >= locales.record_count || numbering >= systems.record_count || row.data[8] > 1 ||
            qjs_intl_data_record(&systems, numbering, &system) != QJS_INTL_DATA_OK ||
            system.length != 52 || system.data[8] != 10 || system.data[9]) return QJS_INTL_DATA_INVALID;
        if (i && (locale < prev_locale || (locale == prev_locale && numbering <= prev_numbering)))
            return QJS_INTL_DATA_INVALID;
        prev_locale = locale; prev_numbering = numbering;
        for (j = 9; j < 16; j++) if (row.data[j]) return QJS_INTL_DATA_INVALID;
        for (j = 16; j <= 24; j += 8) {
            if (qjs_intl_data_record_string(view, &clocks, i, (uint32_t)j, &text) != QJS_INTL_DATA_OK ||
                !scalar_utf8(text)) return QJS_INTL_DATA_INVALID;
        }
    }
    return QJS_INTL_DATA_OK;
}
