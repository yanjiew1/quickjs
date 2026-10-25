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
#include "resolve.h"
#include "identifier.h"
#include "../common/locale-grammar.h"
#include <string.h>

static int same(QJSIntlBytes a, QJSIntlBytes b)
{
    return !!a.data == !!b.data && a.length == b.length &&
           (!a.length || !memcmp(a.data, b.data, a.length));
}

static int key_index(const char key[2])
{
    int first = key[0] >= '0' && key[0] <= '9' ? key[0] - '0' :
                key[0] >= 'a' && key[0] <= 'z' ? key[0] - 'a' + 10 : -1;
    return first < 0 || key[1] < 'a' || key[1] > 'z' ? -1 : first * 26 + key[1] - 'a';
}

static QJSIntlStatus copy(const QJSIntlAllocator *allocator, QJSIntlBytes text, char **out)
{
    char *result;
    *out = NULL;
    if (!text.data)
        return QJS_INTL_OK;
    if (text.length == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    result = allocator->malloc(allocator->opaque, text.length + 1);
    if (!result)
        return QJS_INTL_NO_MEMORY;
    if (text.length)
        memcpy(result, text.data, text.length);
    result[text.length] = 0;
    *out = result;
    return QJS_INTL_OK;
}

static QJSIntlStatus negotiate(const QJSIntlLocaleService *identity,
                               const QJSIntlAllocator *allocator,
                               const QJSIntlLocaleResolveKey *key,
                               QJSIntlBytes requested, char **out, int *retained)
{
    QJSIntlBytes value = key->default_value;
    QJSIntlBytes truth = {"true", 4};
    QJSIntlStatus status;
    char *canonical = NULL;
    int accepted;
    *retained = 0;
    if (requested.data) {
        status = key->supported(key->opaque, requested.length ? requested : truth, &accepted);
        if (status)
            return status;
        if (accepted) {
            value = requested.length ? requested : truth;
            *retained = 1;
        }
    }
    if (key->option.data || key->option_is_null) {
        QJSIntlBytes option = {0};
        if (!key->option_is_null) {
            status = qjs_intl_locale_canonicalize_uvalue(identity,
                         (QJSIntlBytes){key->key, 2}, key->option, &canonical);
            if (status)
                return status;
            option = canonical[0] ? (QJSIntlBytes){canonical, strlen(canonical)} : truth;
        }
        if (!same(option, value)) {
            status = key->supported(key->opaque, option, &accepted);
            if (status)
                goto done;
            if (accepted) {
                value = option;
                *retained = 0;
            }
        }
    }
    status = copy(allocator, value, out);
done:
    qjs_intl_locale_result_free(identity, canonical);
    return status;
}

void qjs_intl_resolved_locale_clear(QJSIntlResolvedLocale *result)
{
    size_t i;
    if (!result)
        return;
    if (result->values) {
        for (i = 0; i < result->count; i++)
            if (result->values[i])
                result->allocator.free(result->allocator.opaque, result->values[i]);
        result->allocator.free(result->allocator.opaque, result->values);
    }
    if (result->locale)
        result->allocator.free(result->allocator.opaque, result->locale);
    memset(result, 0, sizeof(*result));
}

QJSIntlStatus qjs_intl_resolve_locale_keys(const QJSIntlLocaleService *identity,
                                           const QJSIntlAllocator *allocator,
                                           QJSIntlBytes base, QJSIntlBytes extension,
                                           const QJSIntlLocaleResolveKey *keys, size_t count,
                                           QJSIntlResolvedLocale *out)
{
    QJSIntlResolvedLocale result = {0};
    QJSIntlLocaleComponents components = {0}, base_parts;
    QJSIntlLocaleKeyword *keywords = NULL;
    unsigned char seen[36 * 26] = {0};
    char *composed = NULL, *canonical = NULL;
    QJSIntlStatus status;
    size_t i, retained = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !identity || !allocator || !allocator->malloc || !allocator->realloc ||
        !allocator->free || (!keys && count) || count > sizeof(seen) ||
        (!extension.data && extension.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_components(base, &base_parts);
    if (status || base_parts.base_name.length != base.length)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < count; i++) {
        int index = key_index(keys[i].key);
        const QJSIntlLocaleResolveKey *key = &keys[i];
        if (index < 0 || seen[index] || !key->supported || key->option_is_null > 1 ||
            (!key->default_value.data && key->default_value.length) ||
            (key->default_value.data && !qjs_intl_unicode_type_well_formed(key->default_value.data,
                                                                          key->default_value.length)) ||
            (!key->option.data && key->option.length) ||
            (key->option_is_null && key->option.data) ||
            (key->option.data && !qjs_intl_unicode_type_well_formed(key->option.data, key->option.length)))
            return QJS_INTL_INVALID_ARGUMENT;
        seen[index] = 1;
    }
    if (count > SIZE_MAX / sizeof(*result.values) || count > SIZE_MAX / sizeof(*keywords))
        return QJS_INTL_OVERFLOW;
    result.allocator = *allocator;
    result.count = count;
    components.unicode_extension = extension;
    if (count) {
        result.values = allocator->malloc(allocator->opaque, count * sizeof(*result.values));
        if (!result.values)
            return QJS_INTL_NO_MEMORY;
        memset(result.values, 0, count * sizeof(*result.values));
        keywords = allocator->malloc(allocator->opaque, count * sizeof(*keywords));
        if (!keywords) {
            status = QJS_INTL_NO_MEMORY;
            goto fail;
        }
    }
    for (i = 0; i < count; i++) {
        QJSIntlBytes requested = qjs_intl_locale_keyword(&components, keys[i].key);
        int keep;
        status = negotiate(identity, allocator, &keys[i], requested, &result.values[i], &keep);
        if (status)
            goto fail;
        if (keep)
            keywords[retained++] = (QJSIntlLocaleKeyword){{keys[i].key[0], keys[i].key[1]}, requested};
    }
    if (retained) {
        status = qjs_intl_locale_replace_keywords(allocator, base, keywords, retained, &composed);
        if (!status)
            status = qjs_intl_locale_canonicalize(identity,
                         (QJSIntlBytes){composed, strlen(composed)}, &canonical);
        if (!status)
            status = copy(allocator, (QJSIntlBytes){canonical, strlen(canonical)}, &result.locale);
    } else {
        status = copy(allocator, base, &result.locale);
    }
    if (status)
        goto fail;
    if (keywords)
        allocator->free(allocator->opaque, keywords);
    if (composed)
        allocator->free(allocator->opaque, composed);
    qjs_intl_locale_result_free(identity, canonical);
    *out = result;
    return QJS_INTL_OK;
fail:
    if (keywords)
        allocator->free(allocator->opaque, keywords);
    if (composed)
        allocator->free(allocator->opaque, composed);
    qjs_intl_locale_result_free(identity, canonical);
    qjs_intl_resolved_locale_clear(&result);
    return status;
}
