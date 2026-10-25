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
    QJSICU4XLocales *result;
    size_t count, length, capacity, bytes;
    QJSIntlStatus error;
    int filling;
} Inventory;

static int inventory_emit(void *opaque, const uint8_t *text, size_t length)
{
    Inventory *state = opaque;
    QJSICU4XLocales *result = state->result;

    if (!text || !length || memchr(text, 0, length))
        state->error = QJS_INTL_DATA_ERROR;
    else if (length == SIZE_MAX || state->length > SIZE_MAX - length - 1 ||
             state->count >= SIZE_MAX / sizeof(*result->tags))
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
        if (state->count && strcmp(result->tags[state->count - 1].data, copy) >= 0) {
            state->error = QJS_INTL_DATA_ERROR;
            return QJS_ICU4X_RANGE;
        }
        result->tags[state->count] = (QJSIntlBytes){copy, length};
    }
    state->length += length + 1;
    state->count++;
    return QJS_ICU4X_OK;
}

void qjs_icu4x_locales_clear(QJSICU4XLocales *value)
{
    if (value->tags)
        value->allocator.free(value->allocator.opaque, value->tags);
    if (value->storage)
        value->allocator.free(value->allocator.opaque, value->storage);
    memset(value, 0, sizeof(*value));
}

QJSIntlStatus qjs_icu4x_locales_open(const QJSIntlAllocator *allocator,
                                     QJSICU4XEnumerate *enumerate, QJSICU4XLocales *out)
{
    QJSICU4XLocales result = {0};
    Inventory state = {.result = &result};
    QJSIntlStatus error;
    int status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !enumerate)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    status = enumerate(&state, inventory_emit);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (error)
        return error;
    if (!state.count)
        return QJS_INTL_DATA_ERROR;
    state.capacity = state.count;
    state.bytes = state.length;
    result.tags = allocator->malloc(allocator->opaque, state.count * sizeof(*result.tags));
    result.storage = allocator->malloc(allocator->opaque, state.length);
    if (!result.tags || !result.storage) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    state.count = state.length = 0;
    state.filling = 1;
    status = enumerate(&state, inventory_emit);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (!error && (state.count != state.capacity || state.length != state.bytes))
        error = QJS_INTL_DATA_ERROR;
    if (error)
        goto fail;
    result.count = state.count;
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_icu4x_locales_clear(&result);
    return error;
}
