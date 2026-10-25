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
#include "symbol-data.h"
#include "../native/wire.h"
#include <string.h>

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int ordered(const uint8_t *records, uint32_t count, unsigned int width)
{
    uint32_t i;
    for (i = 1; i < count; i++) {
        unsigned int j;
        for (j = 0; j < width; j++) {
            uint32_t a = word(records, (size_t)(i - 1) * width + j);
            uint32_t b = word(records, (size_t)i * width + j);
            if (a != b) {
                if (a > b)
                    return 0;
                break;
            }
        }
        if (j == width)
            return 0;
    }
    return 1;
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 &&
           (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
            !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

QJSIntlStatus qjs_intl_number_symbol_data_open(const QJSIntlNumberingData *numbering,
                                             const QJSIntlAllocator *allocator,
                                             QJSIntlNumberSymbolData *out)
{
    QJSIntlNumberSymbolData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint8_t *used;
    size_t length, at, i, scratch_size;
    uint32_t numeric = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !numbering || !numbering->data.pack.bytes || !numbering->systems.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.numbering = *numbering;
    if (qjs_intl_pack_section(&numbering->data.pack, QJS_INTL_SECTION_NUMBER_SYMBOLS,
                              &result.section) ||
        result.section.count != numbering->data.locales.count || !result.section.count ||
        result.section.length < 12 || result.section.length % 4)
        return status;
    length = result.section.length / 4;
    result.system_count = word(result.section.bytes, 0);
    result.vector_count = word(result.section.bytes, 1);
    result.row_count = word(result.section.bytes, 2);
    if (!result.system_count || !result.vector_count || !result.row_count ||
        result.row_count > result.section.count)
        return status;
    at = 3;
    if (result.system_count > length - at)
        return status;
    result.systems = result.section.bytes + at * 4;
    at += result.system_count;
    if (result.section.count > length - at)
        return status;
    result.locale_rows = result.section.bytes + at * 4;
    at += result.section.count;
    if (result.row_count > (length - at) / result.system_count)
        return status;
    result.rows = result.section.bytes + at * 4;
    at += (size_t)result.row_count * result.system_count;
    if ((length - at) % QJS_INTL_SYMBOL_COUNT ||
        result.vector_count != (length - at) / QJS_INTL_SYMBOL_COUNT)
        return status;
    result.vectors = result.section.bytes + at * 4;
    for (i = 0; i < numbering->systems.count; i++) {
        QJSIntlNumberingSystem system;
        if (qjs_intl_numbering_system_at(numbering, i, &system))
            return status;
        if (system.kind == QJS_INTL_NUMBERING_NUMERIC) {
            if (numeric == result.system_count || word(result.systems, numeric) != i)
                return status;
            numeric++;
        }
    }
    if (numeric != result.system_count ||
        !ordered(result.rows, result.row_count, result.system_count) ||
        !ordered(result.vectors, result.vector_count, QJS_INTL_SYMBOL_COUNT))
        return status;
    /* Both counts are bounded by disjoint word ranges within the pack. */
    scratch_size = (size_t)result.row_count + result.vector_count;
    used = allocator->malloc(allocator->opaque, scratch_size);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch_size);
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * result.system_count; i++) {
        uint32_t index = word(result.rows, i);
        if (index >= result.vector_count)
            goto done;
        used[result.row_count + (size_t)index] = 1;
    }
    for (i = 0; i < scratch_size; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < (size_t)result.vector_count * QJS_INTL_SYMBOL_COUNT; i++) {
        uint32_t index = word(result.vectors, i);
        QJSIntlBytes text;
        if (i % QJS_INTL_SYMBOL_COUNT >= QJS_INTL_SYMBOL_CURRENCY_DECIMAL &&
            (index == QJS_INTL_NUMBERING_ABSENT || index == QJS_INTL_NUMBERING_BLOCKED))
            continue;
        if (qjs_intl_data_string(&numbering->data, index, &text) || unresolved(text))
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_number_symbols_at(const QJSIntlNumberSymbolData *data,
                                       uint32_t locale, uint32_t system,
                                       QJSIntlNumberSymbols *out)
{
    uint32_t lo = 0, hi, row, vector;
    unsigned int i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        system >= data->numbering.systems.count)
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->system_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (word(data->systems, mid) < system)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == data->system_count || word(data->systems, lo) != system)
        return QJS_INTL_UNSUPPORTED;
    row = word(data->locale_rows, locale);
    vector = word(data->rows, (size_t)row * data->system_count + lo);
    for (i = 0; i < QJS_INTL_SYMBOL_COUNT; i++) {
        uint32_t index = word(data->vectors, (size_t)vector * QJS_INTL_SYMBOL_COUNT + i);
        if (index == QJS_INTL_NUMBERING_ABSENT)
            out->fields[i].state = QJS_INTL_SYMBOL_ABSENT;
        else if (index == QJS_INTL_NUMBERING_BLOCKED)
            out->fields[i].state = QJS_INTL_SYMBOL_BLOCKED;
        else
            qjs_intl_data_string(&data->numbering.data, index, &out->fields[i].text);
    }
    return QJS_INTL_OK;
}
