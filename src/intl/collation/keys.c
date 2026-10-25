/* Key spans and prefix splits shared by raw and logical payload readers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "keys.h"
#include "../native/wire.h"

QJSIntlStatus qjs_intl_collation_keys_at(const QJSIntlCollationKeyIndex *view, uint32_t index,
                                         QJSIntlCollationKeySpan *out)
{
    const uint8_t *row;
    if (out)
        *out = (QJSIntlCollationKeySpan){0};
    if (!view || !out || index >= view->row_count)
        return QJS_INTL_INVALID_ARGUMENT;
    row = view->rows + (size_t)index * 16;
    out->keys = view->keys + (size_t)qjs_intl_read_u32(row) * 4;
    out->count = qjs_intl_read_u32(row + 4);
    return QJS_INTL_OK;
}

uint32_t qjs_intl_collation_keys_prefix(const QJSIntlCollationKeyIndex *view, uint32_t mapping)
{
    uint32_t low = 0, high = view->context_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t *row = view->contexts + (size_t)middle * 12;
        uint32_t id = qjs_intl_read_u32(row);
        if (id < mapping)
            low = middle + 1;
        else if (id > mapping)
            high = middle;
        else
            return qjs_intl_read_u32(row + 4);
    }
    return 0;
}

QJSIntlStatus qjs_intl_collation_keys_context(const QJSIntlCollationKeyIndex *view, uint32_t index,
                                              uint32_t *mapping, uint32_t *prefix)
{
    const uint8_t *row;
    if (mapping)
        *mapping = 0;
    if (prefix)
        *prefix = 0;
    if (!view || !mapping || !prefix || index >= view->context_count)
        return QJS_INTL_INVALID_ARGUMENT;
    row = view->contexts + (size_t)index * 12;
    *mapping = qjs_intl_read_u32(row);
    *prefix = qjs_intl_read_u32(row + 4);
    return QJS_INTL_OK;
}
