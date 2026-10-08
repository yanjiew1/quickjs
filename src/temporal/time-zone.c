/* Native Temporal time-zone conversion using the shared ICU backend.
 * Disabled Temporal/Intl builds omit this source and need no ICU.
 * Epoch arithmetic remains exact integer words.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "time-zone.h"
#include "parse.h"
#include "format.h"
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#ifdef CONFIG_ICU
#include "../intl/icu-config.h"
#include <unicode/ucal.h>
#include "../intl/locale-data.h"
#endif
#define NS_DAY INT64_C(86400000000000)
static int ascii_equal(const char *a, size_t length, const char *b)
{
    size_t i;
    if (length != strlen(b)) return 0;
    for (i = 0; i < length; i++) {
        unsigned char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
    }
    return 1;
}
#ifdef CONFIG_ICU
static int any_milliseconds(int64_t *milliseconds, QJSTemporalEpochNs value)
{
    QJSTemporalEpochNs quotient;
    uint64_t remainder;
    if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, 1000000) ||
        qjs_temporal_epoch_ns_to_int64(milliseconds, quotient))
        return QJS_TEMPORAL_ERROR_RANGE;
    return 0;
}
static int icu_error(UErrorCode status)
{
    return U_SUCCESS(status) ? 0 : status == U_MEMORY_ALLOCATION_ERROR ?
        QJS_TEMPORAL_ERROR_MEMORY : QJS_TEMPORAL_ERROR_BACKEND;
}
static UCalendar *zone_open(const QJSTemporalZone *zone, UErrorCode *status)
{
    UChar identifier[256], primary[256];
    size_t i, length = strlen(zone->identifier);
    UCalendar *calendar;
    if (length >= sizeof(identifier) / sizeof(*identifier) ||
        !intl_iana_zone_name(zone->identifier, length)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR; return NULL;
    }
    /* A whitelist/data mismatch must not use ICU's unknown-zone GMT
       fallback. The pinned IANA resolver confirms backend availability. */
    intl_iana_zone_primary(zone->identifier, primary, 256, status);
    if (U_FAILURE(*status)) return NULL;
    for (i = 0; i <= length; i++) identifier[i] = (unsigned char)zone->identifier[i];
    calendar = ucal_open(identifier, (int32_t)length,
                         "en_US@calendar=gregorian", UCAL_GREGORIAN, status);
    if (calendar) ucal_setGregorianChange(calendar, -1.0e16, status);
    return calendar;
}
static int local_offsets(const QJSTemporalZone *zone,
                         QJSTemporalEpochNs wall, int64_t offsets[2])
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *calendar = zone_open(zone, &status);
    int64_t milliseconds;
    int32_t raw, dst;
    int i, error;
    if (!calendar) return U_SUCCESS(status) ? QJS_TEMPORAL_ERROR_MEMORY : icu_error(status);
    error = any_milliseconds(&milliseconds, wall);
    if (!error) {
        ucal_setMillis(calendar, (UDate)milliseconds, &status);
        for (i = 0; i < 2 && U_SUCCESS(status); i++) {
            UTimeZoneLocalOption option = i ? UCAL_TZ_LOCAL_LATTER : UCAL_TZ_LOCAL_FORMER;
            ucal_getTimeZoneOffsetFromLocal(calendar, option, option, &raw, &dst, &status);
            if (U_SUCCESS(status)) offsets[i] = ((int64_t)raw + dst) * 1000000;
        }
        error = icu_error(status);
    }
    ucal_close(calendar);
    return error;
}
#endif
int qjs_temporal_zone_parse(QJSTemporalZone *result,
                           const char *identifier, size_t length)
{
    QJSTemporalZone zone = {0};
    QJSTemporalUTCOffset offset;
    size_t consumed;
    if (!length || length >= sizeof(zone.identifier) || memchr(identifier, 0, length))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (identifier[0] == '+' || identifier[0] == '-') {
        if (qjs_temporal_parse_utc_offset_prefix(&offset, &consumed,
                identifier, length, 0) || consumed != length)
            return QJS_TEMPORAL_ERROR_RANGE;
        zone.is_offset = 1;
        zone.offset_nanoseconds = offset.nanoseconds;
        if (qjs_temporal_format_utc_offset(zone.identifier, sizeof(zone.identifier),
                                         offset.nanoseconds, 0) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
        *result = zone;
        return 0;
    }
    if (ascii_equal(identifier, length, "UTC")) {
        memcpy(zone.identifier, "UTC", 4);
        *result = zone;
        return 0;
    }
#ifdef CONFIG_ICU
    {
        const char *id = intl_iana_zone_name(identifier, length);
        size_t size;
        if (!id) return QJS_TEMPORAL_ERROR_RANGE;
        size = strlen(id);
        if (size >= sizeof(zone.identifier)) return QJS_TEMPORAL_ERROR_BACKEND;
        memcpy(zone.identifier, id, size + 1);
        *result = zone;
        return 0;
    }
#else
    return QJS_TEMPORAL_ERROR_RANGE;
#endif
}
static int zone_offset_any(const QJSTemporalZone *zone,
                           QJSTemporalEpochNs epoch, int64_t *result)
{
    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        *result = zone->is_offset ? zone->offset_nanoseconds : 0;
        return 0;
    }
#ifdef CONFIG_ICU
    {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *calendar = zone_open(zone, &status);
        int64_t milliseconds, offset;
        int error;
        int32_t raw, dst;
        if (!calendar) return U_SUCCESS(status) ? QJS_TEMPORAL_ERROR_MEMORY : icu_error(status);
        error = any_milliseconds(&milliseconds, epoch);
        if (!error) {
            ucal_setMillis(calendar, (UDate)milliseconds, &status);
            raw = ucal_get(calendar, UCAL_ZONE_OFFSET, &status);
            dst = ucal_get(calendar, UCAL_DST_OFFSET, &status);
            offset = ((int64_t)raw + dst) * 1000000;
            error = icu_error(status);
            if (!error && (offset <= -NS_DAY || offset >= NS_DAY))
                error = QJS_TEMPORAL_ERROR_BACKEND;
            if (!error) *result = offset;
        }
        ucal_close(calendar);
        return error;
    }
#else
    return QJS_TEMPORAL_ERROR_UNSUPPORTED;
#endif
}
int qjs_temporal_zone_offset(const QJSTemporalZone *zone,
                            QJSTemporalEpochNs epoch, int64_t *result)
{
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) return QJS_TEMPORAL_ERROR_RANGE;
    return zone_offset_any(zone, epoch, result);
}
int qjs_temporal_zone_datetime(const QJSTemporalZone *zone,
                              QJSTemporalEpochNs epoch,
                              QJSTemporalISODateTime *result)
{
    int64_t offset;
    QJSTemporalEpochNs local;
    int error = qjs_temporal_zone_offset(zone, epoch, &offset);
    if (error) return error;
    if (qjs_temporal_epoch_ns_add(&local, epoch,
                qjs_temporal_epoch_ns_from_int64(offset)) ||
        qjs_temporal_iso_datetime_from_epoch_ns(result, local))
        return QJS_TEMPORAL_ERROR_RANGE;
    return 0;
}
int qjs_temporal_zone_possible_epochs(const QJSTemporalZone *zone,
                         QJSTemporalISODateTime datetime,
                         QJSTemporalEpochNs result[2], int *count)
{
    QJSTemporalEpochNs wall, candidate, choices[2];
    int64_t offsets[2], actual;
    int i, n = 0, error;
    if (qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        offsets[0] = zone->is_offset ? zone->offset_nanoseconds : 0;
        offsets[1] = offsets[0];
    } else {
#ifdef CONFIG_ICU
        error = local_offsets(zone, wall, offsets);
        if (error) return error;
#else
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
#endif
    }
    for (i = 0; i < 2; i++) {
        if (i && offsets[0] == offsets[1]) break;
        if (qjs_temporal_epoch_ns_subtract(&candidate, wall,
                    qjs_temporal_epoch_ns_from_int64(offsets[i])))
            return QJS_TEMPORAL_ERROR_RANGE;
        error = zone_offset_any(zone, candidate, &actual);
        if (error) return error;
        if (actual == offsets[i]) {
            if (!qjs_temporal_epoch_ns_is_valid(candidate))
                return QJS_TEMPORAL_ERROR_RANGE;
            choices[n++] = candidate;
        }
    }
    if (n == 2 && qjs_temporal_epoch_ns_compare(choices[0], choices[1]) > 0) {
        candidate = choices[0]; choices[0] = choices[1]; choices[1] = candidate;
    }
    for (i = 0; i < n; i++) result[i] = choices[i];
    *count = n;
    return 0;
}
int qjs_temporal_zone_epoch(const QJSTemporalZone *zone,
                           QJSTemporalISODateTime datetime,
                           QJSTemporalDisambiguation disambiguation,
                           QJSTemporalEpochNs *result)
{
    QJSTemporalEpochNs choices[2], wall, epoch;
    int n, error;
    int64_t offsets[2], offset;
    if (disambiguation < QJS_TEMPORAL_COMPATIBLE ||
        disambiguation > QJS_TEMPORAL_DISAMBIGUATION_REJECT)
        return QJS_TEMPORAL_ERROR_RANGE;
    error = qjs_temporal_zone_possible_epochs(zone, datetime, choices, &n);
    if (error) return error;
    if (n) {
        if (n > 1 && disambiguation == QJS_TEMPORAL_DISAMBIGUATION_REJECT)
            return QJS_TEMPORAL_ERROR_RANGE;
        *result = choices[disambiguation == QJS_TEMPORAL_LATER ? n - 1 : 0];
        return 0;
    }
    if (disambiguation == QJS_TEMPORAL_DISAMBIGUATION_REJECT)
        return QJS_TEMPORAL_ERROR_RANGE;
#ifdef CONFIG_ICU
    if (qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return QJS_TEMPORAL_ERROR_RANGE;
    error = local_offsets(zone, wall, offsets);
    if (error) return error;
    /* Gap: former and latter offsets bracket the skipped wall interval.
       Earlier moves by the gap to the former side; compatible uses later. */
    offset = disambiguation == QJS_TEMPORAL_EARLIER ? offsets[1] : offsets[0];
    if (qjs_temporal_epoch_ns_subtract(&epoch, wall,
                    qjs_temporal_epoch_ns_from_int64(offset)) ||
        !qjs_temporal_epoch_ns_is_valid(epoch))
        return QJS_TEMPORAL_ERROR_RANGE;
    *result = epoch;
    return 0;
#else
    (void)wall; (void)epoch; (void)offsets; (void)offset;
    return QJS_TEMPORAL_ERROR_RANGE;
#endif
}
int qjs_temporal_zone_start_of_day(const QJSTemporalZone *zone,
                                 QJSTemporalISODate date,
                                 QJSTemporalEpochNs *result)
{
    QJSTemporalISODateTime midnight = { date, {0} };
    QJSTemporalEpochNs choices[2], earlier, transition;
    int n, found, error;
    error = qjs_temporal_zone_possible_epochs(zone, midnight, choices, &n);
    if (error) return error;
    if (n) { *result = choices[0]; return 0; }
    error = qjs_temporal_zone_epoch(zone, midnight, QJS_TEMPORAL_EARLIER, &earlier);
    if (error) return error;
    error = qjs_temporal_zone_transition(zone, earlier, 1, &transition, &found);
    if (error) return error;
    if (!found) return QJS_TEMPORAL_ERROR_BACKEND;
    *result = transition;
    return 0;
}
int qjs_temporal_zone_transition(const QJSTemporalZone *zone,
                                QJSTemporalEpochNs epoch, int next,
                                QJSTemporalEpochNs *result, int *found)
{
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        *found = 0; return 0;
    }
#ifdef CONFIG_ICU
    {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *calendar = zone_open(zone, &status);
        QJSTemporalEpochNs quotient, transition;
        uint64_t remainder;
        int64_t milliseconds;
        UTimeZoneTransitionType type;
        UDate date;
        UBool exists = 0;
        int error, attempts = 0;
        int32_t raw, dst;
        int64_t before, after;
        if (!calendar) return U_SUCCESS(status) ? QJS_TEMPORAL_ERROR_MEMORY : icu_error(status);
        error = any_milliseconds(&milliseconds, epoch);
        if (!error) {
            qjs_temporal_epoch_ns_divide(&quotient, &remainder, epoch, 1000000);
            type = next ? UCAL_TZ_TRANSITION_NEXT : remainder ?
                UCAL_TZ_TRANSITION_PREVIOUS_INCLUSIVE : UCAL_TZ_TRANSITION_PREVIOUS;
            ucal_setMillis(calendar, (UDate)milliseconds, &status);
            for (;;) {
                exists = ucal_getTimeZoneTransitionDate(calendar, type, &date, &status);
                error = icu_error(status);
                if (error || !exists) break;
                if (qjs_temporal_epoch_ns_from_milliseconds(&transition, date)) {
                    exists = 0; break;
                }
                /* Temporal transitions change the *total* UTC offset. ICU
                   also reports raw/DST repartition or abbreviation changes. */
                ucal_setMillis(calendar, date - 1, &status);
                raw = ucal_get(calendar, UCAL_ZONE_OFFSET, &status);
                dst = ucal_get(calendar, UCAL_DST_OFFSET, &status);
                before = (int64_t)raw + dst;
                ucal_setMillis(calendar, date, &status);
                raw = ucal_get(calendar, UCAL_ZONE_OFFSET, &status);
                dst = ucal_get(calendar, UCAL_DST_OFFSET, &status);
                after = (int64_t)raw + dst;
                error = icu_error(status);
                if (error) break;
                if (before != after) { *result = transition; break; }
                if (++attempts > 4096) { error = QJS_TEMPORAL_ERROR_BACKEND; break; }
                /* The next query is strict even when the original previous
                   query was inclusive for a submillisecond starting point. */
                type = next ? UCAL_TZ_TRANSITION_NEXT : UCAL_TZ_TRANSITION_PREVIOUS;
            }
        }
        ucal_close(calendar);
        if (!error) *found = exists;
        return error;
    }
#else
    (void)next; (void)result;
    return QJS_TEMPORAL_ERROR_UNSUPPORTED;
#endif
}
int qjs_temporal_zones_equal(const QJSTemporalZone *a,
                            const QJSTemporalZone *b, int *result)
{
    if (a->is_offset || b->is_offset) {
        *result = a->is_offset && b->is_offset &&
            a->offset_nanoseconds == b->offset_nanoseconds;
        return 0;
    }
    if (!strcmp(a->identifier, b->identifier)) { *result = 1; return 0; }
#ifdef CONFIG_ICU
    {
        UChar first[256], second[256];
        int32_t sizes[2];
        UErrorCode status = U_ZERO_ERROR;
        sizes[0] = intl_iana_zone_primary(a->identifier, first, 256, &status);
        sizes[1] = intl_iana_zone_primary(b->identifier, second, 256, &status);
        if (icu_error(status)) return icu_error(status);
        *result = sizes[0] == sizes[1] &&
            !memcmp(first, second, (size_t)sizes[0] * sizeof(UChar));
        return 0;
    }
#else
    *result = 0;
    return 0;
#endif
}
/* ICU caches the host/embedding default. Configure it before runtime creation;
   there is no automatic host rediscovery or concurrent global-default mutation.
   One operation copies and uses one result with pinned zone-database mappings. */
int qjs_temporal_system_zone(QJSTemporalZone *result)
{
#ifdef CONFIG_ICU
    UChar id[256];
    char identifier[256];
    QJSTemporalZone known;
    int error;
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = ucal_getDefaultTimeZone(id, 256, &status);
    if (icu_error(status)) return icu_error(status);
    if (length < 0 || (size_t)length >= sizeof(identifier))
        return QJS_TEMPORAL_ERROR_BACKEND;
    for (int32_t i = 0; i < length; i++) {
        if (id[i] > 127 || !id[i]) return QJS_TEMPORAL_ERROR_BACKEND;
        identifier[i] = (char)id[i];
    }
    identifier[length] = 0;
    if (!strcmp(identifier, "Etc/Unknown"))
        return qjs_temporal_zone_parse(result, "UTC", 3);
    if (length > 3 && !memcmp(identifier, "GMT", 3) &&
        (identifier[3] == '+' || identifier[3] == '-'))
        return qjs_temporal_zone_parse(result, identifier + 3, (size_t)length - 3);
    error = qjs_temporal_zone_parse(&known, identifier, (size_t)length);
    if (error) return error;
    /* The system identifier is primary; user-supplied Temporal identifiers
       preserve aliases. Both policies use the same IANA metadata owner. */
    length = intl_iana_zone_primary(known.identifier, id, 256, &status);
    if (icu_error(status)) return icu_error(status);
    if (length <= 0 || (size_t)length >= sizeof(identifier))
        return QJS_TEMPORAL_ERROR_BACKEND;
    for (int32_t i = 0; i < length; i++) {
        if (id[i] > 127 || !id[i]) return QJS_TEMPORAL_ERROR_BACKEND;
        identifier[i] = (char)id[i];
    }
    identifier[length] = 0;
    return qjs_temporal_zone_parse(result, identifier, (size_t)length);
#else
    return qjs_temporal_zone_parse(result, "UTC", 3);
#endif
}
int qjs_temporal_system_epoch(QJSTemporalEpochNs *result)
{
    QJSTemporalEpochNs seconds, fraction, epoch;
#ifdef _WIN32
    FILETIME filetime;
    uint64_t ticks;
    GetSystemTimeAsFileTime(&filetime);
    ticks = ((uint64_t)filetime.dwHighDateTime << 32) | filetime.dwLowDateTime;
    seconds = qjs_temporal_epoch_ns_from_int64((int64_t)(ticks / 10000000) - INT64_C(11644473600));
    fraction = qjs_temporal_epoch_ns_from_int64((int64_t)(ticks % 10000000) * 100);
#else
#ifdef CLOCK_REALTIME
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now)) return QJS_TEMPORAL_ERROR_BACKEND;
    seconds = qjs_temporal_epoch_ns_from_int64((int64_t)now.tv_sec);
    fraction = qjs_temporal_epoch_ns_from_int64(now.tv_nsec);
#else
    struct timeval now;
    if (gettimeofday(&now, NULL)) return QJS_TEMPORAL_ERROR_BACKEND;
    seconds = qjs_temporal_epoch_ns_from_int64((int64_t)now.tv_sec);
    fraction = qjs_temporal_epoch_ns_from_int64((int64_t)now.tv_usec * 1000);
#endif
#endif
    if (qjs_temporal_epoch_ns_multiply(&epoch, seconds, 1000000000) ||
        qjs_temporal_epoch_ns_add(&epoch, epoch, fraction)) return QJS_TEMPORAL_ERROR_RANGE;
    /* HostSystemUTCEpochNanoseconds clamps an out-of-range system clock. */
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) {
        QJSTemporalEpochNs limit;
        qjs_temporal_epoch_ns_from_milliseconds(&limit,
            epoch.high >> 63 ? -8640000000000000.0 : 8640000000000000.0);
        epoch = limit;
    }
    *result = epoch;
    return 0;
}
