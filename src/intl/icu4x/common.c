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
#include "common.h"
#include <string.h>

QJSIntlStatus qjs_icu4x_status(int status)
{
    switch (status) {
    case QJS_ICU4X_OK:
        return QJS_INTL_OK;
    case QJS_ICU4X_RANGE:
        return QJS_INTL_INVALID_ARGUMENT;
    case QJS_ICU4X_UNSUPPORTED:
        return QJS_INTL_UNSUPPORTED;
    case QJS_ICU4X_MEMORY:
        return QJS_INTL_NO_MEMORY;
    default:
        return QJS_INTL_DATA_ERROR;
    }
}

typedef struct Inventory {
    QJSICU4XInventory *result;
    size_t count, length, capacity, bytes;
    QJSIntlStatus error;
    int filling;
} Inventory;

static int inventory_emit(void *opaque, const uint8_t *text, size_t length)
{
    Inventory *state = opaque;
    QJSICU4XInventory *result = state->result;

    if (!text || !length || memchr(text, 0, length))
        state->error = QJS_INTL_DATA_ERROR;
    else if (length == SIZE_MAX || state->length > SIZE_MAX - length - 1 ||
             state->count >= SIZE_MAX / sizeof(*result->items))
        state->error = QJS_INTL_OVERFLOW;
    if (state->error)
        return QJS_ICU4X_RANGE;
    if (state->filling) {
        char *copy;
        if (state->count >= state->capacity || length + 1 > state->bytes - state->length) {
            state->error = QJS_INTL_DATA_ERROR;
            return QJS_ICU4X_RANGE;
        }
        copy = result->storage + state->length;
        memcpy(copy, text, length);
        copy[length] = 0;
        if (state->count && strcmp(result->items[state->count - 1].data, copy) >= 0) {
            state->error = QJS_INTL_DATA_ERROR;
            return QJS_ICU4X_RANGE;
        }
        result->items[state->count] = (QJSIntlBytes){copy, length};
    }
    state->length += length + 1;
    state->count++;
    return QJS_ICU4X_OK;
}

void qjs_icu4x_inventory_clear(QJSICU4XInventory *value)
{
    if (value->items)
        value->allocator.free(value->allocator.opaque, value->items);
    if (value->storage)
        value->allocator.free(value->allocator.opaque, value->storage);
    memset(value, 0, sizeof(*value));
}

QJSIntlStatus qjs_icu4x_inventory_open_context(const QJSIntlAllocator *allocator,
                                               QJSICU4XEnumerateContext *enumerate, void *context,
                                               QJSICU4XInventory *out)
{
    QJSICU4XInventory result = {0};
    Inventory state = {.result = &result};
    QJSIntlStatus error;
    int status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !enumerate)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    status = enumerate(context, &state, inventory_emit);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (error)
        return error;
    if (!state.count)
        return QJS_INTL_DATA_ERROR;
    state.capacity = state.count;
    state.bytes = state.length;
    result.items = allocator->malloc(allocator->opaque, state.count * sizeof(*result.items));
    result.storage = allocator->malloc(allocator->opaque, state.length);
    if (!result.items || !result.storage) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    state.count = state.length = 0;
    state.filling = 1;
    status = enumerate(context, &state, inventory_emit);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (!error && (state.count != state.capacity || state.length != state.bytes))
        error = QJS_INTL_DATA_ERROR;
    if (error)
        goto fail;
    result.count = state.count;
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_icu4x_inventory_clear(&result);
    return error;
}

typedef struct SimpleEnumerator {
    QJSICU4XEnumerate *enumerate;
} SimpleEnumerator;

static int enumerate_simple(void *context, void *output, QJSICU4XEmit *emit)
{
    const SimpleEnumerator *state = context;
    return state->enumerate(output, emit);
}

QJSIntlStatus qjs_icu4x_inventory_open(const QJSIntlAllocator *allocator,
                                       QJSICU4XEnumerate *enumerate, QJSICU4XInventory *out)
{
    SimpleEnumerator context = {enumerate};
    if (!enumerate) {
        if (out)
            memset(out, 0, sizeof(*out));
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return qjs_icu4x_inventory_open_context(allocator, enumerate_simple, &context, out);
}
