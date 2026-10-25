/* ICU4C named zones; UTC and fixed offsets remain in the common layer.
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
#include "timezone-backend.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

struct QJSTimeZoneRules {
    UChar *identifier;
};

typedef struct ICUZone {
    QJSTimeZone zone;
    QJSTimeZoneRules rules;
} ICUZone;

/* The facade borrows stable handles for the process lifetime. Build this small
   immutable inventory once; UCalendar instances remain local to each operation.
   Do not install ICU allocator hooks or call its process-global u_cleanup(). */
static ICUZone *zones;
static size_t zone_count;
static int inventory_error;

static int icu_error(UErrorCode status)
{
    return status == U_MEMORY_ALLOCATION_ERROR ? QJS_TZ_MEMORY : QJS_TZ_DATA;
}

static int compare_zone(const void *a, const void *b)
{
    return strcmp(((const ICUZone *)a)->zone.identifier,
                  ((const ICUZone *)b)->zone.identifier);
}

static char *copy_ascii(const UChar *text, int32_t length)
{
    char *result;
    int32_t i;

    result = malloc((size_t)length + 1);
    if (!result)
        return NULL;
    for (i = 0; i < length; i++) {
        if (text[i] > 127) {
            free(result);
            return NULL;
        }
        result[i] = (char)text[i];
    }
    result[length] = 0;
    return result;
}

/* ICU retains legacy Java abbreviations and SystemV names which are not
   IANA identifiers. They must not become additional ECMAScript zone names. */
static int icu_compatibility_name(const UChar *name, int32_t length)
{
    static const char *const aliases[] = {
        "ACT", "AET", "AGT", "ART", "AST", "BET", "BST", "CAT", "CNT",
        "CST", "CTT", "EAT", "ECT", "IET", "IST", "JST", "MIT", "NET",
        "NST", "PLT", "PNT", "PRT", "PST", "SST", "VST",
    };
    size_t i;

    if (length >= 8 && !u_memcmp(name,
            (const UChar[]){'S','y','s','t','e','m','V','/'}, 8))
        return 1;
    if (length != 3)
        return 0;
    for (i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (name[0] == aliases[i][0] && name[1] == aliases[i][1] &&
            name[2] == aliases[i][2])
            return 1;
    }
    return 0;
}

static void initialize_inventory(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *names = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY,
                                                         NULL, NULL, &status);
    ICUZone *entries = NULL;
    int32_t capacity, length;
    const UChar *name;
    size_t count = 0, i;

    if (U_FAILURE(status))
        goto fail;
    capacity = uenum_count(names, &status);
    if (U_FAILURE(status) || capacity < 0)
        goto fail;
    entries = calloc((size_t)capacity, sizeof(*entries));
    if (!entries) {
        status = U_MEMORY_ALLOCATION_ERROR;
        goto fail;
    }
    while ((name = uenum_unext(names, &length, &status)) != NULL) {
        ICUZone *entry = entries + count;
        UChar *canonical;
        int32_t canonical_length;

        if (icu_compatibility_name(name, length))
            continue;
        if (count >= (size_t)capacity) {
            status = U_INTERNAL_PROGRAM_ERROR;
            goto fail;
        }
        if ((length == 3 && !u_memcmp(name, (const UChar[]){'U','T','C'}, 3)) ||
            (length == 11 && !u_memcmp(name,
                (const UChar[]){'E','t','c','/','U','n','k','n','o','w','n'}, 11)))
            continue;
        /* ICU's IANA API selects zone.tab representatives, preserving
           territory names while modernizing CLDR spellings such as Calcutta. */
        canonical_length = ucal_getIanaTimeZoneID(name, length, NULL, 0, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR)
            status = U_ZERO_ERROR;
        if (U_FAILURE(status))
            goto fail;
        if (length < 0 || canonical_length < 0 || canonical_length == INT32_MAX ||
            (size_t)length > SIZE_MAX / sizeof(UChar) - 1 ||
            (size_t)canonical_length > SIZE_MAX / sizeof(UChar) - 1) {
            status = U_MEMORY_ALLOCATION_ERROR;
            goto fail;
        }
        canonical = malloc(((size_t)canonical_length + 1) * sizeof(*canonical));
        entry->rules.identifier = malloc(((size_t)length + 1) * sizeof(UChar));
        if (!canonical || !entry->rules.identifier) {
            free(canonical);
            status = U_MEMORY_ALLOCATION_ERROR;
            goto fail;
        }
        ucal_getIanaTimeZoneID(name, length, canonical, canonical_length + 1, &status);
        if (U_FAILURE(status)) {
            free(canonical);
            goto fail;
        }
        u_memcpy(entry->rules.identifier, name, length);
        entry->rules.identifier[length] = 0;
        entry->zone.identifier = copy_ascii(name, length);
        entry->zone.primary = copy_ascii(canonical, canonical_length);
        free(canonical);
        if (!entry->zone.identifier || !entry->zone.primary) {
            status = U_MEMORY_ALLOCATION_ERROR;
            goto fail;
        }
        if (!strcmp(entry->zone.primary, "Etc/UTC") ||
            !strcmp(entry->zone.primary, "Etc/GMT")) {
            free((char *)entry->zone.primary);
            entry->zone.primary = copy_ascii((const UChar[]){'U', 'T', 'C'}, 3);
            if (!entry->zone.primary) {
                status = U_MEMORY_ALLOCATION_ERROR;
                goto fail;
            }
        }
        entry->zone.rules = !strcmp(entry->zone.primary, "UTC") ? NULL : &entry->rules;
        count++;
    }
    if (U_FAILURE(status))
        goto fail;
    uenum_close(names);
    qsort(entries, count, sizeof(*entries), compare_zone);
    /* Sorting moves the embedded rule records: repair their self pointers. */
    for (i = 0; i < count; i++) {
        if (entries[i].zone.rules)
            entries[i].zone.rules = &entries[i].rules;
    }
    zones = entries;
    zone_count = count;
    return;

fail:
    if (entries) {
        for (i = 0; i < (size_t)capacity; i++) {
            free((char *)entries[i].zone.identifier);
            free((char *)entries[i].zone.primary);
            free(entries[i].rules.identifier);
        }
        free(entries);
    }
    uenum_close(names);
    inventory_error = icu_error(status);
}

#ifdef _WIN32
static INIT_ONCE inventory_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK initialize_once(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    initialize_inventory();
    return TRUE;
}
static void ensure_inventory(void)
{
    InitOnceExecuteOnce(&inventory_once, initialize_once, NULL, NULL);
}
#else
static pthread_once_t inventory_once = PTHREAD_ONCE_INIT;
static void ensure_inventory(void)
{
    pthread_once(&inventory_once, initialize_inventory);
}
#endif

static int folded_equal(const char *a, const char *b, size_t length)
{
    size_t i;

    for (i = 0; i < length && a[i]; i++) {
        unsigned char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
    }
    return i == length && !a[i];
}

int qjs_timezone_backend_lookup(const QJSTimeZone **out, const char *id, size_t length)
{
    size_t i;

    ensure_inventory();
    if (inventory_error)
        return inventory_error;
    for (i = 0; i < zone_count; i++) {
        if (folded_equal(zones[i].zone.identifier, id, length)) {
            *out = &zones[i].zone;
            return 0;
        }
    }
    return QJS_TZ_RANGE;
}

const char *qjs_timezone_backend_data_version(void)
{
    UErrorCode status = U_ZERO_ERROR;
    const char *version = ucal_getTZDataVersion(&status);

    return U_SUCCESS(status) ? version : "unknown";
}

size_t qjs_timezone_backend_count(void)
{
    ensure_inventory();
    return zone_count;
}

const QJSTimeZone *qjs_timezone_backend_at(size_t index)
{
    ensure_inventory();
    return index < zone_count ? &zones[index].zone : NULL;
}

static UCalendar *open_calendar(const QJSTimeZoneRules *rules, UErrorCode *status)
{
    return ucal_open(rules->identifier, -1, "en_US_POSIX", UCAL_GREGORIAN, status);
}

/* Stay far enough inside double's exact-second range for adjacent boundary
   probes. This includes the complete Temporal range and its edge-date margin. */
static int valid_second(int64_t seconds)
{
    return seconds >= -9007199254740LL && seconds <= 9007199254740LL;
}

static int calendar_info(UCalendar *cal, int64_t seconds, QJSTimeZoneInfo *out)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t raw, dst;
    int64_t total;

    if (!valid_second(seconds))
        return QJS_TZ_RANGE;
    ucal_setMillis(cal, (UDate)seconds * 1000, &status);
    raw = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    dst = ucal_get(cal, UCAL_DST_OFFSET, &status);
    total = (int64_t)raw + dst;
    if (U_FAILURE(status))
        return icu_error(status);
    if (total % 1000 || total <= -86400000 || total >= 86400000)
        return QJS_TZ_DATA;
    *out = (QJSTimeZoneInfo){(int32_t)(total / 1000), dst != 0};
    return 0;
}

int qjs_timezone_backend_info(const QJSTimeZoneRules *rules, int64_t seconds,
                              QJSTimeZoneInfo *out)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open_calendar(rules, &status);
    int error;

    if (U_FAILURE(status)) {
        ucal_close(cal);
        return icu_error(status);
    }
    error = calendar_info(cal, seconds, out);
    ucal_close(cal);
    return error;
}

int qjs_timezone_backend_offset(const QJSTimeZoneRules *rules, int64_t seconds,
                                int32_t *out)
{
    QJSTimeZoneInfo info;
    int error = qjs_timezone_backend_info(rules, seconds, &info);

    if (!error) *out = info.offset_seconds;
    return error;
}

/* Includes DST-designation-only boundaries; public transition queries filter
   those, while constant-interval queries must retain them. */
static int transition(UCalendar *cal, int64_t seconds, int next, int inclusive,
                       int64_t *out, int *found)
{
    UErrorCode status = U_ZERO_ERROR;
    UDate value;
    UTimeZoneTransitionType type = next ?
        (inclusive ? UCAL_TZ_TRANSITION_NEXT_INCLUSIVE : UCAL_TZ_TRANSITION_NEXT) :
        (inclusive ? UCAL_TZ_TRANSITION_PREVIOUS_INCLUSIVE : UCAL_TZ_TRANSITION_PREVIOUS);
    UBool exists;

    if (!valid_second(seconds)) return QJS_TZ_RANGE;
    ucal_setMillis(cal, (UDate)seconds * 1000, &status);
    exists = ucal_getTimeZoneTransitionDate(cal, type, &value, &status);
    if (U_FAILURE(status)) return icu_error(status);
    if (exists) {
        double second = value / 1000;
        if (!isfinite(second) || second < -9007199254740.0 ||
            second > 9007199254740.0 || floor(second) != second)
            return QJS_TZ_RANGE;
        *out = (int64_t)second;
    }
    *found = exists;
    return 0;
}

int qjs_timezone_backend_transition(const QJSTimeZoneRules *rules, int64_t seconds,
                                    int next, int inclusive, int64_t *out, int *found)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open_calendar(rules, &status);
    QJSTimeZoneInfo before, after;
    int64_t current = seconds, boundary;
    int exists, error;

    if (U_FAILURE(status)) { ucal_close(cal); return icu_error(status); }
    for (;;) {
        error = transition(cal, current, next, inclusive, &boundary, &exists);
        if (error || !exists) break;
        error = calendar_info(cal, boundary - 1, &before);
        if (!error) error = calendar_info(cal, boundary, &after);
        if (error) break;
        if (before.offset_seconds != after.offset_seconds) {
            *out = boundary;
            break;
        }
        current = boundary;
        inclusive = 0;
    }
    ucal_close(cal);
    if (!error) *found = exists;
    return error;
}

int qjs_timezone_backend_info_is_constant(const QJSTimeZoneRules *rules, int64_t first,
                                          int64_t last, int *out)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open_calendar(rules, &status);
    QJSTimeZoneInfo initial, value;
    int64_t current = first, boundary;
    int found, error, constant = 1;

    if (U_FAILURE(status)) { ucal_close(cal); return icu_error(status); }
    error = calendar_info(cal, first, &initial);
    if (!error && !valid_second(last)) error = QJS_TZ_RANGE;
    while (!error) {
        error = transition(cal, current, 1, 0, &boundary, &found);
        if (error || !found || boundary > last) break;
        error = calendar_info(cal, boundary, &value);
        if (error) break;
        if (value.offset_seconds != initial.offset_seconds || value.is_dst != initial.is_dst) {
            constant = 0;
            break;
        }
        current = boundary;
    }
    ucal_close(cal);
    if (!error) *out = constant;
    return error;
}

int qjs_timezone_backend_candidates(const QJSTimeZoneRules *rules, int64_t wall,
                                    QJSTimeZoneVisit *visit, void *opaque,
                                    size_t *out_count, int32_t *out_before,
                                    int32_t *out_after, int64_t *out_end)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal;
    QJSTimeZoneInfo before, after;
    int64_t start, boundary = 0, end, candidate, gap_end = 0;
    int64_t *values = NULL;
    size_t count = 0, capacity = 0, i;
    int32_t gap_before = 0, gap_after = 0;
    int found, error = 0;

    if (wall < -9007199168340LL || wall > 9007199168340LL)
        return QJS_TZ_RANGE;
    cal = open_calendar(rules, &status);
    if (U_FAILURE(status)) { ucal_close(cal); return icu_error(status); }
    start = wall - 86400;
    end = wall + 86400;
    error = calendar_info(cal, start, &before);
    while (!error) {
        error = transition(cal, start, 1, 0, &boundary, &found);
        if (error) break;
        if (!found || boundary > end) boundary = end + 1;
        candidate = wall - before.offset_seconds;
        if (candidate >= start && candidate < boundary) {
            if (count == capacity) {
                size_t next_capacity = capacity ? capacity * 2 : 2;
                int64_t *next_values;
                if (next_capacity < capacity || next_capacity > SIZE_MAX / sizeof(*values)) {
                    error = QJS_TZ_MEMORY;
                    break;
                }
                next_values = realloc(values, next_capacity * sizeof(*values));
                if (!next_values) { error = QJS_TZ_MEMORY; break; }
                values = next_values;
                capacity = next_capacity;
            }
            values[count++] = candidate;
        }
        if (boundary > end) break;
        error = calendar_info(cal, boundary, &after);
        if (error) break;
        if (after.offset_seconds > before.offset_seconds &&
            wall >= boundary + before.offset_seconds &&
            wall < boundary + after.offset_seconds) {
            gap_before = before.offset_seconds;
            gap_after = after.offset_seconds;
            gap_end = boundary;
        }
        before = after;
        start = boundary;
    }
    ucal_close(cal);
    /* No callback or caller-visible output is published before all ICU queries
       and candidate allocations have succeeded. */
    if (!error) {
        for (i = 0; i < count; i++) visit(opaque, values[i]);
        *out_count = count;
        *out_before = gap_before;
        *out_after = gap_after;
        *out_end = gap_end;
    }
    free(values);
    return error;
}
