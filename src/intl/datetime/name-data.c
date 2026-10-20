/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "name-data.h"
#include "dayperiod-code.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 7
#define GROUP_WIDTH 5
#define NODE_WIDTH 4
#define ENTRY_WIDTH 4
#define BLOCKED UINT32_C(0xfffffffe)
#define SOURCE_CHECKED 1
#define ERA_CHECKED 2

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

static int selectors_valid(uint32_t family, uint32_t cycle, uint32_t context, uint32_t width)
{
    if (family >= QJS_INTL_CAL_NAME_FAMILY_COUNT || cycle >= QJS_INTL_CAL_NAME_CYCLE_COUNT ||
        context >= QJS_INTL_CAL_NAME_CONTEXT_COUNT || width >= QJS_INTL_CAL_NAME_WIDTH_COUNT)
        return 0;
    if ((family == QJS_INTL_CAL_NAME_CYCLIC) != (cycle != QJS_INTL_CAL_NAME_NO_CYCLE))
        return 0;
    if (family == QJS_INTL_CAL_NAME_ERA)
        return context == QJS_INTL_CAL_NAME_NO_CONTEXT && width <= QJS_INTL_CAL_NAME_WIDE;
    if (context == QJS_INTL_CAL_NAME_NO_CONTEXT)
        return 0;
    if (family == QJS_INTL_CAL_NAME_MONTH_PATTERN)
        return width <= QJS_INTL_CAL_NAME_WIDE || width == QJS_INTL_CAL_NAME_ALL;
    return context <= QJS_INTL_CAL_NAME_STAND_ALONE &&
           (width <= QJS_INTL_CAL_NAME_WIDE ||
            (family == QJS_INTL_CAL_NAME_DAY && width == QJS_INTL_CAL_NAME_SHORT));
}

static int decimal_code(QJSIntlBytes text, unsigned int limit)
{
    unsigned int value;
    if (!text.length || text.length > 2 || text.data[0] < '1' || text.data[0] > '9')
        return 0;
    value = (unsigned int)(text.data[0] - '0');
    if (text.length == 2) {
        if (text.data[1] < '0' || text.data[1] > '9')
            return 0;
        value = value * 10 + (unsigned int)(text.data[1] - '0');
    }
    return value <= limit;
}

static int code_valid(QJSIntlBytes text, uint32_t family)
{
    static const char *const days[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    size_t i;
    if (family == QJS_INTL_CAL_NAME_MONTH)
        return decimal_code(text, 13);
    if (family == QJS_INTL_CAL_NAME_QUARTER)
        return decimal_code(text, 4);
    if (family == QJS_INTL_CAL_NAME_CYCLIC)
        return decimal_code(text, 60);
    if (family == QJS_INTL_CAL_NAME_ERA) {
        if (!text.length || (text.length > 1 && text.data[0] == '0'))
            return 0;
        for (i = 0; i < text.length; i++) {
            if (text.data[i] < '0' || text.data[i] > '9')
                return 0;
        }
        return 1;
    }
    if (family == QJS_INTL_CAL_NAME_DAY) {
        if (text.length != 3)
            return 0;
        for (i = 0; i < sizeof(days) / sizeof(*days); i++) {
            if (equal(text, days[i]))
                return 1;
        }
        return 0;
    }
    if (family == QJS_INTL_CAL_NAME_DAY_PERIOD)
        return qjs_intl_dayperiod_code_find(text) >= 0;
    return equal(text, "leap") || equal(text, "standardAfterLeap") || equal(text, "combined");
}

static uint32_t group_family(const QJSIntlCalendarNameData *data, uint32_t group)
{
    return word(data->groups, (size_t)group * GROUP_WIDTH + 1);
}

QJSIntlStatus qjs_intl_calendar_name_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                               QJSIntlCalendarNameData *out)
{
    QJSIntlCalendarNameData result = {0};
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
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_CALENDAR_NAMES, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.calendar_count = word(result.section.bytes, 1);
    result.group_count = word(result.section.bytes, 2);
    result.row_count = word(result.section.bytes, 3);
    result.map_count = word(result.section.bytes, 4);
    result.node_count = word(result.section.bytes, 5);
    result.entry_count = word(result.section.bytes, 6);
    if (!result.calendar_count || !result.group_count || !result.row_count ||
        result.row_count > result.section.count || result.map_count > result.node_count ||
        result.entry_count < result.map_count || qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.calendar_count, 1, &result.calendars) ||
        !take(&result.section, &at, result.group_count, GROUP_WIDTH, &result.groups) ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, result.group_count, &result.rows) ||
        !take(&result.section, &at, result.map_count, 2, &result.maps) ||
        !take(&result.section, &at, result.node_count, NODE_WIDTH, &result.nodes) ||
        !take(&result.section, &at, result.entry_count, ENTRY_WIDTH, &result.entries) ||
        at != result.section.length / 4 || !ordered(result.calendars, result.calendar_count, 1) ||
        !ordered(result.groups, result.group_count, GROUP_WIDTH) ||
        !ordered(result.rows, result.row_count, result.group_count))
        return status;
    for (i = 0; i < result.calendar_count; i++) {
        QJSIntlBytes text;
        if (qjs_intl_data_string(data, word(result.calendars, i), &text) || !calendar_valid(text))
            return status;
    }
    for (i = 0; i < result.group_count; i++) {
        const uint8_t *group = result.groups + i * GROUP_WIDTH * 4;
        if (word(group, 0) >= result.calendar_count ||
            !selectors_valid(word(group, 1), word(group, 2), word(group, 3), word(group, 4)))
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
        if (word(entry, 0) >= data->strings.count || word(entry, 1) >= QJS_INTL_CAL_NAME_YEAR_COUNT ||
            word(entry, 2) >= data->strings.count ||
            (word(entry, 3) != BLOCKED &&
             (qjs_intl_data_string(data, word(entry, 3), &text) ||
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
            uint32_t code = word(entry, 0), year = word(entry, 1);
            QJSIntlBytes text;
            if (year && family != QJS_INTL_CAL_NAME_MONTH)
                goto done;
            /* Raw era IDs have no fixed length cap. Cache by string too, so
               distinct maps sharing one long ID cannot repeat its scan. */
            if (family == QJS_INTL_CAL_NAME_ERA && (strings[code] & ERA_CHECKED))
                continue;
            qjs_intl_data_string(data, code, &text);
            if (!code_valid(text, family))
                goto done;
            if (family == QJS_INTL_CAL_NAME_ERA)
                strings[code] |= ERA_CHECKED;
        }
    }
    for (i = 0; i < result.section.count; i++) {
        uint32_t row = word(result.locale_rows, i);
        if (row >= result.row_count)
            goto done;
        used_rows[row] = 1;
    }
    for (i = 0; i < result.row_count; i++) {
        size_t group;
        if (!used_rows[i])
            goto done;
        for (group = 0; group < result.group_count; group++) {
            uint32_t head = word(result.rows, i * result.group_count + group);
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

QJSIntlStatus qjs_intl_calendar_name_calendar_at(const QJSIntlCalendarNameData *data, uint32_t index,
                                                 QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->calendar_count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_data_string(&data->data, word(data->calendars, index), out);
}

QJSIntlStatus qjs_intl_calendar_name_group_at(const QJSIntlCalendarNameData *data, uint32_t index,
                                              QJSIntlCalendarNameGroup *out)
{
    const uint8_t *group;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    group = data->groups + (size_t)index * GROUP_WIDTH * 4;
    out->calendar = word(group, 0);
    out->family = word(group, 1);
    out->cycle = word(group, 2);
    out->context = word(group, 3);
    out->width = word(group, 4);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_calendar_name_find_group(const QJSIntlCalendarNameData *data,
                                                const QJSIntlCalendarNameGroup *key, uint32_t *out)
{
    uint32_t low = 0, high, wanted[GROUP_WIDTH];
    size_t i;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || !key || key->calendar >= data->calendar_count ||
        !selectors_valid(key->family, key->cycle, key->context, key->width))
        return QJS_INTL_INVALID_ARGUMENT;
    wanted[0] = key->calendar;
    wanted[1] = key->family;
    wanted[2] = key->cycle;
    wanted[3] = key->context;
    wanted[4] = key->width;
    high = data->group_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        for (i = 0; i < GROUP_WIDTH; i++) {
            if (word(data->groups, (size_t)mid * GROUP_WIDTH + i) != wanted[i])
                break;
        }
        if (i < GROUP_WIDTH && word(data->groups, (size_t)mid * GROUP_WIDTH + i) < wanted[i])
            low = mid + 1;
        else
            high = mid;
    }
    if (low >= data->group_count)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < GROUP_WIDTH; i++) {
        if (word(data->groups, (size_t)low * GROUP_WIDTH + i) != wanted[i])
            return QJS_INTL_UNSUPPORTED;
    }
    *out = low;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_calendar_name_head(const QJSIntlCalendarNameData *data, uint32_t locale,
                                          uint32_t group, uint32_t *out)
{
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || locale >= data->section.count || group >= data->group_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = word(data->rows, (size_t)word(data->locale_rows, locale) * data->group_count + group);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_calendar_name_node(const QJSIntlCalendarNameData *data, uint32_t index,
                                          QJSIntlCalendarNameNode *out)
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

static int compare_bytes(QJSIntlBytes left, QJSIntlBytes right)
{
    size_t common = left.length < right.length ? left.length : right.length;
    int order = common ? memcmp(left.data, right.data, common) : 0;
    if (order)
        return order;
    return left.length == right.length ? 0 : left.length < right.length ? -1 : 1;
}

static int compare_key(const QJSIntlCalendarNameData *data, const uint8_t *entry,
                       QJSIntlBytes code, QJSIntlCalendarNameYear year, QJSIntlBytes alt)
{
    QJSIntlBytes text;
    int order;
    qjs_intl_data_string(&data->data, word(entry, 0), &text);
    order = compare_bytes(text, code);
    if (order)
        return order;
    if (word(entry, 1) != (unsigned int)year)
        return word(entry, 1) < (unsigned int)year ? -1 : 1;
    qjs_intl_data_string(&data->data, word(entry, 2), &text);
    return compare_bytes(text, alt);
}

QJSIntlStatus qjs_intl_calendar_name_lookup(const QJSIntlCalendarNameData *data, uint32_t index,
                                            QJSIntlBytes code, QJSIntlCalendarNameYear year,
                                            QJSIntlBytes alt, QJSIntlCalendarNameEntry *out)
{
    uint32_t map, start, low = 0, high, count;
    const uint8_t *entry;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !index || index > data->node_count ||
        (!code.data && code.length) || (!alt.data && alt.length) ||
        (unsigned int)year >= QJS_INTL_CAL_NAME_YEAR_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    map = word(data->nodes, ((size_t)index - 1) * NODE_WIDTH + 2);
    start = word(data->maps, (size_t)map * 2);
    count = high = word(data->maps, (size_t)map * 2 + 1);
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        entry = data->entries + ((size_t)start + mid) * ENTRY_WIDTH * 4;
        if (compare_key(data, entry, code, year, alt) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    if (low >= count)
        return QJS_INTL_UNSUPPORTED;
    entry = data->entries + ((size_t)start + low) * ENTRY_WIDTH * 4;
    if (compare_key(data, entry, code, year, alt))
        return QJS_INTL_UNSUPPORTED;
    qjs_intl_data_string(&data->data, word(entry, 0), &out->code);
    out->year = word(entry, 1);
    qjs_intl_data_string(&data->data, word(entry, 2), &out->alt);
    out->blocked = word(entry, 3) == BLOCKED;
    if (!out->blocked)
        qjs_intl_data_string(&data->data, word(entry, 3), &out->text);
    return QJS_INTL_OK;
}
