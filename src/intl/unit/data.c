/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "data.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 6
#define BLOCKED UINT32_C(0xfffffffe)

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

static int identifier(QJSIntlBytes text, int case_name)
{
    size_t i;
    if (!text.data || !text.length || text.data[0] < 'a' || text.data[0] > 'z')
        return 0;
    for (i = 1; i < text.length; i++) {
        int c = text.data[i];
        if (c >= 'a' && c <= 'z')
            continue;
        if (case_name)
            return 0;
        if (c >= '0' && c <= '9')
            continue;
        if (c != '-' || text.data[i - 1] == '-' || i + 1 == text.length)
            return 0;
    }
    return 1;
}

static int selector_valid(unsigned int field, unsigned int count, unsigned int case_id)
{
    if (field >= QJS_INTL_UNIT_FIELD_COUNT || count >= QJS_INTL_UNIT_COUNT_COUNT)
        return 0;
    if (field == QJS_INTL_UNIT_PATTERN)
        return count != QJS_INTL_UNIT_COUNT_NONE;
    return count == QJS_INTL_UNIT_COUNT_NONE &&
           (field == QJS_INTL_UNIT_COMPOUND_PATTERN || !case_id);
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 && (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
                                !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

static QJSIntlUnitMap map_at(const QJSIntlUnitData *data, uint32_t index)
{
    QJSIntlUnitMap result;
    result.entries = data->entries + (size_t)word(data->maps, (size_t)index * 2) * 8;
    result.count = word(data->maps, (size_t)index * 2 + 1);
    return result;
}

QJSIntlStatus qjs_intl_unit_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                      QJSIntlUnitData *out)
{
    QJSIntlUnitData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, scratch, i, end = 0;
    uint32_t row_width;
    uint8_t *used, *used_maps, *used_cases, *families;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes || !allocator ||
        !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_UNIT_PATTERNS, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.group_count = word(result.section.bytes, 1);
    result.case_count = word(result.section.bytes, 2);
    result.row_count = word(result.section.bytes, 3);
    result.map_count = word(result.section.bytes, 4);
    result.entry_count = word(result.section.bytes, 5);
    if (result.group_count < 2 || result.group_count > UINT32_MAX / QJS_INTL_UNIT_WIDTH_COUNT ||
        !result.case_count || result.case_count > 65536 || !result.row_count ||
        result.row_count > result.section.count || !result.map_count ||
        qjs_intl_data_string(data, 0, &empty) || empty.length)
        return status;
    row_width = result.group_count * QJS_INTL_UNIT_WIDTH_COUNT;
    if (!take(&result.section, &at, result.group_count, 2, &result.groups) ||
        !take(&result.section, &at, result.case_count, 1, &result.cases) ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, row_width, &result.rows) ||
        !take(&result.section, &at, result.map_count, 2, &result.maps) ||
        !take(&result.section, &at, result.entry_count, 2, &result.entries) ||
        at != result.section.length / 4 || result.map_count > (size_t)result.row_count * row_width)
        return status;
    for (i = 0; i < result.group_count; i++) {
        uint32_t kind = word(result.groups, i * 2), name = word(result.groups, i * 2 + 1);
        QJSIntlBytes text;
        if (qjs_intl_data_string(data, name, &text) || !identifier(text, 0) ||
            (i == 0 ? kind != QJS_INTL_UNIT_GROUP_COMPOUND || text.length != 3 ||
                          memcmp(text.data, "per", 3)
                    : kind != QJS_INTL_UNIT_GROUP_SINGLE) ||
            (i > 1 && name <= word(result.groups, (i - 1) * 2 + 1)))
            return status;
    }
    for (i = 0; i < result.case_count; i++) {
        uint32_t reference = word(result.cases, i);
        QJSIntlBytes text;
        if (qjs_intl_data_string(data, reference, &text) ||
            (i == 0 ? text.length != 0 : !identifier(text, 1)) ||
            (i && reference <= word(result.cases, i - 1)))
            return status;
    }
    for (i = 1; i < result.row_count; i++) {
        const uint8_t *previous = result.rows + (i - 1) * (size_t)row_width * 4;
        if (compare_words(previous, row_width, previous + (size_t)row_width * 4, row_width) >= 0)
            return status;
    }
    /* Every count fits a disjoint validated word range. Map descriptors hold
       two words each, bounding the extra one-byte family flag per map too. */
    scratch = (size_t)result.row_count + result.map_count + result.case_count + result.map_count;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    used_maps = used + result.row_count;
    used_cases = used_maps + result.map_count;
    families = used_cases + result.case_count;
    used_cases[0] = 1;
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * row_width; i++) {
        uint32_t index = word(result.rows, i);
        if (index >= result.map_count)
            goto done;
        used_maps[index] = 1;
    }
    for (i = 0; i < result.map_count; i++) {
        uint32_t offset = word(result.maps, i * 2), count = word(result.maps, i * 2 + 1), j;
        QJSIntlUnitMap map;
        if (offset != end || count > result.entry_count - end)
            goto done;
        map = map_at(&result, i);
        if (i) {
            QJSIntlUnitMap previous = map_at(&result, i - 1);
            if (compare_words(previous.entries, (size_t)previous.count * 2, map.entries,
                              (size_t)map.count * 2) >= 0)
                goto done;
        }
        end += count;
        for (j = 0; j < count; j++) {
            uint32_t selector = word(map.entries, (size_t)j * 2);
            uint32_t reference = word(map.entries, (size_t)j * 2 + 1);
            uint32_t field = selector >> 24, plural = (selector >> 16) & 255,
                     case_id = selector & 65535;
            QJSIntlBytes text;
            if (!selector_valid(field, plural, case_id) || case_id >= result.case_count ||
                (j && selector <= word(map.entries, (size_t)(j - 1) * 2)))
                goto done;
            families[i] |= field == QJS_INTL_UNIT_COMPOUND_PATTERN ? 2 : 1;
            used_cases[case_id] = 1;
            if (reference != BLOCKED &&
                (qjs_intl_data_string(data, reference, &text) || unresolved(text) ||
                 (field == QJS_INTL_UNIT_GENDER && text.length && !identifier(text, 0))))
                goto done;
        }
        if (families[i] == 3)
            goto done;
    }
    if (end != result.entry_count)
        goto done;
    for (i = 0; i < scratch - result.map_count; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < (size_t)result.row_count * row_width; i++) {
        uint32_t index = word(result.rows, i);
        uint32_t kind = word(result.groups, (i % result.group_count) * 2);
        if (families[index] && families[index] != (kind == QJS_INTL_UNIT_GROUP_COMPOUND ? 2 : 1))
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_unit_group_at(const QJSIntlUnitData *data, uint32_t index,
                                     QJSIntlUnitGroup *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    out->kind = word(data->groups, (size_t)index * 2);
    return qjs_intl_data_string(&data->data, word(data->groups, (size_t)index * 2 + 1), &out->name);
}

QJSIntlStatus qjs_intl_unit_group_find(const QJSIntlUnitData *data, QJSIntlUnitGroupKind kind,
                                       QJSIntlBytes name, uint32_t *out)
{
    uint32_t lo = 0, hi;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || (unsigned int)kind > QJS_INTL_UNIT_GROUP_SINGLE ||
        !identifier(name, 0))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->group_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlUnitGroup group;
        qjs_intl_unit_group_at(data, mid, &group);
        if (group.kind < kind || (group.kind == kind && compare_bytes(group.name, name) < 0))
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < data->group_count) {
        QJSIntlUnitGroup group;
        qjs_intl_unit_group_at(data, lo, &group);
        if (group.kind == kind && !compare_bytes(group.name, name)) {
            *out = lo;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_unit_map_at(const QJSIntlUnitData *data, uint32_t locale,
                                   QJSIntlUnitWidth width, uint32_t group, QJSIntlUnitMap *out)
{
    uint32_t row, index;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)width >= QJS_INTL_UNIT_WIDTH_COUNT || group >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    row = word(data->locale_rows, locale);
    index = word(data->rows,
                 ((size_t)row * QJS_INTL_UNIT_WIDTH_COUNT + width) * data->group_count + group);
    *out = map_at(data, index);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_unit_form_at(const QJSIntlUnitData *data, const QJSIntlUnitMap *map,
                                    uint32_t index, QJSIntlUnitForm *out)
{
    uint32_t selector, reference;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !map || !map->entries || index >= map->count)
        return QJS_INTL_INVALID_ARGUMENT;
    selector = word(map->entries, (size_t)index * 2);
    reference = word(map->entries, (size_t)index * 2 + 1);
    out->field = selector >> 24;
    out->count = (selector >> 16) & 255;
    qjs_intl_data_string(&data->data, word(data->cases, selector & 65535), &out->case_name);
    if (reference == BLOCKED)
        out->state = QJS_INTL_UNIT_FORM_BLOCKED;
    else
        qjs_intl_data_string(&data->data, reference, &out->text);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_unit_form_find(const QJSIntlUnitData *data, const QJSIntlUnitMap *map,
                                      QJSIntlUnitField field, QJSIntlUnitCount count,
                                      QJSIntlBytes case_name, QJSIntlUnitForm *out)
{
    uint32_t lo = 0, hi, case_id, selector;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !map || !map->entries ||
        !selector_valid(field, count, case_name.length != 0) ||
        (case_name.length && !identifier(case_name, 1)))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->case_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlBytes text;
        qjs_intl_data_string(&data->data, word(data->cases, mid), &text);
        if (compare_bytes(text, case_name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == data->case_count)
        return QJS_INTL_UNSUPPORTED;
    {
        QJSIntlBytes text;
        qjs_intl_data_string(&data->data, word(data->cases, lo), &text);
        if (compare_bytes(text, case_name))
            return QJS_INTL_UNSUPPORTED;
    }
    case_id = lo;
    selector = (uint32_t)field << 24 | (uint32_t)count << 16 | case_id;
    lo = 0;
    hi = map->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (word(map->entries, (size_t)mid * 2) < selector)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == map->count || word(map->entries, (size_t)lo * 2) != selector)
        return QJS_INTL_UNSUPPORTED;
    return qjs_intl_unit_form_at(data, map, lo, out);
}
