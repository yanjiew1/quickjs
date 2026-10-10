/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "relative-native-data.h"
#include "data/plural-data-schema.h"
#include <string.h>

static int32_t signed_u32(uint32_t n)
{
    return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
}
QJSIntlStatus qjs_intl_native_relative_open_data(const QJSIntlAllocator *a,
    const QJSIntlDataView *view, uint32_t locale_index,
    const QJSIntlRelativeOptions *options, const QJSIntlRelativeNumberBridge *bridge,
    QJSIntlNativeRelative **out)
{
    QJSIntlDataSection patterns, literals, bindings, rules, locales;
    QJSIntlDataSlice row, text;
    QJSIntlRelativeData data;
    QJSIntlRelativeLiteral *selected = NULL;
    QJSIntlPluralRulesData plural_data;
    QJSIntlPluralRule selected_rules[6];
    QJSIntlDataStatus validity;
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint32_t i, index, bits, first = 0, count = 0;
    unsigned int u, t, c;
    size_t selected_count = 0, cursor = 0;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->free || !view || !options || !bridge ||
        (unsigned int)options->style > 2) return QJS_INTL_INVALID_ARGUMENT;
    validity = qjs_intl_data_validate_relative_extension(view);
    if (validity == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
    if (validity != QJS_INTL_DATA_OK ||
        qjs_intl_data_validate_plural_extension(view) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_RELATIVE_PATTERN, &patterns) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_RELATIVE_LITERAL, &literals) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_LOCALE, &bindings) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_RULE, &rules) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (locale_index >= locales.record_count) return QJS_INTL_INVALID_ARGUMENT;
    memset(&data, 0, sizeof(data));
    memset(&plural_data, 0, sizeof(plural_data));
    for (i = 0; i < bindings.record_count; i++) {
        if (qjs_intl_data_record_u32(&bindings, i, 0, &index) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&bindings, i, &row) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (index == locale_index && row.data[4] == QJS_INTL_PLURAL_CARDINAL) {
            if (qjs_intl_data_record_u32(&bindings, i, 8, &first) != QJS_INTL_DATA_OK ||
                qjs_intl_data_record_u32(&bindings, i, 12, &count) != QJS_INTL_DATA_OK)
                return QJS_INTL_DATA_ERROR;
            break;
        }
    }
    if (!count) return QJS_INTL_UNSUPPORTED;
    if (count > 6) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < count; i++) {
        if (qjs_intl_data_record(&rules, first + i, &row) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_string(view, &rules, first + i, 4, &text) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        selected_rules[i].category = (QJSIntlPluralCategory)row.data[0];
        selected_rules[i].relation.data = (const char *)text.data;
        selected_rules[i].relation.length = text.length;
    }
    plural_data.rules = selected_rules; plural_data.rule_count = count;
    for (u = 0; u < 8; u++) for (t = 0; t < 2; t++) for (c = 0; c < 6; c++) {
        index = locale_index * 288 + u * 36 + (unsigned int)options->style * 12 + t * 6 + c;
        if (qjs_intl_data_record_string(view, &patterns, index, 8, &text) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        data.patterns[u][t][c].data = (const char *)text.data;
        data.patterns[u][t][c].length = text.length;
    }
    for (i = 0; i < literals.record_count; i++) {
        if (qjs_intl_data_record_u32(&literals, i, 0, &index) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&literals, i, &row) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (index == locale_index && row.data[5] == options->style) selected_count++;
    }
    if (selected_count > SIZE_MAX / sizeof(*selected)) return QJS_INTL_OVERFLOW;
    if (selected_count) {
        selected = a->malloc(a->opaque, selected_count * sizeof(*selected));
        if (!selected) return QJS_INTL_NO_MEMORY;
    }
    for (i = 0; i < literals.record_count; i++) {
        if (qjs_intl_data_record_u32(&literals, i, 0, &index) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&literals, i, &row) != QJS_INTL_DATA_OK) goto done;
        if (index != locale_index || row.data[5] != options->style) continue;
        if (qjs_intl_data_record_u32(&literals, i, 8, &bits) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_string(view, &literals, i, 12, &text) != QJS_INTL_DATA_OK) goto done;
        selected[cursor].unit = (QJSIntlRelativeUnit)row.data[4];
        selected[cursor].offset = signed_u32(bits);
        selected[cursor].text.data = (const char *)text.data;
        selected[cursor++].text.length = text.length;
    }
    data.literals = selected; data.literal_count = selected_count;
    status = qjs_intl_native_relative_open(a, options, &data, &plural_data, bridge, out);
 done:
    if (selected) a->free(a->opaque, selected);
    return status;
}
