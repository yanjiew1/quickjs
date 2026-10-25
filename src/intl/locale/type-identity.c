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
#include "type-identity.h"
#include <string.h>

static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return !compare(a, b);
}

static int alias_matches(QJSIntlBytes aliases, QJSIntlBytes spelling)
{
    size_t at = 0;
    while (at < aliases.length) {
        size_t start = at;
        while (at < aliases.length && aliases.data[at] != ' ')
            at++;
        if (equal((QJSIntlBytes){aliases.data + start, at - start}, spelling))
            return 1;
        if (at < aliases.length)
            at++;
    }
    return 0;
}

static QJSIntlStatus terminal(const QJSIntlLocaleMetadata *metadata, uint32_t index, uint32_t *out)
{
    uint32_t step;
    for (step = 0; step < metadata->types.count; step++) {
        QJSIntlLocaleType type;
        QJSIntlStatus status = qjs_intl_locale_type_at(metadata, index, &type);
        if (status || type.variable)
            return QJS_INTL_DATA_ERROR;
        if (!type.preferred.length) {
            *out = index;
            return QJS_INTL_OK;
        }
        status = qjs_intl_locale_type_find(metadata, type.extension, type.key, type.preferred, &index);
        if (status)
            return QJS_INTL_DATA_ERROR;
    }
    return QJS_INTL_DATA_ERROR;
}

QJSIntlStatus qjs_intl_locale_type_identity(const QJSIntlLocaleMetadata *metadata,
                                           QJSIntlBytes extension, QJSIntlBytes key,
                                           QJSIntlBytes spelling, uint32_t *out)
{
    uint32_t i, low = 0, high, chosen = 0;
    int found = 0;
    if (out)
        *out = 0;
    if (!out || !metadata || !metadata->types.bytes || (!extension.data && extension.length) ||
        (!key.data && key.length) || (!spelling.data && spelling.length))
        return QJS_INTL_INVALID_ARGUMENT;
    /* Source aliases need a scan, but only within this extension/key range;
       unrelated keys can contain much larger timezone or transform catalogs. */
    high = metadata->types.count;
    while (low < high) {
        QJSIntlLocaleType type;
        uint32_t mid = low + (high - low) / 2;
        int order;
        QJSIntlStatus status = qjs_intl_locale_type_at(metadata, mid, &type);
        if (status)
            return status;
        order = compare(type.extension, extension);
        if (!order)
            order = compare(type.key, key);
        if (order < 0)
            low = mid + 1;
        else
            high = mid;
    }
    for (i = low; i < metadata->types.count; i++) {
        QJSIntlLocaleType type;
        uint32_t target;
        QJSIntlStatus status = qjs_intl_locale_type_at(metadata, i, &type);
        if (status)
            return status;
        if (!equal(type.extension, extension) || !equal(type.key, key))
            break;
        if (type.variable || (!equal(type.name, spelling) && !alias_matches(type.aliases, spelling)))
            continue;
        status = terminal(metadata, i, &target);
        if (status)
            return status;
        if (found && target != chosen)
            return QJS_INTL_DATA_ERROR;
        chosen = target;
        found = 1;
    }
    if (!found)
        return QJS_INTL_UNSUPPORTED;
    *out = chosen;
    return QJS_INTL_OK;
}
