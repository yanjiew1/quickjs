/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "timezone-name-data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 9
#define GROUP_WIDTH 5
#define NODE_WIDTH 4
#define ENTRY_WIDTH 4
#define BLOCKED UINT32_C(0xfffffffe)
#define SOURCE_CHECKED 1
#define ZONE_CHECKED 2
#define METAZONE_CHECKED 4

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, uint32_t width,
                const uint8_t **out)
{
    if (!width || *at > section->length / 4 ||
        count > (section->length / 4 - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int compare_words(const uint8_t *left, const uint8_t *right, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        uint32_t a = word(left, i), b = word(right, i);
        if (a != b)
            return a < b ? -1 : 1;
    }
    return 0;
}

static int ordered(const uint8_t *records, uint32_t count, uint32_t width)
{
    size_t i;
    for (i = 1; i < count; i++) {
        if (compare_words(records + (i - 1) * width * 4, records + i * width * 4, width) >= 0)
            return 0;
    }
    return 1;
}

static int equal(QJSIntlBytes text, const char *value)
{
    size_t length = strlen(value);
    return text.length == length && (!length || !memcmp(text.data, value, length));
}

/* Match the source identity grammars, not ECMA primary-zone canonicalization. */
static int identity_valid(QJSIntlBytes text, int metazone)
{
    size_t i;
    unsigned int segment = 0;
    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (metazone && !i && !letter)
            return 0;
        if (letter || (c >= '0' && c <= '9') || c == '_' || (!metazone && (c == '+' || c == '.' || c == '-')))
            segment = 1;
        else if (!metazone && c == '/' && segment)
            segment = 0;
        else
            return 0;
    }
    return segment;
}

static int code_valid(QJSIntlBytes text, uint32_t family)
{
    static const char *const formats[] = {
        "hourFormat", "gmtFormat", "gmtZeroFormat", "gmtUnknownFormat",
        "dualOffsetFormat", "regionFormat", "fallbackFormat"
    };
    size_t i;
    if (family == QJS_INTL_TIMEZONE_NAME_CITY)
        return equal(text, "exemplarCity");
    if (family >= QJS_INTL_TIMEZONE_NAME_ZONE)
        return equal(text, "generic") || equal(text, "standard") || equal(text, "daylight");
    for (i = 0; i < sizeof(formats) / sizeof(*formats); i++) {
        if (equal(text, formats[i]))
            return 1;
    }
    return 0;
}

static uint32_t semantic(uint32_t family)
{
    return family < QJS_INTL_TIMEZONE_NAME_ZONE ? family : QJS_INTL_TIMEZONE_NAME_ZONE;
}

static uint32_t group_family(const QJSIntlTimeZoneNameData *data, uint32_t group)
{
    return word(data->groups, (size_t)group * GROUP_WIDTH);
}

QJSIntlStatus qjs_intl_timezone_name_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                               QJSIntlTimeZoneNameData *out)
{
    QJSIntlTimeZoneNameData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, i, scratch;
    uint32_t family, max_rows = 0, *depths;
    uint8_t *used_rows, *used_nodes, *map_classes, *strings;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes || !data->locales.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_TIMEZONE_NAMES, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.group_count = word(result.section.bytes, 1);
    result.map_count = word(result.section.bytes, 2);
    result.node_count = word(result.section.bytes, 3);
    result.entry_count = word(result.section.bytes, 4);
    if (!result.group_count || result.map_count > result.node_count || result.entry_count < result.map_count ||
        qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.group_count, GROUP_WIDTH, &result.groups) ||
        !ordered(result.groups, result.group_count, GROUP_WIDTH))
        return status;
    for (i = 0; i < result.group_count; i++) {
        const uint8_t *group = result.groups + i * GROUP_WIDTH * 4;
        uint32_t width = word(group, 3);
        family = word(group, 0);
        if (family >= QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT || word(group, 1) >= data->strings.count ||
            word(group, 2) >= data->strings.count || word(group, 4) >= data->strings.count ||
            width >= QJS_INTL_TIMEZONE_NAME_WIDTH_COUNT ||
            (family == QJS_INTL_TIMEZONE_NAME_FORMAT && (word(group, 1) || word(group, 2) || width || word(group, 4))) ||
            (family == QJS_INTL_TIMEZONE_NAME_CITY && (width || word(group, 4))) ||
            (family >= QJS_INTL_TIMEZONE_NAME_ZONE && !width))
            return status;
        if (!result.families[family].group_count)
            result.families[family].group_start = (uint32_t)i;
        result.families[family].group_count++;
    }
    for (family = 0; family < QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT; family++) {
        QJSIntlTimeZoneNameRows *rows = &result.families[family];
        rows->row_count = word(result.section.bytes, 5 + family);
        if (!rows->group_count) {
            if (rows->row_count)
                return status;
            continue;
        }
        if (!rows->row_count || rows->row_count > result.section.count ||
            !take(&result.section, &at, result.section.count, 1, &rows->locale_rows) ||
            !take(&result.section, &at, rows->row_count, rows->group_count, &rows->rows) ||
            !ordered(rows->rows, rows->row_count, rows->group_count))
            return status;
        if (rows->row_count > max_rows)
            max_rows = rows->row_count;
    }
    if (!take(&result.section, &at, result.map_count, 2, &result.maps) ||
        !take(&result.section, &at, result.node_count, NODE_WIDTH, &result.nodes) ||
        !take(&result.section, &at, result.entry_count, ENTRY_WIDTH, &result.entries) ||
        at != result.section.length / 4)
        return status;
    at = 0;
    for (i = 0; i < result.map_count; i++) {
        uint32_t start = word(result.maps, i * 2), count = word(result.maps, i * 2 + 1);
        size_t j;
        if (start != at || !count || count > result.entry_count - at)
            return status;
        for (j = 1; j < count; j++) {
            if (compare_words(result.entries + (at + j - 1) * ENTRY_WIDTH * 4,
                              result.entries + (at + j) * ENTRY_WIDTH * 4, 3) >= 0)
                return status;
        }
        if (i) {
            uint32_t previous = word(result.maps, (i - 1) * 2);
            uint32_t previous_count = word(result.maps, (i - 1) * 2 + 1);
            size_t common = count < previous_count ? count : previous_count;
            int order = compare_words(result.entries + (size_t)previous * ENTRY_WIDTH * 4,
                                      result.entries + at * ENTRY_WIDTH * 4, common * ENTRY_WIDTH);
            if (order > 0 || (!order && previous_count >= count))
                return status;
        }
        at += count;
    }
    if (at != result.entry_count)
        return status;
    for (i = 0; i < result.entry_count; i++) {
        const uint8_t *entry = result.entries + i * ENTRY_WIDTH * 4;
        QJSIntlBytes text;
        if (word(entry, 0) >= data->strings.count || word(entry, 1) >= QJS_INTL_TIMEZONE_NAME_TYPE_COUNT ||
            word(entry, 2) >= data->strings.count || (word(entry, 3) != BLOCKED &&
             (qjs_intl_data_string(data, word(entry, 3), &text) ||
              equal(text, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91") ||
              equal(text, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"))))
            return status;
    }
    /* All counts are backed by disjoint physical spans in the bounded pack.
       These smaller coverage/depth arrays also fit on 32-bit. */
    scratch = ((size_t)result.node_count + 1) * sizeof(*depths) + max_rows +
              result.node_count + result.map_count + data->strings.count;
    depths = allocator->malloc(allocator->opaque, scratch);
    if (!depths)
        return QJS_INTL_NO_MEMORY;
    memset(depths, 0, scratch);
    used_rows = (uint8_t *)(depths + (size_t)result.node_count + 1);
    used_nodes = used_rows + max_rows;
    map_classes = used_nodes + result.node_count;
    strings = map_classes + result.map_count;
    for (i = 0; i < result.group_count; i++) {
        uint32_t identity = word(result.groups, i * GROUP_WIDTH + 1);
        uint8_t bit;
        family = group_family(&result, (uint32_t)i);
        if (family == QJS_INTL_TIMEZONE_NAME_FORMAT)
            continue;
        bit = family == QJS_INTL_TIMEZONE_NAME_METAZONE ? METAZONE_CHECKED : ZONE_CHECKED;
        if (!(strings[identity] & bit)) {
            QJSIntlBytes text;
            qjs_intl_data_string(data, identity, &text);
            if (!identity_valid(text, family == QJS_INTL_TIMEZONE_NAME_METAZONE))
                goto done;
            strings[identity] |= bit;
        }
    }
    for (i = 0; i < result.node_count; i++) {
        const uint8_t *node = result.nodes + i * NODE_WIDTH * 4;
        uint32_t origin = word(node, 0), group = word(node, 1), map = word(node, 2), parent = word(node, 3);
        uint32_t start, count;
        size_t j;
        if (origin >= data->strings.count || group >= result.group_count || map >= result.map_count || parent > i)
            goto done;
        family = semantic(group_family(&result, group));
        if (parent && semantic(group_family(&result, word(result.nodes, ((size_t)parent - 1) * NODE_WIDTH + 1))) != family)
            goto done;
        depths[i + 1] = depths[parent] + 1;
        if (i && (depths[i + 1] < depths[i] ||
                  (depths[i + 1] == depths[i] && compare_words(node - NODE_WIDTH * 4, node, NODE_WIDTH) >= 0)))
            goto done;
        if (!(strings[origin] & SOURCE_CHECKED)) {
            QJSIntlBytes text;
            QJSIntlStatus check;
            qjs_intl_data_string(data, origin, &text);
            check = qjs_intl_source_locale_validate(allocator, text);
            if (check) {
                if (check == QJS_INTL_NO_MEMORY || check == QJS_INTL_OVERFLOW)
                    status = check;
                goto done;
            }
            strings[origin] |= SOURCE_CHECKED;
        }
        if (map_classes[map] & (1u << family))
            continue;
        map_classes[map] |= (uint8_t)(1u << family);
        start = word(result.maps, (size_t)map * 2);
        count = word(result.maps, (size_t)map * 2 + 1);
        for (j = 0; j < count; j++) {
            const uint8_t *entry = result.entries + ((size_t)start + j) * ENTRY_WIDTH * 4;
            uint32_t code = word(entry, 0);
            uint8_t bit = (uint8_t)(1u << (family + 3));
            QJSIntlBytes text;
            qjs_intl_data_string(data, code, &text);
            if (word(entry, 1) && (family != QJS_INTL_TIMEZONE_NAME_FORMAT || !equal(text, "regionFormat")))
                goto done;
            if (!(strings[code] & bit)) {
                if (!code_valid(text, family))
                    goto done;
                strings[code] |= bit;
            }
        }
    }
    for (family = 0; family < QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT; family++) {
        const QJSIntlTimeZoneNameRows *rows = &result.families[family];
        if (!rows->group_count)
            continue;
        memset(used_rows, 0, rows->row_count);
        for (i = 0; i < result.section.count; i++) {
            uint32_t row = word(rows->locale_rows, i);
            if (row >= rows->row_count)
                goto done;
            used_rows[row] = 1;
        }
        for (i = 0; i < rows->row_count; i++) {
            size_t group;
            if (!used_rows[i])
                goto done;
            for (group = 0; group < rows->group_count; group++) {
                uint32_t head = word(rows->rows, i * rows->group_count + group);
                if (head > result.node_count)
                    goto done;
                if (head) {
                    uint32_t origin_group = word(result.nodes, ((size_t)head - 1) * NODE_WIDTH + 1);
                    if (semantic(group_family(&result, origin_group)) != semantic(family))
                        goto done;
                    used_nodes[head - 1] = 1;
                }
            }
        }
    }
    for (i = result.node_count; i > 0; i--) {
        uint32_t parent = word(result.nodes, (i - 1) * NODE_WIDTH + 3);
        if (!used_nodes[i - 1])
            goto done;
        if (parent)
            used_nodes[parent - 1] = 1;
    }
    for (i = 0; i < result.map_count; i++) {
        if (!map_classes[i])
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, depths);
    return status;
}

QJSIntlStatus qjs_intl_timezone_name_group_at(const QJSIntlTimeZoneNameData *data, uint32_t index,
                                              QJSIntlTimeZoneNameGroup *out)
{
    const uint8_t *group;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    group = data->groups + (size_t)index * GROUP_WIDTH * 4;
    out->family = word(group, 0);
    qjs_intl_data_string(&data->data, word(group, 1), &out->entity);
    qjs_intl_data_string(&data->data, word(group, 2), &out->entity_alt);
    out->width = word(group, 3);
    qjs_intl_data_string(&data->data, word(group, 4), &out->width_alt);
    return QJS_INTL_OK;
}

static int compare_bytes(QJSIntlBytes left, QJSIntlBytes right)
{
    size_t common = left.length < right.length ? left.length : right.length;
    int order = common ? memcmp(left.data, right.data, common) : 0;
    if (order)
        return order;
    return left.length == right.length ? 0 : left.length < right.length ? -1 : 1;
}

static int compare_group(const QJSIntlTimeZoneNameData *data, uint32_t index,
                         const QJSIntlTimeZoneNameGroup *key)
{
    QJSIntlTimeZoneNameGroup group;
    int order;
    qjs_intl_timezone_name_group_at(data, index, &group);
    if (group.family != key->family)
        return group.family < key->family ? -1 : 1;
    order = compare_bytes(group.entity, key->entity);
    if (order)
        return order;
    order = compare_bytes(group.entity_alt, key->entity_alt);
    if (order)
        return order;
    if (group.width != key->width)
        return group.width < key->width ? -1 : 1;
    return compare_bytes(group.width_alt, key->width_alt);
}

QJSIntlStatus qjs_intl_timezone_name_find_group(const QJSIntlTimeZoneNameData *data,
                                                const QJSIntlTimeZoneNameGroup *key, uint32_t *out)
{
    uint32_t low = 0, high;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || !key ||
        (unsigned int)key->family >= QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT ||
        (unsigned int)key->width >= QJS_INTL_TIMEZONE_NAME_WIDTH_COUNT ||
        (!key->entity.data && key->entity.length) || (!key->entity_alt.data && key->entity_alt.length) ||
        (!key->width_alt.data && key->width_alt.length))
        return QJS_INTL_INVALID_ARGUMENT;
    high = data->group_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        if (compare_group(data, mid, key) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    if (low >= data->group_count || compare_group(data, low, key))
        return QJS_INTL_UNSUPPORTED;
    *out = low;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_name_head(const QJSIntlTimeZoneNameData *data, uint32_t locale,
                                          uint32_t group, uint32_t *out)
{
    const QJSIntlTimeZoneNameRows *rows;
    uint32_t row;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || locale >= data->section.count || group >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    rows = &data->families[group_family(data, group)];
    row = word(rows->locale_rows, locale);
    *out = word(rows->rows, (size_t)row * rows->group_count + group - rows->group_start);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_timezone_name_node(const QJSIntlTimeZoneNameData *data, uint32_t index,
                                          QJSIntlTimeZoneNameNode *out)
{
    const uint8_t *node;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !index || index > data->node_count)
        return QJS_INTL_INVALID_ARGUMENT;
    node = data->nodes + ((size_t)index - 1) * NODE_WIDTH * 4;
    qjs_intl_data_string(&data->data, word(node, 0), &out->origin_locale);
    out->group = word(node, 1);
    out->map = word(node, 2);
    out->parent = word(node, 3);
    return QJS_INTL_OK;
}

static int compare_key(const QJSIntlTimeZoneNameData *data, const uint8_t *entry,
                       QJSIntlBytes code, QJSIntlTimeZoneNameType type, QJSIntlBytes alt)
{
    QJSIntlBytes text;
    int order;
    qjs_intl_data_string(&data->data, word(entry, 0), &text);
    order = compare_bytes(text, code);
    if (order)
        return order;
    if (word(entry, 1) != (unsigned int)type)
        return word(entry, 1) < (unsigned int)type ? -1 : 1;
    qjs_intl_data_string(&data->data, word(entry, 2), &text);
    return compare_bytes(text, alt);
}

QJSIntlStatus qjs_intl_timezone_name_lookup(const QJSIntlTimeZoneNameData *data, uint32_t index,
                                            QJSIntlBytes code, QJSIntlTimeZoneNameType type, QJSIntlBytes alt,
                                            QJSIntlTimeZoneNameEntry *out)
{
    uint32_t map, start, low = 0, high, count;
    const uint8_t *entry;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !index || index > data->node_count ||
        (unsigned int)type >= QJS_INTL_TIMEZONE_NAME_TYPE_COUNT ||
        (!code.data && code.length) || (!alt.data && alt.length))
        return QJS_INTL_INVALID_ARGUMENT;
    map = word(data->nodes, ((size_t)index - 1) * NODE_WIDTH + 2);
    start = word(data->maps, (size_t)map * 2);
    count = high = word(data->maps, (size_t)map * 2 + 1);
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        entry = data->entries + ((size_t)start + mid) * ENTRY_WIDTH * 4;
        if (compare_key(data, entry, code, type, alt) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    if (low >= count)
        return QJS_INTL_UNSUPPORTED;
    entry = data->entries + ((size_t)start + low) * ENTRY_WIDTH * 4;
    if (compare_key(data, entry, code, type, alt))
        return QJS_INTL_UNSUPPORTED;
    qjs_intl_data_string(&data->data, word(entry, 0), &out->code);
    out->type = word(entry, 1);
    qjs_intl_data_string(&data->data, word(entry, 2), &out->alt);
    out->blocked = word(entry, 3) == BLOCKED;
    if (!out->blocked)
        qjs_intl_data_string(&data->data, word(entry, 3), &out->text);
    return QJS_INTL_OK;
}
