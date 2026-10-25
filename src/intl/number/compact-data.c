/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "compact-data.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 8
#define ENTRY_WORDS 4
#define PATTERN_WORDS 6
#define OPTIONAL_EMPTY UINT32_C(0xfffffffd)
#define OPTIONAL_BLOCKED UINT32_C(0xfffffffe)

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

static int compare(const uint8_t *a, size_t ac, const uint8_t *b, size_t bc)
{
    size_t i, count = ac < bc ? ac : bc;
    for (i = 0; i < count; i++) {
        uint32_t av = word(a, i), bv = word(b, i);
        if (av != bv)
            return av < bv ? -1 : 1;
    }
    return ac < bc ? -1 : ac > bc;
}

static int ordered(const uint8_t *records, uint32_t count, size_t width)
{
    uint32_t i;
    for (i = 1; i < count; i++) {
        const uint8_t *previous = records + (size_t)(i - 1) * width * 4;
        if (compare(previous, width, previous + width * 4, width) >= 0)
            return 0;
    }
    return 1;
}

static QJSIntlCompactMap map_at(const QJSIntlCompactData *data, uint32_t index)
{
    QJSIntlCompactMap result;
    result.entries = data->entries + (size_t)word(data->maps, (size_t)index * 2) * ENTRY_WORDS * 4;
    result.count = word(data->maps, (size_t)index * 2 + 1);
    return result;
}

static const uint8_t *find(const QJSIntlCompactMap *map, uint32_t magnitude, uint32_t count,
                           uint32_t variant)
{
    uint32_t lo = 0, hi = map->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t *entry = map->entries + (size_t)mid * ENTRY_WORDS * 4;
        uint32_t m = word(entry, 0), c = word(entry, 1), v = word(entry, 2);
        if (m < magnitude || (m == magnitude && (c < count || (c == count && v < variant))))
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < map->count) {
        const uint8_t *entry = map->entries + (size_t)lo * ENTRY_WORDS * 4;
        if (word(entry, 0) == magnitude && word(entry, 1) == count && word(entry, 2) == variant)
            return entry;
    }
    return NULL;
}

static const uint8_t *pattern_for(const QJSIntlCompactData *data, const uint8_t *entry)
{
    uint32_t reference;
    if (!entry)
        return NULL;
    reference = word(entry, 3);
    return reference < data->pattern_count ? data->patterns + (size_t)reference * PATTERN_WORDS * 4
                                           : NULL;
}

static int fallback_form(const QJSIntlCompactData *data, uint32_t affix, unsigned int form)
{
    uint32_t offset = word(data->affixes, (size_t)affix * 2);
    uint32_t count = word(data->affixes, (size_t)affix * 2 + 1);
    const uint8_t *tokens = data->tokens + (size_t)offset * 8;
    if (count != (form ? 2u : 1u))
        return 0;
    if (form &&
        word(tokens, 0) != (form == QJS_INTL_NUMBER_PATTERN_NEGATIVE ? QJS_INTL_NUMBER_TOKEN_MINUS
                                                                     : QJS_INTL_NUMBER_TOKEN_PLUS))
        return 0;
    return word(tokens, (size_t)(count - 1) * 2) == QJS_INTL_NUMBER_TOKEN_NUMBER;
}

QJSIntlStatus qjs_intl_compact_data_open(const QJSIntlNumberingData *numbering,
                                         const QJSIntlAllocator *allocator, QJSIntlCompactData *out)
{
    QJSIntlCompactData data = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint8_t *used, *used_rows, *used_maps, *used_patterns, *used_affixes, *numbers;
    size_t at = HEADER_WORDS, scratch, i, end = 0, row_width;
    uint32_t numeric = 0;
    QJSIntlBytes empty;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !numbering || !numbering->data.pack.bytes || !numbering->systems.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    data.numbering = *numbering;
    if (qjs_intl_pack_section(&numbering->data.pack, QJS_INTL_SECTION_COMPACT_PATTERNS,
                              &data.section) ||
        !data.section.count || data.section.count != numbering->data.locales.count ||
        data.section.length % 4 || data.section.length < HEADER_WORDS * 4 ||
        word(data.section.bytes, 0) != 1)
        return status;
    data.system_count = word(data.section.bytes, 1);
    data.row_count = word(data.section.bytes, 2);
    data.map_count = word(data.section.bytes, 3);
    data.entry_count = word(data.section.bytes, 4);
    data.pattern_count = word(data.section.bytes, 5);
    data.affix_count = word(data.section.bytes, 6);
    data.token_count = word(data.section.bytes, 7);
    if (!data.system_count || data.system_count > UINT32_MAX / QJS_INTL_COMPACT_KIND_COUNT ||
        !data.row_count || data.row_count > data.section.count || !data.map_count ||
        !data.entry_count || !data.pattern_count || !data.affix_count || !data.token_count)
        return status;
    row_width = (size_t)data.system_count * QJS_INTL_COMPACT_KIND_COUNT;
    if (!take(&data.section, &at, data.system_count, 1, &data.systems) ||
        !take(&data.section, &at, data.section.count, 1, &data.locale_rows) ||
        !take(&data.section, &at, data.row_count, row_width, &data.rows) ||
        !take(&data.section, &at, data.map_count, 2, &data.maps) ||
        !take(&data.section, &at, data.entry_count, ENTRY_WORDS, &data.entries) ||
        !take(&data.section, &at, data.pattern_count, PATTERN_WORDS, &data.patterns) ||
        !take(&data.section, &at, data.affix_count, 2, &data.affixes) ||
        !take(&data.section, &at, data.token_count, 2, &data.tokens) ||
        at != data.section.length / 4)
        return status;
    for (i = 0; i < numbering->systems.count; i++) {
        QJSIntlNumberingSystem system;
        if (qjs_intl_numbering_system_at(numbering, i, &system))
            return status;
        if (system.kind == QJS_INTL_NUMBERING_NUMERIC) {
            if (numeric == data.system_count || word(data.systems, numeric) != i)
                return status;
            numeric++;
        }
    }
    if (numeric != data.system_count || !ordered(data.rows, data.row_count, row_width) ||
        !ordered(data.patterns, data.pattern_count, PATTERN_WORDS) ||
        qjs_intl_data_string(&numbering->data, 0, &empty) || empty.length)
        return status;
    /* These counts are bounded by disjoint word ranges above. Even with two
       bytes per affix, scratch fits size_t on a 32-bit target. */
    scratch =
        (size_t)data.row_count + data.map_count + data.pattern_count + (size_t)data.affix_count * 2;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    used_rows = used;
    used_maps = used_rows + data.row_count;
    used_patterns = used_maps + data.map_count;
    used_affixes = used_patterns + data.pattern_count;
    numbers = used_affixes + data.affix_count;
    for (i = 0; i < data.section.count; i++) {
        uint32_t row = word(data.locale_rows, i);
        if (row >= data.row_count)
            goto done;
        used_rows[row] = 1;
    }
    for (i = 0; i < (size_t)data.row_count * row_width; i++) {
        uint32_t map = word(data.rows, i);
        if (map >= data.map_count ||
            (i % QJS_INTL_COMPACT_KIND_COUNT < QJS_INTL_COMPACT_CURRENCY_SHORT &&
             !word(data.maps, (size_t)map * 2 + 1)))
            goto done;
        used_maps[map] = 1;
    }
    for (i = 0; i < data.map_count; i++) {
        uint32_t offset = word(data.maps, i * 2), count = word(data.maps, i * 2 + 1), j;
        QJSIntlCompactMap map;
        if (offset != end || count > data.entry_count - end)
            goto done;
        map = map_at(&data, i);
        if (i) {
            QJSIntlCompactMap previous = map_at(&data, i - 1);
            if (compare(previous.entries, (size_t)previous.count * ENTRY_WORDS, map.entries,
                        (size_t)count * ENTRY_WORDS) >= 0)
                goto done;
        }
        for (j = 0; j < count; j++) {
            const uint8_t *entry = map.entries + (size_t)j * ENTRY_WORDS * 4;
            uint32_t reference = word(entry, 3);
            if (word(entry, 1) >= QJS_INTL_COMPACT_COUNT_COUNT ||
                word(entry, 2) >= QJS_INTL_COMPACT_VARIANT_COUNT ||
                (j && compare(entry - ENTRY_WORDS * 4, 3, entry, 3) >= 0))
                goto done;
            if (reference < data.pattern_count)
                used_patterns[reference] = 1;
            else if (reference != OPTIONAL_EMPTY && reference != OPTIONAL_BLOCKED)
                goto done;
        }
        end += count;
    }
    if (end != data.entry_count)
        goto done;
    end = 0;
    for (i = 0; i < data.affix_count; i++) {
        uint32_t offset = word(data.affixes, i * 2), count = word(data.affixes, i * 2 + 1), j;
        uint32_t previous = QJS_INTL_NUMBER_TOKEN_COUNT, number_count = 0;
        if (offset != end || !count || count > data.token_count - end)
            goto done;
        if (i && compare(data.tokens + (size_t)word(data.affixes, (i - 1) * 2) * 8,
                         (size_t)word(data.affixes, (i - 1) * 2 + 1) * 2,
                         data.tokens + (size_t)offset * 8, (size_t)count * 2) >= 0)
            goto done;
        for (j = 0; j < count; j++) {
            const uint8_t *token = data.tokens + ((size_t)offset + j) * 8;
            uint32_t kind = word(token, 0), text_id = word(token, 1);
            QJSIntlBytes text;
            if (kind >= QJS_INTL_NUMBER_TOKEN_COUNT || kind == QJS_INTL_NUMBER_TOKEN_PERCENT ||
                kind == QJS_INTL_NUMBER_TOKEN_PER_MILLE)
                goto done;
            if (kind == QJS_INTL_NUMBER_TOKEN_LITERAL) {
                if (!text_id || previous == kind ||
                    qjs_intl_data_string(&numbering->data, text_id, &text) || !text.length)
                    goto done;
            } else if (text_id) {
                goto done;
            }
            if (kind == QJS_INTL_NUMBER_TOKEN_NUMBER && ++number_count > 1)
                goto done;
            previous = kind;
        }
        numbers[i] = number_count;
        end += count;
    }
    if (end != data.token_count)
        goto done;
    for (i = 0; i < data.pattern_count; i++) {
        const uint8_t *pattern = data.patterns + i * PATTERN_WORDS * 4;
        uint32_t mode = word(pattern, 0), exponent = word(pattern, 1), zeros = word(pattern, 2);
        unsigned int form;
        if (mode > QJS_INTL_COMPACT_FALLBACK || (mode == QJS_INTL_COMPACT_SCALED && !zeros) ||
            (mode == QJS_INTL_COMPACT_LITERAL && zeros) ||
            (mode == QJS_INTL_COMPACT_FALLBACK && (exponent || zeros != 1)))
            goto done;
        for (form = 0; form < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; form++) {
            uint32_t affix = word(pattern, 3 + form);
            if (affix >= data.affix_count || numbers[affix] != (mode != QJS_INTL_COMPACT_LITERAL) ||
                (mode == QJS_INTL_COMPACT_FALLBACK && !fallback_form(&data, affix, form)))
                goto done;
            used_affixes[affix] = 1;
        }
    }
    for (i = 0; i < scratch - data.affix_count; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < data.map_count; i++) {
        QJSIntlCompactMap map = map_at(&data, i);
        uint32_t j;
        for (j = 0; j < map.count; j++) {
            const uint8_t *entry = map.entries + (size_t)j * ENTRY_WORDS * 4;
            uint32_t magnitude = word(entry, 0);
            const uint8_t *baseline = pattern_for(
                &data, find(&map, magnitude, QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_STANDARD));
            const uint8_t *pattern = pattern_for(&data, entry);
            if (!baseline || word(baseline, 0) == QJS_INTL_COMPACT_LITERAL)
                goto done;
            if (pattern) {
                uint32_t mode = word(pattern, 0), exponent = word(pattern, 1),
                         zeros = word(pattern, 2);
                if (exponent > magnitude || (mode == QJS_INTL_COMPACT_SCALED &&
                                             (uint64_t)exponent + zeros != (uint64_t)magnitude + 1))
                    goto done;
                if (mode == QJS_INTL_COMPACT_LITERAL) {
                    const uint8_t *other = pattern_for(
                        &data, find(&map, magnitude, QJS_INTL_COMPACT_OTHER, word(entry, 2)));
                    if (!other || word(other, 0) == QJS_INTL_COMPACT_LITERAL ||
                        exponent != word(other, 1))
                        goto done;
                }
            }
        }
    }
    *out = data;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_compact_map_at(const QJSIntlCompactData *data, uint32_t locale,
                                      uint32_t system, QJSIntlCompactKind kind,
                                      QJSIntlCompactMap *out)
{
    uint32_t lo = 0, hi, row, reference;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        system >= data->numbering.systems.count ||
        (unsigned int)kind >= QJS_INTL_COMPACT_KIND_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->system_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (word(data->systems, mid) < system)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == data->system_count || word(data->systems, lo) != system)
        return QJS_INTL_UNSUPPORTED;
    row = word(data->locale_rows, locale);
    reference = word(data->rows,
                     ((size_t)row * data->system_count + lo) * QJS_INTL_COMPACT_KIND_COUNT + kind);
    *out = map_at(data, reference);
    return QJS_INTL_OK;
}

static void decode_pattern(const QJSIntlCompactData *data, const uint8_t *entry,
                           QJSIntlCompactPattern *out)
{
    uint32_t reference = word(entry, 3);
    const uint8_t *pattern;
    unsigned int form;
    out->magnitude = word(entry, 0);
    out->count = word(entry, 1);
    out->variant = word(entry, 2);
    if (reference >= OPTIONAL_EMPTY) {
        out->state = reference == OPTIONAL_EMPTY ? QJS_INTL_NUMBER_PATTERN_EMPTY
                                                 : QJS_INTL_NUMBER_PATTERN_BLOCKED;
        return;
    }
    pattern = data->patterns + (size_t)reference * PATTERN_WORDS * 4;
    out->mode = word(pattern, 0);
    out->exponent = word(pattern, 1);
    out->zeros = word(pattern, 2);
    for (form = 0; form < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; form++) {
        uint32_t index = word(pattern, 3 + form);
        out->forms[form].tokens = data->tokens + (size_t)word(data->affixes, (size_t)index * 2) * 8;
        out->forms[form].count = word(data->affixes, (size_t)index * 2 + 1);
    }
}

QJSIntlStatus qjs_intl_compact_pattern_at(const QJSIntlCompactData *data,
                                          const QJSIntlCompactMap *map, uint32_t index,
                                          QJSIntlCompactPattern *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !map || !map->entries || index >= map->count)
        return QJS_INTL_INVALID_ARGUMENT;
    decode_pattern(data, map->entries + (size_t)index * ENTRY_WORDS * 4, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compact_pattern_find(const QJSIntlCompactData *data,
                                            const QJSIntlCompactMap *map, uint32_t magnitude,
                                            QJSIntlCompactCount count,
                                            QJSIntlCompactVariant variant,
                                            QJSIntlCompactPattern *out)
{
    const uint8_t *entry;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !map || !map->entries ||
        (unsigned int)count >= QJS_INTL_COMPACT_COUNT_COUNT ||
        (unsigned int)variant >= QJS_INTL_COMPACT_VARIANT_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    entry = find(map, magnitude, count, variant);
    if (!entry)
        return QJS_INTL_UNSUPPORTED;
    decode_pattern(data, entry, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compact_token_at(const QJSIntlCompactData *data,
                                        const QJSIntlNumberAffix *affix, uint32_t index,
                                        QJSIntlNumberPatternToken *out)
{
    const uint8_t *token;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !affix || !affix->tokens || index >= affix->count)
        return QJS_INTL_INVALID_ARGUMENT;
    token = affix->tokens + (size_t)index * 8;
    out->kind = word(token, 0);
    if (out->kind == QJS_INTL_NUMBER_TOKEN_LITERAL)
        qjs_intl_data_string(&data->numbering.data, word(token, 1), &out->text);
    return QJS_INTL_OK;
}
