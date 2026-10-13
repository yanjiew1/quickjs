/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "relative-data-validation.h"
#include "plural-data-validation.h"
#include "plural-data-schema.h"
#include <limits.h>

static int32_t signed_u32(uint32_t n)
{
    return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
}
static int valid_text(QJSIntlDataSlice s, int pattern)
{
    size_t i;
    unsigned int tokens = 0;
    if (!s.length) return 0;
    if (!pattern) return 1; /* scalar UTF8 is checked by shared pool reader */
    for (i = 0; i < s.length; i++) {
        if (s.data[i] == '{') {
            if (s.length - i < 3 || s.data[i+1] != '0' || s.data[i+2] != '}') return 0;
            tokens++;
            i += 2;
        } else if (s.data[i] == '}') return 0;
    }
    return tokens <= 1;
}
QJSIntlDataStatus qjs_intl_data_validate_relative_extension(const QJSIntlDataView *view)
{
    QJSIntlDataSection locales, patterns, literals, bindings;
    QJSIntlDataSlice row, text;
    QJSIntlDataStatus p, l;
    uint32_t i, locale, previous_locale = 0;
    unsigned int unit, style, previous_unit = 0, previous_style = 0;
    int32_t offset, previous_offset = 0;
    uint32_t offset_bits, cardinal_locale = 0;
    if (!view || !view->data) return QJS_INTL_DATA_INVALID_ARGUMENT;
    p = qjs_intl_data_section(view, QJS_INTL_DATA_RELATIVE_PATTERN, &patterns);
    l = qjs_intl_data_section(view, QJS_INTL_DATA_RELATIVE_LITERAL, &literals);
    if (p == QJS_INTL_DATA_NOT_FOUND && l == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_DATA_NOT_FOUND;
    if (p != QJS_INTL_DATA_OK || l != QJS_INTL_DATA_OK ||
        patterns.record_width != QJS_INTL_DATA_RELATIVE_PATTERN_WIDTH ||
        literals.record_width != QJS_INTL_DATA_RELATIVE_LITERAL_WIDTH ||
        qjs_intl_data_section(view, QJS_INTL_DATA_PLURAL_LOCALE, &bindings) != QJS_INTL_DATA_OK ||
        qjs_intl_data_validate_plural_extension(view) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) != QJS_INTL_DATA_OK ||
        locales.record_count > UINT32_MAX / 288 ||
        patterns.record_count != locales.record_count * 288) return QJS_INTL_DATA_INVALID;
    /* Numeric patterns require cardinal rules for every represented locale.
     * Shared plural validation bounds and sorts the binding rows first. */
    for (i = 0; i < bindings.record_count; i++) {
        if (qjs_intl_data_record(&bindings, i, &row) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&bindings, i, 0, &locale) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_INVALID;
        if (row.data[4] == 0) {
            if (locale != cardinal_locale++) return QJS_INTL_DATA_INVALID;
        }
    }
    if (cardinal_locale != locales.record_count) return QJS_INTL_DATA_INVALID;
    for (i = 0; i < patterns.record_count; i++) {
        uint32_t key = i % 288;
        if (qjs_intl_data_record_u32(&patterns, i, 0, &locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&patterns, i, &row) != QJS_INTL_DATA_OK ||
            locale != i / 288 || row.data[4] != key / 36 ||
            row.data[5] != (key % 36) / 12 || row.data[6] != (key % 12) / 6 ||
            row.data[7] != key % 6 ||
            qjs_intl_data_record_string(view, &patterns, i, 8, &text) != QJS_INTL_DATA_OK ||
            !valid_text(text, 1)) return QJS_INTL_DATA_INVALID;
    }
    for (i = 0; i < literals.record_count; i++) {
        if (qjs_intl_data_record_u32(&literals, i, 0, &locale) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_u32(&literals, i, 8, &offset_bits) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record(&literals, i, &row) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_string(view, &literals, i, 12, &text) != QJS_INTL_DATA_OK ||
            !valid_text(text, 0)) return QJS_INTL_DATA_INVALID;
        unit = row.data[4]; style = row.data[5]; offset = signed_u32(offset_bits);
        if (locale >= locales.record_count || unit >= 8 || style >= 3 || row.data[6] || row.data[7] ||
            (i && (locale < previous_locale || (locale == previous_locale &&
             (unit < previous_unit || (unit == previous_unit &&
              (style < previous_style || (style == previous_style && offset <= previous_offset))))))))
            return QJS_INTL_DATA_INVALID;
        previous_locale = locale; previous_unit = unit;
        previous_style = style; previous_offset = offset;
    }
    return QJS_INTL_DATA_OK;
}
