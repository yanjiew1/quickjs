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
#include "../locale/identifier.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/uloc.h>

QJSIntlStatus qjs_icu4c_status(UErrorCode status)
{
    if (U_SUCCESS(status))
        return QJS_INTL_OK;
    if (status == U_MEMORY_ALLOCATION_ERROR)
        return QJS_INTL_NO_MEMORY;
    if (status == U_BUFFER_OVERFLOW_ERROR)
        return QJS_INTL_OVERFLOW;
    if (status == U_ILLEGAL_ARGUMENT_ERROR)
        return QJS_INTL_INVALID_ARGUMENT;
    if (status == U_UNSUPPORTED_ERROR)
        return QJS_INTL_UNSUPPORTED;
    return QJS_INTL_DATA_ERROR;
}

QJSIntlStatus qjs_icu4c_copy_bytes(const QJSIntlAllocator *allocator, QJSIntlBytes text, char **out)
{
    char *result;

    *out = NULL;
    if (text.length >= INT32_MAX)
        return QJS_INTL_OVERFLOW;
    if ((text.length && !text.data) || (text.length && memchr(text.data, 0, text.length)))
        return QJS_INTL_INVALID_ARGUMENT;
    result = allocator->malloc(allocator->opaque, text.length + 1);
    if (!result)
        return QJS_INTL_NO_MEMORY;
    if (text.length)
        memcpy(result, text.data, text.length);
    result[text.length] = 0;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_icu4c_language_tag(const QJSIntlAllocator *allocator, const char *id, char **out)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = uloc_toLanguageTag(id, NULL, 0, 1, &status);
    char *result;

    *out = NULL;
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length < 0 || length == INT32_MAX)
        return QJS_INTL_OVERFLOW;
    result = allocator->malloc(allocator->opaque, (size_t)length + 1);
    if (!result)
        return QJS_INTL_NO_MEMORY;
    uloc_toLanguageTag(id, result, length + 1, 1, &status);
    if (U_FAILURE(status)) {
        allocator->free(allocator->opaque, result);
        return qjs_icu4c_status(status);
    }
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_icu4c_locale_id(const QJSIntlAllocator *allocator, QJSIntlBytes tag, char **out)
{
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;
    char *input, *result = NULL;
    int32_t parsed = 0, length;

    *out = NULL;
    error = qjs_icu4c_copy_bytes(allocator, tag, &input);
    if (error)
        return error;
    length = uloc_forLanguageTag(input, NULL, 0, &parsed, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status) || parsed != (int32_t)tag.length) {
        error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_INVALID_ARGUMENT;
        goto done;
    }
    if (length < 0 || length == INT32_MAX) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    result = allocator->malloc(allocator->opaque, (size_t)length + 1);
    if (!result) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    uloc_forLanguageTag(input, result, length + 1, &parsed, &status);
    error = qjs_icu4c_status(status);
    if (!error) {
        *out = result;
        result = NULL;
    }
done:
    allocator->free(allocator->opaque, input);
    if (result)
        allocator->free(allocator->opaque, result);
    return error;
}

QJSIntlStatus qjs_icu4c_locale_keyword(const QJSIntlAllocator *allocator, const char *base,
                                       const char *key, QJSIntlBytes value, char **out)
{
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;
    size_t base_length = strlen(base);
    char *text, *result = NULL, *larger;
    int32_t length;
    const char *legacy_key, *legacy_value;

    *out = NULL;
    error = qjs_icu4c_copy_bytes(allocator, value, &text);
    if (error)
        return error;
    error = qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){base, base_length}, &result);
    if (error)
        goto done;
    /* Keyword mutation uses ICU legacy names, not Unicode extension types:
       co=phonebk must become collation=phonebook, ca=gregory becomes gregorian. */
    legacy_key = uloc_toLegacyKey(key);
    legacy_value = uloc_toLegacyType(key, text);
    if (!legacy_key || !legacy_value) {
        error = QJS_INTL_INVALID_ARGUMENT;
        goto done;
    }
    length =
        uloc_setKeywordValue(legacy_key, legacy_value, result, (int32_t)base_length + 1, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        if (length < 0 || length == INT32_MAX) {
            error = QJS_INTL_OVERFLOW;
            goto done;
        }
        larger = allocator->malloc(allocator->opaque, (size_t)length + 1);
        if (!larger) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
        memcpy(larger, base, base_length + 1);
        allocator->free(allocator->opaque, result);
        result = larger;
        uloc_setKeywordValue(legacy_key, legacy_value, result, length + 1, &status);
    }
    error = qjs_icu4c_status(status);
    if (!error) {
        *out = result;
        result = NULL;
    }
done:
    allocator->free(allocator->opaque, text);
    if (result)
        allocator->free(allocator->opaque, result);
    return error;
}

static int compare_tag(const void *a, const void *b)
{
    const QJSIntlBytes *left = a, *right = b;
    size_t length = left->length < right->length ? left->length : right->length;
    int order = memcmp(left->data, right->data, length);

    return order ? order : (left->length > right->length) - (left->length < right->length);
}

void qjs_icu4c_locales_clear(QJSICU4CLocales *locales)
{
    size_t i;

    if (!locales)
        return;
    for (i = 0; i < locales->count; i++)
        locales->allocator.free(locales->allocator.opaque, (void *)locales->tags[i].data);
    if (locales->tags)
        locales->allocator.free(locales->allocator.opaque, locales->tags);
    memset(locales, 0, sizeof(*locales));
}

QJSIntlStatus qjs_icu4c_locales_open(const QJSIntlAllocator *allocator, int32_t count,
                                     QJSICU4CLocaleAt *at, QJSICU4CLocales *out)
{
    QJSICU4CLocales value = {0};
    QJSIntlStatus error = QJS_INTL_OK;
    int32_t index;
    size_t read, write;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!allocator || !allocator->malloc || !allocator->free || !out || !at)
        return QJS_INTL_INVALID_ARGUMENT;
    if (count <= 0)
        return QJS_INTL_DATA_ERROR;
    if ((size_t)count > SIZE_MAX / (2 * sizeof(*value.tags)))
        return QJS_INTL_OVERFLOW;
    value.allocator = *allocator;
    value.tags = allocator->malloc(allocator->opaque, (size_t)count * 2 * sizeof(*value.tags));
    if (!value.tags)
        return QJS_INTL_NO_MEMORY;
    for (index = 0; index < count; index++) {
        const char *id = at(index);
        char *tag;
        size_t i, length;

        if (!id) {
            error = QJS_INTL_DATA_ERROR;
            goto fail;
        }
        if (!*id || !strcmp(id, "root"))
            continue;
        error = qjs_icu4c_language_tag(allocator, id, &tag);
        if (error)
            goto fail;
        length = strlen(tag);
        /* ICU's POSIX/traditional inventory entries can carry Unicode
           extensions. AvailableLocales contains their extension-free base;
           the core negotiates every relevant option separately. */
        for (i = 0; i + 2 < length; i++) {
            if (tag[i] == '-' && tag[i + 2] == '-') {
                tag[i] = 0;
                length = i;
                break;
            }
        }
        if (!length) {
            allocator->free(allocator->opaque, tag);
            error = QJS_INTL_DATA_ERROR;
            goto fail;
        }
        value.tags[value.count++] = (QJSIntlBytes){tag, length};
        /* AvailableLocales must also admit the language-region counterpart of
           a language-script-region entry. Otherwise Lookup may select a less
           specific language with different script data (zh-TW -> zh). */
        {
            QJSIntlLocaleComponents parts;
            char *counterpart;
            size_t size;

            error = qjs_intl_locale_components((QJSIntlBytes){tag, length}, &parts);
            if (error)
                goto fail;
            if (!parts.script.length || !parts.region.length)
                continue;
            size = parts.language.length + 1 + parts.region.length;
            counterpart = allocator->malloc(allocator->opaque, size + 1);
            if (!counterpart) {
                error = QJS_INTL_NO_MEMORY;
                goto fail;
            }
            memcpy(counterpart, parts.language.data, parts.language.length);
            counterpart[parts.language.length] = '-';
            memcpy(counterpart + parts.language.length + 1, parts.region.data, parts.region.length);
            counterpart[size] = 0;
            value.tags[value.count++] = (QJSIntlBytes){counterpart, size};
        }
    }
    if (!value.count) {
        error = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    qsort(value.tags, value.count, sizeof(*value.tags), compare_tag);
    for (read = write = 0; read < value.count; read++) {
        if (write && !compare_tag(&value.tags[write - 1], &value.tags[read]))
            allocator->free(allocator->opaque, (void *)value.tags[read].data);
        else
            value.tags[write++] = value.tags[read];
    }
    value.count = write;
    *out = value;
    return QJS_INTL_OK;
fail:
    qjs_icu4c_locales_clear(&value);
    return error;
}
