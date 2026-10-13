/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "list-native-data.h"
#include <string.h>

static int key_compare(const QJSIntlDataSection *section, uint32_t index,
                       uint32_t locale, unsigned int type, unsigned int style,
                       unsigned int context, int *comparison)
{
    uint32_t key;
    QJSIntlDataSlice row;
    if (qjs_intl_data_record_u32(section, index, 0, &key) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record(section, index, &row) != QJS_INTL_DATA_OK ||
        row.length != 40)
        return 0;
    *comparison = key < locale ? -1 : key > locale ? 1 :
        row.data[4] < type ? -1 : row.data[4] > type ? 1 :
        row.data[5] < style ? -1 : row.data[5] > style ? 1 :
        row.data[6] < context ? -1 : row.data[6] > context ? 1 : 0;
    return 1;
}
QJSIntlStatus qjs_intl_native_list_open_data(const QJSIntlAllocator *a,
                                           const QJSIntlDataView *view,
                                           uint32_t locale_index,
                                           QJSIntlListType type,
                                           QJSIntlListStyle style,
                                           QJSIntlNativeList **out)
{
    QJSIntlDataSection section, locales, scripts;
    QJSIntlDataSlice row, text;
    QJSIntlDataStatus r;
    QJSIntlListTemplates templates;
    QJSIntlListScriptRange *ranges = NULL;
    QJSIntlStatus status;
    uint32_t lo, hi, i, base_index;
    int comparison;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !view || (unsigned int)type > 2 ||
        (unsigned int)style > 2)
        return QJS_INTL_INVALID_ARGUMENT;
    r = qjs_intl_data_section(view, QJS_INTL_DATA_LIST_PATTERN, &section);
    if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (r != QJS_INTL_DATA_OK || section.record_width != 40 ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (locale_index >= locales.record_count) return QJS_INTL_INVALID_ARGUMENT;
    lo = 0;
    hi = section.record_count;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (!key_compare(&section, middle, locale_index, type, style, 0, &comparison))
            return QJS_INTL_DATA_ERROR;
        if (comparison < 0) lo = middle + 1;
        else hi = middle;
    }
    if (lo == section.record_count) return QJS_INTL_UNSUPPORTED;
    if (!key_compare(&section, lo, locale_index, type, style, 0, &comparison))
        return QJS_INTL_DATA_ERROR;
    if (comparison) return QJS_INTL_UNSUPPORTED;
    memset(&templates, 0, sizeof(templates));
    base_index = lo;
    for (i = 0; i < 4; i++) {
        if (qjs_intl_data_record_string(view, &section, base_index, 8 + i * 8,
                                        &text) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        templates.base[i].data = (const char *)text.data;
        templates.base[i].length = text.length;
    }
    if (lo + 1 < section.record_count) {
        uint32_t next_locale;
        if (qjs_intl_data_record_u32(&section, lo + 1, 0, &next_locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&section, lo + 1, &row) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (next_locale == locale_index && row.data[4] == type && row.data[5] == style) {
            if (row.data[6] < 1 || row.data[6] > 3 || row.data[7])
                return QJS_INTL_DATA_ERROR;
            templates.context = (QJSIntlListContext)row.data[6];
            for (i = 0; i < 4; i++) {
                if (qjs_intl_data_record_string(view, &section, lo + 1, 8 + i * 8,
                                                &text) != QJS_INTL_DATA_OK)
                    return QJS_INTL_DATA_ERROR;
                templates.alternate[i].data = (const char *)text.data;
                templates.alternate[i].length = text.length;
            }
        }
    }
    if (templates.context == QJS_INTL_LIST_CONTEXT_HEBREW_AND) {
        size_t bytes, range_count;
        r = qjs_intl_data_section(view, QJS_INTL_DATA_LIST_HEBREW_SCRIPT, &scripts);
        if (r == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
        if (r != QJS_INTL_DATA_OK || scripts.record_width != 8 || !scripts.record_count)
            return QJS_INTL_DATA_ERROR;
        range_count = scripts.record_count;
        if (range_count > SIZE_MAX / sizeof(*ranges)) return QJS_INTL_OVERFLOW;
        bytes = range_count * sizeof(*ranges);
        ranges = a->malloc(a->opaque, bytes);
        if (!ranges) return QJS_INTL_NO_MEMORY;
        for (i = 0; i < scripts.record_count; i++) {
            if (qjs_intl_data_record_u32(&scripts, i, 0, &ranges[i].first) != QJS_INTL_DATA_OK ||
                qjs_intl_data_record_u32(&scripts, i, 4, &ranges[i].last) != QJS_INTL_DATA_OK) {
                a->free(a->opaque, ranges);
                return QJS_INTL_DATA_ERROR;
            }
        }
        templates.hebrew_script = ranges;
        templates.hebrew_script_count = scripts.record_count;
    }
    status = qjs_intl_native_list_open(a, &templates, out);
    if (ranges) a->free(a->opaque, ranges);
    return status;
}
