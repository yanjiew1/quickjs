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
#include "collation.h"
#include "../icu4c/common.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ucol.h>
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/uiter.h>

typedef struct ICUCollationLocale {
    QJSIntlCollationOptions defaults[2];
    QJSIntlBytes *types;
    size_t count;
} ICUCollationLocale;

struct QJSIntlCollationService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
    ICUCollationLocale *rows;
    QJSIntlBytes *types; /* Borrow strings owned by rows. */
    size_t count;
};

struct QJSIntlCollator {
    QJSIntlAllocator allocator;
    UCollator *handle;
};

static int type_order(const void *a, const void *b)
{
    const QJSIntlBytes *left = a, *right = b;
    size_t length = left->length < right->length ? left->length : right->length;
    int order = length ? memcmp(left->data, right->data, length) : 0;

    return order ? order : (left->length > right->length) - (left->length < right->length);
}

static int location(const QJSIntlCollationService *service, size_t locale,
                    QJSIntlCollationUsage usage)
{
    return service && locale < service->locales.count &&
           (unsigned int)usage <= QJS_INTL_COLLATION_SEARCH;
}

static QJSIntlStatus open_handle(const QJSIntlAllocator *allocator, QJSIntlBytes tag,
                                 QJSIntlCollationUsage usage, QJSIntlBytes type, UCollator **out)
{
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    char *locale = NULL, *selected = NULL;

    *out = NULL;
    error = qjs_icu4c_locale_id(allocator, tag, &locale);
    if (error)
        return error;
    if (usage == QJS_INTL_COLLATION_SEARCH)
        type = (QJSIntlBytes){"search", 6};
    if (type.length) {
        error = qjs_icu4c_locale_keyword(allocator, locale, "collation", type, &selected);
        if (error)
            goto done;
    }
    *out = ucol_open(selected ? selected : locale, &status);
    error = qjs_icu4c_status(status);
    if (error) {
        ucol_close(*out);
        *out = NULL;
    }
done:
    allocator->free(allocator->opaque, locale);
    if (selected)
        allocator->free(allocator->opaque, selected);
    return error;
}

static QJSIntlStatus read_defaults(const QJSIntlAllocator *allocator, QJSIntlBytes tag,
                                   QJSIntlCollationUsage usage, QJSIntlCollationOptions *out)
{
    UCollator *handle;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error = open_handle(allocator, tag, usage, (QJSIntlBytes){0}, &handle);
    UColAttributeValue strength, case_level, case_first;

    if (error)
        return error;
    memset(out, 0, sizeof(*out));
    out->usage = usage;
    out->sensitivity = QJS_INTL_COLLATION_VARIANT;
    strength = ucol_getAttribute(handle, UCOL_STRENGTH, &status);
    case_level = ucol_getAttribute(handle, UCOL_CASE_LEVEL, &status);
    case_first = ucol_getAttribute(handle, UCOL_CASE_FIRST, &status);
    if (usage == QJS_INTL_COLLATION_SEARCH) {
        if (strength == UCOL_PRIMARY)
            out->sensitivity =
                case_level == UCOL_ON ? QJS_INTL_COLLATION_CASE : QJS_INTL_COLLATION_BASE;
        else if (strength == UCOL_SECONDARY && case_level != UCOL_ON)
            out->sensitivity = QJS_INTL_COLLATION_ACCENT;
    }
    out->case_first = case_first == UCOL_UPPER_FIRST   ? QJS_INTL_COLLATION_UPPER_FIRST
                      : case_first == UCOL_LOWER_FIRST ? QJS_INTL_COLLATION_LOWER_FIRST
                                                       : QJS_INTL_COLLATION_CASE_OFF;
    out->numeric = ucol_getAttribute(handle, UCOL_NUMERIC_COLLATION, &status) == UCOL_ON;
    out->ignore_punctuation =
        ucol_getAttribute(handle, UCOL_ALTERNATE_HANDLING, &status) == UCOL_SHIFTED;
    ucol_close(handle);
    return qjs_icu4c_status(status);
}

static QJSIntlStatus read_types(QJSIntlCollationService *service, size_t index)
{
    const QJSIntlAllocator *allocator = &service->allocator;
    ICUCollationLocale *row = &service->rows[index];
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *values = NULL;
    QJSIntlStatus error;
    char *locale;
    const char *value;
    int32_t count;

    error = qjs_icu4c_locale_id(allocator, service->locales.tags[index], &locale);
    if (error)
        return error;
    values = ucol_getKeywordValuesForLocale("collation", locale, 0, &status);
    allocator->free(allocator->opaque, locale);
    count = uenum_count(values, &status);
    if (U_FAILURE(status) || count < 0) {
        error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
        goto done;
    }
    if ((size_t)count > SIZE_MAX / sizeof(*row->types)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (count) {
        row->types = allocator->malloc(allocator->opaque, (size_t)count * sizeof(*row->types));
        if (!row->types) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
    }
    while ((value = uenum_next(values, NULL, &status))) {
        const char *canonical = uloc_toUnicodeLocaleType("co", value);
        char *owned;

        /* Search is selected by usage, not by the collation option. ICU's
           "standard" is the null/default entry in ResolveLocale. */
        if (!canonical || !strcmp(canonical, "standard") || !strcmp(canonical, "search"))
            continue;
        error =
            qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){canonical, strlen(canonical)}, &owned);
        if (error)
            goto done;
        row->types[row->count++] = (QJSIntlBytes){owned, strlen(owned)};
    }
    error = qjs_icu4c_status(status);
    if (!error && row->count > 1) {
        size_t read, write = 0;

        qsort(row->types, row->count, sizeof(*row->types), type_order);
        for (read = 0; read < row->count; read++) {
            if (write && !type_order(&row->types[write - 1], &row->types[read]))
                allocator->free(allocator->opaque, (void *)row->types[read].data);
            else
                row->types[write++] = row->types[read];
        }
        row->count = write;
    }
done:
    uenum_close(values);
    return error;
}

QJSIntlStatus qjs_intl_collation_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                      QJSIntlCollationService **out)
{
    QJSIntlCollationService *service;
    QJSIntlStatus error;
    size_t i, count = 0, write = 0;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4c_locales_open(allocator, ucol_countAvailable(), ucol_getAvailable,
                                   &service->locales);
    if (error)
        goto fail;
    if (service->locales.count > SIZE_MAX / sizeof(*service->rows)) {
        error = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->rows =
        allocator->malloc(allocator->opaque, service->locales.count * sizeof(*service->rows));
    if (!service->rows) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    memset(service->rows, 0, service->locales.count * sizeof(*service->rows));
    for (i = 0; i < service->locales.count; i++) {
        ICUCollationLocale *row = &service->rows[i];

        error = read_types(service, i);
        if (!error)
            error = read_defaults(allocator, service->locales.tags[i], QJS_INTL_COLLATION_SORT,
                                  &row->defaults[0]);
        if (!error)
            error = read_defaults(allocator, service->locales.tags[i], QJS_INTL_COLLATION_SEARCH,
                                  &row->defaults[1]);
        if (error)
            goto fail;
        if (row->count > SIZE_MAX / sizeof(*service->types) - count) {
            error = QJS_INTL_OVERFLOW;
            goto fail;
        }
        count += row->count;
    }
    if (count) {
        service->types = allocator->malloc(allocator->opaque, count * sizeof(*service->types));
        if (!service->types) {
            error = QJS_INTL_NO_MEMORY;
            goto fail;
        }
        for (i = 0; i < service->locales.count; i++) {
            ICUCollationLocale *row = &service->rows[i];

            if (row->count)
                memcpy(service->types + write, row->types, row->count * sizeof(*row->types));
            write += row->count;
        }
        qsort(service->types, count, sizeof(*service->types), type_order);
        for (i = write = 0; i < count; i++) {
            if (!write || type_order(&service->types[write - 1], &service->types[i]))
                service->types[write++] = service->types[i];
        }
        service->count = write;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_collation_close(service);
    return error;
}

void qjs_intl_collation_close(QJSIntlCollationService *service)
{
    size_t i, j;
    QJSIntlAllocator allocator;

    if (!service)
        return;
    allocator = service->allocator;
    if (service->rows) {
        for (i = 0; i < service->locales.count; i++) {
            ICUCollationLocale *row = &service->rows[i];

            for (j = 0; j < row->count; j++)
                allocator.free(allocator.opaque, (void *)row->types[j].data);
            if (row->types)
                allocator.free(allocator.opaque, row->types);
        }
        allocator.free(allocator.opaque, service->rows);
    }
    if (service->types)
        allocator.free(allocator.opaque, service->types);
    qjs_icu4c_locales_clear(&service->locales);
    allocator.free(allocator.opaque, service);
}

QJSIntlStatus qjs_intl_collation_available(const QJSIntlCollationService *service,
                                           const QJSIntlBytes **out, size_t *count)
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

QJSIntlStatus qjs_intl_collation_defaults(const QJSIntlCollationService *service, size_t locale,
                                          QJSIntlCollationUsage usage, QJSIntlCollationOptions *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !location(service, locale, usage))
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->rows[locale].defaults[usage];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_type_supported(const QJSIntlCollationService *service,
                                                size_t locale, QJSIntlCollationUsage usage,
                                                QJSIntlBytes name, int *out)
{
    size_t i;

    if (out)
        *out = 0;
    if (!out || !location(service, locale, usage) || (name.length && !name.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (usage == QJS_INTL_COLLATION_SEARCH || !name.length)
        return QJS_INTL_OK;
    for (i = 0; i < service->rows[locale].count; i++) {
        if (!type_order(&name, &service->rows[locale].types[i])) {
            *out = 1;
            break;
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_type_next(const QJSIntlCollationService *service, size_t locale,
                                           QJSIntlCollationUsage usage, uint32_t *cursor,
                                           QJSIntlBytes *out)
{
    size_t count;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !cursor || !location(service, locale, usage))
        return QJS_INTL_INVALID_ARGUMENT;
    count = usage == QJS_INTL_COLLATION_SORT ? service->rows[locale].count : 0;
    if (*cursor > count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == count)
        return QJS_INTL_UNSUPPORTED;
    *out = service->rows[locale].types[(*cursor)++];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_available_type_next(const QJSIntlCollationService *service,
                                                     uint32_t *cursor, QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !cursor || !service || *cursor > service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == service->count)
        return QJS_INTL_UNSUPPORTED;
    *out = service->types[(*cursor)++];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collator_open(const QJSIntlCollationService *service, size_t locale,
                                     const QJSIntlCollationOptions *options, QJSIntlCollator **out)
{
    QJSIntlCollator *collator;
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    int supported;

    if (out)
        *out = NULL;
    if (!out || !options || !location(service, locale, options->usage) ||
        (unsigned int)options->sensitivity > QJS_INTL_COLLATION_CASE ||
        (unsigned int)options->case_first > QJS_INTL_COLLATION_LOWER_FIRST ||
        (unsigned int)options->numeric > 1 || (unsigned int)options->ignore_punctuation > 1 ||
        (options->collation.length && !options->collation.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (options->collation.length) {
        error = qjs_intl_collation_type_supported(service, locale, options->usage,
                                                  options->collation, &supported);
        if (error || !supported)
            return error ? error : QJS_INTL_UNSUPPORTED;
    }
    collator = service->allocator.malloc(service->allocator.opaque, sizeof(*collator));
    if (!collator)
        return QJS_INTL_NO_MEMORY;
    memset(collator, 0, sizeof(*collator));
    collator->allocator = service->allocator;
    error = open_handle(&collator->allocator, service->locales.tags[locale], options->usage,
                        options->collation, &collator->handle);
    if (error)
        goto fail;
    ucol_setAttribute(collator->handle, UCOL_STRENGTH,
                      options->sensitivity == QJS_INTL_COLLATION_VARIANT  ? UCOL_TERTIARY
                      : options->sensitivity == QJS_INTL_COLLATION_ACCENT ? UCOL_SECONDARY
                                                                          : UCOL_PRIMARY,
                      &status);
    ucol_setAttribute(collator->handle, UCOL_CASE_LEVEL,
                      options->sensitivity == QJS_INTL_COLLATION_CASE ? UCOL_ON : UCOL_OFF,
                      &status);
    ucol_setAttribute(collator->handle, UCOL_CASE_FIRST,
                      options->case_first == QJS_INTL_COLLATION_UPPER_FIRST   ? UCOL_UPPER_FIRST
                      : options->case_first == QJS_INTL_COLLATION_LOWER_FIRST ? UCOL_LOWER_FIRST
                                                                              : UCOL_OFF,
                      &status);
    ucol_setAttribute(collator->handle, UCOL_NUMERIC_COLLATION,
                      options->numeric ? UCOL_ON : UCOL_OFF, &status);
    ucol_setAttribute(collator->handle, UCOL_ALTERNATE_HANDLING,
                      options->ignore_punctuation ? UCOL_SHIFTED : UCOL_NON_IGNORABLE, &status);
    ucol_setMaxVariable(collator->handle, UCOL_REORDER_CODE_PUNCTUATION, &status);
    /* ECMA-402 requires canonical equivalence even for non-FCD input. ICU's
       locale default may disable this check, so resolve it explicitly. */
    ucol_setAttribute(collator->handle, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    error = qjs_icu4c_status(status);
    if (error)
        goto fail;
    *out = collator;
    return QJS_INTL_OK;
fail:
    qjs_intl_collator_close(collator);
    return error;
}

void qjs_intl_collator_close(QJSIntlCollator *collator)
{
    if (collator) {
        ucol_close(collator->handle);
        collator->allocator.free(collator->allocator.opaque, collator);
    }
}

QJSIntlStatus qjs_intl_collator_compare(const QJSIntlCollator *collator, const uint16_t *left,
                                        size_t left_length, const uint16_t *right,
                                        size_t right_length, int *out)
{
    static const UChar empty = 0;
    UCollationResult order;
    UCharIterator left_iterator, right_iterator;
    UErrorCode status = U_ZERO_ERROR;

    if (out)
        *out = 0;
    if (!out || !collator || (left_length && !left) || (right_length && !right))
        return QJS_INTL_INVALID_ARGUMENT;
    if (left_length > INT32_MAX || right_length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    /* The string convenience API discards ICU's internal allocation status.
       Counted iterators retain UTF-16 semantics and expose normalization OOM. */
    uiter_setString(&left_iterator, left ? left : &empty, (int32_t)left_length);
    uiter_setString(&right_iterator, right ? right : &empty, (int32_t)right_length);
    order = ucol_strcollIter(collator->handle, &left_iterator, &right_iterator, &status);
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    *out = (order > 0) - (order < 0);
    return QJS_INTL_OK;
}
