/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "data.h"
#include "wire.h"
#include <string.h>

enum { LIST_VARIANTS = 9, LIST_PARTS = 4, LIST_WIDTH = 36 };

static QJSIntlBytes string_at(const QJSIntlData *data, uint32_t index)
{
    const uint8_t *table = data->strings.bytes;
    size_t low = qjs_intl_read_u32(table + (size_t)index * 4);
    size_t high = qjs_intl_read_u32(table + ((size_t)index + 1) * 4);
    QJSIntlBytes result = {
        (const char *)table + ((size_t)data->strings.count + 1) * 4 + low,
        high - low,
    };
    return result;
}

static int compare_text(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = memcmp(a.data, b.data, length);

    if (order)
        return order;
    return (a.length > b.length) - (a.length < b.length);
}

static int valid_utf8(const uint8_t *bytes, size_t length)
{
    size_t at = 0;

    while (at < length) {
        uint32_t value = bytes[at++], minimum;
        unsigned int more;

        if (!value)
            return 0;
        if (value < 0x80)
            continue;
        if (value >= 0xc2 && value <= 0xdf) {
            more = 1;
            minimum = 0x80;
            value &= 0x1f;
        } else if (value >= 0xe0 && value <= 0xef) {
            more = 2;
            minimum = 0x800;
            value &= 0xf;
        } else if (value >= 0xf0 && value <= 0xf4) {
            more = 3;
            minimum = 0x10000;
            value &= 7;
        } else {
            return 0;
        }
        if (more > length - at)
            return 0;
        while (more--) {
            uint32_t next = bytes[at++];
            if ((next & 0xc0) != 0x80)
                return 0;
            value = value << 6 | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            return 0;
    }
    return 1;
}

static int valid_strings(const QJSIntlData *data)
{
    const QJSIntlSection *section = &data->strings;
    const uint8_t *table = section->bytes;
    uint32_t index;
    size_t start;

    if (!section->count || section->count >= section->length / 4)
        return 0;
    start = ((size_t)section->count + 1) * 4;
    if (qjs_intl_read_u32(table) || qjs_intl_read_u32(table + 4) ||
        qjs_intl_read_u32(table + (size_t)section->count * 4) != section->length - start)
        return 0;
    for (index = 0; index < section->count; index++) {
        size_t low = qjs_intl_read_u32(table + (size_t)index * 4);
        size_t high = qjs_intl_read_u32(table + ((size_t)index + 1) * 4);

        if (high < low || high > section->length - start ||
            !valid_utf8(table + start + low, high - low))
            return 0;
        if (index && compare_text(string_at(data, index - 1), string_at(data, index)) >= 0)
            return 0;
    }
    return 1;
}

static uint32_t locale_parent(const QJSIntlData *data, uint32_t index)
{
    return qjs_intl_read_u32(data->locales.bytes + (size_t)index * 8 + 4);
}

static int valid_locales(const QJSIntlData *data, uint8_t *states)
{
    const QJSIntlSection *section = &data->locales;
    uint32_t index, previous = 0;
    unsigned int roots = 0;

    for (index = 0; index < section->count; index++) {
        const uint8_t *row = section->bytes + (size_t)index * 8;
        uint32_t name = qjs_intl_read_u32(row), parent = locale_parent(data, index);
        QJSIntlBytes text;
        int root;

        if (!name || name >= data->strings.count || (index && name <= previous) ||
            (parent != UINT32_MAX && parent >= section->count))
            return 0;
        text = string_at(data, name);
        root = text.length == 4 && !memcmp(text.data, "root", 4);
        if (root != (parent == UINT32_MAX))
            return 0;
        roots += root;
        previous = name;
    }
    if (roots != 1)
        return 0;
    memset(states, 0, section->count);
    for (index = 0; index < section->count; index++) {
        uint32_t next = index;
        while (next != UINT32_MAX && !states[next]) {
            states[next] = 1;
            next = locale_parent(data, next);
        }
        if (next != UINT32_MAX && states[next] == 1)
            return 0;
        next = index;
        while (next != UINT32_MAX && states[next] == 1) {
            states[next] = 2;
            next = locale_parent(data, next);
        }
    }
    return 1;
}

static int valid_template(QJSIntlBytes text)
{
    size_t at;
    unsigned int seen = 0;

    for (at = 0; at < text.length; at++) {
        if (text.data[at] == '{') {
            unsigned int bit;
            if (text.length - at < 3 || text.data[at + 2] != '}' ||
                (text.data[at + 1] != '0' && text.data[at + 1] != '1'))
                return 0;
            bit = 1u << (text.data[at + 1] - '0');
            if (seen & bit)
                return 0;
            seen |= bit;
            at += 2;
        } else if (text.data[at] == '}') {
            return 0;
        }
    }
    return seen == 3;
}

static int valid_lists(const QJSIntlData *data, uint8_t *states)
{
    const QJSIntlSection *section = &data->lists;
    const uint8_t *vectors;
    uint32_t sets, index, field;
    size_t head;

    if (!section->bytes)
        return 1;
    if (section->count != data->locales.count || section->length < 4)
        return 0;
    sets = qjs_intl_read_u32(section->bytes);
    head = 4 + (size_t)section->count * 4;
    if (!sets || sets > section->count || head > section->length ||
        (section->length - head) / (LIST_WIDTH * 4) != sets ||
        (section->length - head) % (LIST_WIDTH * 4))
        return 0;
    memset(states, 0, sets);
    for (index = 0; index < section->count; index++) {
        uint32_t set = qjs_intl_read_u32(section->bytes + 4 + (size_t)index * 4);
        if (set >= sets)
            return 0;
        states[set] = 1;
    }
    for (index = 0; index < sets; index++) {
        if (!states[index])
            return 0;
    }
    /* A long template may be shared by many vectors. Validate its text only
       once rather than multiplying its length by the reference count. */
    memset(states, 0, data->strings.count);
    vectors = section->bytes + head;
    for (index = 0; index < sets; index++) {
        const uint8_t *row = vectors + (size_t)index * LIST_WIDTH * 4;
        int order = 0;

        for (field = 0; field < LIST_WIDTH; field++) {
            uint32_t value = qjs_intl_read_u32(row + field * 4);
            if (value >= data->strings.count)
                return 0;
            if (!states[value]) {
                if (!valid_template(string_at(data, value)))
                    return 0;
                states[value] = 1;
            }
            if (index && !order) {
                uint32_t prior = qjs_intl_read_u32(row - LIST_WIDTH * 4 + field * 4);
                order = (value > prior) - (value < prior);
            }
        }
        if (index && order <= 0)
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_data_open(const void *bytes, size_t length,
                               const QJSIntlAllocator *allocator,
                               QJSIntlData *out)
{
    QJSIntlData view;
    QJSIntlStatus status;
    uint8_t *states;
    size_t scratch;

    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(&view, 0, sizeof(view));
    status = qjs_intl_pack_open(bytes, length, &view.pack);
    if (status != QJS_INTL_OK)
        return status;
    if (qjs_intl_pack_section(&view.pack, QJS_INTL_SECTION_STRINGS, &view.strings) != QJS_INTL_OK ||
        qjs_intl_pack_section(&view.pack, QJS_INTL_SECTION_LOCALES, &view.locales) != QJS_INTL_OK ||
        !valid_strings(&view) || !view.locales.count ||
        view.locales.length % 8 || view.locales.length / 8 != view.locales.count)
        return QJS_INTL_DATA_ERROR;
    qjs_intl_pack_section(&view.pack, QJS_INTL_SECTION_LISTS, &view.lists);
    scratch = view.locales.count > view.strings.count ? view.locales.count : view.strings.count;
    states = allocator->malloc(allocator->opaque, scratch);
    if (!states)
        return QJS_INTL_NO_MEMORY;
    status = valid_locales(&view, states) && valid_lists(&view, states) ?
             QJS_INTL_OK : QJS_INTL_DATA_ERROR;
    allocator->free(allocator->opaque, states);
    if (status == QJS_INTL_OK)
        *out = view;
    return status;
}

QJSIntlStatus qjs_intl_data_string(const QJSIntlData *data, uint32_t index,
                                 QJSIntlBytes *out)
{
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data || !data->pack.bytes || index >= data->strings.count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = string_at(data, index);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_data_locale(const QJSIntlData *data, uint32_t index,
                                 QJSIntlBytes *tag, uint32_t *parent)
{
    if (tag)
        memset(tag, 0, sizeof(*tag));
    if (parent)
        *parent = 0;
    if (!tag || !parent || !data || !data->pack.bytes || index >= data->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    *tag = string_at(data, qjs_intl_read_u32(data->locales.bytes + (size_t)index * 8));
    *parent = locale_parent(data, index);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_data_list_template(const QJSIntlData *data, uint32_t locale,
                                        unsigned int variant, unsigned int part,
                                        QJSIntlBytes *out)
{
    size_t head, set;
    uint32_t index;

    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data || !data->pack.bytes || locale >= data->locales.count ||
        variant >= LIST_VARIANTS || part >= LIST_PARTS)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!data->lists.bytes)
        return QJS_INTL_UNSUPPORTED;
    set = qjs_intl_read_u32(data->lists.bytes + 4 + (size_t)locale * 4);
    head = 4 + (size_t)data->lists.count * 4;
    index = qjs_intl_read_u32(data->lists.bytes + head +
                            (set * LIST_WIDTH + variant * LIST_PARTS + part) * 4);
    *out = string_at(data, index);
    return QJS_INTL_OK;
}
