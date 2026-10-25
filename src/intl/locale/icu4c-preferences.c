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
#include "icu4c-private.h"
#include <limits.h>
#include <stdlib.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>

static int preference_order(const void *a, const void *b)
{
    return strcmp(((const ICUPreference *)a)->key, ((const ICUPreference *)b)->key);
}

static int week_order(const void *a, const void *b)
{
    return strcmp(((const ICUWeek *)a)->region, ((const ICUWeek *)b)->region);
}

static int region_valid(const char *region)
{
    size_t length = strlen(region), i;

    if (length != 2 && length != 3)
        return 0;
    for (i = 0; i < length; i++) {
        if (length == 2 ? region[i] < 'A' || region[i] > 'Z' : region[i] < '0' || region[i] > '9')
            return 0;
    }
    return 1;
}

static QJSIntlStatus load_weeks(QJSIntlLocaleService *service, const UResourceBundle *supplemental)
{
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *table = ures_getByKey(supplemental, "weekData", NULL, &status);
    int32_t count = U_SUCCESS(status) ? ures_getSize(table) : 0, index;
    QJSIntlStatus error = QJS_INTL_OK;

    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (count <= 0 || (size_t)count > SIZE_MAX / sizeof(*service->weeks)) {
        error = count <= 0 ? QJS_INTL_DATA_ERROR : QJS_INTL_OVERFLOW;
        goto done;
    }
    service->weeks = service->allocator.malloc(service->allocator.opaque,
                                               (size_t)count * sizeof(*service->weeks));
    if (!service->weeks) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    for (index = 0; index < count; index++) {
        UResourceBundle *entry = ures_getByIndex(table, index, NULL, &status);
        const char *key;
        const int32_t *values;
        int32_t length;
        ICUWeek row = {0};
        int day, step;

        if (U_FAILURE(status)) {
            ures_close(entry);
            error = qjs_icu4c_status(status);
            goto done;
        }
        key = ures_getKey(entry);
        if (!key || !region_valid(key)) {
            ures_close(entry);
            continue; /* Alternative metadata keys are not canonical regions. */
        }
        values = ures_getIntVector(entry, &length, &status);
        if (U_FAILURE(status) || length != 6 || values[0] < 1 || values[0] > 7 || values[1] < 1 ||
            values[1] > 7 || values[2] < 1 || values[2] > 7 || values[4] < 1 || values[4] > 7 ||
            values[3] < 0 || values[3] > 86400000 || values[5] < 0 || values[5] > 86400000) {
            error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
            ures_close(entry);
            goto done;
        }
        memcpy(row.region, key, strlen(key) + 1);
        row.info.first_day = (unsigned int)((values[0] + 5) % 7 + 1);
        row.info.minimum_days = (unsigned int)values[1];
        for (day = values[2], step = 0; step < 7; step++, day = day % 7 + 1) {
            int include =
                (day != values[2] || values[3] < 86400000) && (day != values[4] || values[5] > 0);

            if (include)
                row.info.weekend |= 1U << ((day + 5) % 7);
            if (day == values[4])
                break;
        }
        service->weeks[service->week_count++] = row;
        ures_close(entry);
    }
    qsort(service->weeks, service->week_count, sizeof(*service->weeks), week_order);
done:
    ures_close(table);
    return error;
}

static int hour_code(const UChar *text, int32_t length)
{
    if (length < 1 || length > 2 ||
        (length == 2 && (text[0] != 'h' || (text[1] != 'b' && text[1] != 'B'))))
        return -1;
    switch (text[0]) {
    case 'K':
        return 0;
    case 'h':
        return 1;
    case 'H':
        return 2;
    case 'k':
        return 3;
    default:
        return -1;
    }
}

static QJSIntlStatus hour_values(const QJSIntlAllocator *allocator, const UResourceBundle *row,
                                 ICUPreference *out)
{
    static const char *const names[] = {"h11", "h12", "h23", "h24"};
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *allowed = NULL;
    const UChar *preferred;
    int32_t length, count, i;
    char values[16];
    size_t used = 0;
    unsigned int seen = 0;
    QJSIntlStatus error = QJS_INTL_OK;

    preferred = ures_getStringByKey(row, "preferred", &length, &status);
    int code = U_SUCCESS(status) ? hour_code(preferred, length) : -1;
    if (U_FAILURE(status) || code < 0)
        return U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
    allowed = ures_getByKey(row, "allowed", NULL, &status);
    count = U_SUCCESS(status) ? ures_getSize(allowed) : 0;
    for (i = -1; U_SUCCESS(status) && i < count; i++) {
        if (i >= 0) {
            const UChar *text = ures_getStringByIndex(allowed, i, &length, &status);

            code = U_SUCCESS(status) ? hour_code(text, length) : -1;
        }
        if (U_FAILURE(status))
            break;
        if (code < 0) {
            error = QJS_INTL_DATA_ERROR;
            goto done;
        }
        if (seen & (1U << code))
            continue;
        if (used)
            values[used++] = ' ';
        memcpy(values + used, names[code], 3);
        used += 3;
        seen |= 1U << code;
        out->count++;
    }
    error = qjs_icu4c_status(status);
    if (!error)
        error = qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){values, used}, &out->values);
done:
    ures_close(allowed);
    return error;
}

static QJSIntlStatus calendar_values(const QJSIntlAllocator *allocator, const UResourceBundle *row,
                                     ICUPreference *out)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t count = ures_getSize(row), i, length;
    size_t size = 0, written = 0;
    const UChar *text;

    if (count <= 0)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < count; i++) {
        text = ures_getStringByIndex(row, i, &length, &status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        if (length <= 0 || (size_t)length >= SIZE_MAX - size)
            return length <= 0 ? QJS_INTL_DATA_ERROR : QJS_INTL_OVERFLOW;
        size += (size_t)length + 1;
    }
    out->values = allocator->malloc(allocator->opaque, size);
    if (!out->values)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < count; i++) {
        int32_t j;

        text = ures_getStringByIndex(row, i, &length, &status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        if (written)
            out->values[written++] = ' ';
        for (j = 0; j < length; j++) {
            if (!((text[j] >= 'a' && text[j] <= 'z') || (text[j] >= '0' && text[j] <= '9') ||
                  text[j] == '-'))
                return QJS_INTL_DATA_ERROR;
            out->values[written++] = (char)text[j];
        }
    }
    out->values[written] = 0;
    out->count = (uint32_t)count;
    return QJS_INTL_OK;
}

static QJSIntlStatus load_preferences(QJSIntlLocaleService *service,
                                      const UResourceBundle *supplemental, int hours)
{
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *table =
        ures_getByKey(supplemental, hours ? "timeData" : "calendarPreferenceData", NULL, &status);
    ICUPreference **rows = hours ? &service->hours : &service->calendars;
    size_t *used = hours ? &service->hour_count : &service->calendar_count;
    int32_t count = U_SUCCESS(status) ? ures_getSize(table) : 0, index;
    QJSIntlStatus error = QJS_INTL_OK;

    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (count <= 0 || (size_t)count > SIZE_MAX / sizeof(**rows)) {
        error = count <= 0 ? QJS_INTL_DATA_ERROR : QJS_INTL_OVERFLOW;
        goto done;
    }
    *rows = service->allocator.malloc(service->allocator.opaque, (size_t)count * sizeof(**rows));
    if (!*rows) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    memset(*rows, 0, (size_t)count * sizeof(**rows));
    *used = (size_t)count; /* Every initialized/partial row is cleaned on error. */
    for (index = 0; index < count; index++) {
        UResourceBundle *entry = ures_getByIndex(table, index, NULL, &status);
        const char *key = U_SUCCESS(status) ? ures_getKey(entry) : NULL;
        ICUPreference *row = &(*rows)[index];
        size_t i;

        if (U_FAILURE(status) || !key) {
            error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
            ures_close(entry);
            goto done;
        }
        error =
            qjs_icu4c_copy_bytes(&service->allocator, (QJSIntlBytes){key, strlen(key)}, &row->key);
        if (!error) {
            for (i = 0; row->key[i]; i++) {
                if (row->key[i] == '_')
                    row->key[i] = '-';
            }
            error = hours ? hour_values(&service->allocator, entry, row)
                          : calendar_values(&service->allocator, entry, row);
        }
        ures_close(entry);
        if (error)
            goto done;
    }
    qsort(*rows, *used, sizeof(**rows), preference_order);
done:
    ures_close(table);
    return error;
}

static QJSIntlStatus preference_list(const QJSIntlLocaleService *service, const ICUPreference *rows,
                                     size_t row_count, QJSIntlBytes key, QJSIntlBytes *out,
                                     uint32_t *count)
{
    size_t low = 0, high = row_count;

    if (out)
        memset(out, 0, sizeof(*out));
    if (count)
        *count = 0;
    if (!out || !count || !service || (key.length && !key.data))
        return QJS_INTL_INVALID_ARGUMENT;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = qjs_icu4c_locale_bytes_order(
            (QJSIntlBytes){rows[middle].key, strlen(rows[middle].key)}, key);

        if (!order) {
            *out = (QJSIntlBytes){rows[middle].values, strlen(rows[middle].values)};
            *count = rows[middle].count;
            return QJS_INTL_OK;
        }
        if (order < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_locale_hour_data(const QJSIntlLocaleService *service, QJSIntlBytes key,
                                        QJSIntlBytes *out, uint32_t *count)
{
    return preference_list(service, service ? service->hours : NULL,
                           service ? service->hour_count : 0, key, out, count);
}

QJSIntlStatus qjs_intl_locale_calendar_data(const QJSIntlLocaleService *service, QJSIntlBytes key,
                                            QJSIntlBytes *out, uint32_t *count)
{
    return preference_list(service, service ? service->calendars : NULL,
                           service ? service->calendar_count : 0, key, out, count);
}

QJSIntlStatus qjs_intl_locale_week_data(const QJSIntlLocaleService *service, QJSIntlBytes region,
                                        QJSIntlWeekInfo *out)
{
    size_t low = 0, high;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || (region.length && !region.data))
        return QJS_INTL_INVALID_ARGUMENT;
    high = service->week_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = qjs_icu4c_locale_bytes_order(
            (QJSIntlBytes){service->weeks[middle].region, strlen(service->weeks[middle].region)},
            region);

        if (!order) {
            *out = service->weeks[middle].info;
            return QJS_INTL_OK;
        }
        if (order < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_icu4c_locale_preferences_open(QJSIntlLocaleService *service)
{
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *supplemental = ures_openDirect(NULL, "supplementalData", &status);
    QJSIntlStatus error = qjs_icu4c_status(status);

    if (!error)
        error = load_weeks(service, supplemental);
    if (!error)
        error = load_preferences(service, supplemental, 1);
    if (!error)
        error = load_preferences(service, supplemental, 0);
    ures_close(supplemental);
    return error;
}

void qjs_icu4c_locale_preferences_clear(QJSIntlLocaleService *service)
{
    QJSIntlAllocator allocator = service->allocator;
    size_t i, group;

    for (group = 0; group < 2; group++) {
        ICUPreference *rows = group ? service->calendars : service->hours;
        size_t count = group ? service->calendar_count : service->hour_count;

        for (i = 0; i < count; i++) {
            if (rows[i].key)
                allocator.free(allocator.opaque, rows[i].key);
            if (rows[i].values)
                allocator.free(allocator.opaque, rows[i].values);
        }
        if (rows)
            allocator.free(allocator.opaque, rows);
    }
    if (service->weeks)
        allocator.free(allocator.opaque, service->weeks);
    service->hours = service->calendars = NULL;
    service->hour_count = service->calendar_count = service->week_count = 0;
    service->weeks = NULL;
}
