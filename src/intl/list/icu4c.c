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
#include "../icu4c/common.h"
#include <limits.h>
#include <string.h>
#include <unicode/ulistformatter.h>
#include <unicode/uloc.h>
#include <unicode/uformattedvalue.h>

struct QJSIntlListService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
};

struct QJSIntlListFormatter {
    QJSIntlAllocator allocator;
    UListFormatter *handle;
};

QJSIntlStatus qjs_intl_list_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                 QJSIntlListService **out)
{
    QJSIntlListService *service;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    /* ListFormatter has no separate C locale enumerator. Its CLDR bundle uses
       the installed ICU locale inventory and normal resource inheritance. */
    error = qjs_icu4c_locales_open(allocator, uloc_countAvailable(), uloc_getAvailable,
                                   &service->locales);
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
        qjs_icu4c_locales_clear(&service->locales);
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
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_list_formatter_open(const QJSIntlListService *service, size_t locale,
                                           QJSIntlListType type, QJSIntlListStyle style,
                                           QJSIntlListFormatter **out)
{
    static const UListFormatterType types[] = {ULISTFMT_TYPE_AND, ULISTFMT_TYPE_OR,
                                               ULISTFMT_TYPE_UNITS};
    static const UListFormatterWidth widths[] = {ULISTFMT_WIDTH_WIDE, ULISTFMT_WIDTH_SHORT,
                                                 ULISTFMT_WIDTH_NARROW};
    QJSIntlListFormatter *formatter;
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    char *id;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count ||
        (unsigned int)type > QJS_INTL_LIST_UNIT || (unsigned int)style > QJS_INTL_LIST_NARROW)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4c_locale_id(&service->allocator, service->locales.tags[locale], &id);
    if (error)
        return error;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter) {
        service->allocator.free(service->allocator.opaque, id);
        return QJS_INTL_NO_MEMORY;
    }
    formatter->allocator = service->allocator;
    formatter->handle = ulistfmt_openForType(id, types[type], widths[style], &status);
    service->allocator.free(service->allocator.opaque, id);
    error = qjs_icu4c_status(status);
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
        ulistfmt_close(formatter->handle);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

static void append(QJSIntlFormatted *out, const UChar *text, size_t length, QJSIntlPartType type)
{
    size_t start = out->length;

    if (length)
        memcpy(out->text + start, text, length * sizeof(*text));
    out->length += length;
    out->parts[out->part_count++] = (QJSIntlPart){start, out->length, type};
}

QJSIntlStatus qjs_intl_list_format(const QJSIntlListFormatter *formatter, const QJSIntlUTF16 *items,
                                   size_t count, QJSIntlFormatted *out)
{
    /* ICU suppresses empty argument spans (ICU-21871). Hebrew alef follows
       the same Spanish/Hebrew contextual branches as an empty string. Replace
       only empty items, then remove exactly their indexed spans. Compare with
       an unmodified ICU result too, so future contextual rules cannot silently
       change the text while reconstructing these mandatory empty parts. */
    static const UChar marker = 0x05d0;
    const QJSIntlAllocator *allocator;
    const UChar **strings = NULL, *text, *original_text;
    int32_t *lengths = NULL, length, original_length, previous = 0;
    UFormattedList *result = NULL, *original = NULL;
    UConstrainedFieldPosition *position = NULL;
    const UFormattedValue *value;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error = QJS_INTL_OK;
    QJSIntlFormatted formatted = {0};
    size_t i, empty = 0, seen = 0, input_length = 0;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || (count && !items))
        return QJS_INTL_INVALID_ARGUMENT;
    if (count > INT32_MAX || count > (SIZE_MAX / sizeof(QJSIntlPart) - 1) / 2 ||
        count > SIZE_MAX / sizeof(*strings) || count > SIZE_MAX / sizeof(*lengths))
        return QJS_INTL_OVERFLOW;
    if (!count)
        return QJS_INTL_OK;
    for (i = 0; i < count; i++) {
        if (items[i].length && !items[i].data)
            return QJS_INTL_INVALID_ARGUMENT;
        if (items[i].length > INT32_MAX)
            return QJS_INTL_OVERFLOW;
        if (items[i].length > (size_t)INT32_MAX - input_length)
            return QJS_INTL_OVERFLOW;
        input_length += items[i].length;
        empty += !items[i].length;
    }
    if (empty > (size_t)INT32_MAX - input_length)
        return QJS_INTL_OVERFLOW;
    allocator = &formatter->allocator;
    strings = allocator->malloc(allocator->opaque, count * sizeof(*strings));
    lengths = allocator->malloc(allocator->opaque, count * sizeof(*lengths));
    formatted.parts =
        allocator->malloc(allocator->opaque, (2 * count + 1) * sizeof(*formatted.parts));
    if (!strings || !lengths || !formatted.parts) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    for (i = 0; i < count; i++) {
        strings[i] = items[i].length ? items[i].data : &marker;
        lengths[i] = (int32_t)items[i].length;
    }
    result = ulistfmt_openResult(&status);
    original = empty ? ulistfmt_openResult(&status) : NULL;
    if (empty) {
        ulistfmt_formatStringsToResult(formatter->handle, strings, lengths, (int32_t)count,
                                       original, &status);
        for (i = 0; i < count; i++) {
            if (!items[i].length)
                lengths[i] = 1;
        }
    }
    ulistfmt_formatStringsToResult(formatter->handle, strings, lengths, (int32_t)count, result,
                                   &status);
    value = ulistfmt_resultAsValue(result, &status);
    text = ufmtval_getString(value, &length, &status);
    position = ucfpos_open(&status);
    ucfpos_constrainCategory(position, UFIELD_CATEGORY_LIST_SPAN, &status);
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (length < 0 || (size_t)length < empty ||
        (size_t)length > SIZE_MAX / sizeof(*formatted.text)) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if ((size_t)length > empty) {
        formatted.text = allocator->malloc(allocator->opaque,
                                           ((size_t)length - empty) * sizeof(*formatted.text));
        if (!formatted.text) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
    }
    while (ufmtval_nextPosition(value, position, &status)) {
        int32_t start, end, item = ucfpos_getField(position, &status);

        ucfpos_getIndexes(position, &start, &end, &status);
        if (U_FAILURE(status))
            break;
        /* Consume each indexed element exactly once, in displayed order.
           Reuse the now-unused positive input lengths as visitation flags. */
        if (item < 0 || (size_t)item >= count || lengths[item] < 0 || start < previous ||
            end < start || end > length ||
            (size_t)(end - start) != (items[item].length ? items[item].length : 1)) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        if (start > previous)
            append(&formatted, text + previous, (size_t)(start - previous), QJS_INTL_PART_LITERAL);
        append(&formatted, text + start, items[item].length, QJS_INTL_PART_ELEMENT);
        previous = end;
        lengths[item] = -1;
        seen++;
    }
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (seen != count) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if (previous < length)
        append(&formatted, text + previous, (size_t)(length - previous), QJS_INTL_PART_LITERAL);
    if (formatted.length != (size_t)length - empty) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if (empty) {
        const UFormattedValue *unmodified = ulistfmt_resultAsValue(original, &status);

        original_text = ufmtval_getString(unmodified, &original_length, &status);
        if (U_FAILURE(status)) {
            error = qjs_icu4c_status(status);
            goto done;
        }
        if (original_length < 0 || (size_t)original_length != formatted.length ||
            (formatted.length &&
             memcmp(original_text, formatted.text, formatted.length * sizeof(*formatted.text)))) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
    }
    *out = formatted;
    memset(&formatted, 0, sizeof(formatted));
done:
    qjs_intl_formatted_clear(allocator, &formatted);
    if (strings)
        allocator->free(allocator->opaque, strings);
    if (lengths)
        allocator->free(allocator->opaque, lengths);
    ucfpos_close(position);
    ulistfmt_closeResult(original);
    ulistfmt_closeResult(result);
    return error;
}

void qjs_intl_list_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_formatted_clear(allocator, out);
}
