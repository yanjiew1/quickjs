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
#include "list.h"
#include "../icu4x/common.h"
#include "../../icu4x/list.h"
#include <limits.h>
#include <string.h>

struct QJSIntlListService {
    QJSIntlAllocator allocator;
    QJSICU4XLocales locales;
};
struct QJSIntlListFormatter {
    QJSIntlAllocator allocator;
    QJSICU4XList *handle;
};

QJSIntlStatus qjs_intl_list_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                 QJSIntlListService **out)
{
    QJSIntlListService *service;
    QJSIntlStatus error;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4x_locales_open(allocator, qjs_icu4x_list_locales, &service->locales);
    if (error) {
        qjs_intl_list_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_list_close(QJSIntlListService *service)
{
    if (service) {
        qjs_icu4x_locales_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_list_available(const QJSIntlListService *service, const QJSIntlBytes **out,
                                      size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_list_formatter_open(const QJSIntlListService *service, size_t locale,
                                           QJSIntlListType type, QJSIntlListStyle style,
                                           QJSIntlListFormatter **out)
{
    QJSIntlListFormatter *formatter;
    QJSIntlStatus error;
    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count ||
        (unsigned)type > QJS_INTL_LIST_UNIT || (unsigned)style > QJS_INTL_LIST_NARROW)
        return QJS_INTL_INVALID_ARGUMENT;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    formatter->allocator = service->allocator;
    formatter->handle = NULL;
    error = qjs_icu4x_status(
        qjs_icu4x_list_open((const uint8_t *)service->locales.tags[locale].data,
                            service->locales.tags[locale].length, type, style, &formatter->handle));
    if (error) {
        qjs_intl_list_formatter_close(formatter);
        return error;
    }
    *out = formatter;
    return QJS_INTL_OK;
}

void qjs_intl_list_formatter_close(QJSIntlListFormatter *formatter)
{
    if (formatter) {
        qjs_icu4x_list_close(formatter->handle);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

typedef struct Output {
    QJSIntlFormatted result;
    size_t length, count, capacity, parts, covered;
    QJSIntlStatus error;
    int filling;
} Output;

static int write_text(void *opaque, const uint16_t *text, size_t length)
{
    Output *state = opaque;
    if (length > INT32_MAX - state->length)
        state->error = QJS_INTL_OVERFLOW;
    if (!state->error && state->filling) {
        if (length > state->capacity - state->length)
            state->error = QJS_INTL_DATA_ERROR;
        else if (length)
            memcpy(state->result.text + state->length, text, length * sizeof(*text));
    }
    if (state->error)
        return QJS_ICU4X_RANGE;
    state->length += length;
    return 0;
}

static int write_part(void *opaque, size_t start, size_t end, uint32_t type)
{
    Output *state = opaque;
    if (type > 1 || start != state->covered || end != state->length || start > end)
        state->error = QJS_INTL_DATA_ERROR;
    else if (state->count == SIZE_MAX / sizeof(*state->result.parts))
        state->error = QJS_INTL_OVERFLOW;
    if (!state->error && state->filling) {
        if (state->count == state->parts)
            state->error = QJS_INTL_DATA_ERROR;
        else
            state->result.parts[state->count] =
                (QJSIntlPart){start, end, type ? QJS_INTL_PART_ELEMENT : QJS_INTL_PART_LITERAL};
    }
    if (state->error)
        return QJS_ICU4X_RANGE;
    state->count++;
    state->covered = end;
    return 0;
}

QJSIntlStatus qjs_intl_list_format(const QJSIntlListFormatter *formatter, const QJSIntlUTF16 *items,
                                   size_t count, QJSIntlFormatted *out)
{
    Output state = {0};
    QJSICU4XListSink sink = {&state, write_text, write_part};
    const QJSIntlAllocator *allocator;
    QJSIntlStatus error;
    size_t i;
    int status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || (count && !items))
        return QJS_INTL_INVALID_ARGUMENT;
    if (count > INT32_MAX || count > SIZE_MAX / sizeof(*items))
        return QJS_INTL_OVERFLOW;
    allocator = &formatter->allocator;
    /* The bridge borrows the existing engine-independent UTF-16 descriptors;
       one common C type avoids both aliasing casts and an extra allocation. */
    for (i = 0; i < count; i++) {
        if (items[i].length && !items[i].data) {
            error = QJS_INTL_INVALID_ARGUMENT;
            goto done;
        }
        if (items[i].length > INT32_MAX) {
            error = QJS_INTL_OVERFLOW;
            goto done;
        }
    }
    status = qjs_icu4x_list_format(formatter->handle, items, count, sink);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (error)
        goto done;
    state.capacity = state.length;
    state.parts = state.count;
    if (state.length)
        state.result.text = allocator->malloc(allocator->opaque, state.length * sizeof(*out->text));
    if (state.count)
        state.result.parts =
            allocator->malloc(allocator->opaque, state.count * sizeof(*out->parts));
    if ((state.length && !state.result.text) || (state.count && !state.result.parts)) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    state.filling = 1;
    state.length = state.count = state.covered = 0;
    status = qjs_icu4x_list_format(formatter->handle, items, count, sink);
    error = state.error ? state.error : qjs_icu4x_status(status);
    if (!error && (state.length != state.capacity || state.count != state.parts ||
                   state.covered != state.length))
        error = QJS_INTL_DATA_ERROR;
    if (!error) {
        state.result.length = state.length;
        state.result.part_count = state.count;
        *out = state.result;
        memset(&state.result, 0, sizeof(state.result));
    }
done:
    qjs_intl_formatted_clear(allocator, &state.result);
    return error;
}

void qjs_intl_list_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_formatted_clear(allocator, out);
}
