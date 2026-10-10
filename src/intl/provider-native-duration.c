/* DurationFormat adapter for the admitted en/en-US development profile.
 * Copyright (c) 2026 Yan-Jie Wang. No ICU, global state or binary owner. */
#include "provider-native-duration.h"
#include <string.h>
static const char *const duration_unit_names[] = {
    "year", "month", "week", "day", "hour", "minute", "second",
    "millisecond", "microsecond", "nanosecond"
};
static int same(QJSIntlBytes a, const char *s)
{
    return a.data && a.length == strlen(s) && !memcmp(a.data, s, a.length);
}
static QJSIntlStatus named_index(const QJSIntlDataView *v, uint32_t id,
                                 QJSIntlBytes name, uint32_t *out)
{
    QJSIntlDataSection section;
    QJSIntlDataSlice tag;
    uint32_t i;
    QJSIntlDataStatus status = qjs_intl_data_section(v, id, &section);
    if (status == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (status != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < section.record_count; i++) {
        if (qjs_intl_data_record_string(v, &section, i, 0, &tag) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (tag.length == name.length && !memcmp(tag.data, name.data, name.length)) {
            *out = i; return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
static QJSIntlStatus indices(QJSIntlProvider *p, QJSIntlBytes locale,
                             QJSIntlBytes numbering, uint32_t *li, uint32_t *ni)
{
    QJSIntlStatus status;
    const QJSIntlDataView *v;
    if (!p || !locale.data || !numbering.data || !numbering.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!same(locale, "en") && !same(locale, "en-US")) return QJS_INTL_UNSUPPORTED;
    v = qjs_intl_native_provider_view(p);
    if (!v) return QJS_INTL_INVALID_ARGUMENT;
    status = named_index(v, QJS_INTL_DATA_LOCALE, (QJSIntlBytes){"en", 2}, li);
    if (status != QJS_INTL_OK) return status;
    return named_index(v, QJS_INTL_DATA_NUMBERING, numbering, ni);
}
QJSIntlStatus qjs_intl_native_provider_duration_clock(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes numbering, uint8_t *two)
{
    uint32_t li, ni;
    QJSIntlBytes separators[2];
    QJSIntlStatus status;
    if (!two) return QJS_INTL_INVALID_ARGUMENT;
    *two = 0;
    status = indices(p, locale, numbering, &li, &ni);
    if (status != QJS_INTL_OK) return status;
    return qjs_intl_native_duration_clock_data(qjs_intl_native_provider_view(p),
                                              li, ni, two, separators);
}
QJSIntlStatus qjs_intl_native_provider_duration_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes numbering, const QJSIntlDurationOptions *o,
    QJSIntlNativeDuration **out)
{
    uint32_t li, ni;
    QJSIntlStatus status;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!o) return QJS_INTL_INVALID_ARGUMENT;
    status = indices(p, locale, numbering, &li, &ni);
    if (status != QJS_INTL_OK) return status;
    return qjs_intl_native_duration_open_data(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p), li, ni, o, out);
}
/* Validate all ten textual units and three list widths from reader-validated
 * records. This is an availability dependency gate, not a conformance receipt.
 * It avoids twenty complete Number snapshots merely for locale discovery. */
QJSIntlStatus qjs_intl_native_provider_duration_available(QJSIntlProvider *p)
{
    const QJSIntlDataView *v;
    QJSIntlDataSection section;
    QJSIntlDataSlice row, name;
    QJSIntlStatus status;
    uint32_t li, ni, i, locale;
    uint8_t two, lists = 0, units[10] = {0};
    size_t unit;
    status = indices(p, (QJSIntlBytes){"en", 2}, (QJSIntlBytes){"latn", 4}, &li, &ni);
    if (status != QJS_INTL_OK) return status;
    status = qjs_intl_native_provider_duration_clock(p, (QJSIntlBytes){"en", 2},
                                                     (QJSIntlBytes){"latn", 4}, &two);
    if (status != QJS_INTL_OK) return status;
    v = qjs_intl_native_provider_view(p);
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_UNIT, &section) == QJS_INTL_DATA_NOT_FOUND)
        return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBER_UNIT, &section) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < section.record_count; i++) {
        if (qjs_intl_data_record_u32(&section, i, 0, &locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&section, i, &row) != QJS_INTL_DATA_OK || row.length != 64)
            return QJS_INTL_DATA_ERROR;
        if (locale != li) continue;
        if (qjs_intl_data_record_string(v, &section, i, 4, &name) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        for (unit = 0; unit < 10; unit++)
            if (same((QJSIntlBytes){(const char *)name.data, name.length}, duration_unit_names[unit]))
                units[unit] |= (uint8_t)(1u << row.data[12]);
    }
    for (unit = 0; unit < 10; unit++) if (units[unit] != 7) return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_LIST_PATTERN, &section) == QJS_INTL_DATA_NOT_FOUND)
        return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_LIST_PATTERN, &section) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < section.record_count; i++) {
        if (qjs_intl_data_record_u32(&section, i, 0, &locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&section, i, &row) != QJS_INTL_DATA_OK || row.length != 40)
            return QJS_INTL_DATA_ERROR;
        if (locale == li && row.data[4] == QJS_INTL_LIST_UNIT && !row.data[6])
            lists |= (uint8_t)(1u << row.data[5]);
    }
    if (lists != 7) return QJS_INTL_UNSUPPORTED;
    return qjs_intl_native_provider_number_available(p);
}
/* Keep Number's actual supported numeric defaults and exclude missing clocks.
 * Compaction transfers owned strings without another allocation. */
QJSIntlStatus qjs_intl_native_provider_duration_key_values(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *out)
{
    QJSIntlStatus status;
    const QJSIntlAllocator *a;
    size_t i, count = 0;
    uint8_t two;
    status = qjs_intl_native_provider_number_key_values(p, locale, key, out);
    if (status != QJS_INTL_OK) return status;
    a = qjs_intl_native_provider_allocator(p);
    for (i = 0; i < out->count; i++) {
        status = qjs_intl_native_provider_duration_clock(p, locale, out->items[i], &two);
        if (status == QJS_INTL_OK) out->items[count++] = out->items[i];
        else if (status == QJS_INTL_UNSUPPORTED) a->free(a->opaque, (void *)out->items[i].data);
        else {
            size_t j;
            for (j = i; j < out->count; j++) a->free(a->opaque, (void *)out->items[j].data);
            out->count = count;
            qjs_intl_tag_list_clear(p, out);
            return status;
        }
    }
    out->count = count;
    if (!count || !same(out->items[0], "latn")) {
        qjs_intl_tag_list_clear(p, out); return QJS_INTL_UNSUPPORTED;
    }
    return QJS_INTL_OK;
}
