/* Literal scalar DAG array views and validation, without container policy.
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
#include "literal-graph.h"
#include "../native/wire.h"
#include <string.h>

static uint32_t word(const uint8_t *array, uint32_t index)
{
    return qjs_intl_read_u32(array + (size_t)index * 4);
}

static uint32_t half(const uint8_t *array, uint32_t index)
{
    const uint8_t *p = array + (size_t)index * 2;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static QJSIntlStatus array(const uint8_t *data, size_t length, size_t *at,
                           size_t count, size_t width, const uint8_t **out)
{
    size_t padding = (4 - (*at & 3)) & 3;
    if (*at > length || padding > length - *at)
        return QJS_INTL_DATA_ERROR;
    while (padding) {
        if (data[(*at)++])
            return QJS_INTL_DATA_ERROR;
        padding--;
    }
    if (count > (length - *at) / width)
        return QJS_INTL_DATA_ERROR;
    *out = data + *at;
    *at += count * width;
    return QJS_INTL_OK;
}

static QJSIntlStatus arrays(const uint8_t *data, size_t length, QJSIntlSegmentLiteralGraph *out)
{
    size_t at = 0;
    QJSIntlStatus status;
    status = array(data, length, &at, out->alphabet_count, 4, &out->alphabet);
    if (!status)
        status = array(data, length, &at, out->value_count, 4, &out->values);
    if (!status)
        status = array(data, length, &at, (size_t)out->state_count + 1, 4, &out->starts);
    if (!status)
        status = array(data, length, &at, out->state_count, 2, &out->terminals);
    if (!status)
        status = array(data, length, &at, out->edge_count, 2, &out->labels);
    if (!status)
        status = array(data, length, &at, out->edge_count, 4, &out->targets);
    return status ? status : at == length ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

QJSIntlStatus qjs_intl_segment_literal_graph_bind(const uint8_t *bytes, size_t length,
                                                   const QJSIntlSegmentLiteralGraph *metadata,
                                                   QJSIntlSegmentLiteralGraph *out)
{
    QJSIntlSegmentLiteralGraph result;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !bytes || !metadata)
        return QJS_INTL_INVALID_ARGUMENT;
    result = *metadata;
    if (!result.words || !result.max_scalars || !result.max_utf16 ||
        !result.alphabet_count || result.alphabet_count > 65536 ||
        !result.value_count || result.value_count > 65535 ||
        result.state_count < 2 || result.state_count > 1000000 ||
        !result.edge_count || result.edge_count > 2000000 ||
        result.root != result.state_count - 1)
        return QJS_INTL_DATA_ERROR;
    status = arrays(bytes, length, &result);
    if (status)
        return status;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_literal_graph_validate(const QJSIntlSegmentLiteralGraph *data,
                                                       const QJSIntlAllocator *allocator)
{
    uint32_t *paths, *scalars, *units, state, i;
    uint8_t *used_alphabet, *used_values, *reachable;
    size_t size, extra;
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    void *scratch;
    if (!data || !data->alphabet || !data->values || !data->starts ||
        !data->terminals || !data->labels || !data->targets ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < data->alphabet_count; i++) {
        uint32_t scalar = word(data->alphabet, i);
        if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff) ||
            (i && scalar <= word(data->alphabet, i - 1)))
            return QJS_INTL_DATA_ERROR;
    }
    for (i = 1; i < data->value_count; i++)
        if (word(data->values, i) <= word(data->values, i - 1))
            return QJS_INTL_DATA_ERROR;
    if (word(data->starts, 0) || word(data->starts, data->state_count) != data->edge_count ||
        half(data->terminals, data->root))
        return QJS_INTL_DATA_ERROR;

    /* Three u32 DP vectors and byte flags. Enforce bounds before arithmetic,
       including on 32-bit targets; no scratch remains in a published view. */
    size = data->state_count;
    if (size > SIZE_MAX / 13)
        return QJS_INTL_OVERFLOW;
    size *= 13;
    extra = (size_t)data->alphabet_count + data->value_count + 1;
    if (extra > SIZE_MAX - size)
        return QJS_INTL_OVERFLOW;
    size += extra;
    scratch = allocator->malloc(allocator->opaque, size);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    paths = scratch;
    scalars = paths + data->state_count;
    units = scalars + data->state_count;
    used_alphabet = (uint8_t *)(units + data->state_count);
    used_values = used_alphabet + data->alphabet_count;
    reachable = used_values + data->value_count + 1;
    memset(used_alphabet, 0, extra + data->state_count);
    for (state = 0; state < data->state_count; state++) {
        uint32_t first = word(data->starts, state), last = word(data->starts, state + 1);
        uint32_t terminal = half(data->terminals, state), count = terminal != 0;
        uint32_t scalar_max = 0, unit_max = 0, previous = 0;
        if (first > last || last > data->edge_count || terminal > data->value_count)
            goto done;
        used_values[terminal] = 1;
        for (i = first; i < last; i++) {
            uint32_t label = half(data->labels, i), child = word(data->targets, i), width;
            if (label >= data->alphabet_count || (i > first && label <= previous) || child >= state)
                goto done;
            previous = label;
            used_alphabet[label] = 1;
            if (paths[child] > data->words - count)
                goto done;
            count += paths[child];
            width = word(data->alphabet, label) > 0xffff ? 2 : 1;
            if (scalars[child] >= data->max_scalars || width > data->max_utf16 ||
                units[child] > data->max_utf16 - width)
                goto done;
            if (1 + scalars[child] > scalar_max)
                scalar_max = 1 + scalars[child];
            if (width + units[child] > unit_max)
                unit_max = width + units[child];
        }
        if (!count)
            goto done;
        paths[state] = count;
        scalars[state] = scalar_max;
        units[state] = unit_max;
    }
    if (paths[data->root] != data->words || scalars[data->root] != data->max_scalars ||
        units[data->root] != data->max_utf16)
        goto done;
    for (i = 0; i < data->alphabet_count; i++)
        if (!used_alphabet[i])
            goto done;
    for (i = 1; i <= data->value_count; i++)
        if (!used_values[i])
            goto done;
    /* Descending order marks each child's reachability before visiting it. */
    reachable[data->root] = 1;
    for (state = data->state_count; state-- > 0;) {
        if (!reachable[state])
            goto done;
        for (i = word(data->starts, state); i < word(data->starts, state + 1); i++)
            reachable[word(data->targets, i)] = 1;
    }
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, scratch);
    return status;
}

QJSIntlStatus qjs_intl_segment_literal_graph_step(const QJSIntlSegmentLiteralGraph *data, uint32_t state,
                                               uint32_t scalar, uint32_t *out)
{
    uint32_t low, high;
    if (out)
        *out = 0;
    if (!out || !data || !data->starts || state >= data->state_count)
        return QJS_INTL_INVALID_ARGUMENT;
    low = word(data->starts, state);
    high = word(data->starts, state + 1);
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t value = word(data->alphabet, half(data->labels, middle));
        if (value < scalar)
            low = middle + 1;
        else
            high = middle;
    }
    if (low == word(data->starts, state + 1) ||
        word(data->alphabet, half(data->labels, low)) != scalar)
        return QJS_INTL_UNSUPPORTED;
    *out = word(data->targets, low);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_literal_graph_value(const QJSIntlSegmentLiteralGraph *data, uint32_t state,
                                                uint32_t *out)
{
    uint32_t terminal;
    if (out)
        *out = 0;
    if (!out || !data || !data->terminals || state >= data->state_count)
        return QJS_INTL_INVALID_ARGUMENT;
    terminal = half(data->terminals, state);
    if (!terminal)
        return QJS_INTL_UNSUPPORTED;
    *out = word(data->values, terminal - 1);
    return QJS_INTL_OK;
}
