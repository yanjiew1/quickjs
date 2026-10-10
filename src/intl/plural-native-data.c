/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "plural-native-data.h"
#include <string.h>

QJSIntlStatus qjs_intl_native_plural_open_data(const QJSIntlAllocator *a,
    const QJSIntlDataView *view, uint32_t locale_index,
    const QJSIntlPluralOptions *options, QJSIntlNativePlural **out)
{
    QJSIntlDataSection locales, bindings, rules, ranges;
    QJSIntlDataSlice row, text;
    QJSIntlDataStatus status;
    QJSIntlPluralRule selected_rules[6];
    QJSIntlPluralRange selected_ranges[36];
    QJSIntlPluralRulesData data;
    uint32_t lo = 0, hi, index, first, count, i, binding;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !view || !options ||
        (unsigned int)options->type > QJS_INTL_PLURAL_ORDINAL ||
        qjs_intl_decimal_options_validate(&options->digits) != QJS_INTL_OK)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_data_validate_plural_extension(view);
    if (status != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    status = qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_LOCALE, &bindings);
    if (status == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (status != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_RULE, &rules) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_RANGE, &ranges) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (locale_index >= locales.record_count) return QJS_INTL_INVALID_ARGUMENT;
    hi = bindings.record_count;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (qjs_intl_data_record_u32(&bindings, middle, 0, &index) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&bindings, middle, &row) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (index < locale_index || (index == locale_index && row.data[4] < options->type))
            lo = middle + 1;
        else hi = middle;
    }
    if (lo == bindings.record_count) return QJS_INTL_UNSUPPORTED;
    binding = lo;
    if (qjs_intl_data_record_u32(&bindings, binding, 0, &index) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record(&bindings, binding, &row) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (index != locale_index || row.data[4] != options->type) return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_record_u32(&bindings, binding, 8, &first) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record_u32(&bindings, binding, 12, &count) != QJS_INTL_DATA_OK || count > 6)
        return QJS_INTL_DATA_ERROR;
    memset(&data, 0, sizeof(data));
    data.rules = selected_rules; data.rule_count = count;
    for (i = 0; i < count; i++) {
        if (qjs_intl_data_record(&rules, first + i, &row) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_string(view, &rules, first + i, 4, &text) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        selected_rules[i].category = (QJSIntlPluralCategory)row.data[0];
        selected_rules[i].relation.data = (const char *)text.data;
        selected_rules[i].relation.length = text.length;
    }
    if (qjs_intl_data_record_u32(&bindings, binding, 16, &first) != QJS_INTL_DATA_OK ||
        qjs_intl_data_record_u32(&bindings, binding, 20, &count) != QJS_INTL_DATA_OK || count > 36)
        return QJS_INTL_DATA_ERROR;
    data.ranges = selected_ranges; data.range_count = count;
    for (i = 0; i < count; i++) {
        if (qjs_intl_data_record(&ranges, first + i, &row) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        selected_ranges[i].start = (QJSIntlPluralCategory)row.data[0];
        selected_ranges[i].end = (QJSIntlPluralCategory)row.data[1];
        selected_ranges[i].result = (QJSIntlPluralCategory)row.data[2];
    }
    return qjs_intl_native_plural_open(a, options, &data, out);
}
