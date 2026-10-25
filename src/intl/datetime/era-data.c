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
#include "era-data.h"
#include "../native/wire.h"
#include <stdlib.h>
#include <string.h>

static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t common = a.length < b.length ? a.length : b.length;
    int result = common ? memcmp(a.data, b.data, common) : 0;
    return result ? result : a.length < b.length ? -1 : a.length != b.length;
}

static int identity(QJSIntlBytes text, int code)
{
    size_t i, start = 0;
    if (!text.length)
        return 0;
    for (i = 0; i <= text.length; i++) {
        if (i == text.length || text.data[i] == '-') {
            size_t length = i - start;
            if (!length || (code && (length < (start ? 3u : 2u) || length > 8)))
                return 0;
            start = i + 1;
        } else if (!((text.data[i] >= 'a' && text.data[i] <= 'z') ||
                     (text.data[i] >= '0' && text.data[i] <= '9'))) {
            return 0;
        }
    }
    return 1;
}

static int raw_key(QJSIntlBytes text)
{
    size_t i;
    if (!text.length || (text.length > 1 && text.data[0] == '0'))
        return 0;
    for (i = 0; i < text.length; i++)
        if (text.data[i] < '0' || text.data[i] > '9')
            return 0;
    return 1;
}

static int key_compare(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
    return a < b ? -1 : a != b;
}

QJSIntlStatus qjs_intl_calendar_era_at(const QJSIntlCalendarEraData *data, uint32_t index,
                                     QJSIntlCalendarEraEntry *out)
{
    QJSIntlCalendarEraEntry result = {0};
    const uint8_t *row;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->rows || index >= data->section.count)
        return QJS_INTL_INVALID_ARGUMENT;
    row = data->rows + (size_t)index * 12;
    if (qjs_intl_data_string(&data->data, qjs_intl_read_u32(row), &result.calendar) ||
        qjs_intl_data_string(&data->data, qjs_intl_read_u32(row + 4), &result.code) ||
        qjs_intl_data_string(&data->data, qjs_intl_read_u32(row + 8), &result.key))
        return QJS_INTL_DATA_ERROR;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_calendar_era_data_open(const QJSIntlData *data,
                                            const QJSIntlAllocator *allocator,
                                            QJSIntlCalendarEraData *out)
{
    QJSIntlCalendarEraData result = {0};
    QJSIntlCalendarEraEntry current;
    QJSIntlStatus status;
    uint64_t *keys = NULL;
    uint8_t *checked;
    uint32_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_CALENDAR_ERAS, &result.section);
    if (status)
        return status;
    if (result.section.length < 4 || (result.section.length - 4) % 12 ||
        (result.section.length - 4) / 12 != result.section.count ||
        qjs_intl_read_u32(result.section.bytes) != 1)
        return QJS_INTL_DATA_ERROR;
    result.data = *data;
    result.rows = result.section.bytes + 4;
    if (result.section.count) {
        size_t size = (size_t)result.section.count * sizeof(*keys);
        /* The exact 12-byte row extent bounds the 8-byte key array. Cache
           grammar checks per string/domain: pooled text can be very long and
           reused by many rows, so rescanning it per row is not pack-linear. */
        if (data->strings.count > SIZE_MAX - size)
            return QJS_INTL_OVERFLOW;
        keys = allocator->malloc(allocator->opaque, size + data->strings.count);
        if (!keys)
            return QJS_INTL_NO_MEMORY;
        checked = (uint8_t *)keys + size;
        memset(checked, 0, data->strings.count);
    } else {
        checked = NULL;
    }
    status = QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.section.count; i++) {
        const uint8_t *row = result.rows + (size_t)i * 12;
        uint32_t calendar = qjs_intl_read_u32(row), code = qjs_intl_read_u32(row + 4);
        uint32_t raw = qjs_intl_read_u32(row + 8);
        if (qjs_intl_calendar_era_at(&result, i, &current))
            goto done;
        if ((!(checked[calendar] & 1) && !identity(current.calendar, 0)) ||
            (!(checked[code] & 2) && !identity(current.code, 1)) ||
            (!(checked[raw] & 4) && !raw_key(current.key)))
            goto done;
        checked[calendar] |= 1;
        checked[code] |= 2;
        checked[raw] |= 4;
        /* Shared string IDs already have strict UTF-8 lexical order. */
        if (i && (qjs_intl_read_u32(row - 12) > calendar ||
                  (qjs_intl_read_u32(row - 12) == calendar &&
                   qjs_intl_read_u32(row - 8) >= code)))
            goto done;
        keys[i] = (uint64_t)calendar << 32 | raw;
    }
    if (result.section.count > 1) {
        qsort(keys, result.section.count, sizeof(*keys), key_compare);
        for (i = 1; i < result.section.count; i++)
            if (keys[i] == keys[i - 1])
                goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    if (keys)
        allocator->free(allocator->opaque, keys);
    return status;
}

QJSIntlStatus qjs_intl_calendar_era_find(const QJSIntlCalendarEraData *data,
                                       QJSIntlBytes calendar, QJSIntlBytes code,
                                       QJSIntlBytes *key)
{
    uint32_t lo = 0, hi;
    if (key)
        memset(key, 0, sizeof(*key));
    if (!key || !data || !data->rows || !calendar.data || !code.data ||
        calendar.length > PTRDIFF_MAX || code.length > PTRDIFF_MAX ||
        !identity(calendar, 0) || !identity(code, 1))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->section.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlCalendarEraEntry row;
        QJSIntlStatus status = qjs_intl_calendar_era_at(data, mid, &row);
        int order;
        if (status)
            return status;
        order = compare(calendar, row.calendar);
        if (!order)
            order = compare(code, row.code);
        if (!order) {
            *key = row.key;
            return QJS_INTL_OK;
        }
        if (order < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return QJS_INTL_UNSUPPORTED;
}
