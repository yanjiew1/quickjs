/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "plural-data-validation.h"
#include "../plural.h"
#include <string.h>

static int span_valid(uint32_t first, uint32_t count, uint32_t total)
{
    return count ? first <= total && count <= total - first : first == 0;
}
static uint32_t wire_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int relation_valid(QJSIntlDataSlice text, unsigned int category)
{
    JSIntlPluralOperands operand = {"0", 1, 0};
    size_t i;
    int matches;
    if (!text.length) return category == 5;
    if (category == 5) return 0;
    for (i = 0; i < text.length; i++)
        if (text.data[i] < 0x20 || text.data[i] > 0x7e || text.data[i] == '@') return 0;
    if (text.data[0] == ' ' || text.data[text.length - 1] == ' ') return 0;
    return js_intl_plural_rule_evaluate((const char *)text.data, text.length,
                                       &operand, &matches) == 0;
}
QJSIntlDataStatus qjs_intl_data_validate_plural_extension(const QJSIntlDataView *view)
{
    QJSIntlDataSection locales, bindings, rules, ranges;
    QJSIntlDataStatus b, r, g;
    QJSIntlDataSlice row, text;
    uint32_t i, j, index, first, count, previous_index = 0;
    unsigned int previous_type = 0;
    int previous;
    if (!view || !view->data) return QJS_INTL_DATA_INVALID_ARGUMENT;
    b = qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_LOCALE, &bindings);
    r = qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_RULE, &rules);
    g = qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_RANGE, &ranges);
    if (b == QJS_INTL_DATA_NOT_FOUND && r == QJS_INTL_DATA_NOT_FOUND &&
        g == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_DATA_OK;
    if (b != QJS_INTL_DATA_OK || r != QJS_INTL_DATA_OK || g != QJS_INTL_DATA_OK ||
        view->length < QJS_INTL_DATA_HEADER_SIZE ||
        view->data[QJS_INTL_H_SCHEMA_MINOR] != 3 ||
        view->data[QJS_INTL_H_SCHEMA_MINOR + 1] != 0 ||
        !QJS_INTL_CLDR_VERSION_SUPPORTED(wire_u32(view->data + QJS_INTL_H_CLDR_VERSION)) ||
        bindings.record_width != QJS_INTL_DATA_PLURAL_LOCALE_WIDTH ||
        rules.record_width != QJS_INTL_DATA_PLURAL_RULE_WIDTH ||
        ranges.record_width != QJS_INTL_DATA_PLURAL_RANGE_WIDTH ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_INVALID;
    for (i = 0; i < rules.record_count; i++) {
        if (qjs_intl_data_record(&rules, i, &row) != QJS_INTL_DATA_OK ||
            row.data[0] > 5 || row.data[1] || row.data[2] || row.data[3] ||
            qjs_intl_data_record_string(view, &rules, i, 4, &text) != QJS_INTL_DATA_OK ||
            !relation_valid(text, row.data[0])) return QJS_INTL_DATA_INVALID;
    }
    for (i = 0; i < ranges.record_count; i++) {
        if (qjs_intl_data_record(&ranges, i, &row) != QJS_INTL_DATA_OK ||
            row.data[0] > 5 || row.data[1] > 5 || row.data[2] > 5 || row.data[3])
            return QJS_INTL_DATA_INVALID;
    }
    for (i = 0; i < bindings.record_count; i++) {
        if (qjs_intl_data_record(&bindings, i, &row) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&bindings, i, 0, &index) != QJS_INTL_DATA_OK ||
            index >= locales.record_count || row.data[4] > 1 ||
            row.data[5] || row.data[6] || row.data[7] ||
            (i && (index < previous_index ||
                   (index == previous_index && row.data[4] <= previous_type))))
            return QJS_INTL_DATA_INVALID;
        previous_index = index; previous_type = row.data[4];
        if (qjs_intl_data_record_u32(&bindings, i, 8, &first) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&bindings, i, 12, &count) != QJS_INTL_DATA_OK ||
            !count || count > 6 || !span_valid(first, count, rules.record_count))
            return QJS_INTL_DATA_INVALID;
        previous = -1;
        for (j = 0; j < count; j++) {
            if (qjs_intl_data_record(&rules, first + j, &row) != QJS_INTL_DATA_OK ||
                row.data[0] <= previous) return QJS_INTL_DATA_INVALID;
            previous = row.data[0];
        }
        if (previous != 5) return QJS_INTL_DATA_INVALID;
        if (qjs_intl_data_record_u32(&bindings, i, 16, &first) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&bindings, i, 20, &count) != QJS_INTL_DATA_OK ||
            count > 36 || !span_valid(first, count, ranges.record_count))
            return QJS_INTL_DATA_INVALID;
        previous = -1;
        for (j = 0; j < count; j++) {
            int key;
            if (qjs_intl_data_record(&ranges, first + j, &row) != QJS_INTL_DATA_OK)
                return QJS_INTL_DATA_INVALID;
            key = row.data[0] * 6 + row.data[1];
            if (key <= previous) return QJS_INTL_DATA_INVALID;
            previous = key;
        }
    }
    return QJS_INTL_DATA_OK;
}
