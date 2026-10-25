/* Calendar display projection over shared BCP47 facts and source-name keys.
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
#include "calendar-names.h"
#include "../common/locale-grammar.h"
#include <stdlib.h>
#include <string.h>

struct QJSIntlCalendarNameRecord {
    QJSIntlBytes code, source;
};

typedef struct ProjectionBuilder {
    QJSIntlCalendarNameRecord *records;
    char *arena;
    size_t capacity, count, bytes, used;
} ProjectionBuilder;

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static int equal(QJSIntlBytes text, const char *literal)
{
    return !compare_bytes(text, (QJSIntlBytes){literal, strlen(literal)});
}

static int calendar_type(const QJSIntlLocaleType *type)
{
    return !type->variable && equal(type->extension, "u") && equal(type->key, "ca");
}

static int next_alias(QJSIntlBytes text, size_t *at, QJSIntlBytes *out)
{
    size_t start = *at;
    if (start == text.length)
        return 0;
    while (*at < text.length && text.data[*at] != ' ')
        ++*at;
    *out = (QJSIntlBytes){text.data + start, *at - start};
    if (*at < text.length)
        ++*at;
    return 1;
}

static unsigned int normalized_byte(unsigned int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c == '_' ? '-' : c;
}

/* Compare a metadata spelling against a normalized source lookup identity. */
static int compare_source(QJSIntlBytes candidate, QJSIntlBytes key)
{
    size_t i, length = candidate.length < key.length ? candidate.length : key.length;
    for (i = 0; i < length; i++) {
        unsigned int a = normalized_byte((uint8_t)candidate.data[i]), b = (uint8_t)key.data[i];
        if (a != b)
            return a < b ? -1 : 1;
    }
    return (candidate.length > key.length) - (candidate.length < key.length);
}

static QJSIntlStatus source_candidate(const QJSIntlNameData *names, uint32_t begin, uint32_t end,
                                     QJSIntlBytes candidate, QJSIntlBytes *chosen)
{
    uint32_t low = begin, high = end;
    QJSIntlNameGroup group;
    QJSIntlBytes key;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        qjs_intl_name_key_at(names, mid, &group, &key);
        if (compare_source(candidate, key) > 0)
            low = mid + 1;
        else
            high = mid;
    }
    if (low == end)
        return QJS_INTL_OK;
    qjs_intl_name_key_at(names, low, &group, &key);
    if (compare_source(candidate, key))
        return QJS_INTL_OK;
    if (chosen->length && compare_bytes(*chosen, key))
        return QJS_INTL_DATA_ERROR;
    *chosen = key;
    return QJS_INTL_OK;
}

static QJSIntlStatus source_for_type(const QJSIntlLocaleMetadata *metadata, const QJSIntlNameData *names,
                                    uint32_t begin, uint32_t end, QJSIntlLocaleType type,
                                    QJSIntlBytes *out)
{
    uint32_t pass;
    memset(out, 0, sizeof(*out));
    for (pass = 0; pass < metadata->types.count; pass++) {
        size_t at = 0;
        QJSIntlBytes alias;
        QJSIntlStatus status;
        uint32_t next;
        if (type.variable)
            return QJS_INTL_DATA_ERROR;
        status = source_candidate(names, begin, end, type.name, out);
        if (status)
            return status;
        while (next_alias(type.aliases, &at, &alias)) {
            status = source_candidate(names, begin, end, alias, out);
            if (status)
                return status;
        }
        if (!type.preferred.length)
            return QJS_INTL_OK;
        status = qjs_intl_locale_type_find(metadata, type.extension, type.key, type.preferred, &next);
        if (status)
            return QJS_INTL_DATA_ERROR;
        qjs_intl_locale_type_at(metadata, next, &type);
    }
    return QJS_INTL_DATA_ERROR;
}

static QJSIntlStatus count_key(QJSIntlBytes code, size_t *capacity, size_t *bytes)
{
    if (!qjs_intl_unicode_type_well_formed(code.data, code.length))
        return QJS_INTL_OK;
    if (*capacity == SIZE_MAX || code.length == SIZE_MAX || *bytes > SIZE_MAX - code.length - 1)
        return QJS_INTL_OVERFLOW;
    ++*capacity;
    *bytes += code.length + 1;
    return QJS_INTL_OK;
}

static QJSIntlStatus add_key(ProjectionBuilder *builder, QJSIntlBytes code, QJSIntlBytes source)
{
    char *key;
    size_t i;
    if (!qjs_intl_unicode_type_well_formed(code.data, code.length))
        return QJS_INTL_OK;
    if (builder->count == builder->capacity || builder->used >= builder->bytes ||
        code.length >= builder->bytes - builder->used)
        return QJS_INTL_DATA_ERROR;
    key = builder->arena + builder->used;
    for (i = 0; i < code.length; i++)
        key[i] = normalized_byte((uint8_t)code.data[i]);
    key[code.length] = '\0';
    builder->records[builder->count++] =
        (QJSIntlCalendarNameRecord){{key, code.length}, source};
    builder->used += code.length + 1;
    return QJS_INTL_OK;
}

static int compare_records(const void *left, const void *right)
{
    const QJSIntlCalendarNameRecord *a = left, *b = right;
    int order = compare_bytes(a->code, b->code);
    return order ? order : compare_bytes(a->source, b->source);
}

QJSIntlStatus qjs_intl_calendar_names_open(const QJSIntlLocaleMetadata *metadata,
                                         const QJSIntlNameData *names,
                                         const QJSIntlAllocator *allocator, QJSIntlCalendarNames *out)
{
    QJSIntlCalendarNames result = {0};
    ProjectionBuilder builder = {0};
    QJSIntlStatus status;
    QJSIntlNameGroup group;
    QJSIntlBytes key;
    uint32_t i, begin, end;
    size_t capacity = 0, bytes = 0, kept, j;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !metadata || !metadata->types.bytes || !names || !names->section.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    begin = end = names->key_count;
    for (i = 0; i < names->key_count; i++) {
        qjs_intl_name_key_at(names, i, &group, &key);
        if (group == QJS_INTL_NAME_CALENDAR) {
            if (begin == names->key_count)
                begin = i;
            end = i + 1;
            status = count_key(key, &capacity, &bytes);
            if (status)
                return status;
        }
    }
    if (begin == end) {
        *out = result;
        return QJS_INTL_OK;
    }
    for (i = 0; i < metadata->types.count; i++) {
        QJSIntlLocaleType type;
        QJSIntlBytes alias;
        size_t at = 0;
        qjs_intl_locale_type_at(metadata, i, &type);
        if (!calendar_type(&type))
            continue;
        status = count_key(type.name, &capacity, &bytes);
        if (status)
            return status;
        while (next_alias(type.aliases, &at, &alias)) {
            status = count_key(alias, &capacity, &bytes);
            if (status)
                return status;
        }
    }
    if (!capacity) {
        *out = result;
        return QJS_INTL_OK;
    }
    if (capacity > (SIZE_MAX - bytes) / sizeof(*result.records))
        return QJS_INTL_OVERFLOW;
    result.records = allocator->malloc(allocator->opaque, capacity * sizeof(*result.records) + bytes);
    if (!result.records)
        return QJS_INTL_NO_MEMORY;
    builder.records = result.records;
    builder.capacity = capacity;
    builder.bytes = bytes;
    builder.arena = (char *)(result.records + capacity);
    for (i = begin; i < end; i++) {
        qjs_intl_name_key_at(names, i, &group, &key);
        status = add_key(&builder, key, key);
        if (status)
            goto failed;
    }
    for (i = 0; i < metadata->types.count; i++) {
        QJSIntlLocaleType type;
        QJSIntlBytes source = {0}, alias;
        size_t at = 0;
        qjs_intl_locale_type_at(metadata, i, &type);
        if (!calendar_type(&type))
            continue;
        status = source_for_type(metadata, names, begin, end, type, &source);
        if (status)
            goto failed;
        if (!source.length)
            continue;
        status = add_key(&builder, type.name, source);
        if (status)
            goto failed;
        while (next_alias(type.aliases, &at, &alias)) {
            status = add_key(&builder, alias, source);
            if (status)
                goto failed;
        }
    }
    qsort(result.records, builder.count, sizeof(*result.records), compare_records);
    for (j = 0, kept = 0; j < builder.count; j++) {
        QJSIntlCalendarNameRecord *record = &result.records[j];
        if (kept && !compare_bytes(result.records[kept - 1].code, record->code)) {
            if (compare_bytes(result.records[kept - 1].source, record->source)) {
                status = QJS_INTL_DATA_ERROR;
                goto failed;
            }
        } else {
            result.records[kept++] = *record;
        }
    }
    result.count = kept;
    if (!kept) {
        allocator->free(allocator->opaque, result.records);
        result.records = NULL;
    }
    *out = result;
    return QJS_INTL_OK;
failed:
    allocator->free(allocator->opaque, result.records);
    return status;
}

void qjs_intl_calendar_names_clear(QJSIntlCalendarNames *names)
{
    if (!names)
        return;
    if (names->records)
        names->allocator.free(names->allocator.opaque, names->records);
    memset(names, 0, sizeof(*names));
}

QJSIntlStatus qjs_intl_calendar_names_find(const QJSIntlCalendarNames *names, QJSIntlBytes code,
                                         QJSIntlBytes *out)
{
    size_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !names || !names->allocator.free || (!code.data && code.length))
        return QJS_INTL_INVALID_ARGUMENT;
    high = names->count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int order = compare_bytes(names->records[mid].code, code);
        if (order < 0)
            low = mid + 1;
        else if (order > 0)
            high = mid;
        else {
            *out = names->records[mid].source;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
