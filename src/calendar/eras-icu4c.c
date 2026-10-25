/* Japanese era metadata from ICU4C, independent of engine values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "eras.h"
#include "iso.h"
#include "../icu4c/once.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ucal.h>
#include <unicode/uchar.h>
#include <unicode/udat.h>
#include <unicode/unorm2.h>
#include <unicode/ures.h>
#include <unicode/utf16.h>

static QJSICUOnce era_once = QJS_ICU_ONCE_INIT;
static QJSCalendarEra *era_records;
static size_t era_count;
static int era_error;

/* English symbols contain macrons (Taishō/Shōwa); canonical calendar era
   identifiers use unaccented lowercase ASCII. ICU's symbol API also resolves
   locale fallback and sparse era IDs, unlike direct resource-array indexing. */
static char *canonical_era_name(UDateFormat *format, int32_t id, UErrorCode *status)
{
    UChar *name = NULL, *decomposed = NULL;
    const UNormalizer2 *nfd;
    int32_t length, normalized, at;
    char identifier[32], *result = NULL;
    size_t count = 0;

    length = udat_getSymbols(format, UDAT_ERAS, id, NULL, 0, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR)
        *status = U_ZERO_ERROR;
    if (U_FAILURE(*status))
        return NULL;
    if (length <= 0 || length == INT32_MAX ||
        (size_t)length > SIZE_MAX / sizeof(UChar) - 1) {
        *status = U_INVALID_FORMAT_ERROR;
        return NULL;
    }
    name = malloc(((size_t)length + 1) * sizeof(*name));
    if (!name) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    udat_getSymbols(format, UDAT_ERAS, id, name, length + 1, status);
    if (U_FAILURE(*status))
        goto done;
    nfd = unorm2_getNFDInstance(status);
    if (U_FAILURE(*status))
        goto done;
    normalized = unorm2_normalize(nfd, name, length, NULL, 0, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR)
        *status = U_ZERO_ERROR;
    if (U_FAILURE(*status))
        goto done;
    if (normalized <= 0 || normalized == INT32_MAX ||
        (size_t)normalized > SIZE_MAX / sizeof(UChar) - 1) {
        *status = U_INVALID_FORMAT_ERROR;
        goto done;
    }
    decomposed = malloc(((size_t)normalized + 1) * sizeof(*decomposed));
    if (!decomposed) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        goto done;
    }
    unorm2_normalize(nfd, name, length, decomposed, normalized + 1, status);
    if (U_FAILURE(*status))
        goto done;
    for (at = 0; at < normalized;) {
        UChar32 cp;
        int8_t type;

        U16_NEXT(decomposed, at, normalized, cp);
        type = u_charType(cp);
        if (type == U_NON_SPACING_MARK || type == U_COMBINING_SPACING_MARK ||
            type == U_ENCLOSING_MARK)
            continue;
        if (cp >= 'A' && cp <= 'Z')
            cp += 'a' - 'A';
        if (cp < 'a' || cp > 'z' || count == sizeof(identifier) - 1) {
            *status = U_INVALID_FORMAT_ERROR;
            goto done;
        }
        identifier[count++] = (char)cp;
    }
    if (!count) {
        *status = U_INVALID_FORMAT_ERROR;
        goto done;
    }
    identifier[count] = 0;
    result = malloc(count + 1);
    if (!result)
        *status = U_MEMORY_ALLOCATION_ERROR;
    else
        memcpy(result, identifier, count + 1);

done:
    free(name);
    free(decomposed);
    return result;
}

static int compare_era(const void *a, const void *b)
{
    int32_t first = ((const QJSCalendarEra *)a)->first_day;
    int32_t second = ((const QJSCalendarEra *)b)->first_day;

    return (first > second) - (first < second);
}

static void initialize_eras(void)
{
    static const UChar utc[] = {'U', 'T', 'C', 0};
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *bundle = NULL, *calendars = NULL, *japanese = NULL;
    UResourceBundle *eras = NULL, *row = NULL, *start = NULL;
    UCalendar *calendar = NULL;
    UDateFormat *format = NULL;
    QJSCalendarEra *records = NULL;
    int32_t first_modern, size, index;
    int64_t cutoff;
    size_t count = 0, i, j;

    if (qjs_iso_date_to_days(&cutoff, (QJSISODate){1873, 1, 1})) {
        status = U_INTERNAL_PROGRAM_ERROR;
        goto done;
    }
    calendar = ucal_open(utc, -1, "en@calendar=japanese", UCAL_DEFAULT, &status);
    if (U_FAILURE(status))
        goto done;
    ucal_setMillis(calendar, (UDate)cutoff * 86400000.0, &status);
    first_modern = ucal_get(calendar, UCAL_ERA, &status);
    format = udat_open(UDAT_NONE, UDAT_DEFAULT, "en@calendar=japanese",
                        utc, -1, NULL, 0, &status);
    bundle = ures_openDirect(NULL, "supplementalData", &status);
    calendars = ures_getByKey(bundle, "calendarData", NULL, &status);
    japanese = ures_getByKey(calendars, "japanese", NULL, &status);
    eras = ures_getByKey(japanese, "eras", NULL, &status);
    if (U_FAILURE(status))
        goto done;
    size = ures_getSize(eras);
    if (size <= 0 || ures_getType(eras) != URES_TABLE) {
        status = U_INVALID_FORMAT_ERROR;
        goto done;
    }
    records = calloc((size_t)size, sizeof(*records));
    if (!records) {
        status = U_MEMORY_ALLOCATION_ERROR;
        goto done;
    }
    for (index = 0; index < size; index++) {
        const char *key;
        char *end;
        long id;
        const int32_t *fields;
        int32_t length;
        int64_t day, offset;

        row = ures_getByIndex(eras, index, row, &status);
        if (U_FAILURE(status))
            goto done;
        key = ures_getKey(row);
        if (!key) {
            status = U_INVALID_FORMAT_ERROR;
            goto done;
        }
        id = strtol(key, &end, 10);
        if (!*key || *end || id < 0 || id > INT32_MAX) {
            status = U_INVALID_FORMAT_ERROR;
            goto done;
        }
        if (id < first_modern)
            continue;
        start = ures_getByKey(row, "start", start, &status);
        fields = ures_getIntVector(start, &length, &status);
        if (U_FAILURE(status))
            goto done;
        if (length != 3 || qjs_iso_date_to_days(&day,
            (QJSISODate){fields[0], fields[1], fields[2]})) {
            status = U_INVALID_FORMAT_ERROR;
            goto done;
        }
        offset = (int64_t)fields[0] - 1;
        if (day < INT32_MIN || day > INT32_MAX || offset < INT32_MIN || offset > INT32_MAX) {
            status = U_INVALID_FORMAT_ERROR;
            goto done;
        }
        records[count].first_day = (int32_t)(day < cutoff ? cutoff : day);
        records[count].year_offset = (int32_t)offset;
        records[count].name = canonical_era_name(format, (int32_t)id, &status);
        if (U_FAILURE(status))
            goto done;
        count++;
    }
    if (!count) {
        status = U_INVALID_FORMAT_ERROR;
        goto done;
    }
    qsort(records, count, sizeof(*records), compare_era);
    if (records[0].first_day != cutoff) {
        status = U_INVALID_FORMAT_ERROR;
        goto done;
    }
    for (i = 0; i < count; i++) {
        if (i && records[i - 1].first_day >= records[i].first_day) {
            status = U_INVALID_FORMAT_ERROR;
            goto done;
        }
        for (j = 0; j < i; j++) {
            if (!strcmp(records[i].name, records[j].name)) {
                status = U_INVALID_FORMAT_ERROR;
                goto done;
            }
        }
    }

done:
    ures_close(start);
    ures_close(row);
    ures_close(eras);
    ures_close(japanese);
    ures_close(calendars);
    ures_close(bundle);
    udat_close(format);
    ucal_close(calendar);
    if (U_FAILURE(status)) {
        for (i = 0; i < count; i++)
            free((char *)records[i].name);
        free(records);
        era_error = status == U_MEMORY_ALLOCATION_ERROR ? QJS_CAL_MEMORY : QJS_CAL_BACKEND;
    } else {
        era_records = records;
        era_count = count;
    }
}

int qjs_cal_japanese_era_count(size_t *out)
{
    qjs_icu4c_once(&era_once, initialize_eras);
    if (era_error)
        return era_error;
    *out = era_count;
    return 0;
}

int qjs_cal_japanese_era_at(size_t index, QJSCalendarEra *out)
{
    qjs_icu4c_once(&era_once, initialize_eras);
    if (era_error)
        return era_error;
    if (index >= era_count)
        return QJS_CAL_RANGE;
    *out = era_records[index];
    return 0;
}
