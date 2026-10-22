/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "dictionary.h"
#include "../native/wire.h"
#include <string.h>

enum { HEADER = 16, RECORD = 48, MAX_STATES = 1000000, MAX_EDGES = 2000000 };

static uint32_t word(const uint8_t *array, uint32_t index)
{
    return qjs_intl_read_u32(array + (size_t)index * 4);
}

static uint32_t half(const uint8_t *array, uint32_t index)
{
    const uint8_t *p = array + (size_t)index * 2;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static QJSIntlStatus array(const uint8_t *data, size_t length, size_t *at,
                           size_t count, size_t width, const uint8_t **out)
{
    size_t padding = (4 - (*at & 3)) & 3;
    if (*at > length || padding > length - *at)
        return QJS_INTL_DATA_ERROR;
    while (padding) {
        if (data[(*at)++])
            return QJS_INTL_DATA_ERROR;
        padding--;
    }
    if (count > (length - *at) / width)
        return QJS_INTL_DATA_ERROR;
    *out = data + *at;
    *at += count * width;
    return QJS_INTL_OK;
}

static QJSIntlStatus arrays(const uint8_t *data, size_t length, QJSIntlSegmentDictionary *out)
{
    size_t at = 0;
    QJSIntlStatus status;
    status = array(data, length, &at, out->alphabet_count, 4, &out->alphabet);
    if (!status)
        status = array(data, length, &at, out->value_count, 4, &out->values);
    if (!status)
        status = array(data, length, &at, (size_t)out->state_count + 1, 4, &out->starts);
    if (!status)
        status = array(data, length, &at, out->state_count, 2, &out->terminals);
    if (!status)
        status = array(data, length, &at, out->edge_count, 2, &out->labels);
    if (!status)
        status = array(data, length, &at, out->edge_count, 4, &out->targets);
    return status ? status : at == length ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

static QJSIntlStatus graph(const QJSIntlSegmentDictionary *data, const QJSIntlAllocator *allocator)
{
    uint32_t *paths, *scalars, *units, state, i;
    uint8_t *used_alphabet, *used_values, *reachable;
    size_t size, extra;
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    void *scratch;
    for (i = 0; i < data->alphabet_count; i++) {
        uint32_t scalar = word(data->alphabet, i);
        if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff) ||
            (i && scalar <= word(data->alphabet, i - 1)))
            return QJS_INTL_DATA_ERROR;
    }
    for (i = 1; i < data->value_count; i++)
        if (word(data->values, i) <= word(data->values, i - 1))
            return QJS_INTL_DATA_ERROR;
    if ((!data->valued && (data->value_count != 1 || word(data->values, 0))) ||
        word(data->starts, 0) || word(data->starts, data->state_count) != data->edge_count ||
        half(data->terminals, data->root))
        return QJS_INTL_DATA_ERROR;

    /* Three u32 DP vectors and byte flags. Enforce bounds before arithmetic,
       including on 32-bit targets; no scratch remains in a published view. */
    size = data->state_count;
    if (size > SIZE_MAX / 13)
        return QJS_INTL_OVERFLOW;
    size *= 13;
    extra = (size_t)data->alphabet_count + data->value_count + 1;
    if (extra > SIZE_MAX - size)
        return QJS_INTL_OVERFLOW;
    size += extra;
    scratch = allocator->malloc(allocator->opaque, size);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    paths = scratch;
    scalars = paths + data->state_count;
    units = scalars + data->state_count;
    used_alphabet = (uint8_t *)(units + data->state_count);
    used_values = used_alphabet + data->alphabet_count;
    reachable = used_values + data->value_count + 1;
    memset(used_alphabet, 0, extra + data->state_count);
    for (state = 0; state < data->state_count; state++) {
        uint32_t first = word(data->starts, state), last = word(data->starts, state + 1);
        uint32_t terminal = half(data->terminals, state), count = terminal != 0;
        uint32_t scalar_max = 0, unit_max = 0, previous = 0;
        if (first > last || last > data->edge_count || terminal > data->value_count)
            goto done;
        used_values[terminal] = 1;
        for (i = first; i < last; i++) {
            uint32_t label = half(data->labels, i), child = word(data->targets, i), width;
            if (label >= data->alphabet_count || (i > first && label <= previous) || child >= state)
                goto done;
            previous = label;
            used_alphabet[label] = 1;
            if (paths[child] > data->words - count)
                goto done;
            count += paths[child];
            width = word(data->alphabet, label) > 0xffff ? 2 : 1;
            if (scalars[child] >= data->max_scalars || width > data->max_utf16 ||
                units[child] > data->max_utf16 - width)
                goto done;
            if (1 + scalars[child] > scalar_max)
                scalar_max = 1 + scalars[child];
            if (width + units[child] > unit_max)
                unit_max = width + units[child];
        }
        if (!count)
            goto done;
        paths[state] = count;
        scalars[state] = scalar_max;
        units[state] = unit_max;
    }
    if (paths[data->root] != data->words || scalars[data->root] != data->max_scalars ||
        units[data->root] != data->max_utf16)
        goto done;
    for (i = 0; i < data->alphabet_count; i++)
        if (!used_alphabet[i])
            goto done;
    for (i = 1; i <= data->value_count; i++)
        if (!used_values[i])
            goto done;
    /* Descending order marks each child's reachability before visiting it. */
    reachable[data->root] = 1;
    for (state = data->state_count; state-- > 0;) {
        if (!reachable[state])
            goto done;
        for (i = word(data->starts, state); i < word(data->starts, state + 1); i++)
            reachable[word(data->targets, i)] = 1;
    }
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, scratch);
    return status;
}

QJSIntlStatus qjs_intl_segment_dictionary_data_open(const QJSIntlPack *pack, const QJSIntlAllocator *allocator,
                                                    QJSIntlSegmentDictionaryData *out)
{
    QJSIntlSegmentDictionaryData result = {0};
    QJSIntlSection *section = &result.section;
    uint32_t index, previous = 0, total_states = 0, total_edges = 0;
    size_t expected;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !pack || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(pack, QJS_INTL_SECTION_SEGMENT_DICTIONARIES, section);
    if (status)
        return status;
    if (section->length < HEADER || memcmp(section->bytes, "QJSDICT\0", 8) ||
        !section->count || section->count > QJS_INTL_DICTIONARY_FAMILY_COUNT ||
        word(section->bytes, 3) != section->count)
        return QJS_INTL_DATA_ERROR;
    if (word(section->bytes, 2) != 1)
        return QJS_INTL_UNSUPPORTED;
    expected = HEADER + (size_t)section->count * RECORD;
    if (expected > section->length)
        return QJS_INTL_DATA_ERROR;
    for (index = 0; index < section->count; index++) {
        const uint8_t *record = section->bytes + HEADER + (size_t)index * RECORD;
        uint32_t family = word(record, 0), valued = word(record, 1);
        size_t start = word(record, 10), length = word(record, 11);
        QJSIntlSegmentDictionary *dictionary;
        if (family >= QJS_INTL_DICTIONARY_FAMILY_COUNT || (index && family <= previous) ||
            valued != (family == QJS_INTL_DICTIONARY_CJK) || start != expected ||
            start > section->length || length > section->length - start)
            return QJS_INTL_DATA_ERROR;
        dictionary = &result.dictionaries[family];
        dictionary->root = word(record, 2);
        dictionary->words = word(record, 3);
        dictionary->max_scalars = word(record, 4);
        dictionary->max_utf16 = word(record, 5);
        dictionary->alphabet_count = word(record, 6);
        dictionary->value_count = word(record, 7);
        dictionary->state_count = word(record, 8);
        dictionary->edge_count = word(record, 9);
        dictionary->valued = valued;
        if (!dictionary->words || !dictionary->max_scalars || !dictionary->max_utf16 ||
            !dictionary->alphabet_count || dictionary->alphabet_count > 65536 ||
            !dictionary->value_count || dictionary->value_count > 65535 ||
            dictionary->state_count < 2 || dictionary->state_count > MAX_STATES - total_states ||
            !dictionary->edge_count || dictionary->edge_count > MAX_EDGES - total_edges ||
            dictionary->root != dictionary->state_count - 1)
            return QJS_INTL_DATA_ERROR;
        total_states += dictionary->state_count;
        total_edges += dictionary->edge_count;
        status = arrays(section->bytes + start, length, dictionary);
        if (!status)
            status = graph(dictionary, allocator);
        if (status)
            return status;
        result.present |= 1U << family;
        previous = family;
        expected = start + length;
    }
    if (expected != section->length)
        return QJS_INTL_DATA_ERROR;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_dictionary_at(const QJSIntlSegmentDictionaryData *data,
                                             QJSIntlSegmentDictionaryFamily family,
                                             const QJSIntlSegmentDictionary **out)
{
    if (out)
        *out = NULL;
    if (!out || !data || !data->section.bytes || (unsigned int)family >= QJS_INTL_DICTIONARY_FAMILY_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!(data->present & (1U << family)))
        return QJS_INTL_UNSUPPORTED;
    *out = &data->dictionaries[family];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_dictionary_step(const QJSIntlSegmentDictionary *data, uint32_t state,
                                               uint32_t scalar, uint32_t *out)
{
    uint32_t low, high;
    if (out)
        *out = 0;
    if (!out || !data || !data->starts || state >= data->state_count)
        return QJS_INTL_INVALID_ARGUMENT;
    low = word(data->starts, state);
    high = word(data->starts, state + 1);
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t value = word(data->alphabet, half(data->labels, middle));
        if (value < scalar)
            low = middle + 1;
        else
            high = middle;
    }
    if (low == word(data->starts, state + 1) ||
        word(data->alphabet, half(data->labels, low)) != scalar)
        return QJS_INTL_UNSUPPORTED;
    *out = word(data->targets, low);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_dictionary_value(const QJSIntlSegmentDictionary *data, uint32_t state,
                                                uint32_t *out)
{
    uint32_t terminal;
    if (out)
        *out = 0;
    if (!out || !data || !data->terminals || state >= data->state_count)
        return QJS_INTL_INVALID_ARGUMENT;
    terminal = half(data->terminals, state);
    if (!terminal)
        return QJS_INTL_UNSUPPORTED;
    *out = word(data->values, terminal - 1);
    return QJS_INTL_OK;
}
