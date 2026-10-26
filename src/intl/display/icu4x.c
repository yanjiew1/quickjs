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
#include "display.h"
#include "icu4x-data.h"
#include "../icu4x/common.h"
#include "../common/locale-grammar.h"
#include "../../icu4x/names.h"
#include "../../icu4x/currency.h"
#include <limits.h>
#include <string.h>

struct QJSIntlDisplayService {
    QJSIntlAllocator allocator;
    QJSICU4XInventory locales, currencies;
};
struct QJSIntlDisplayFormatter {
    QJSIntlAllocator allocator;
    QJSIntlDisplayOptions options;
    const QJSICU4XDisplayLocale *source;
    QJSICU4XNames *handle;
};

static int order(QJSIntlBytes left, QJSIntlBytes right)
{
    size_t common = left.length < right.length ? left.length : right.length;
    int result = common ? memcmp(left.data, right.data, common) : 0;
    return result ? result : (left.length > right.length) - (left.length < right.length);
}

static QJSIntlBytes text(uint32_t offset)
{
    const char *value = qjs_icu4x_display_strings + offset;
    return (QJSIntlBytes){value, strlen(value)};
}

static const QJSICU4XDisplayLocale *source_locale(QJSIntlBytes tag)
{
    size_t low = 0, high = qjs_icu4x_display_locale_count;

    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const QJSICU4XDisplayLocale *row = &qjs_icu4x_display_locales[middle];
        int result = order(text(row->tag), tag);
        if (!result)
            return row;
        if (result < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return NULL;
}

typedef struct Enumeration {
    void *opaque;
    QJSICU4XEmit *emit;
    const QJSIntlDisplayService *service;
} Enumeration;

static int locale_emit(void *opaque, const uint8_t *value, size_t length)
{
    Enumeration *state = opaque;
    // The public payload inventory establishes name coverage; the matching
    // source registry establishes the companion labels/context coverage.
    if (!source_locale((QJSIntlBytes){(const char *)value, length}))
        return QJS_ICU4X_OK;
    return state->emit(state->opaque, value, length);
}

static int enumerate_locales(void *opaque, QJSICU4XEmit *emit)
{
    Enumeration state = {opaque, emit, NULL};
    return qjs_icu4x_names_locales(&state, locale_emit);
}

static int currency_emit(void *opaque, const uint8_t *code, size_t length)
{
    Enumeration *state = opaque;
    const QJSICU4XInventory *locales = &state->service->locales;
    size_t i;
    int status;

    // A code present only in an excluded library locale cannot establish this
    // service's supported-values inventory. Test public fallback in admitted
    // locales; no text equality or symbols/ISO metadata substitute for presence.
    for (i = 0; i < locales->count; i++) {
        status = qjs_icu4x_currency_present((const uint8_t *)locales->items[i].data,
                                            locales->items[i].length, code, length);
        if (!status)
            return state->emit(state->opaque, code, length);
        if (status != QJS_ICU4X_UNSUPPORTED)
            return status;
    }
    return QJS_ICU4X_OK;
}

static int enumerate_currencies(void *context, void *opaque, QJSICU4XEmit *emit)
{
    Enumeration state = {opaque, emit, context};
    return qjs_icu4x_currency_names(&state, currency_emit);
}

QJSIntlStatus qjs_intl_display_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                    QJSIntlDisplayService **out)
{
    QJSIntlDisplayService *service;
    QJSIntlStatus error;
    QJSIntlBytes version = text(qjs_icu4x_display_version);

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4x_status(
        qjs_icu4x_names_check_version((const uint8_t *)version.data, version.length));
    if (error)
        return error;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4x_inventory_open(allocator, enumerate_locales, &service->locales);
    if (!error)
        error = qjs_icu4x_inventory_open_context(allocator, enumerate_currencies, service,
                                                 &service->currencies);
    if (error) {
        qjs_intl_display_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_display_close(QJSIntlDisplayService *service)
{
    if (service) {
        qjs_icu4x_inventory_clear(&service->currencies);
        qjs_icu4x_inventory_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_display_available(const QJSIntlDisplayService *service,
                                         const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.items;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_display_currency_next(const QJSIntlDisplayService *service, uint32_t *cursor,
                                             QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || !cursor || *cursor > service->currencies.count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == service->currencies.count)
        return QJS_INTL_UNSUPPORTED;
    *out = service->currencies.items[(*cursor)++];
    return QJS_INTL_OK;
}

void qjs_intl_display_formatter_close(QJSIntlDisplayFormatter *formatter)
{
    if (formatter) {
        qjs_icu4x_names_close(formatter->handle);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

QJSIntlStatus qjs_intl_display_formatter_open(const QJSIntlDisplayService *service, size_t locale,
                                              const QJSIntlDisplayOptions *options,
                                              QJSIntlDisplayFormatter **out)
{
    QJSIntlDisplayFormatter *formatter;
    QJSIntlStatus error;
    const QJSICU4XDisplayLocale *source;

    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->locales.count ||
        (unsigned)options->type >= QJS_INTL_DISPLAY_TYPE_COUNT ||
        (unsigned)options->style > QJS_INTL_NAME_NARROW || (unsigned)options->dialect > 1 ||
        !options->maximum_output_length ||
        options->maximum_output_length > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_INVALID_ARGUMENT;
    source = source_locale(service->locales.items[locale]);
    if (!source)
        return QJS_INTL_DATA_ERROR;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->options = *options;
    formatter->source = source;
    error = qjs_icu4x_status(qjs_icu4x_names_open_context(
        (const uint8_t *)service->locales.items[locale].data, service->locales.items[locale].length,
        options->style, options->dialect, source->titlecase, &formatter->handle));
    if (error) {
        qjs_intl_display_formatter_close(formatter);
        return error;
    }
    *out = formatter;
    return QJS_INTL_OK;
}

void qjs_intl_name_text_clear(const QJSIntlAllocator *allocator, QJSIntlNameText *out)
{
    if (!out)
        return;
    if (out->text)
        allocator->free(allocator->opaque, out->text);
    memset(out, 0, sizeof(*out));
}

static int label(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code, QJSIntlBytes *out)
{
    unsigned int slot = formatter->options.type == QJS_INTL_DISPLAY_SCRIPT     ? 0
                        : formatter->options.type == QJS_INTL_DISPLAY_CALENDAR ? 1
                                                                               : 2;
    const QJSICU4XDisplayMap *map = &qjs_icu4x_display_maps[formatter->source->maps[slot]];
    size_t low = map->begin, high = low + map->count, limit = high;

    // Locate the first code; at most three disjoint width records can follow.
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (order(text(qjs_icu4x_display_labels[middle].code), code) < 0)
            low = middle + 1;
        else
            high = middle;
    }
    while (low < limit) {
        const QJSICU4XDisplayLabel *row = &qjs_icu4x_display_labels[low++];
        if (order(text(row->code), code))
            break;
        if (row->styles & (1u << formatter->options.style)) {
            *out = text(row->text);
            return 1;
        }
    }
    return 0;
}

typedef struct NameOutput {
    uint16_t *text;
    size_t length, limit;
    QJSIntlStatus error;
} NameOutput;

static int write_name(void *opaque, const uint16_t *value, size_t length)
{
    NameOutput *out = opaque;
    if (length > out->limit - out->length) {
        out->error = QJS_INTL_OVERFLOW;
        return QJS_ICU4X_RANGE;
    }
    if (out->text && length)
        memcpy(out->text + out->length, value, length * sizeof(*value));
    out->length += length;
    return QJS_ICU4X_OK;
}

static QJSIntlStatus render(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code,
                            int literal, QJSIntlBytes value, NameOutput *output)
{
    QJSICU4XTextSink sink = {output, write_name};
    int status = literal ? qjs_icu4x_names_text(formatter->handle, formatter->options.type,
                                                (const uint8_t *)value.data, value.length, sink)
                         : qjs_icu4x_names_format(formatter->handle, formatter->options.type,
                                                  (const uint8_t *)code.data, code.length, sink);
    return output->error ? output->error : qjs_icu4x_status(status);
}

QJSIntlStatus qjs_intl_display_format(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code,
                                      QJSIntlNameText *out)
{
    QJSIntlNameText result = {0};
    NameOutput output = {0};
    QJSIntlBytes value = {0};
    QJSIntlStatus error;
    size_t expected;
    int literal = 0;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !code.data || !code.length || code.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    if (formatter->options.type == QJS_INTL_DISPLAY_CALENDAR &&
        !qjs_intl_unicode_type_well_formed(code.data, code.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (formatter->options.type == QJS_INTL_DISPLAY_SCRIPT ||
        formatter->options.type == QJS_INTL_DISPLAY_CALENDAR ||
        formatter->options.type == QJS_INTL_DISPLAY_DATE_TIME_FIELD) {
        literal = label(formatter, code, &value);
        if (!literal && formatter->options.type != QJS_INTL_DISPLAY_SCRIPT)
            return QJS_INTL_OK;
    }
    output.limit = formatter->options.maximum_output_length;
    error = render(formatter, code, literal, value, &output);
    if (error == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (error)
        return error;
    expected = output.length;
    if (expected > SIZE_MAX / sizeof(*result.text))
        return QJS_INTL_OVERFLOW;
    if (expected) {
        result.text = formatter->allocator.malloc(formatter->allocator.opaque,
                                                  expected * sizeof(*result.text));
        if (!result.text)
            return QJS_INTL_NO_MEMORY;
    }
    output = (NameOutput){result.text, 0, expected, QJS_INTL_OK};
    error = render(formatter, code, literal, value, &output);
    if (!error && output.length != expected)
        error = QJS_INTL_DATA_ERROR;
    if (error) {
        qjs_intl_name_text_clear(&formatter->allocator, &result);
        return error;
    }
    result.length = expected;
    result.found = 1;
    *out = result;
    return QJS_INTL_OK;
}
