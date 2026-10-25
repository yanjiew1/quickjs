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
#include "data.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 9
#define FRACTION_WORDS 5
#define ABSENT UINT32_C(0xffffffff)
#define BLOCKED UINT32_C(0xfffffffe)
#define EMPTY UINT32_C(0xfffffffd)
#define VECTOR_WORDS (QJS_INTL_CURRENCY_FIELD_COUNT + 1)
#define ROOT_NAME_MASK ((UINT32_C(1) << (QJS_INTL_CURRENCY_NAME_OTHER + 1)) - 2)

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, uint32_t width,
                const uint8_t **out)
{
    if (count > (section->length / 4 - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int ordered(const uint8_t *records, uint32_t count, uint32_t width)
{
    uint32_t i, j;
    for (i = 1; i < count; i++) {
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

static int code_valid(QJSIntlBytes code)
{
    size_t i;
    if (!code.data || code.length != 3)
        return 0;
    for (i = 0; i < 3; i++) {
        if (code.data[i] < 'A' || code.data[i] > 'Z')
            return 0;
    }
    return 1;
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 && (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
                                !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

static QJSIntlCurrencyFractions fractions_at(const uint8_t *record)
{
    QJSIntlCurrencyFractions result;
    result.digits = word(record, 0);
    result.rounding = word(record, 1);
    result.cash_digits = word(record, 2);
    result.cash_rounding = word(record, 3);
    return result;
}

QJSIntlStatus qjs_intl_currency_data_open(const QJSIntlData *data,
                                          const QJSIntlAllocator *allocator,
                                          QJSIntlCurrencyData *out)
{
    QJSIntlCurrencyData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty, previous = {0};
    size_t at = HEADER_WORDS, scratch, i;
    uint8_t *used;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes || !allocator ||
        !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_CURRENCIES, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 2)
        return status;
    result.code_count = word(result.section.bytes, 1);
    result.fraction_count = word(result.section.bytes, 2);
    result.vector_count = word(result.section.bytes, 3);
    result.map_count = word(result.section.bytes, 4);
    result.defaults = fractions_at(result.section.bytes + 5 * 4);
    if (!result.code_count || result.fraction_count > result.code_count || !result.map_count ||
        result.map_count > result.section.count || result.defaults.digits > 100 ||
        result.defaults.cash_digits > 100 || qjs_intl_data_string(data, 0, &empty) ||
        empty.length || !take(&result.section, &at, result.code_count, 1, &result.codes) ||
        !take(&result.section, &at, result.fraction_count, FRACTION_WORDS, &result.fractions) ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_maps) ||
        !take(&result.section, &at, result.map_count, result.code_count, &result.maps) ||
        !take(&result.section, &at, result.vector_count, VECTOR_WORDS,
              &result.vectors) ||
        at != result.section.length / 4 ||
        result.vector_count > (size_t)result.map_count * result.code_count)
        return status;
    for (i = 0; i < result.code_count; i++) {
        QJSIntlBytes code;
        if (qjs_intl_data_string(data, word(result.codes, i), &code) || !code_valid(code) ||
            (i && memcmp(previous.data, code.data, 3) >= 0))
            return status;
        previous = code;
    }
    for (i = 0; i < result.fraction_count; i++) {
        const uint8_t *record = result.fractions + i * FRACTION_WORDS * 4;
        if (word(record, 0) >= result.code_count || word(record, 1) > 100 ||
            word(record, 3) > 100 || (i && word(record, 0) <= word(record - FRACTION_WORDS * 4, 0)))
            return status;
    }
    if (!ordered(result.maps, result.map_count, result.code_count) ||
        !ordered(result.vectors, result.vector_count, VECTOR_WORDS))
        return status;
    /* Counts occupy disjoint nonempty word ranges, bounding their sum. */
    scratch = (size_t)result.map_count + result.vector_count;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_maps, i);
        if (index >= result.map_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.map_count * result.code_count; i++) {
        uint32_t index = word(result.maps, i);
        if (index == ABSENT)
            continue;
        if (index >= result.vector_count)
            goto done;
        used[result.map_count + (size_t)index] = 1;
    }
    for (i = 0; i < scratch; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < result.vector_count; i++) {
        unsigned int j, present = 0;
        uint32_t mask = word(result.vectors, i * VECTOR_WORDS + QJS_INTL_CURRENCY_FIELD_COUNT);
        if (mask & ~ROOT_NAME_MASK)
            goto done;
        for (j = 0; j < QJS_INTL_CURRENCY_FIELD_COUNT; j++) {
            uint32_t index = word(result.vectors, i * VECTOR_WORDS + j);
            QJSIntlBytes text;
            if ((mask & (UINT32_C(1) << j)) && index == ABSENT)
                goto done;
            present |= index != ABSENT;
            if (index == ABSENT || index == BLOCKED || index == EMPTY)
                continue;
            if (!index || qjs_intl_data_string(data, index, &text) || !text.length ||
                unresolved(text))
                goto done;
        }
        if (!present)
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_currency_code_at(const QJSIntlCurrencyData *data, uint32_t index,
                                        QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->code_count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_data_string(&data->data, word(data->codes, index), out);
}

QJSIntlStatus qjs_intl_currency_code_find(const QJSIntlCurrencyData *data, QJSIntlBytes code,
                                          uint32_t *out)
{
    uint32_t lo = 0, hi;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || !code_valid(code))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->code_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlBytes candidate;
        qjs_intl_currency_code_at(data, mid, &candidate);
        if (memcmp(candidate.data, code.data, 3) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < data->code_count) {
        QJSIntlBytes candidate;
        qjs_intl_currency_code_at(data, lo, &candidate);
        if (!memcmp(candidate.data, code.data, 3)) {
            *out = lo;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_currency_field_at(const QJSIntlCurrencyData *data, uint32_t locale,
                                         uint32_t code, QJSIntlCurrencyField field,
                                         QJSIntlCurrencyText *out)
{
    uint32_t map, vector, value;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        code >= data->code_count || (unsigned int)field >= QJS_INTL_CURRENCY_FIELD_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    map = word(data->locale_maps, locale);
    vector = word(data->maps, (size_t)map * data->code_count + code);
    value = vector == ABSENT
                ? ABSENT
                : word(data->vectors, (size_t)vector * VECTOR_WORDS + field);
    if (vector != ABSENT)
        out->from_root = (word(data->vectors, (size_t)vector * VECTOR_WORDS +
                                             QJS_INTL_CURRENCY_FIELD_COUNT) >> field) & 1;
    if (value == ABSENT)
        out->state = QJS_INTL_CURRENCY_ABSENT;
    else if (value == BLOCKED)
        out->state = QJS_INTL_CURRENCY_BLOCKED;
    else if (value == EMPTY)
        out->state = QJS_INTL_CURRENCY_EMPTY;
    else
        qjs_intl_data_string(&data->data, value, &out->text);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_currency_fractions(const QJSIntlCurrencyData *data, QJSIntlBytes code,
                                          QJSIntlCurrencyFractions *out)
{
    QJSIntlStatus status;
    uint32_t index, lo = 0, hi;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_currency_code_find(data, code, &index);
    if (status && status != QJS_INTL_UNSUPPORTED)
        return status;
    *out = data->defaults;
    if (status)
        return QJS_INTL_OK;
    hi = data->fraction_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (word(data->fractions, (size_t)mid * FRACTION_WORDS) < index)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < data->fraction_count) {
        const uint8_t *record = data->fractions + (size_t)lo * FRACTION_WORDS * 4;
        if (word(record, 0) == index)
            *out = fractions_at(record + 4);
    }
    return QJS_INTL_OK;
}
