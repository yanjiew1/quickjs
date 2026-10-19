/* CLDR source identifiers; distinct from caller Unicode locale identifiers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "source-locale.h"
#include "locale-grammar.h"
#include <stdlib.h>
#include <string.h>

static int lower(int c)
{
    return c >= 'a' && c <= 'z';
}
static int upper(int c)
{
    return c >= 'A' && c <= 'Z';
}
static int digit(int c)
{
    return c >= '0' && c <= '9';
}

static int compare_variants(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

QJSIntlStatus qjs_intl_source_locale_validate(const QJSIntlAllocator *allocator, QJSIntlBytes text)
{
    size_t at = 0, variant_start = 0, count = 0, index;
    unsigned int stage = 0;
    uint64_t *variants;
    QJSIntlStatus status = QJS_INTL_OK;
    if (text.data && text.length == 4 && !memcmp(text.data, "root", 4))
        return status;
    if (!text.data || !text.length)
        return QJS_INTL_DATA_ERROR;
    while (at < text.length) {
        size_t start = at, i;
        QJSIntlLocaleSubtagKind kind;
        while (at < text.length && text.data[at] != '-')
            at++;
        if (!stage) {
            kind = QJS_INTL_LANGUAGE_SUBTAG;
            stage = 1;
        } else if (stage == 1 &&
                   qjs_intl_unicode_subtag_well_formed(QJS_INTL_SCRIPT_SUBTAG,
                                                       text.data + start, at - start)) {
            kind = QJS_INTL_SCRIPT_SUBTAG;
            stage = 2;
        } else if (stage <= 2 &&
                   qjs_intl_unicode_subtag_well_formed(QJS_INTL_REGION_SUBTAG,
                                                       text.data + start, at - start)) {
            kind = QJS_INTL_REGION_SUBTAG;
            stage = 3;
        } else {
            kind = QJS_INTL_VARIANT_SUBTAG;
            stage = 4;
            if (!count)
                variant_start = start;
            count++;
        }
        if (!qjs_intl_unicode_subtag_well_formed(kind, text.data + start, at - start))
            return QJS_INTL_DATA_ERROR;
        for (i = start; i < at; i++) {
            int c = (unsigned char)text.data[i];
            int valid = kind == QJS_INTL_SCRIPT_SUBTAG
                            ? (i == start ? upper(c) : lower(c))
                        : kind == QJS_INTL_REGION_SUBTAG   ? upper(c) || digit(c)
                        : kind == QJS_INTL_LANGUAGE_SUBTAG ? lower(c)
                                                           : lower(c) || digit(c);
            if (!valid)
                return QJS_INTL_DATA_ERROR;
        }
        if (at < text.length && ++at == text.length)
            return QJS_INTL_DATA_ERROR;
    }
    if (count < 2)
        return status;
    if (count > SIZE_MAX / sizeof(*variants))
        return QJS_INTL_OVERFLOW;
    variants = allocator->malloc(allocator->opaque, count * sizeof(*variants));
    if (!variants)
        return QJS_INTL_NO_MEMORY;
    /* Each variant has at most eight nonzero ASCII bytes. Pack them with zero
       padding so duplicate detection remains O(n log n), even for long keys. */
    at = variant_start;
    for (index = 0; index < count; index++) {
        uint64_t value = 0;
        while (at < text.length && text.data[at] != '-')
            value = (value << 8) | (unsigned char)text.data[at++];
        variants[index] = value;
        if (at < text.length)
            at++;
    }
    qsort(variants, count, sizeof(*variants), compare_variants);
    for (index = 1; index < count; index++) {
        if (variants[index] == variants[index - 1]) {
            status = QJS_INTL_DATA_ERROR;
            break;
        }
    }
    allocator->free(allocator->opaque, variants);
    return status;
}

