/* Minimal/native share algorithms; only the prepared immutable data differs.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "ranked-compare.h"
#include "../native/data.h"
#include <stdlib.h>
#include <string.h>

struct QJSIntlCollationService {
    QJSIntlAllocator allocator;
    QJSIntlCompiledCollationData data;
    QJSIntlBytes *available, *types;
    uint32_t *starts;
    size_t locale_count, type_count;
};

struct QJSIntlCollator {
    QJSIntlAllocator allocator;
    QJSIntlCompiledCollationProgram program;
    QJSIntlCollationPolicy policy;
};

static int text_order(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;

    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static int type_order(const void *a, const void *b)
{
    return text_order(*(const QJSIntlBytes *)a, *(const QJSIntlBytes *)b);
}

static uint32_t target(const QJSIntlCollationBinding *binding, QJSIntlCollationUsage usage)
{
    return usage == QJS_INTL_COLLATION_SORT ? binding->sort_program : binding->search_program;
}

static int location(const QJSIntlCollationService *service, size_t locale, QJSIntlCollationUsage usage)
{
    return service && locale < service->locale_count && (unsigned int)usage <= QJS_INTL_COLLATION_SEARCH;
}

static uint32_t find_program(const QJSIntlCollationService *service, size_t locale,
                             QJSIntlCollationUsage usage, QJSIntlBytes name)
{
    uint32_t low = service->starts[locale], high = service->starts[locale + 1];
    QJSIntlCollationBinding binding;

    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        int order;

        qjs_intl_compiled_collation_binding(&service->data, middle, &binding);
        order = text_order(binding.collation, name);
        if (order < 0)
            low = middle + 1;
        else if (order > 0)
            high = middle;
        else
            return target(&binding, usage);
    }
    return UINT32_MAX;
}

QJSIntlStatus qjs_intl_collation_open(const QJSIntlAllocator *allocator,
                                      const struct QJSIntlData *checked_data,
                                      QJSIntlCollationService **out)
{
    QJSIntlCollationService *service;
    QJSIntlData outer;
    QJSIntlSection section;
    QJSIntlStatus status;
    uint32_t i, outer_index = 0;
    size_t count, binding_count, found = 0, unique = 0;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    if (checked_data && !checked_data->pack.bytes)
        return QJS_INTL_INVALID_ARGUMENT;

    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;

    if (checked_data) {
        outer = *checked_data;
        status = QJS_INTL_OK;
    } else {
        status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &outer);
    }
    if (!status)
        status = qjs_intl_pack_section(&outer.pack, QJS_INTL_SECTION_COMPILED_COLLATION, &section);
    /* The outer generation hashes a complete recipe, not just the CLDR source
       manifest. Source pairing happened during preparation; never substitute
       outer.pack.generation for this reader's expected source fingerprint. */
    if (!status)
        status = qjs_intl_compiled_collation_open(section.bytes, section.length, NULL, allocator, &service->data);
    if (status)
        goto fail;
    if (outer.locales.count < 2 || !service->data.sections[QJS_INTL_COMPILED_BINDINGS].count ||
        section.count != service->data.program_count) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }

    count = outer.locales.count - 1;
    binding_count = service->data.sections[QJS_INTL_COMPILED_BINDINGS].count;
    if (count > SIZE_MAX / sizeof(*service->available) ||
        count > SIZE_MAX / sizeof(*service->starts) - 1 ||
        binding_count > SIZE_MAX / sizeof(*service->types)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available = allocator->malloc(allocator->opaque, count * sizeof(*service->available));
    service->starts = allocator->malloc(allocator->opaque, (count + 1) * sizeof(*service->starts));
    service->types = allocator->malloc(allocator->opaque, binding_count * sizeof(*service->types));
    if (!service->available || !service->starts || !service->types) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }

    /* Default rows define public service identity. Prove the crosswalk to the
       outer profile rather than exposing source-parent or program labels. */
    for (i = 0; i < binding_count; i++) {
        QJSIntlCollationBinding binding;

        qjs_intl_compiled_collation_binding(&service->data, i, &binding);
        if (!binding.collation.length) {
            QJSIntlBytes tag;
            uint32_t parent;

            do {
                if (found == count || outer_index == outer.locales.count) {
                    status = QJS_INTL_DATA_ERROR;
                    goto fail;
                }
                qjs_intl_data_locale(&outer, outer_index++, &tag, &parent);
            } while (parent == UINT32_MAX);
            if (text_order(tag, binding.locale)) {
                status = QJS_INTL_DATA_ERROR;
                goto fail;
            }
            service->available[found] = binding.locale;
            service->starts[found++] = i;
        } else {
            service->types[service->type_count++] = binding.collation;
        }
    }
    if (found != count) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    service->locale_count = count;
    service->starts[count] = i;

    qsort(service->types, service->type_count, sizeof(*service->types), type_order);
    for (i = 0; i < service->type_count; i++) {
        if (!unique || text_order(service->types[unique - 1], service->types[i]))
            service->types[unique++] = service->types[i];
    }
    service->type_count = unique;
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_collation_close(service);
    return status;
}

void qjs_intl_collation_close(QJSIntlCollationService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;

        if (service->available)
            allocator.free(allocator.opaque, service->available);
        if (service->starts)
            allocator.free(allocator.opaque, service->starts);
        if (service->types)
            allocator.free(allocator.opaque, service->types);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_collation_available(const QJSIntlCollationService *service,
                                           const QJSIntlBytes **locales, size_t *count)
{
    if (locales)
        *locales = NULL;
    if (count)
        *count = 0;
    if (!service || !locales || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *locales = service->available;
    *count = service->locale_count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_defaults(const QJSIntlCollationService *service, size_t locale,
                                          QJSIntlCollationUsage usage, QJSIntlCollationOptions *out)
{
    QJSIntlCompiledCollationProgram program;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !location(service, locale, usage))
        return QJS_INTL_INVALID_ARGUMENT;

    qjs_intl_compiled_collation_program(&service->data,
        find_program(service, locale, usage, (QJSIntlBytes){0}), &program);
    out->usage = usage;
    out->sensitivity = QJS_INTL_COLLATION_VARIANT;
    if (usage == QJS_INTL_COLLATION_SEARCH) {
        if (program.policy.strength == QJS_INTL_COLLATION_STRENGTH_PRIMARY)
            out->sensitivity = program.policy.case_level ? QJS_INTL_COLLATION_CASE : QJS_INTL_COLLATION_BASE;
        else if (program.policy.strength == QJS_INTL_COLLATION_STRENGTH_SECONDARY && !program.policy.case_level)
            out->sensitivity = QJS_INTL_COLLATION_ACCENT;
    }
    out->case_first = program.policy.case_first;
    out->numeric = program.policy.numeric;
    out->ignore_punctuation = program.policy.shifted;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_type_supported(const QJSIntlCollationService *service, size_t locale,
                                                QJSIntlCollationUsage usage, QJSIntlBytes name, int *out)
{
    if (out)
        *out = 0;
    if (!out || !location(service, locale, usage) || (name.length && !name.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (name.length)
        *out = find_program(service, locale, usage, name) != UINT32_MAX;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_type_next(const QJSIntlCollationService *service, size_t locale,
                                           QJSIntlCollationUsage usage, uint32_t *cursor, QJSIntlBytes *out)
{
    uint32_t count;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !cursor || !location(service, locale, usage))
        return QJS_INTL_INVALID_ARGUMENT;
    count = service->starts[locale + 1] - service->starts[locale];
    if (*cursor > count)
        return QJS_INTL_INVALID_ARGUMENT;
    while (*cursor < count) {
        QJSIntlCollationBinding binding;

        qjs_intl_compiled_collation_binding(&service->data, service->starts[locale] + (*cursor)++, &binding);
        if (binding.collation.length && target(&binding, usage) != UINT32_MAX) {
            *out = binding.collation;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_collation_available_type_next(const QJSIntlCollationService *service,
                                                     uint32_t *cursor, QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !cursor || !service || *cursor > service->type_count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == service->type_count)
        return QJS_INTL_UNSUPPORTED;
    *out = service->types[(*cursor)++];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collator_open(const QJSIntlCollationService *service, size_t locale,
                                     const QJSIntlCollationOptions *options, QJSIntlCollator **out)
{
    QJSIntlCollator *collator;
    uint32_t program;

    if (out)
        *out = NULL;
    if (!out || !options || !location(service, locale, options->usage) ||
        (unsigned int)options->sensitivity > QJS_INTL_COLLATION_CASE ||
        (unsigned int)options->case_first > QJS_INTL_COLLATION_LOWER_FIRST ||
        (unsigned int)options->numeric > 1 || (unsigned int)options->ignore_punctuation > 1 ||
        (options->collation.length && !options->collation.data))
        return QJS_INTL_INVALID_ARGUMENT;
    program = find_program(service, locale, options->usage, options->collation);
    if (program == UINT32_MAX)
        return QJS_INTL_UNSUPPORTED;

    collator = service->allocator.malloc(service->allocator.opaque, sizeof(*collator));
    if (!collator)
        return QJS_INTL_NO_MEMORY;
    collator->allocator = service->allocator;
    qjs_intl_compiled_collation_program(&service->data, program, &collator->program);
    collator->policy = collator->program.policy;
    switch (options->sensitivity) {
    case QJS_INTL_COLLATION_VARIANT:
        collator->policy.strength = QJS_INTL_COLLATION_STRENGTH_TERTIARY;
        break;
    case QJS_INTL_COLLATION_ACCENT:
        collator->policy.strength = QJS_INTL_COLLATION_STRENGTH_SECONDARY;
        break;
    default:
        collator->policy.strength = QJS_INTL_COLLATION_STRENGTH_PRIMARY;
        break;
    }
    collator->policy.case_level = options->sensitivity == QJS_INTL_COLLATION_CASE;
    collator->policy.case_first = options->case_first;
    collator->policy.numeric = options->numeric;
    collator->policy.shifted = options->ignore_punctuation;
    collator->policy.max_variable = QJS_INTL_COLLATION_VARIABLE_PUNCT;
    *out = collator;
    return QJS_INTL_OK;
}

void qjs_intl_collator_close(QJSIntlCollator *collator)
{
    if (collator)
        collator->allocator.free(collator->allocator.opaque, collator);
}

QJSIntlStatus qjs_intl_collator_compare(const QJSIntlCollator *collator,
                                        const uint16_t *left, size_t left_length,
                                        const uint16_t *right, size_t right_length, int *order)
{
    if (order)
        *order = 0;
    if (!order || !collator)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_collation_compare_ranked(&collator->program, left, left_length, right, right_length,
                                             &collator->allocator, &collator->policy, order);
}
