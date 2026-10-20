/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "interval-data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 7
#define SUFFIX_WIDTH 4
#define NODE_WIDTH 4
#define ENTRY_WIDTH 3
#define BLOCKED UINT32_C(0xfffffffe)
#define SOURCE_CHECKED 1
#define SKELETON_CHECKED 16

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

static int calendar_valid(QJSIntlBytes text)
{
    size_t i;
    unsigned int separator = 0;
    if (!text.length || text.data[0] < 'a' || text.data[0] > 'z')
        return 0;
    for (i = 1; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            separator = 0;
        else if (c == '-' && !separator)
            separator = 1;
        else
            return 0;
    }
    return !separator;
}

static int skeleton_valid(QJSIntlBytes text)
{
    size_t i;
    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            return 0;
    }
    return 1;
}

static int code_valid(QJSIntlBytes text, uint32_t family)
{
    if (family == QJS_INTL_DATE_INTERVAL_FALLBACK)
        return !text.length;
    if (family == QJS_INTL_DATE_INTERVAL_ITEM)
        return text.length == 1 && memchr("BGHMadhmy", (unsigned char)text.data[0], 9) != NULL;
    return equal(text, "numeric") || equal(text, "non-numeric") || equal(text, "mixed");
}

static uint32_t group_family(const QJSIntlDateIntervalData *data, uint32_t group)
{
    return word(data->suffixes, (size_t)(group % data->suffix_count) * SUFFIX_WIDTH + 1);
}

QJSIntlStatus qjs_intl_date_interval_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                               QJSIntlDateIntervalData *out)
{
    QJSIntlDateIntervalData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, i, scratch;
    uint32_t *depths;
    uint8_t *used_rows, *used_nodes, *map_families, *strings;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes || !data->locales.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_DATE_INTERVALS, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.calendar_count = word(result.section.bytes, 1);
    result.suffix_count = word(result.section.bytes, 2);
    result.row_count = word(result.section.bytes, 3);
    result.map_count = word(result.section.bytes, 4);
    result.node_count = word(result.section.bytes, 5);
    result.entry_count = word(result.section.bytes, 6);
    if (!result.calendar_count || !result.suffix_count ||
        result.calendar_count > UINT32_MAX / result.suffix_count)
        return status;
    result.group_count = result.calendar_count * result.suffix_count;
    if (!result.row_count || result.map_count > result.node_count ||
        result.entry_count < result.map_count || qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.calendar_count, 1, &result.calendars) ||
        !take(&result.section, &at, result.suffix_count, SUFFIX_WIDTH, &result.suffixes) ||
        !take(&result.section, &at, result.section.count, result.calendar_count, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, result.suffix_count, &result.rows) ||
        !take(&result.section, &at, result.map_count, 2, &result.maps) ||
        !take(&result.section, &at, result.node_count, NODE_WIDTH, &result.nodes) ||
        !take(&result.section, &at, result.entry_count, ENTRY_WIDTH, &result.entries) ||
        at != result.section.length / 4 ||
        result.row_count > (size_t)result.section.count * result.calendar_count ||
        !ordered(result.calendars, result.calendar_count, 1) ||
        !ordered(result.suffixes, result.suffix_count, SUFFIX_WIDTH) ||
        !ordered(result.rows, result.row_count, result.suffix_count))
        return status;
    for (i = 0; i < result.calendar_count; i++) {
        QJSIntlBytes text;
        if (qjs_intl_data_string(data, word(result.calendars, i), &text) || !calendar_valid(text))
            return status;
    }
    for (i = 0; i < result.suffix_count; i++) {
        const uint8_t *suffix = result.suffixes + i * SUFFIX_WIDTH * 4;
        if (word(suffix, 0) >= data->strings.count ||
            word(suffix, 1) >= QJS_INTL_DATE_INTERVAL_FAMILY_COUNT ||
            word(suffix, 2) >= data->strings.count || word(suffix, 3) >= data->strings.count ||
            (word(suffix, 1) != QJS_INTL_DATE_INTERVAL_ITEM && (word(suffix, 2) || word(suffix, 3))))
            return status;
    }
    at = 0;
    for (i = 0; i < result.map_count; i++) {
        uint32_t start = word(result.maps, i * 2), count = word(result.maps, i * 2 + 1);
        size_t j;
        if (start != at || !count || count > result.entry_count - at)
            return status;
        for (j = 1; j < count; j++) {
            if (compare_words(result.entries + (at + j - 1) * ENTRY_WIDTH * 4,
                              result.entries + (at + j) * ENTRY_WIDTH * 4, 2) >= 0)
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
        if (word(entry, 0) >= data->strings.count || word(entry, 1) >= data->strings.count ||
            (word(entry, 2) != BLOCKED &&
             (qjs_intl_data_string(data, word(entry, 2), &text) ||
              equal(text, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91") ||
              equal(text, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"))))
            return status;
    }
    /* Counts occupy disjoint spans in the bounded pack, including its string
       offsets. These smaller depth/coverage arrays also fit on 32-bit. */
    scratch = ((size_t)result.node_count + 1) * sizeof(*depths) + result.row_count +
              result.node_count + result.map_count + data->strings.count;
    depths = allocator->malloc(allocator->opaque, scratch);
    if (!depths)
        return QJS_INTL_NO_MEMORY;
    memset(depths, 0, scratch);
    used_rows = (uint8_t *)(depths + (size_t)result.node_count + 1);
    used_nodes = used_rows + result.row_count;
    map_families = used_nodes + result.node_count;
    strings = map_families + result.map_count;
    for (i = 0; i < result.suffix_count; i++) {
        const uint8_t *suffix = result.suffixes + i * SUFFIX_WIDTH * 4;
        uint32_t skeleton = word(suffix, 2);
        if (word(suffix, 1) == QJS_INTL_DATE_INTERVAL_ITEM && !(strings[skeleton] & SKELETON_CHECKED)) {
            QJSIntlBytes text;
            qjs_intl_data_string(data, skeleton, &text);
            if (!skeleton_valid(text))
                goto done;
            strings[skeleton] |= SKELETON_CHECKED;
        }
    }
    /* Validate all node group indexes before row heads dereference them. */
    for (i = 0; i < result.node_count; i++) {
        const uint8_t *node = result.nodes + i * NODE_WIDTH * 4;
        uint32_t origin = word(node, 0), group = word(node, 1), map = word(node, 2), parent = word(node, 3);
        uint32_t family, start, count;
        size_t j;
        if (origin >= data->strings.count || group >= result.group_count || map >= result.map_count || parent > i)
            goto done;
        family = group_family(&result, group);
        if (parent) {
            uint32_t parent_group = word(result.nodes, ((size_t)parent - 1) * NODE_WIDTH + 1);
            if (group_family(&result, parent_group) != family)
                goto done;
        }
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
        /* Shared maps are checked once for each consuming family. */
        if (map_families[map] & (1u << family))
            continue;
        map_families[map] |= (uint8_t)(1u << family);
        start = word(result.maps, (size_t)map * 2);
        count = word(result.maps, (size_t)map * 2 + 1);
        for (j = 0; j < count; j++) {
            const uint8_t *entry = result.entries + ((size_t)start + j) * ENTRY_WIDTH * 4;
            uint32_t code = word(entry, 0);
            uint8_t bit = (uint8_t)(1u << (family + 1));
            QJSIntlBytes text;
            if (strings[code] & bit)
                continue;
            qjs_intl_data_string(data, code, &text);
            if (!code_valid(text, family))
                goto done;
            strings[code] |= bit;
        }
    }
    for (i = 0; i < (size_t)result.section.count * result.calendar_count; i++) {
        uint32_t row = word(result.locale_rows, i);
        if (row >= result.row_count)
            goto done;
        used_rows[row] = 1;
    }
    for (i = 0; i < result.row_count; i++) {
        size_t group;
        if (!used_rows[i])
            goto done;
        for (group = 0; group < result.suffix_count; group++) {
            uint32_t head = word(result.rows, i * result.suffix_count + group);
            if (head > result.node_count)
                goto done;
            if (head) {
                uint32_t origin_group = word(result.nodes, ((size_t)head - 1) * NODE_WIDTH + 1);
                if (group_family(&result, origin_group) != group_family(&result, (uint32_t)group))
                    goto done;
                used_nodes[head - 1] = 1;
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
        if (!map_families[i])
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, depths);
    return status;
}

QJSIntlStatus qjs_intl_date_interval_calendar_at(const QJSIntlDateIntervalData *data, uint32_t index,
                                                 QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->calendar_count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_data_string(&data->data, word(data->calendars, index), out);
}

QJSIntlStatus qjs_intl_date_interval_group_at(const QJSIntlDateIntervalData *data, uint32_t index,
                                              QJSIntlDateIntervalGroup *out)
{
    const uint8_t *suffix;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    suffix = data->suffixes + (size_t)(index % data->suffix_count) * SUFFIX_WIDTH * 4;
    out->calendar = index / data->suffix_count;
    qjs_intl_data_string(&data->data, word(suffix, 0), &out->alt);
    out->family = word(suffix, 1);
    qjs_intl_data_string(&data->data, word(suffix, 2), &out->skeleton);
    qjs_intl_data_string(&data->data, word(suffix, 3), &out->item_alt);
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

static int compare_suffix(const QJSIntlDateIntervalData *data, uint32_t index,
                          const QJSIntlDateIntervalGroup *key)
{
    const uint8_t *suffix = data->suffixes + (size_t)index * SUFFIX_WIDTH * 4;
    QJSIntlBytes text;
    int order;
    qjs_intl_data_string(&data->data, word(suffix, 0), &text);
    order = compare_bytes(text, key->alt);
    if (order)
        return order;
    if (word(suffix, 1) != (unsigned int)key->family)
        return word(suffix, 1) < (unsigned int)key->family ? -1 : 1;
    qjs_intl_data_string(&data->data, word(suffix, 2), &text);
    order = compare_bytes(text, key->skeleton);
    if (order)
        return order;
    qjs_intl_data_string(&data->data, word(suffix, 3), &text);
    return compare_bytes(text, key->item_alt);
}

QJSIntlStatus qjs_intl_date_interval_find_group(const QJSIntlDateIntervalData *data,
                                                const QJSIntlDateIntervalGroup *key, uint32_t *out)
{
    uint32_t low = 0, high;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || !key || key->calendar >= data->calendar_count ||
        (unsigned int)key->family >= QJS_INTL_DATE_INTERVAL_FAMILY_COUNT ||
        (!key->alt.data && key->alt.length) || (!key->skeleton.data && key->skeleton.length) ||
        (!key->item_alt.data && key->item_alt.length))
        return QJS_INTL_INVALID_ARGUMENT;
    high = data->suffix_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        if (compare_suffix(data, mid, key) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    if (low >= data->suffix_count || compare_suffix(data, low, key))
        return QJS_INTL_UNSUPPORTED;
    *out = key->calendar * data->suffix_count + low;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_interval_head(const QJSIntlDateIntervalData *data, uint32_t locale,
                                          uint32_t group, uint32_t *out)
{
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || locale >= data->section.count || group >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    {
        size_t slot = (size_t)locale * data->calendar_count + group / data->suffix_count;
        uint32_t row = word(data->locale_rows, slot);
        *out = word(data->rows, (size_t)row * data->suffix_count + group % data->suffix_count);
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_interval_node(const QJSIntlDateIntervalData *data, uint32_t index,
                                          QJSIntlDateIntervalNode *out)
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

static int compare_key(const QJSIntlDateIntervalData *data, const uint8_t *entry,
                       QJSIntlBytes code, QJSIntlBytes alt)
{
    QJSIntlBytes text;
    int order;
    qjs_intl_data_string(&data->data, word(entry, 0), &text);
    order = compare_bytes(text, code);
    if (order)
        return order;
    qjs_intl_data_string(&data->data, word(entry, 1), &text);
    return compare_bytes(text, alt);
}

QJSIntlStatus qjs_intl_date_interval_lookup(const QJSIntlDateIntervalData *data, uint32_t index,
                                            QJSIntlBytes code, QJSIntlBytes alt,
                                            QJSIntlDateIntervalEntry *out)
{
    uint32_t map, start, low = 0, high, count;
    const uint8_t *entry;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !index || index > data->node_count ||
        (!code.data && code.length) || (!alt.data && alt.length))
        return QJS_INTL_INVALID_ARGUMENT;
    map = word(data->nodes, ((size_t)index - 1) * NODE_WIDTH + 2);
    start = word(data->maps, (size_t)map * 2);
    count = high = word(data->maps, (size_t)map * 2 + 1);
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        entry = data->entries + ((size_t)start + mid) * ENTRY_WIDTH * 4;
        if (compare_key(data, entry, code, alt) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    if (low >= count)
        return QJS_INTL_UNSUPPORTED;
    entry = data->entries + ((size_t)start + low) * ENTRY_WIDTH * 4;
    if (compare_key(data, entry, code, alt))
        return QJS_INTL_UNSUPPORTED;
    qjs_intl_data_string(&data->data, word(entry, 0), &out->code);
    qjs_intl_data_string(&data->data, word(entry, 1), &out->alt);
    out->blocked = word(entry, 2) == BLOCKED;
    if (!out->blocked)
        qjs_intl_data_string(&data->data, word(entry, 2), &out->text);
    return QJS_INTL_OK;
}
