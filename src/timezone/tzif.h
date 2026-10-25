/* Immutable validated TZif views, private to the native backend and tools.
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
#ifndef QUICKJS_TIMEZONE_TZIF_H
#define QUICKJS_TIMEZONE_TZIF_H

#include "timezone.h"

#define QJS_TZ_MAX_FILE_SIZE 65536
#define QJS_TZ_INSTANT_LIMIT INT64_C(8640000000000)
#define QJS_TZ_OFFSET_LIMIT (QJS_TZ_INSTANT_LIMIT + 172800)
#define QJS_TZ_WALL_LIMIT (QJS_TZ_INSTANT_LIMIT + 86400)

enum { QJS_TZ_RULE_DAY, QJS_TZ_RULE_JULIAN, QJS_TZ_RULE_MONTH };
typedef struct QJSTzifRule {
    int kind, day, month, week;
    int32_t seconds;
} QJSTzifRule;

typedef struct QJSTzifState {
    const unsigned char *times, *indices, *types, *characters;
    uint32_t time_count, type_count, character_count;
    /* 0: no footer, 1: fixed, 2: periodic, 3: perpetual daylight time. */
    int future;
    int32_t standard_offset, daylight_offset;
    QJSTzifRule start, end;
} QJSTzifState;

/* Parse without allocating. Views borrow immutable input bytes; the output
   is unchanged on failure. Queries require a successfully validated view. */
int qjs_tzif_parse(QJSTzifState *result, const unsigned char *bytes,
                   size_t length);
int qjs_tzif_offset(const QJSTzifState *state, int64_t seconds,
                    int32_t *offset);
int qjs_tzif_info(const QJSTzifState *state, int64_t seconds,
                  QJSTimeZoneInfo *result);
/* Raw rule lookup includes transitions just outside the Instant interval.
   Common Temporal code applies public Instant bounds after disambiguation. */
int qjs_tzif_transition(const QJSTzifState *state, int64_t seconds, int next,
                        int inclusive, int64_t *result, int *found);
/* Closed interval; detect offset or raw DST changes, including footer handoff.
   Abbreviation-only transitions do not change the information record. Preserve
   output on invalid arguments/range. */
int qjs_tzif_info_is_constant(const QJSTzifState *state, int64_t first, int64_t last,
                              int *constant);
int qjs_tzif_candidates(const QJSTzifState *state, int64_t wall,
                        QJSTimeZoneVisit *visit, void *opaque, size_t *count,
                        int32_t *gap_before, int32_t *gap_after,
                        int64_t *gap_end);

#endif /* QUICKJS_TIMEZONE_TZIF_H */
