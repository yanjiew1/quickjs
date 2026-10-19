/* Native ListFormat section validation and borrowed queries.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "data.h"
#include "../native/wire.h"
#include <string.h>

enum { LIST_VARIANTS = 9, LIST_PARTS = 4, LIST_WIDTH = 36, CONTEXT_WIDTH = 27 };

static QJSIntlBytes string_at(const QJSIntlListData *data, uint32_t index)
{
    QJSIntlBytes text;
    qjs_intl_data_string(&data->data, index, &text);
    return text;
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

static int valid_lists(const QJSIntlListData *data, uint8_t *states)
{
    const QJSIntlSection *section = &data->lists;
    const uint8_t *vectors;
    uint32_t sets, index, field;
    size_t head;

    if (!section->bytes)
        return 1;
    if (section->count != data->data.locales.count || section->length < 4)
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
    memset(states, 0, data->data.strings.count);
    vectors = section->bytes + head;
    for (index = 0; index < sets; index++) {
        const uint8_t *row = vectors + (size_t)index * LIST_WIDTH * 4;
        int order = 0;

        for (field = 0; field < LIST_WIDTH; field++) {
            uint32_t value = qjs_intl_read_u32(row + field * 4);
            if (value >= data->data.strings.count)
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

static const uint8_t *context_at(const QJSIntlListData *data, uint32_t locale,
                                 unsigned int variant)
{
    const QJSIntlSection *section = &data->list_contexts;
    size_t set = qjs_intl_read_u32(section->bytes + 4 + (size_t)locale * 4);
    size_t head = 4 + (size_t)section->count * 4;
    return section->bytes + head + (set * CONTEXT_WIDTH + variant * 3) * 4;
}

static int equals(QJSIntlBytes text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && !memcmp(text.data, expected, length);
}

static int language(QJSIntlBytes tag, const char *expected)
{
    return tag.length >= 2 && !memcmp(tag.data, expected, 2) &&
           (tag.length == 2 || tag.data[2] == '-');
}

static int valid_contexts(const QJSIntlListData *data, uint8_t *states)
{
    const QJSIntlSection *section = &data->list_contexts;
    uint32_t sets, index, field;
    size_t head;

    if (!section->bytes)
        return 1;
    if (!data->lists.bytes || section->count != data->data.locales.count ||
        section->length < 4)
        return 0;
    sets = qjs_intl_read_u32(section->bytes);
    head = 4 + (size_t)section->count * 4;
    if (!sets || sets > section->count || head > section->length ||
        (section->length - head) / (CONTEXT_WIDTH * 4) != sets ||
        (section->length - head) % (CONTEXT_WIDTH * 4))
        return 0;
    memset(states, 0, sets);
    for (index = 0; index < section->count; index++) {
        uint32_t set = qjs_intl_read_u32(section->bytes + 4 + (size_t)index * 4);
        if (set >= sets)
            return 0;
        states[set] = 1;
    }
    for (index = 0; index < sets; index++) {
        const uint8_t *row = section->bytes + head + (size_t)index * CONTEXT_WIDTH * 4;
        int order = 0;
        if (!states[index])
            return 0;
        for (field = 0; field < CONTEXT_WIDTH; field++) {
            uint32_t value = qjs_intl_read_u32(row + field * 4);
            if ((field % 3 == 0 && value > 3) ||
                (field % 3 != 0 && value >= data->data.strings.count))
                return 0;
            if (index && !order) {
                uint32_t prior = qjs_intl_read_u32(row - CONTEXT_WIDTH * 4 + field * 4);
                order = (value > prior) - (value < prior);
            }
        }
        if (index && order <= 0)
            return 0;
    }
    /* Exact-default safeguards belong to the data admission boundary too:
       a valid string ID must not enable an unrelated language or replace a
       locale-specific pair/end pattern with the generic contextual form. */
    for (index = 0; index < section->count; index++) {
        QJSIntlBytes tag;
        uint32_t parent;
        unsigned int variant;
        qjs_intl_data_locale(&data->data, index, &tag, &parent);
        for (variant = 0; variant < LIST_VARIANTS; variant++) {
            const uint8_t *row = context_at(data, index, variant);
            uint32_t kind = qjs_intl_read_u32(row);
            uint32_t ids[2] = {qjs_intl_read_u32(row + 4), qjs_intl_read_u32(row + 8)};
            QJSIntlBytes base[2];
            const char *old = "", *replacement = "";
            unsigned int expected = 0, part;

            if (language(tag, "es")) {
                int disjunction = variant >= 3 && variant < 6;
                expected = disjunction ? 2 : 1;
                old = disjunction ? "{0} o {1}" : "{0} y {1}";
                replacement = disjunction ? "{0} u {1}" : "{0} e {1}";
            } else if (language(tag, "he")) {
                expected = 3;
                old = "{0} ו{1}";
                replacement = "{0} ו-{1}";
            }
            qjs_intl_list_data_template(data, index, variant, 0, &base[0]);
            qjs_intl_list_data_template(data, index, variant, 3, &base[1]);
            if (!equals(base[0], old) && !equals(base[1], old))
                expected = 0;
            if (kind != expected)
                return 0;
            for (part = 0; part < 2; part++) {
                QJSIntlBytes alternate = string_at(data, ids[part]);
                if (!kind) {
                    if (ids[part])
                        return 0;
                } else if (equals(base[part], old)) {
                    if (!equals(alternate, replacement))
                        return 0;
                } else if (base[part].data != alternate.data ||
                           base[part].length != alternate.length) {
                    /* Validated strings are unique. Their borrowed identity
                       avoids rescanning a shared, arbitrarily long template
                       for every locale that inherits it. */
                    return 0;
                }
            }
        }
    }
    return 1;
}

QJSIntlStatus qjs_intl_list_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                      QJSIntlListData *out)
{
    QJSIntlListData result = {0};
    QJSIntlStatus status;
    uint8_t *states;
    size_t scratch;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_LISTS, &result.lists);
    qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_LIST_CONTEXTS, &result.list_contexts);
    if (!result.lists.bytes)
        return result.list_contexts.bytes ? QJS_INTL_DATA_ERROR : QJS_INTL_UNSUPPORTED;
    scratch = data->locales.count > data->strings.count ? data->locales.count : data->strings.count;
    states = allocator->malloc(allocator->opaque, scratch);
    if (!states)
        return QJS_INTL_NO_MEMORY;
    status = valid_lists(&result, states) && valid_contexts(&result, states)
                 ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
    allocator->free(allocator->opaque, states);
    if (!status)
        *out = result;
    return status;
}

QJSIntlStatus qjs_intl_list_data_template(const QJSIntlListData *data, uint32_t locale,
                                        unsigned int variant, unsigned int part,
                                        QJSIntlBytes *out)
{
    size_t head, set;
    uint32_t index;

    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data || !data->data.pack.bytes || locale >= data->data.locales.count ||
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

QJSIntlStatus qjs_intl_list_data_context(const QJSIntlListData *data, uint32_t locale,
                                       unsigned int variant, unsigned int *kind,
                                       QJSIntlBytes *pair, QJSIntlBytes *end)
{
    const uint8_t *row;

    if (kind)
        *kind = 0;
    if (pair)
        memset(pair, 0, sizeof(*pair));
    if (end)
        memset(end, 0, sizeof(*end));
    if (!data || !data->data.pack.bytes || !kind || !pair || !end ||
        locale >= data->data.locales.count || variant >= LIST_VARIANTS)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!data->list_contexts.bytes)
        return QJS_INTL_UNSUPPORTED;
    row = context_at(data, locale, variant);
    *kind = qjs_intl_read_u32(row);
    if (*kind) {
        *pair = string_at(data, qjs_intl_read_u32(row + 4));
        *end = string_at(data, qjs_intl_read_u32(row + 8));
    }
    return QJS_INTL_OK;
}
