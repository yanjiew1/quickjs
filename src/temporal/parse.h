/*
 * Portable Temporal ISO parsing primitives
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#ifndef QUICKJS_TEMPORAL_PARSE_H
#define QUICKJS_TEMPORAL_PARSE_H

#include <stddef.h>
#include <stdint.h>
#include "iso.h"

#define QJS_TEMPORAL_OFFSET_ALLOW_Z (1U << 0)
#define QJS_TEMPORAL_OFFSET_ALLOW_SECONDS (1U << 1)

typedef struct QJSTemporalUTCOffset {
    int64_t nanoseconds;
    int is_z;
    int has_seconds; /* Preserve offset match-minute versus match-exactly. */
} QJSTemporalUTCOffset;

typedef struct QJSTemporalInstantPrefix {
    QJSTemporalISODateTime datetime;
    QJSTemporalUTCOffset offset;
} QJSTemporalInstantPrefix;

typedef enum QJSTemporalISOGrammar {
    QJS_TEMPORAL_PARSE_DATE_TIME = 1U << 0,
    QJS_TEMPORAL_PARSE_ZONED = 1U << 1,
    QJS_TEMPORAL_PARSE_INSTANT = 1U << 2,
    QJS_TEMPORAL_PARSE_TIME = 1U << 3,
    QJS_TEMPORAL_PARSE_YEAR_MONTH = 1U << 4,
    QJS_TEMPORAL_PARSE_MONTH_DAY = 1U << 5,
} QJSTemporalISOGrammar;

/* String slices borrow the original input. Numeric fields are copied.
   A missing year is represented by year_absent and a leap reference 1972.
   A short YearMonth has reference day 1 and short_year_month set. */
typedef struct QJSTemporalParsedISO {
    QJSTemporalISODateTime datetime;
    QJSTemporalUTCOffset offset;
    const char *offset_text;
    size_t offset_length;
    const char *time_zone;
    size_t time_zone_length;
    const char *calendar;
    size_t calendar_length;
    int offset_present;
    int has_time;
    int year_absent;
    int short_year_month;
} QJSTemporalParsedISO;

/* The allowed bit mask represents ParseISODateTime's grammar goals.
   Complete annotations and PlainTime ambiguity checks are enforced.
   No zone / calendar availability lookup or narrower range policy occurs.
   Failure leaves *result unchanged. */
int qjs_temporal_parse_iso_datetime(QJSTemporalParsedISO *result,
                                    const char *text, size_t length,
                                    unsigned allowed_grammars);

/* Parse bounded ASCII numeric grammar fragments without allocation or JS
   coercion. Return 0 and a consumed byte count on success, otherwise -1
   with both outputs unchanged. Required output pointers do not overlap.
   Non-ASCII bytes are never accepted as signs, digits or separators. */
int qjs_temporal_parse_iso_date_prefix(QJSTemporalISODate *result,
                                       size_t *consumed,
                                       const char *text, size_t length);
int qjs_temporal_parse_iso_time_prefix(QJSTemporalISOTime *result,
                                       size_t *consumed,
                                       const char *text, size_t length);
int qjs_temporal_parse_utc_offset_prefix(QJSTemporalUTCOffset *result,
                                         size_t *consumed,
                                         const char *text, size_t length,
                                         unsigned flags);

/* Parse Date + separator + Time + required UTC offset, stopping at EOF
   or '['. This is NOT complete TemporalInstantString validation: callers
   must validate every trailing time-zone/calendar/critical annotation and
   apply epoch range policy after exact offset subtraction. The complete
   Instant entry point below performs this continuation. */
int qjs_temporal_parse_instant_prefix(QJSTemporalInstantPrefix *result,
                                      size_t *consumed,
                                      const char *text, size_t length);

/* Validate the complete TemporalInstantString grammar and annotation rules,
   subtract the exact UTC offset, then check the final Instant range. Zone
   and calendar annotations are recognized syntactically and ignored as
   required for Instant; no host zone/calendar lookup is performed. Return
   0 on success or -1 with the output unchanged. JS coercion and exception
   construction remain the engine adapter's responsibility. */
int qjs_temporal_parse_instant(QJSTemporalEpochNs *result,
                               const char *text, size_t length);

#endif /* QUICKJS_TEMPORAL_PARSE_H */
