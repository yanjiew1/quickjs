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
#include "numbering.h"
#include <stdlib.h>
#include <string.h>
#include <unicode/uenum.h>
#include <unicode/unumsys.h>

static int compare(const void *a, const void *b)
{
    const QJSIntlBytes *left = a, *right = b;
    size_t count = left->length < right->length ? left->length : right->length;
    int order = count ? memcmp(left->data, right->data, count) : 0;

    return order ? order : (left->length > right->length) - (left->length < right->length);
}

static const QJSIntlBytes *find(const QJSICU4CNumbering *view, QJSIntlBytes name)
{
    size_t low = 0, high = view->count;

    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = compare(&view->names[middle], &name);

        if (!order)
            return &view->names[middle];
        if (order < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return NULL;
}

void qjs_icu4c_numbering_clear(QJSICU4CNumbering *view)
{
    size_t i;

    if (!view)
        return;
    for (i = 0; i < view->count; i++)
        view->allocator.free(view->allocator.opaque, (void *)view->names[i].data);
    if (view->names)
        view->allocator.free(view->allocator.opaque, view->names);
    if (view->defaults)
        view->allocator.free(view->allocator.opaque, view->defaults);
    memset(view, 0, sizeof(*view));
}

QJSIntlStatus qjs_icu4c_numbering_open(const QJSIntlAllocator *allocator,
                                       const QJSICU4CLocales *locales, QJSICU4CNumbering *out)
{
    QJSICU4CNumbering view = {0};
    QJSIntlStatus error = QJS_INTL_OK;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *names = NULL;
    const char *name;
    int32_t capacity;
    size_t i;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !locales ||
        !locales->count || !locales->tags)
        return QJS_INTL_INVALID_ARGUMENT;
    view.allocator = *allocator;
    names = unumsys_openAvailableNames(&status);
    capacity = uenum_count(names, &status);
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (capacity <= 0 || (size_t)capacity > SIZE_MAX / sizeof(*view.names) ||
        locales->count > SIZE_MAX / sizeof(*view.defaults)) {
        error = capacity <= 0 ? QJS_INTL_DATA_ERROR : QJS_INTL_OVERFLOW;
        goto done;
    }
    view.names = allocator->malloc(allocator->opaque, (size_t)capacity * sizeof(*view.names));
    view.defaults = allocator->malloc(allocator->opaque, locales->count * sizeof(*view.defaults));
    if (!view.names || !view.defaults) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    while ((name = uenum_next(names, NULL, &status))) {
        UNumberingSystem *system = unumsys_openByName(name, &status);
        int numeric =
            U_SUCCESS(status) && !unumsys_isAlgorithmic(system) && unumsys_getRadix(system) == 10;
        char *owned;

        unumsys_close(system);
        if (U_FAILURE(status))
            break;
        if (!numeric)
            continue;
        if (view.count >= (size_t)capacity) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        error = qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){name, strlen(name)}, &owned);
        if (error)
            goto done;
        view.names[view.count++] = (QJSIntlBytes){owned, strlen(owned)};
    }
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (!view.count) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    qsort(view.names, view.count, sizeof(*view.names), compare);
    for (i = 0; i < locales->count; i++) {
        UNumberingSystem *system;
        const QJSIntlBytes *preferred;
        char *id;

        error = qjs_icu4c_locale_id(allocator, locales->tags[i], &id);
        if (error)
            goto done;
        status = U_ZERO_ERROR;
        system = unumsys_open(id, &status);
        allocator->free(allocator->opaque, id);
        if (U_FAILURE(status)) {
            unumsys_close(system);
            error = qjs_icu4c_status(status);
            goto done;
        }
        name = unumsys_getName(system);
        preferred = name ? find(&view, (QJSIntlBytes){name, strlen(name)}) : NULL;
        if (!preferred) {
            unumsys_close(system);
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        view.defaults[i] = *preferred;
        unumsys_close(system);
    }
    view.locale_count = locales->count;
    *out = view;
    memset(&view, 0, sizeof(view));
done:
    uenum_close(names);
    qjs_icu4c_numbering_clear(&view);
    return error;
}

QJSIntlStatus qjs_icu4c_numbering_default(const QJSICU4CNumbering *view, size_t locale,
                                          QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !view || locale >= view->locale_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = view->defaults[locale];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_icu4c_numbering_supported(const QJSICU4CNumbering *view, QJSIntlBytes name,
                                            int *out)
{
    if (out)
        *out = 0;
    if (!out || !view || (name.length && !name.data))
        return QJS_INTL_INVALID_ARGUMENT;
    *out = name.length && find(view, name);
    return QJS_INTL_OK;
}
