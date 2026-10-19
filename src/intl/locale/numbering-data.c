/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "numbering-data.h"
#include "../common/locale-grammar.h"
#include "../native/wire.h"
#include <stdlib.h>
#include <string.h>

static uint32_t field(const QJSIntlSection *table, unsigned int width, uint32_t row,
                      unsigned int column)
{
    return qjs_intl_read_u32(table->bytes + ((size_t)row * width + column) * 4);
}

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

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && !memcmp(text.data, expected, text.length);
}

static int compare_variants(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static QJSIntlStatus source_locale(const QJSIntlAllocator *allocator, QJSIntlBytes text)
{
    size_t at = 0, variant_start = 0, count = 0, index;
    unsigned int stage = 0;
    uint64_t *variants;
    QJSIntlStatus status = QJS_INTL_OK;
    if (equal(text, "root"))
        return status;
    if (!text.length)
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

static int numeric_digits(QJSIntlBytes text)
{
    uint32_t points[10];
    size_t at = 0, count = 0, i;
    /* The shared view already validated UTF-8 scalars and excluded NUL. */
    if (text.length < 10 || text.length > 40)
        return 0;
    while (at < text.length) {
        unsigned int first = (unsigned char)text.data[at++];
        unsigned int extra = first < 0x80 ? 0 : first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
        uint32_t point = first & (extra ? (1u << (6 - extra)) - 1 : 0x7f);
        if (count == 10 || extra > text.length - at)
            return 0;
        while (extra--)
            point = (point << 6) | ((unsigned char)text.data[at++] & 0x3f);
        for (i = 0; i < count; i++) {
            if (points[i] == point)
                return 0;
        }
        points[count++] = point;
    }
    return count == 10;
}

static int rule_reference(QJSIntlBytes text)
{
    size_t i, group = 0;
    for (i = 0; i < text.length; i++) {
        int c = (unsigned char)text.data[i];
        if (c == '/') {
            if (!group)
                return 0;
            group = 0;
        } else if (lower(c) || upper(c) || digit(c) || c == '_' || c == '-') {
            group++;
        } else {
            return 0;
        }
    }
    return group != 0;
}

static int table_open(const QJSIntlData *data, uint32_t identity, unsigned int width,
                      QJSIntlSection *out)
{
    return !qjs_intl_pack_section(&data->pack, identity, out) && out->count &&
           out->length % (width * 4) == 0 && out->length / (width * 4) == out->count;
}

QJSIntlStatus qjs_intl_numbering_data_open(const QJSIntlData *data,
                                           const QJSIntlAllocator *allocator,
                                           QJSIntlNumberingData *out)
{
    QJSIntlNumberingData result = {0};
    uint32_t i;
    int latin = 0, root = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !allocator || !allocator->malloc ||
        !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (!table_open(data, QJS_INTL_SECTION_NUMBERING_SYSTEMS, 3, &result.systems) ||
        !table_open(data, QJS_INTL_SECTION_NUMBERING_PREFERENCES, 5,
                    &result.preferences))
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.systems.count; i++) {
        QJSIntlNumberingSystem system;
        uint32_t key = field(&result.systems, 3, i, 0);
        size_t at;
        if ((i && key <= field(&result.systems, 3, i - 1, 0)) ||
            qjs_intl_numbering_system_at(&result, i, &system) ||
            system.name.length < 3 || system.name.length > 8)
            return QJS_INTL_DATA_ERROR;
        for (at = 0; at < system.name.length; at++) {
            if (!lower(system.name.data[at]) && !digit(system.name.data[at]))
                return QJS_INTL_DATA_ERROR;
        }
        if (system.kind == QJS_INTL_NUMBERING_NUMERIC) {
            if (!numeric_digits(system.value))
                return QJS_INTL_DATA_ERROR;
            if (equal(system.name, "latn") && equal(system.value, "0123456789"))
                latin = 1;
        } else if (system.kind != QJS_INTL_NUMBERING_ALGORITHMIC ||
                   !rule_reference(system.value)) {
            return QJS_INTL_DATA_ERROR;
        }
    }
    if (!latin)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.preferences.count; i++) {
        QJSIntlBytes key;
        QJSIntlStatus status;
        uint32_t id = field(&result.preferences, 5, i, 0), column;
        if ((i && id <= field(&result.preferences, 5, i - 1, 0)) ||
            qjs_intl_data_string(data, id, &key))
            return QJS_INTL_DATA_ERROR;
        status = source_locale(allocator, key);
        if (status)
            return status;
        root |= equal(key, "root");
        for (column = 1; column < 5; column++) {
            uint32_t index = field(&result.preferences, 5, i, column);
            if (index >= result.systems.count &&
                (column < 3 || index < QJS_INTL_NUMBERING_BLOCKED))
                return QJS_INTL_DATA_ERROR;
        }
    }
    if (!root)
        return QJS_INTL_DATA_ERROR;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_numbering_system_at(const QJSIntlNumberingData *data,
                                           uint32_t index, QJSIntlNumberingSystem *out)
{
    QJSIntlNumberingSystem result;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->data.pack.bytes || index >= data->systems.count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_data_string(&data->data, field(&data->systems, 3, index, 0),
                             &result.name) ||
        qjs_intl_data_string(&data->data, field(&data->systems, 3, index, 2),
                             &result.value))
        return QJS_INTL_DATA_ERROR;
    result.kind = field(&data->systems, 3, index, 1);
    *out = result;
    return QJS_INTL_OK;
}

static QJSIntlStatus find(const QJSIntlNumberingData *data, const QJSIntlSection *table,
                          unsigned int width, QJSIntlBytes key, uint32_t *out)
{
    uint32_t low = 0, high = table->count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        QJSIntlBytes name;
        size_t length;
        int order;
        qjs_intl_data_string(&data->data, field(table, width, mid, 0), &name);
        length = name.length < key.length ? name.length : key.length;
        order = length ? memcmp(key.data, name.data, length) : 0;
        if (!order)
            order = (key.length > name.length) - (key.length < name.length);
        if (order < 0)
            high = mid;
        else if (order > 0)
            low = mid + 1;
        else {
            *out = mid;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_numbering_system_find(const QJSIntlNumberingData *data,
                                             QJSIntlBytes name, uint32_t *out)
{
    if (out)
        *out = 0;
    if (!out || !data || !data->data.pack.bytes || !name.data)
        return QJS_INTL_INVALID_ARGUMENT;
    return find(data, &data->systems, 3, name, out);
}

QJSIntlStatus qjs_intl_numbering_preferences(const QJSIntlNumberingData *data,
                                             QJSIntlBytes locale,
                                             QJSIntlNumberingPreferences *out)
{
    QJSIntlStatus status;
    uint32_t index;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->data.pack.bytes || !locale.data)
        return QJS_INTL_INVALID_ARGUMENT;
    status = find(data, &data->preferences, 5, locale, &index);
    if (status)
        return status;
    out->standard = field(&data->preferences, 5, index, 1);
    out->native = field(&data->preferences, 5, index, 2);
    out->traditional = field(&data->preferences, 5, index, 3);
    out->finance = field(&data->preferences, 5, index, 4);
    return QJS_INTL_OK;
}
