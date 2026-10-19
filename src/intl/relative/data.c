/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 5
#define ROW_WIDTH (QJS_INTL_RELATIVE_UNIT_COUNT * QJS_INTL_RELATIVE_WIDTH_COUNT)
#define BLOCKED UINT32_C(0xfffffffe)
#define COUNTED (2 * QJS_INTL_PLURAL_CATEGORY_COUNT)

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, uint32_t width,
                const uint8_t **out)
{
    if (count > (section->length / 4 - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int compare_words(const uint8_t *a, size_t ac, const uint8_t *b, size_t bc)
{
    size_t i, shared = ac < bc ? ac : bc;
    for (i = 0; i < shared; i++) {
        uint32_t av = word(a, i), bv = word(b, i);
        if (av != bv)
            return av < bv ? -1 : 1;
    }
    return (ac > bc) - (ac < bc);
}

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t shared = a.length < b.length ? a.length : b.length;
    int order = shared ? memcmp(a.data, b.data, shared) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static int literal_key(QJSIntlBytes key)
{
    size_t at = 0, digits;
    if (!key.length || key.length > 17)
        return 0;
    if (key.data[0] == '-')
        at++;
    digits = key.length - at;
    if (!digits || digits > 16 || (key.data[at] == '0' && (at || digits > 1)))
        return 0;
    if (digits == 16 && memcmp(key.data + at, "9007199254740992", 16) > 0)
        return 0;
    while (at < key.length) {
        if (key.data[at] < '0' || key.data[at] > '9')
            return 0;
        at++;
    }
    return 1;
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 && (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
                                !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

static int numeric_pattern(QJSIntlBytes text)
{
    size_t i;
    int placeholder = 0;
    for (i = 0; i < text.length; i++) {
        if (text.data[i] == '{') {
            if (placeholder || text.length - i < 3 || text.data[i + 1] != '0' || text.data[i + 2] != '}')
                return 0;
            placeholder = 1;
            i += 2;
        } else if (text.data[i] == '}') {
            return 0;
        }
    }
    return 1;
}

static QJSIntlRelativeMap map_at(const QJSIntlRelativeData *data, uint32_t index)
{
    QJSIntlRelativeMap map;
    map.entries = data->entries + (size_t)word(data->maps, (size_t)index * 2) * 16;
    map.count = word(data->maps, (size_t)index * 2 + 1);
    return map;
}

static const uint8_t *find(const QJSIntlRelativeMap *map, uint32_t selector)
{
    uint32_t lo = 0, hi = map->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (word(map->entries, (size_t)mid * 4) < selector)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == map->count || word(map->entries, (size_t)lo * 4) != selector)
        return NULL;
    return map->entries + (size_t)lo * 16;
}

QJSIntlStatus qjs_intl_relative_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                          QJSIntlRelativeData *out)
{
    QJSIntlRelativeData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, i, j, end = 0, scratch;
    uint8_t *used, *used_maps, *used_keys, *sources;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_RELATIVE_PATTERNS, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.key_count = word(result.section.bytes, 1);
    result.row_count = word(result.section.bytes, 2);
    result.map_count = word(result.section.bytes, 3);
    result.entry_count = word(result.section.bytes, 4);
    if (!result.row_count || result.row_count > result.section.count || !result.map_count ||
        result.key_count > UINT32_MAX - COUNTED ||
        qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.key_count, 1, &result.keys) ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, ROW_WIDTH, &result.rows) ||
        !take(&result.section, &at, result.map_count, 2, &result.maps) ||
        !take(&result.section, &at, result.entry_count, 4, &result.entries) ||
        at != result.section.length / 4 ||
        result.map_count > (size_t)result.row_count * ROW_WIDTH)
        return status;
    for (i = 0; i < result.key_count; i++) {
        QJSIntlBytes key;
        uint32_t reference = word(result.keys, i);
        if (qjs_intl_data_string(data, reference, &key) || !literal_key(key) ||
            (i && reference <= word(result.keys, i - 1)))
            return status;
    }
    for (i = 1; i < result.row_count; i++) {
        const uint8_t *previous = result.rows + (i - 1) * ROW_WIDTH * 4;
        if (compare_words(previous, ROW_WIDTH, previous + ROW_WIDTH * 4, ROW_WIDTH) >= 0)
            return status;
    }
    /* Counts fit disjoint validated word spans in the bounded shared pack. */
    scratch = (size_t)result.row_count + result.map_count + result.key_count + data->strings.count;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    used_maps = used + result.row_count;
    used_keys = used_maps + result.map_count;
    sources = used_keys + result.key_count;
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * ROW_WIDTH; i++) {
        uint32_t index = word(result.rows, i);
        if (index >= result.map_count)
            goto done;
        used_maps[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count + result.map_count; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < result.map_count; i++) {
        uint32_t start = word(result.maps, i * 2), count = word(result.maps, i * 2 + 1);
        uint32_t last = 0;
        if (start != end || count > result.entry_count - end || count > COUNTED + result.key_count)
            goto done;
        if (i) {
            QJSIntlRelativeMap previous = map_at(&result, i - 1);
            if (compare_words(previous.entries, (size_t)previous.count * 4,
                              result.entries + (size_t)start * 16, (size_t)count * 4) >= 0)
                goto done;
        }
        for (j = start; j < (size_t)start + count; j++) {
            uint32_t selector = word(result.entries, j * 4);
            uint32_t text = word(result.entries, j * 4 + 1);
            uint32_t origin = word(result.entries, j * 4 + 2);
            uint32_t group = word(result.entries, j * 4 + 3);
            QJSIntlBytes value;
            if (selector >= COUNTED + result.key_count || (j != start && selector <= last) ||
                origin >= data->strings.count || group >= ROW_WIDTH)
                goto done;
            if (selector >= COUNTED)
                used_keys[selector - COUNTED] = 1;
            if (!sources[origin]) {
                QJSIntlStatus check;
                qjs_intl_data_string(data, origin, &value);
                check = qjs_intl_source_locale_validate(allocator, value);
                if (check) {
                    if (check == QJS_INTL_NO_MEMORY)
                        status = check;
                    goto done;
                }
                sources[origin] = 1;
            }
            if (text != BLOCKED &&
                (qjs_intl_data_string(data, text, &value) || unresolved(value) ||
                 (selector < COUNTED && !numeric_pattern(value))))
                goto done;
            last = selector;
        }
        end += count;
    }
    if (end != result.entry_count)
        goto done;
    for (i = 0; i < result.key_count; i++) {
        if (!used_keys[i])
            goto done;
    }
    for (i = 0; i < result.row_count; i++) {
        for (j = 0; j < QJS_INTL_RELATIVE_UNIT_COUNT; j++) {
            QJSIntlRelativeMap map = map_at(&result, word(result.rows, i * ROW_WIDTH + j * 3));
            unsigned int tense;
            for (tense = 0; tense < 2; tense++) {
                const uint8_t *entry = find(&map, tense * QJS_INTL_PLURAL_CATEGORY_COUNT + QJS_INTL_PLURAL_OTHER);
                if (!entry || word(entry, 1) == BLOCKED)
                    goto done;
            }
        }
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_relative_map_at(const QJSIntlRelativeData *data, uint32_t locale,
                                       QJSIntlRelativeUnit unit, QJSIntlRelativeWidth width,
                                       QJSIntlRelativeMap *out)
{
    uint32_t row, index;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)unit >= QJS_INTL_RELATIVE_UNIT_COUNT ||
        (unsigned int)width >= QJS_INTL_RELATIVE_WIDTH_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    row = word(data->locale_rows, locale);
    index = word(data->rows, (size_t)row * ROW_WIDTH + (unsigned int)unit * 3 + width);
    *out = map_at(data, index);
    return QJS_INTL_OK;
}

static QJSIntlStatus form_find(const QJSIntlRelativeData *data, const QJSIntlRelativeMap *map,
                              uint32_t selector, QJSIntlRelativeForm *out)
{
    const uint8_t *entry = find(map, selector);
    uint32_t reference, group;
    if (!entry)
        return QJS_INTL_UNSUPPORTED;
    reference = word(entry, 1);
    out->blocked = reference == BLOCKED;
    if (!out->blocked)
        qjs_intl_data_string(&data->data, reference, &out->text);
    qjs_intl_data_string(&data->data, word(entry, 2), &out->origin_locale);
    group = word(entry, 3);
    out->origin_unit = group / 3;
    out->origin_width = group % 3;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_relative_pattern_find(const QJSIntlRelativeData *data, const QJSIntlRelativeMap *map,
                                             QJSIntlRelativeTense tense, QJSIntlPluralCategory category,
                                             QJSIntlRelativeForm *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !map || !map->entries ||
        (unsigned int)tense > QJS_INTL_RELATIVE_FUTURE ||
        (unsigned int)category >= QJS_INTL_PLURAL_CATEGORY_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    return form_find(data, map, (unsigned int)tense * QJS_INTL_PLURAL_CATEGORY_COUNT + category, out);
}

QJSIntlStatus qjs_intl_relative_literal_find(const QJSIntlRelativeData *data, const QJSIntlRelativeMap *map,
                                             QJSIntlBytes key, QJSIntlRelativeForm *out)
{
    uint32_t lo = 0, hi;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !map || !map->entries || (!key.data && key.length))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->key_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlBytes candidate;
        qjs_intl_data_string(&data->data, word(data->keys, mid), &candidate);
        if (compare_bytes(candidate, key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == data->key_count)
        return QJS_INTL_UNSUPPORTED;
    {
        QJSIntlBytes candidate;
        qjs_intl_data_string(&data->data, word(data->keys, lo), &candidate);
        if (compare_bytes(candidate, key))
            return QJS_INTL_UNSUPPORTED;
    }
    return form_find(data, map, COUNTED + lo, out);
}
