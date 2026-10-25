/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "number-fields.h"
#include <limits.h>
#include <string.h>
#include <unicode/unum.h>
#include <unicode/uchar.h>

void qjs_icu4c_number_fields_clear(const QJSIntlAllocator *allocator, QJSICU4CNumberFields *map)
{
    if (!map)
        return;
    if (map->owned_text)
        allocator->free(allocator->opaque, map->owned_text);
    if (map->fields)
        allocator->free(allocator->opaque, map->fields);
    if (map->flags)
        allocator->free(allocator->opaque, map->flags);
    memset(map, 0, sizeof(*map));
}

/* Some ICU scientific writers insert exponent digits right-to-left using
   code-point counts as UTF-16 indexes. Supplementary digits then become nested
   surrogate halves: H5 H2 L2 L5 instead of H2 L2 H5 L5 for exponent 25.
   Repair only that verified shape inside an exponent field. Digit identities,
   text length and every ICU field/source offset remain unchanged. Healthy
   output, including a future corrected ICU release, takes no copy. */
static QJSIntlStatus repair_exponent(const QJSIntlAllocator *allocator, QJSICU4CNumberFields *map)
{
    const uint16_t *source = map->text;
    size_t start, end, leads, i;

    for (start = 0; start < map->length; start = end) {
        for (end = start + 1; end < map->length && map->fields[end] == map->fields[start] &&
                              map->flags[end] == map->flags[start];
             end++)
            ;
        if (map->fields[start] != UNUM_EXPONENT_FIELD)
            continue;
        for (leads = 0; leads < end - start && source[start + leads] >= 0xd800 &&
                        source[start + leads] <= 0xdbff;
             leads++)
            ;
        if (leads < 2 || leads != (end - start) / 2 || (end - start) % 2)
            continue;
        for (i = 0; i < leads; i++) {
            uint16_t high = source[start + leads - 1 - i], low = source[start + leads + i];
            uint32_t code;

            if (low < 0xdc00 || low > 0xdfff)
                break;
            code = 0x10000U + (((uint32_t)high - 0xd800U) << 10) + low - 0xdc00U;
            if (u_charDigitValue((UChar32)code) < 0)
                break;
        }
        if (i != leads)
            continue;
        if (!map->owned_text) {
            map->owned_text =
                allocator->malloc(allocator->opaque, map->length * sizeof(*map->owned_text));
            if (!map->owned_text)
                return QJS_INTL_NO_MEMORY;
            memcpy(map->owned_text, source, map->length * sizeof(*map->owned_text));
            map->text = map->owned_text;
        }
        for (i = 0; i < leads; i++) {
            map->owned_text[start + 2 * i] = source[start + leads - 1 - i];
            map->owned_text[start + 2 * i + 1] = source[start + leads + i];
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_icu4c_number_fields(const QJSIntlAllocator *allocator,
                                      const UFormattedValue *value, size_t limit,
                                      QJSICU4CNumberFields *out)
{
    UErrorCode status = U_ZERO_ERROR;
    UConstrainedFieldPosition *position = NULL;
    QJSICU4CNumberFields map = {0};
    QJSIntlStatus error;
    int32_t length;
    size_t i;
    int has_exponent = 0;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!allocator || !allocator->malloc || !allocator->free || !value || !out || !limit)
        return QJS_INTL_INVALID_ARGUMENT;
    map.text = ufmtval_getString(value, &length, &status);
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length < 0)
        return QJS_INTL_DATA_ERROR;
    if ((size_t)length > limit || (size_t)length > SIZE_MAX / sizeof(*map.fields))
        return QJS_INTL_OVERFLOW;
    map.length = (size_t)length;
    if (length) {
        map.fields = allocator->malloc(allocator->opaque, (size_t)length * sizeof(*map.fields));
        map.flags = allocator->malloc(allocator->opaque, (size_t)length * sizeof(*map.flags));
        if (!map.fields || !map.flags) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
        for (i = 0; i < map.length; i++)
            map.fields[i] = -1;
        memset(map.flags, 0, map.length * sizeof(*map.flags));
    }
    position = ucfpos_open(&status);
    while (ufmtval_nextPosition(value, position, &status)) {
        int32_t start, end;
        int32_t category = ucfpos_getCategory(position, &status);
        int32_t field = ucfpos_getField(position, &status);

        ucfpos_getIndexes(position, &start, &end, &status);
        if (U_FAILURE(status))
            break;
        if (start < 0 || end < start || end > length ||
            (category == UFIELD_CATEGORY_NUMBER && (field < 0 || field > INT16_MAX)) ||
            (category == UFIELD_CATEGORY_NUMBER_RANGE_SPAN && (field < 0 || field > 1))) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        if (category == UFIELD_CATEGORY_NUMBER && field == UNUM_EXPONENT_FIELD)
            has_exponent = 1;
        for (i = (size_t)start; i < (size_t)end; i++) {
            if (category == UFIELD_CATEGORY_NUMBER &&
                (field != UNUM_INTEGER_FIELD || map.fields[i] < 0))
                map.fields[i] = (int16_t)field;
            if (category == UFIELD_CATEGORY_NUMBER_RANGE_SPAN)
                map.flags[i] = field ? QJS_ICU4C_FIELD_END : QJS_ICU4C_FIELD_START;
        }
    }
    error = qjs_icu4c_status(status);
    if (!error && has_exponent)
        error = repair_exponent(allocator, &map);
    if (!error) {
        *out = map;
        memset(&map, 0, sizeof(map));
    }
done:
    ucfpos_close(position);
    qjs_icu4c_number_fields_clear(allocator, &map);
    return error;
}

QJSIntlPartType qjs_icu4c_number_part_type(int field, const QJSIntlMathematicalValue *value,
                                           QJSIntlNumberStyle style)
{
    switch (field) {
    case UNUM_INTEGER_FIELD:
        if (value && value->kind == QJS_INTL_NAN)
            return QJS_INTL_PART_NAN;
        if (value && (value->kind == QJS_INTL_POSITIVE_INFINITY ||
                      value->kind == QJS_INTL_NEGATIVE_INFINITY))
            return QJS_INTL_PART_INFINITY;
        return QJS_INTL_PART_INTEGER;
    case UNUM_FRACTION_FIELD:
        return QJS_INTL_PART_FRACTION;
    case UNUM_DECIMAL_SEPARATOR_FIELD:
        return QJS_INTL_PART_DECIMAL;
    case UNUM_EXPONENT_SYMBOL_FIELD:
        return QJS_INTL_PART_EXPONENT_SEPARATOR;
    case UNUM_EXPONENT_SIGN_FIELD:
        return QJS_INTL_PART_EXPONENT_MINUS_SIGN;
    case UNUM_EXPONENT_FIELD:
        return QJS_INTL_PART_EXPONENT_INTEGER;
    case UNUM_GROUPING_SEPARATOR_FIELD:
        return QJS_INTL_PART_GROUP;
    case UNUM_CURRENCY_FIELD:
        return QJS_INTL_PART_CURRENCY;
    case UNUM_PERCENT_FIELD:
        return style == QJS_INTL_NUMBER_UNIT ? QJS_INTL_PART_UNIT : QJS_INTL_PART_PERCENT_SIGN;
    case UNUM_SIGN_FIELD:
        if (value &&
            (value->kind == QJS_INTL_NEGATIVE_INFINITY || value->kind == QJS_INTL_NEGATIVE_ZERO ||
             (value->kind == QJS_INTL_FINITE && value->decimal.length &&
              value->decimal.data[0] == '-')))
            return QJS_INTL_PART_MINUS_SIGN;
        return QJS_INTL_PART_PLUS_SIGN;
    case UNUM_MEASURE_UNIT_FIELD:
        return style == QJS_INTL_NUMBER_PERCENT ? QJS_INTL_PART_PERCENT_SIGN : QJS_INTL_PART_UNIT;
    case UNUM_COMPACT_FIELD:
        return QJS_INTL_PART_COMPACT;
    case UNUM_APPROXIMATELY_SIGN_FIELD:
        return QJS_INTL_PART_APPROXIMATELY_SIGN;
    default:
        return QJS_INTL_PART_LITERAL;
    }
}

QJSIntlStatus qjs_icu4c_number_parts(const QJSIntlAllocator *allocator,
                                     const UFormattedValue *value,
                                     const QJSIntlMathematicalValue *number,
                                     QJSIntlNumberStyle style, size_t limit, QJSIntlFormatted *out)
{
    QJSICU4CNumberFields map = {0};
    QJSIntlFormatted formatted = {0};
    QJSIntlStatus error;
    size_t start, end;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4c_number_fields(allocator, value, limit, &map);
    if (error)
        return error;
    if (map.length > SIZE_MAX / sizeof(*formatted.parts) ||
        map.length > SIZE_MAX / sizeof(*formatted.text)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (map.length) {
        formatted.text = allocator->malloc(allocator->opaque, map.length * sizeof(*formatted.text));
        formatted.parts =
            allocator->malloc(allocator->opaque, map.length * sizeof(*formatted.parts));
        if (!formatted.text || !formatted.parts) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
        memcpy(formatted.text, map.text, map.length * sizeof(*formatted.text));
    }
    formatted.length = map.length;
    for (start = 0; start < map.length; start = end) {
        QJSIntlPartType type = qjs_icu4c_number_part_type(map.fields[start], number, style);

        for (end = start + 1; end < map.length && map.fields[end] == map.fields[start]; end++)
            ;
        formatted.parts[formatted.part_count++] = (QJSIntlPart){start, end, type};
    }
    *out = formatted;
    memset(&formatted, 0, sizeof(formatted));
done:
    qjs_intl_formatted_clear(allocator, &formatted);
    qjs_icu4c_number_fields_clear(allocator, &map);
    return error;
}
