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
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 3
#define ROW_WIDTH QJS_INTL_DURATION_SKELETON_COUNT
#define RECORD_WIDTH 4

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

static int ordered(const uint8_t *records, uint32_t count, unsigned int width)
{
    size_t i, j;
    for (i = 1; i < count; i++) {
        for (j = 0; j < width; j++) {
            uint32_t previous = word(records, (i - 1) * width + j);
            uint32_t current = word(records, i * width + j);
            if (previous < current)
                break;
            if (previous > current)
                return 0;
        }
        if (j == width)
            return 0;
    }
    return 1;
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 && (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
                                !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

QJSIntlStatus qjs_intl_duration_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                        QJSIntlDurationData *out)
{
    QJSIntlDurationData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, i, scratch;
    uint8_t *used, *used_records, *sources;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_DURATION_PATTERNS, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.row_count = word(result.section.bytes, 1);
    result.record_count = word(result.section.bytes, 2);
    if (!result.row_count || result.row_count > result.section.count || !result.record_count ||
        result.record_count > (uint64_t)result.row_count * ROW_WIDTH ||
        qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, ROW_WIDTH, &result.rows) ||
        !take(&result.section, &at, result.record_count, RECORD_WIDTH, &result.records) ||
        at != result.section.length / 4 ||
        !ordered(result.rows, result.row_count, ROW_WIDTH) ||
        !ordered(result.records, result.record_count, RECORD_WIDTH))
        return status;
    /* Validated counts occupy disjoint spans in the bounded shared pack. */
    scratch = (size_t)result.row_count + result.record_count + data->strings.count;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    used_records = used + result.row_count;
    sources = used_records + result.record_count;
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * ROW_WIDTH; i++) {
        uint32_t index = word(result.rows, i);
        if (index >= result.record_count)
            goto done;
        used_records[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count + result.record_count; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < result.record_count; i++) {
        uint32_t state = word(result.records, i * RECORD_WIDTH);
        uint32_t text = word(result.records, i * RECORD_WIDTH + 1);
        uint32_t origin = word(result.records, i * RECORD_WIDTH + 2);
        uint32_t key = word(result.records, i * RECORD_WIDTH + 3);
        QJSIntlBytes value;
        if (state > QJS_INTL_DURATION_PATTERN_BLOCKED || text >= data->strings.count ||
            origin >= data->strings.count || key >= ROW_WIDTH)
            goto done;
        if (state == QJS_INTL_DURATION_PATTERN_MISSING) {
            if (text || origin || key)
                goto done;
            continue;
        }
        if (state == QJS_INTL_DURATION_PATTERN_BLOCKED && text)
            goto done;
        if (qjs_intl_data_string(data, text, &value) || unresolved(value))
            goto done;
        if (!sources[origin]) {
            QJSIntlStatus check;
            if (qjs_intl_data_string(data, origin, &value))
                goto done;
            check = qjs_intl_source_locale_validate(allocator, value);
            if (check) {
                if (check == QJS_INTL_NO_MEMORY || check == QJS_INTL_OVERFLOW)
                    status = check;
                goto done;
            }
            sources[origin] = 1;
        }
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_duration_pattern_at(const QJSIntlDurationData *data, uint32_t locale,
                                         QJSIntlDurationSkeleton skeleton, QJSIntlDurationPattern *out)
{
    uint32_t row, index;
    const uint8_t *record;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)skeleton >= QJS_INTL_DURATION_SKELETON_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    row = word(data->locale_rows, locale);
    index = word(data->rows, (size_t)row * ROW_WIDTH + (unsigned int)skeleton);
    record = data->records + (size_t)index * RECORD_WIDTH * 4;
    out->state = word(record, 0);
    if (out->state != QJS_INTL_DURATION_PATTERN_MISSING) {
        qjs_intl_data_string(&data->data, word(record, 1), &out->text);
        qjs_intl_data_string(&data->data, word(record, 2), &out->origin_locale);
        out->origin_skeleton = word(record, 3);
    }
    return QJS_INTL_OK;
}
