/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "combination-data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 5
#define SELECTOR_WIDTH (QJS_INTL_DATE_STYLE_WIDTH_COUNT * QJS_INTL_DATE_COMBINATION_FORM_COUNT)
#define RECORD_WIDTH 8

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

static int ordered(const uint8_t *records, uint32_t count, uint32_t width)
{
    size_t i, j;
    for (i = 1; i < count; i++) {
        for (j = 0; j < width; j++) {
            uint32_t previous = word(records, (i - 1) * width + j);
            uint32_t current = word(records, i * width + j);
            if (previous < current)
                break;
            if (previous > current)
                return 0;
        }
        if (j == width)
            return 0;
    }
    return 1;
}

static int alternate_valid(QJSIntlBytes text)
{
    size_t i;
    int separator = 1;
    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            separator = 0;
        } else if ((c == '-' || c == '_' || c == ' ') && !separator) {
            separator = 1;
        } else {
            return 0;
        }
    }
    return !separator;
}

static int calendar_valid(QJSIntlBytes text)
{
    size_t i;
    int separator = 0;
    if (!text.length || text.data[0] < 'a' || text.data[0] > 'z')
        return 0;
    for (i = 1; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            separator = 0;
        } else if (c == '-' && !separator) {
            separator = 1;
        } else {
            return 0;
        }
    }
    return !separator;
}

static int unresolved(QJSIntlBytes text)
{
    return text.length == 9 && (!memcmp(text.data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91", 9) ||
                                !memcmp(text.data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", 9));
}

QJSIntlStatus qjs_intl_date_combination_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                                  QJSIntlDateCombinationData *out)
{
    QJSIntlDateCombinationData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, i, scratch;
    uint8_t *used, *used_records, *sources;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_DATE_COMBINATIONS, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.calendar_count = word(result.section.bytes, 1);
    result.alternate_count = word(result.section.bytes, 2);
    result.row_count = word(result.section.bytes, 3);
    result.record_count = word(result.section.bytes, 4);
    if (!result.calendar_count || result.calendar_count > UINT32_MAX / SELECTOR_WIDTH ||
        !result.alternate_count ||
        result.alternate_count > UINT32_MAX / (SELECTOR_WIDTH * result.calendar_count) ||
        !result.row_count || result.row_count > result.section.count || !result.record_count)
        return status;
    result.row_width = SELECTOR_WIDTH * result.calendar_count * result.alternate_count;
    if (result.record_count > (uint64_t)result.row_count * result.row_width ||
        qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.calendar_count, 1, &result.calendars) ||
        !take(&result.section, &at, result.alternate_count, 1, &result.alternates) ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, result.row_width, &result.rows) ||
        !take(&result.section, &at, result.record_count, RECORD_WIDTH, &result.records) ||
        at != result.section.length / 4 || word(result.alternates, 0) ||
        !ordered(result.calendars, result.calendar_count, 1) ||
        !ordered(result.alternates, result.alternate_count, 1) ||
        !ordered(result.rows, result.row_count, result.row_width) ||
        !ordered(result.records, result.record_count, RECORD_WIDTH))
        return status;
    for (i = 0; i < result.calendar_count; i++) {
        QJSIntlBytes calendar;
        if (qjs_intl_data_string(data, word(result.calendars, i), &calendar) ||
            !calendar_valid(calendar))
            return status;
    }
    for (i = 1; i < result.alternate_count; i++) {
        QJSIntlBytes alternate;
        if (qjs_intl_data_string(data, word(result.alternates, i), &alternate) ||
            !alternate_valid(alternate))
            return status;
    }
    /* Validated counts occupy disjoint spans in the bounded shared pack. */
    scratch = (size_t)result.row_count + result.record_count + data->strings.count;
    used = allocator->malloc(allocator->opaque, scratch);
    if (!used)
        return QJS_INTL_NO_MEMORY;
    memset(used, 0, scratch);
    used_records = used + result.row_count;
    sources = used_records + result.record_count;
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count * result.row_width; i++) {
        uint32_t index = word(result.rows, i);
        if (index >= result.record_count)
            goto done;
        used_records[index] = 1;
    }
    for (i = 0; i < (size_t)result.row_count + result.record_count; i++) {
        if (!used[i])
            goto done;
    }
    for (i = 0; i < result.record_count; i++) {
        const uint8_t *record = result.records + i * RECORD_WIDTH * 4;
        uint32_t state = word(record, 0), text = word(record, 1);
        uint32_t numbers = word(record, 2), origin = word(record, 3);
        uint32_t calendar = word(record, 4), width = word(record, 5);
        uint32_t form = word(record, 6), alt = word(record, 7);
        QJSIntlBytes value;
        if (state > QJS_INTL_DATE_COMBINATION_BLOCKED || text >= data->strings.count ||
            numbers >= data->strings.count || origin >= data->strings.count ||
            calendar >= result.calendar_count || width >= QJS_INTL_DATE_STYLE_WIDTH_COUNT ||
            form >= QJS_INTL_DATE_COMBINATION_FORM_COUNT ||
            alt >= result.alternate_count)
            goto done;
        if (state == QJS_INTL_DATE_COMBINATION_MISSING) {
            if (text || numbers || origin || calendar || width || form || alt)
                goto done;
            continue;
        }
        if (state == QJS_INTL_DATE_COMBINATION_BLOCKED && (text || numbers))
            goto done;
        if (qjs_intl_data_string(data, text, &value) || unresolved(value))
            goto done;
        if (!sources[origin]) {
            QJSIntlStatus check;
            if (qjs_intl_data_string(data, origin, &value))
                goto done;
            check = qjs_intl_source_locale_validate(allocator, value);
            if (check) {
                if (check == QJS_INTL_NO_MEMORY || check == QJS_INTL_OVERFLOW)
                    status = check;
                goto done;
            }
            sources[origin] = 1;
        }
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, used);
    return status;
}

QJSIntlStatus qjs_intl_date_combination_calendar_at(const QJSIntlDateCombinationData *data, uint32_t index,
                                                    QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->calendar_count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_data_string(&data->data, word(data->calendars, index), out);
}

QJSIntlStatus qjs_intl_date_combination_alternate_at(const QJSIntlDateCombinationData *data, uint32_t index,
                                                     QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || index >= data->alternate_count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_data_string(&data->data, word(data->alternates, index), out);
}

QJSIntlStatus qjs_intl_date_combination_record_at(const QJSIntlDateCombinationData *data, uint32_t locale,
                                                  uint32_t calendar, QJSIntlDateStyleWidth width,
                                                  QJSIntlDateCombinationForm form, uint32_t alternate,
                                                  QJSIntlDateCombinationRecord *out)
{
    uint32_t row, index, selector;
    const uint8_t *record;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        calendar >= data->calendar_count ||
        (unsigned int)width >= QJS_INTL_DATE_STYLE_WIDTH_COUNT ||
        (unsigned int)form >= QJS_INTL_DATE_COMBINATION_FORM_COUNT || alternate >= data->alternate_count)
        return QJS_INTL_INVALID_ARGUMENT;
    selector = (((calendar * QJS_INTL_DATE_STYLE_WIDTH_COUNT + (unsigned int)width) *
                 QJS_INTL_DATE_COMBINATION_FORM_COUNT + (unsigned int)form) *
                data->alternate_count + alternate);
    row = word(data->locale_rows, locale);
    index = word(data->rows, (size_t)row * data->row_width + selector);
    record = data->records + (size_t)index * RECORD_WIDTH * 4;
    out->state = word(record, 0);
    if (out->state != QJS_INTL_DATE_COMBINATION_MISSING) {
        qjs_intl_data_string(&data->data, word(record, 1), &out->text);
        qjs_intl_data_string(&data->data, word(record, 2), &out->numbers);
        qjs_intl_data_string(&data->data, word(record, 3), &out->origin_locale);
        qjs_intl_date_combination_calendar_at(data, word(record, 4), &out->origin_calendar);
        out->origin_width = word(record, 5);
        out->origin_form = word(record, 6);
        qjs_intl_date_combination_alternate_at(data, word(record, 7), &out->origin_alternate);
    }
    return QJS_INTL_OK;
}
