/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "layout-data.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 4
#define ABSENT UINT32_C(0xffffffff)
#define BLOCKED UINT32_C(0xfffffffe)
#define VECTOR_WORDS (QJS_INTL_LAYOUT_FIELD_COUNT + 1)
#define ROOT_UNIT_MASK ((UINT32_C(1) << (QJS_INTL_LAYOUT_CURRENCY_UNIT_OTHER + 1)) - \
                        (UINT32_C(1) << QJS_INTL_LAYOUT_CURRENCY_UNIT_EXACT_ZERO))

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

static int ordered(const uint8_t *records, uint32_t count, uint32_t width)
{
    uint32_t i, j;
    for (i = 1; i < count; i++) {
        for (j = 0; j < width; j++) {
            uint32_t a = word(records, (size_t)(i - 1) * width + j);
            uint32_t b = word(records, (size_t)i * width + j);
            if (a != b) {
                if (a > b)
                    return 0;
                break;
            }
        }
        if (j == width)
            return 0;
    }
    return 1;
}

static int required(unsigned int field)
{
    return field <= QJS_INTL_LAYOUT_AT_MOST || (field >= QJS_INTL_LAYOUT_CURRENCY_BEFORE_MATCH &&
                                                field <= QJS_INTL_LAYOUT_CURRENCY_AFTER_INSERT);
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 && (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
                                !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

QJSIntlStatus qjs_intl_number_layout_data_open(const QJSIntlNumberingData *numbering,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlNumberLayoutData *out)
{
    QJSIntlNumberLayoutData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    uint8_t *used;
    uint32_t numeric = 0;
    size_t at = HEADER_WORDS, scratch, i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !numbering || !numbering->data.pack.bytes || !numbering->systems.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.numbering = *numbering;
    if (qjs_intl_pack_section(&numbering->data.pack, QJS_INTL_SECTION_NUMBER_LAYOUTS,
                              &result.section) ||
        !result.section.count || result.section.count != numbering->data.locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 2)
        return status;
    result.system_count = word(result.section.bytes, 1);
    result.vector_count = word(result.section.bytes, 2);
    result.row_count = word(result.section.bytes, 3);
    if (!result.system_count || !result.row_count || result.row_count > result.section.count ||
        !result.vector_count || qjs_intl_data_string(&numbering->data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.system_count, 1, &result.systems) ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, result.system_count, &result.rows) ||
        !take(&result.section, &at, result.vector_count, VECTOR_WORDS,
              &result.vectors) ||
        at != result.section.length / 4 ||
        result.vector_count > (size_t)result.row_count * result.system_count)
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
        !ordered(result.vectors, result.vector_count, VECTOR_WORDS))
        return status;
    /* The two counts fit disjoint word ranges already bounded above. */
    scratch = (size_t)result.row_count + result.vector_count;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * result.system_count; i++) {
        uint32_t index = word(result.rows, i);
        if (index >= result.vector_count)
            goto done;
        used[result.row_count + (size_t)index] = 1;
    }
    for (i = 0; i < scratch; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < result.vector_count; i++) {
        unsigned int field;
        uint32_t mask = word(result.vectors, i * VECTOR_WORDS + QJS_INTL_LAYOUT_FIELD_COUNT);
        if (mask & ~ROOT_UNIT_MASK)
            goto done;
        for (field = 0; field < QJS_INTL_LAYOUT_FIELD_COUNT; field++) {
            uint32_t reference = word(result.vectors, i * VECTOR_WORDS + field);
            QJSIntlBytes text;
            if ((mask & (UINT32_C(1) << field)) && reference == ABSENT)
                goto done;
            if (reference == ABSENT || reference == BLOCKED) {
                if (required(field))
                    goto done;
                continue;
            }
            if (qjs_intl_data_string(&numbering->data, reference, &text) || unresolved(text) ||
                (!text.length && required(field) && field != QJS_INTL_LAYOUT_CURRENCY_BEFORE_INSERT &&
                 field != QJS_INTL_LAYOUT_CURRENCY_AFTER_INSERT))
                goto done;
        }
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_number_layouts_at(const QJSIntlNumberLayoutData *data, uint32_t locale,
                                         uint32_t system, QJSIntlNumberLayouts *out)
{
    uint32_t lo = 0, hi, row, vector;
    unsigned int i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        system >= data->numbering.systems.count)
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
    vector = word(data->rows, (size_t)row * data->system_count + lo);
    for (i = 0; i < QJS_INTL_LAYOUT_FIELD_COUNT; i++) {
        uint32_t value = word(data->vectors, (size_t)vector * VECTOR_WORDS + i);
        out->fields[i].from_root =
            (word(data->vectors, (size_t)vector * VECTOR_WORDS + QJS_INTL_LAYOUT_FIELD_COUNT) >> i) & 1;
        if (value == ABSENT)
            out->fields[i].state = QJS_INTL_LAYOUT_ABSENT;
        else if (value == BLOCKED)
            out->fields[i].state = QJS_INTL_LAYOUT_BLOCKED;
        else
            qjs_intl_data_string(&data->numbering.data, value, &out->fields[i].text);
    }
    return QJS_INTL_OK;
}
