/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "pattern-data.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 8
#define NUMERIC_WORDS 9
#define PATTERN_WORDS (NUMERIC_WORDS + QJS_INTL_NUMBER_PATTERN_FORM_COUNT)
#define OPTIONAL_EMPTY UINT32_C(0xfffffffd)

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, unsigned int width,
                const uint8_t **out)
{
    size_t length = section->length / 4;
    if (count > (length - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int compare_words(const uint8_t *a, uint32_t a_count, const uint8_t *b, uint32_t b_count)
{
    uint32_t i, count = a_count < b_count ? a_count : b_count;
    for (i = 0; i < count; i++) {
        uint32_t av = word(a, i), bv = word(b, i);
        if (av != bv)
            return av < bv ? -1 : 1;
    }
    return a_count < b_count ? -1 : a_count > b_count;
}

static int ordered(const uint8_t *records, uint32_t count, unsigned int width)
{
    uint32_t i;
    for (i = 1; i < count; i++) {
        const uint8_t *previous = records + (size_t)(i - 1) * width * 4;
        if (compare_words(previous, width, previous + (size_t)width * 4, width) >= 0)
            return 0;
    }
    return 1;
}

static int numeric_valid(const uint8_t *row)
{
    uint32_t primary = word(row, 0), secondary = word(row, 1);
    uint32_t minimum = word(row, 2), places = word(row, 3);
    uint32_t low = word(row, 4), high = word(row, 5), point = word(row, 6);
    uint32_t exponent = word(row, 7), plus = word(row, 8);
    return places && minimum <= places && low <= high && point <= 1 && plus <= 1 &&
           (point || !high) && (exponent || !plus) &&
           ((!primary && !secondary) ||
            (primary && primary < places && secondary && secondary < places && !exponent));
}

QJSIntlStatus qjs_intl_number_pattern_data_open(const QJSIntlNumberingData *numbering,
                                                const QJSIntlAllocator *allocator,
                                                QJSIntlNumberPatternData *out)
{
    QJSIntlNumberPatternData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint8_t *used, *used_rows, *used_vectors, *used_patterns, *used_affixes;
    size_t at = HEADER_WORDS, scratch_size, i, end = 0;
    uint32_t numeric = 0;
    QJSIntlBytes empty;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !numbering || !numbering->data.pack.bytes || !numbering->systems.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.numbering = *numbering;
    if (qjs_intl_pack_section(&numbering->data.pack, QJS_INTL_SECTION_NUMBER_PATTERNS,
                              &result.section) ||
        result.section.count != numbering->data.locales.count || !result.section.count ||
        result.section.length < HEADER_WORDS * 4 || result.section.length % 4 ||
        word(result.section.bytes, 0) != 2)
        return status;
    result.system_count = word(result.section.bytes, 1);
    result.vector_count = word(result.section.bytes, 2);
    result.row_count = word(result.section.bytes, 3);
    result.pattern_count = word(result.section.bytes, 4);
    result.affix_count = word(result.section.bytes, 5);
    result.token_count = word(result.section.bytes, 6);
    result.override_count = word(result.section.bytes, 7);
    if (!result.system_count || !result.vector_count || !result.row_count ||
        !result.pattern_count || !result.affix_count || !result.token_count ||
        result.row_count > result.section.count ||
        !take(&result.section, &at, result.system_count, 1, &result.systems) ||
        !take(&result.section, &at, result.section.count, 2, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, result.system_count, &result.rows) ||
        !take(&result.section, &at, result.vector_count, QJS_INTL_NUMBER_PATTERN_FIELD_COUNT,
              &result.vectors) ||
        !take(&result.section, &at, result.override_count, 2, &result.overrides) ||
        !take(&result.section, &at, result.pattern_count, PATTERN_WORDS, &result.patterns) ||
        !take(&result.section, &at, result.affix_count, 2, &result.affixes) ||
        !take(&result.section, &at, result.token_count, 2, &result.tokens) ||
        at != result.section.length / 4)
        return status;
    for (i = 0; i < numbering->systems.count; i++) {
        QJSIntlNumberingSystem system;
        if (qjs_intl_numbering_system_at(numbering, i, &system))
            return status;
        if (system.kind == QJS_INTL_NUMBERING_NUMERIC) {
            if (numeric == result.system_count || word(result.systems, numeric) != i)
                return status;
            numeric++;
        }
    }
    if (numeric != result.system_count ||
        !ordered(result.rows, result.row_count, result.system_count) ||
        !ordered(result.vectors, result.vector_count, QJS_INTL_NUMBER_PATTERN_FIELD_COUNT) ||
        !ordered(result.patterns, result.pattern_count, PATTERN_WORDS) ||
        qjs_intl_data_string(&numbering->data, 0, &empty) || empty.length)
        return status;
    /* Each count is bounded by a distinct nonempty word range in this pack.
       Their sum therefore fits size_t, including a 32-bit build. */
    scratch_size =
        (size_t)result.row_count + result.vector_count + result.pattern_count + result.affix_count;
    used = allocator->malloc(allocator->opaque, scratch_size);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch_size);
    used_rows = used;
    used_vectors = used_rows + result.row_count;
    used_patterns = used_vectors + result.vector_count;
    used_affixes = used_patterns + result.pattern_count;
    for (i = 0; i < result.section.count; i++) {
        uint32_t row = word(result.locale_rows, i * 2 + 1);
        if (!word(result.locale_rows, i * 2) || row >= result.row_count)
            goto done;
        used_rows[row] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * result.system_count; i++) {
        uint32_t vector = word(result.rows, i);
        if (vector >= result.vector_count)
            goto done;
        used_vectors[vector] = 1;
    }
    for (i = 0; i < (size_t)result.vector_count * QJS_INTL_NUMBER_PATTERN_FIELD_COUNT; i++) {
        uint32_t pattern = word(result.vectors, i);
        if (pattern < result.pattern_count)
            used_patterns[pattern] = 1;
        else if (i % QJS_INTL_NUMBER_PATTERN_FIELD_COUNT <
                     QJS_INTL_PATTERN_CURRENCY_STANDARD_ALPHA ||
                 pattern < OPTIONAL_EMPTY)
            goto done;
    }
    for (i = 0; i < result.override_count; i++) {
        uint32_t raw = word(result.overrides, i * 2);
        uint32_t pattern = word(result.overrides, i * 2 + 1);
        QJSIntlBytes text;
        if (!raw || (i && raw <= word(result.overrides, (i - 1) * 2)) ||
            pattern >= result.pattern_count || qjs_intl_data_string(&numbering->data, raw, &text) ||
            !text.length || (text.length == 9 && !memcmp(text.data, "∅∅∅", 9)))
            goto done;
        used_patterns[pattern] = 1;
    }
    for (i = 0; i < result.pattern_count; i++) {
        const uint8_t *pattern = result.patterns + i * PATTERN_WORDS * 4;
        unsigned int form;
        if (!numeric_valid(pattern))
            goto done;
        for (form = 0; form < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; form++) {
            uint32_t affix = word(pattern, NUMERIC_WORDS + form);
            if (affix >= result.affix_count)
                goto done;
            used_affixes[affix] = 1;
        }
    }
    for (i = 0; i < scratch_size; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < result.affix_count; i++) {
        uint32_t offset = word(result.affixes, i * 2);
        uint32_t count = word(result.affixes, i * 2 + 1);
        uint32_t j, numbers = 0, previous = QJS_INTL_NUMBER_TOKEN_COUNT;
        if (offset != end || !count || count > result.token_count - end)
            goto done;
        if (i && compare_words(result.tokens + (size_t)word(result.affixes, (i - 1) * 2) * 8,
                               word(result.affixes, (i - 1) * 2 + 1) * 2,
                               result.tokens + (size_t)offset * 8, count * 2) >= 0)
            goto done;
        for (j = 0; j < count; j++) {
            const uint8_t *token = result.tokens + ((size_t)offset + j) * 8;
            uint32_t kind = word(token, 0), text_id = word(token, 1);
            if (kind >= QJS_INTL_NUMBER_TOKEN_COUNT)
                goto done;
            if (kind == QJS_INTL_NUMBER_TOKEN_LITERAL) {
                QJSIntlBytes text;
                if (!text_id || previous == QJS_INTL_NUMBER_TOKEN_LITERAL ||
                    qjs_intl_data_string(&numbering->data, text_id, &text) || !text.length)
                    goto done;
            } else if (text_id) {
                goto done;
            }
            numbers += kind == QJS_INTL_NUMBER_TOKEN_NUMBER;
            previous = kind;
        }
        if (numbers != 1)
            goto done;
        end += count;
    }
    if (end != result.token_count)
        goto done;
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

static void read_pattern(const QJSIntlNumberPatternData *data, uint32_t reference,
                         QJSIntlNumberPattern *out)
{
    const uint8_t *pattern = data->patterns + (size_t)reference * PATTERN_WORDS * 4;
    unsigned int i;
    out->primary_grouping = word(pattern, 0);
    out->secondary_grouping = word(pattern, 1);
    out->minimum_integer_digits = word(pattern, 2);
    out->integer_placeholders = word(pattern, 3);
    out->minimum_fraction_digits = word(pattern, 4);
    out->maximum_fraction_digits = word(pattern, 5);
    out->decimal_point = word(pattern, 6);
    out->exponent_digits = word(pattern, 7);
    out->exponent_plus = word(pattern, 8);
    for (i = 0; i < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; i++) {
        uint32_t index = word(pattern, NUMERIC_WORDS + i);
        out->forms[i].tokens = data->tokens + (size_t)word(data->affixes, (size_t)index * 2) * 8;
        out->forms[i].count = word(data->affixes, (size_t)index * 2 + 1);
    }
}

QJSIntlStatus qjs_intl_number_pattern_at(const QJSIntlNumberPatternData *data, uint32_t locale,
                                         uint32_t system, QJSIntlNumberPatternField field,
                                         QJSIntlNumberPattern *out)
{
    uint32_t lo = 0, hi, row, vector, reference;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        system >= data->numbering.systems.count ||
        (unsigned int)field >= QJS_INTL_NUMBER_PATTERN_FIELD_COUNT)
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
    out->minimum_grouping_digits = word(data->locale_rows, (size_t)locale * 2);
    row = word(data->locale_rows, (size_t)locale * 2 + 1);
    vector = word(data->rows, (size_t)row * data->system_count + lo);
    reference = word(data->vectors, (size_t)vector * QJS_INTL_NUMBER_PATTERN_FIELD_COUNT + field);
    if (reference >= OPTIONAL_EMPTY) {
        out->state = reference == OPTIONAL_EMPTY              ? QJS_INTL_NUMBER_PATTERN_EMPTY
                     : reference == QJS_INTL_NUMBERING_ABSENT ? QJS_INTL_NUMBER_PATTERN_ABSENT
                                                              : QJS_INTL_NUMBER_PATTERN_BLOCKED;
        return QJS_INTL_OK;
    }
    read_pattern(data, reference, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_pattern_override(const QJSIntlNumberPatternData *data,
                                               QJSIntlBytes raw, QJSIntlNumberPattern *out)
{
    uint32_t lo = 0, hi;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !raw.data || !raw.length)
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->override_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlBytes candidate;
        int cmp;
        qjs_intl_data_string(&data->numbering.data, word(data->overrides, (size_t)mid * 2),
                             &candidate);
        cmp = memcmp(candidate.data, raw.data,
                     candidate.length < raw.length ? candidate.length : raw.length);
        if (!cmp)
            cmp = candidate.length < raw.length ? -1 : candidate.length > raw.length;
        if (!cmp) {
            read_pattern(data, word(data->overrides, (size_t)mid * 2 + 1), out);
            return QJS_INTL_OK;
        }
        if (cmp < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_number_pattern_token_at(const QJSIntlNumberPatternData *data,
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
