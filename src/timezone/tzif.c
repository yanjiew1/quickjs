/* Independent TZif v2/v3/v4 reader and POSIX future rules.
   Copyright (c) 2026 Yan-Jie Wang. MIT license.

   This file includes no tzcode implementation. The pinned tzfile.5 and
   localtime.c were consulted for format semantics, including type zero,
   Julian days, signed transition times and perpetual daylight time.
   All decoding uses byte reads; input need not satisfy integer alignment. */
#include "tzif.h"
#include "../calendar/iso.h"
#include <string.h>

static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
        p[3];
}

static int32_t signed32(const unsigned char *p)
{
    uint32_t value = read32(p);
    return value <= INT32_MAX ? (int32_t)value
                              : -1 - (int32_t)(UINT32_MAX - value);
}

static int64_t signed64(const unsigned char *p)
{
    uint64_t value = (uint64_t)read32(p) << 32 | read32(p + 4);
    return value <= INT64_MAX ? (int64_t)value
                              : -INT64_C(1) - (int64_t)(UINT64_MAX - value);
}

static int64_t transition_at(const QJSTzifState *s, uint32_t i)
{
    return signed64(s->times + (size_t)i * 8);
}

static int32_t type_offset(const QJSTzifState *s, uint32_t type)
{
    return signed32(s->types + (size_t)type * 6);
}

/* Validate each complete block before making any views. Counts and byte
   arithmetic remain unsigned; negative count encodings become over-limit. */
static int block(const unsigned char *bytes, size_t length, size_t *position,
                 unsigned char version, int width, QJSTzifState *result)
{
    const unsigned char *h, *times, *indices, *types, *chars, *stds, *uts;
    uint32_t ntime, ntype, nchar, nstd, nut, i;
    uint64_t size;
    int64_t previous = 0;
    if (*position > length || length - *position < 44)
        return QJS_TZ_DATA;
    h = bytes + *position;
    if (memcmp(h, "TZif", 4) || h[4] != version || read32(h + 28))
        return QJS_TZ_DATA; /* Leap-aware data is not a POSIX timeline. */
    for (i = 5; i < 20; i++)
        if (h[i])
            return QJS_TZ_DATA;
    nut = read32(h + 20);
    nstd = read32(h + 24);
    ntime = read32(h + 32);
    ntype = read32(h + 36);
    nchar = read32(h + 40);
    if (!ntype || ntype > 256 || !nchar || (nstd && nstd != ntype) ||
        (nut && nut != ntype))
        return QJS_TZ_DATA;
    size = (uint64_t)ntime * (width + 1) + (uint64_t)ntype * 6 + nchar + nstd +
        nut;
    *position += 44;
    if (size > length - *position)
        return QJS_TZ_DATA;
    times = bytes + *position;
    indices = times + (size_t)ntime * width;
    types = indices + ntime;
    chars = types + (size_t)ntype * 6;
    stds = chars + nchar;
    uts = stds + nstd;
    for (i = 0; i < ntime; i++) {
        int64_t value = width == 8 ? signed64(times + (size_t)i * 8)
                                   : signed32(times + (size_t)i * 4);
        if ((i && value <= previous) || indices[i] >= ntype)
            return QJS_TZ_DATA;
        previous = value;
    }
    for (i = 0; i < ntype; i++) {
        int32_t offset = signed32(types + (size_t)i * 6);
        unsigned char abbreviation = types[(size_t)i * 6 + 5];
        if (offset <= -86400 || offset >= 86400 ||
            types[(size_t)i * 6 + 4] > 1 || abbreviation >= nchar ||
            !memchr(chars + abbreviation, 0, nchar - abbreviation) ||
            (nstd && stds[i] > 1) ||
            (nut && (uts[i] > 1 || (uts[i] && (!nstd || !stds[i])))))
            return QJS_TZ_DATA;
    }
    for (i = 0; i < nchar; i++)
        if (chars[i] && (chars[i] < 0x20 || chars[i] > 0x7e))
            return QJS_TZ_DATA;
    *position += (size_t)size;
    if (result) {
        result->times = times;
        result->indices = indices;
        result->types = types;
        result->characters = chars;
        result->time_count = ntime;
        result->type_count = ntype;
        result->character_count = nchar;
    }
    return QJS_TZ_OK;
}

typedef struct Cursor {
    const unsigned char *p, *end;
    unsigned char version;
} Cursor;
static int consume(Cursor *c, unsigned char byte)
{
    if (c->p == c->end || *c->p != byte)
        return 0;
    c->p++;
    return 1;
}

static int number(Cursor *c, int minimum, int maximum, int *output)
{
    int value = 0;
    const unsigned char *start = c->p;
    while (c->p != c->end && *c->p >= '0' && *c->p <= '9') {
        int digit = *c->p++ - '0';
        if (value > (maximum - digit) / 10)
            return 0;
        value = value * 10 + digit;
        if (value > maximum)
            return 0;
    }
    if (start == c->p || value < minimum)
        return 0;
    *output = value;
    return 1;
}

static int letter(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int name(Cursor *c)
{
    const unsigned char *start;
    size_t length;
    int quoted = consume(c, '<');
    start = c->p;
    while (c->p != c->end &&
           (letter(*c->p) ||
            (quoted &&
             ((*c->p >= '0' && *c->p <= '9') || *c->p == '+' || *c->p == '-'))))
        c->p++;
    length = (size_t)(c->p - start);
    if (length < 3 || (quoted && !consume(c, '>')))
        return 0;
    return 1;
}

static int clock_value(Cursor *c, int rule_time, int32_t *output)
{
    int sign = 1, hour, minute = 0, second = 0;
    int signed_time = 0,
        maximum = rule_time ? (c->version == '2' ? 24 : 167) : 24;
    if (consume(c, '-')) {
        sign = -1;
        signed_time = 1;
    } else if (consume(c, '+'))
        signed_time = 1;
    if (rule_time && c->version == '2' && signed_time)
        return 0;
    if (!number(c, 0, maximum, &hour))
        return 0;
    if (consume(c, ':')) {
        if (!number(c, 0, 59, &minute))
            return 0;
        if (consume(c, ':') && !number(c, 0, 59, &second))
            return 0;
    }
    *output = sign * (hour * 3600 + minute * 60 + second);
    return rule_time || (*output > -86400 && *output < 86400);
}

static int rule(Cursor *c, QJSTzifRule *r)
{
    memset(r, 0, sizeof *r);
    r->seconds = 7200;
    if (consume(c, 'J')) {
        r->kind = QJS_TZ_RULE_JULIAN;
        if (!number(c, 1, 365, &r->day))
            return 0;
    } else if (consume(c, 'M')) {
        r->kind = QJS_TZ_RULE_MONTH;
        if (!number(c, 1, 12, &r->month) || !consume(c, '.') ||
            !number(c, 1, 5, &r->week) || !consume(c, '.') ||
            !number(c, 0, 6, &r->day))
            return 0;
    } else {
        r->kind = QJS_TZ_RULE_DAY;
        if (!number(c, 0, 365, &r->day))
            return 0;
    }
    return !consume(c, '/') || clock_value(c, 1, &r->seconds);
}

static int64_t floor_div(int64_t value, int64_t divisor)
{
    return value / divisor - (value % divisor < 0);
}

static int leap(int64_t year)
{
    return qjs_iso_leap_year((int32_t)year);
}

static int64_t year_days(int64_t year)
{
    QJSISODate date = {(int32_t)year, 1, 1};
    int64_t days;

    qjs_iso_date_to_days(&days, date);
    return days;
}

static int64_t epoch_year(int64_t seconds)
{
    QJSISODate date;

    qjs_iso_date_from_days(&date, floor_div(seconds, 86400));
    return date.year;
}

static int64_t rule_second(int64_t year, const QJSTzifRule *r, int32_t offset)
{
    static const int cumulative[] = {0,   31,  59,  90,  120, 151, 181,
                                     212, 243, 273, 304, 334, 365};
    int day = r->day;
    if (r->kind == QJS_TZ_RULE_JULIAN) {
        day--;
        if (leap(year) && r->day >= 60)
            day++;
    } else if (r->kind == QJS_TZ_RULE_MONTH) {
        int first = cumulative[r->month - 1] + (leap(year) && r->month > 2);
        int length = cumulative[r->month] - cumulative[r->month - 1] +
            (leap(year) && r->month == 2);
        int weekday = (int)((year_days(year) + first + 4) % 7);
        if (weekday < 0)
            weekday += 7;
        day = (r->day - weekday + 7) % 7 + (r->week - 1) * 7;
        if (day >= length)
            day -= 7;
        day += first;
    }
    return (year_days(year) + day) * 86400 + r->seconds - offset;
}

static int year_events(const QJSTzifState *s, int64_t year, int64_t events[2])
{
    events[0] = rule_second(year, &s->start, s->standard_offset);
    events[1] = rule_second(year, &s->end, s->daylight_offset);
    /* Equal/full-year northern intervals denote perpetual daylight time.
       A reversed interval is the ordinary southern-hemisphere season. */
    return events[1] < events[0] ||
        (events[0] < events[1] &&
         events[1] - events[0] < (365 + leap(year)) * INT64_C(86400));
}

static int future_is_dst(const QJSTzifState *s, int64_t seconds)
{
    int64_t year = epoch_year(seconds), latest = 0;
    int adjustment, exists = 0, is_dst = 0;

    if (s->future == 1)
        return 0;
    if (s->future == 3)
        return 1;
    /* Rule dates/times extend at most eight days beyond their nominal year
       after offset conversion. A complete Gregorian cycle also covers rare
       seasons combining a leap-year condition with a particular weekday. */
    for (adjustment = 1; adjustment >= -400; adjustment--) {
        int64_t events[2];
        int i;

        if (!year_events(s, year + adjustment, events))
            continue;
        for (i = 0; i < 2; i++) {
            if (events[i] <= seconds && (!exists || events[i] > latest)) {
                latest = events[i];
                exists = 1;
                is_dst = !i;
            }
        }
        if (exists &&
            latest >= year_days(year + adjustment) * 86400 + 8 * 86400)
            break; /* Earlier nominal years cannot contain a later event. */
    }
    return is_dst;
}

static int32_t future_offset(const QJSTzifState *s, int64_t seconds)
{
    return future_is_dst(s, seconds) ? s->daylight_offset : s->standard_offset;
}

static int footer(QJSTzifState *s, const unsigned char *bytes, size_t length,
                  unsigned char version)
{
    Cursor c = {bytes, bytes + length, version};
    int32_t offset;
    int year, periodic = 0;
    if (!length)
        return QJS_TZ_OK;
    if (!name(&c) || !clock_value(&c, 0, &offset))
        return QJS_TZ_DATA;
    s->standard_offset = -offset;
    s->future = 1;
    if (c.p == c.end)
        return QJS_TZ_OK;
    if (!name(&c))
        return QJS_TZ_DATA;
    s->daylight_offset = s->standard_offset + 3600;
    if (c.p != c.end && *c.p != ',') {
        if (!clock_value(&c, 0, &offset))
            return QJS_TZ_DATA;
        s->daylight_offset = -offset;
    }
    if (s->daylight_offset <= -86400 || s->daylight_offset >= 86400 ||
        !consume(&c, ',') || !rule(&c, &s->start) || !consume(&c, ',') ||
        !rule(&c, &s->end) || c.p != c.end)
        return QJS_TZ_DATA;
    for (year = 2000; year < 2400; year++) {
        int64_t events[2];
        if (year_events(s, year, events)) {
            periodic = 1;
            break;
        }
    }
    s->future = periodic ? 2 : 3;
    return QJS_TZ_OK;
}

int qjs_tzif_parse(QJSTzifState *output, const unsigned char *bytes,
                   size_t length)
{
    QJSTzifState s;
    size_t position = 0, tail_length;
    unsigned char version;
    if (!output || !bytes || length < 44 || length > QJS_TZ_MAX_FILE_SIZE)
        return QJS_TZ_DATA;
    version = bytes[4];
    if (version < '2' || version > '4')
        return QJS_TZ_DATA;
    memset(&s, 0, sizeof s);
    if (block(bytes, length, &position, version, 4, NULL) ||
        block(bytes, length, &position, version, 8, &s))
        return QJS_TZ_DATA;
    if (length - position < 2 || bytes[position] != '\n' ||
        bytes[length - 1] != '\n')
        return QJS_TZ_DATA;
    tail_length = length - position - 2;
    if (memchr(bytes + position + 1, 0, tail_length) ||
        memchr(bytes + position + 1, '\n', tail_length) ||
        footer(&s, bytes + position + 1, tail_length, version))
        return QJS_TZ_DATA;
    if (s.future && s.time_count) {
        int64_t last = transition_at(&s, s.time_count - 1);
        if (last >= -QJS_TZ_OFFSET_LIMIT && last <= QJS_TZ_OFFSET_LIMIT &&
            future_offset(&s, last) !=
                type_offset(&s, s.indices[s.time_count - 1]))
            return QJS_TZ_DATA;
    }
    *output = s;
    return QJS_TZ_OK;
}

static uint32_t type_at(const QJSTzifState *s, int64_t seconds)
{
    uint32_t lo = 0, hi = s->time_count;

    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        if (transition_at(s, middle) <= seconds)
            lo = middle + 1;
        else
            hi = middle;
    }
    return lo ? s->indices[lo - 1] : 0;
}

int qjs_tzif_offset(const QJSTzifState *s, int64_t seconds, int32_t *output)
{
    if (!s || !output)
        return QJS_TZ_DATA;
    if (seconds < -QJS_TZ_OFFSET_LIMIT || seconds > QJS_TZ_OFFSET_LIMIT)
        return QJS_TZ_RANGE;
    if (s->future && (!s->time_count ||
                      seconds > transition_at(s, s->time_count - 1)))
        *output = future_offset(s, seconds);
    else
        *output = type_offset(s, type_at(s, seconds));
    return QJS_TZ_OK;
}

int qjs_tzif_info(const QJSTzifState *s, int64_t seconds, QJSTimeZoneInfo *output)
{
    QJSTimeZoneInfo info;

    if (!s || !output)
        return QJS_TZ_DATA;
    if (seconds < -QJS_TZ_OFFSET_LIMIT || seconds > QJS_TZ_OFFSET_LIMIT)
        return QJS_TZ_RANGE;
    if (s->future && (!s->time_count ||
                      seconds > transition_at(s, s->time_count - 1))) {
        info.is_dst = future_is_dst(s, seconds);
        info.offset_seconds = info.is_dst ? s->daylight_offset : s->standard_offset;
    } else {
        uint32_t type = type_at(s, seconds);

        info.offset_seconds = type_offset(s, type);
        info.is_dst = s->types[type * 6 + 4];
    }
    *output = info;
    return QJS_TZ_OK;
}

static void select_transition(const QJSTzifState *s, int64_t candidate,
                              int64_t seconds, int next, int inclusive,
                              int64_t *best, int *exists)
{
    int32_t before, after;
    if (candidate <= -QJS_TZ_OFFSET_LIMIT || candidate > QJS_TZ_OFFSET_LIMIT ||
        (next ? (candidate < seconds || (!inclusive && candidate == seconds))
              : (candidate > seconds || (!inclusive && candidate == seconds))))
        return;
    if (*exists && (next ? candidate >= *best : candidate <= *best))
        return;
    if (qjs_tzif_offset(s, candidate - 1, &before) ||
        qjs_tzif_offset(s, candidate, &after) || before == after)
        return;
    if (!*exists || (next ? candidate < *best : candidate > *best)) {
        *best = candidate;
        *exists = 1;
    }
}

int qjs_tzif_transition(const QJSTzifState *s, int64_t seconds, int next,
                        int inclusive, int64_t *output, int *found)
{
    uint32_t lo = 0, hi, i;
    int64_t best = 0;
    int exists = 0;
    if (!s || !output || !found)
        return QJS_TZ_DATA;
    if (seconds < -QJS_TZ_OFFSET_LIMIT || seconds > QJS_TZ_OFFSET_LIMIT)
        return QJS_TZ_RANGE;
    /* Find the explicit boundary, then skip designation/DST-only changes. */
    hi = s->time_count;
    while (lo < hi) {
        uint32_t middle = lo + (hi - lo) / 2;
        int64_t value = transition_at(s, middle);
        if (value < seconds ||
            (value == seconds && (next ? !inclusive : inclusive)))
            lo = middle + 1;
        else
            hi = middle;
    }
    if (next) {
        for (i = lo; i < s->time_count && !exists; i++)
            select_transition(s, transition_at(s, i), seconds, next, inclusive,
                              &best, &exists);
    } else {
        for (i = lo; i && !exists; i--)
            select_transition(s, transition_at(s, i - 1), seconds, next,
                              inclusive, &best, &exists);
    }
    if (s->future == 2) {
        int64_t last =
            s->time_count ? transition_at(s, s->time_count - 1) : INT64_MIN;
        int64_t anchor = seconds, year;
        int adjustment;
        if (next && last > anchor)
            anchor = last;
        /* The query range and candidate range are deliberately different. */
        if (anchor <= QJS_TZ_OFFSET_LIMIT && (next || seconds > last)) {
            year = epoch_year(anchor);
            for (adjustment = next ? -1 : 1;
                 next ? adjustment <= 400 : adjustment >= -400;
                 adjustment += next ? 1 : -1) {
                int64_t events[2];
                int j;
                if (!year_events(s, year + adjustment, events))
                    continue;
                for (j = 0; j < 2; j++)
                    if (events[j] > last)
                        select_transition(s, events[j], seconds, next,
                                          inclusive, &best, &exists);
                if (exists &&
                    (next ? best <= year_days(year + adjustment + 1) * 86400 -
                                 8 * 86400
                          : best >=
                             year_days(year + adjustment) * 86400 + 8 * 86400))
                    break;
            }
        }
    }
    if (exists)
        *output = best;
    *found = exists;
    return QJS_TZ_OK;
}

int qjs_tzif_candidates(const QJSTzifState *s, int64_t wall,
                        QJSTimeZoneVisit *visit, void *opaque, size_t *count,
                        int32_t *gap_before, int32_t *gap_after,
                        int64_t *gap_end)
{
    int32_t offsets[258];
    int64_t candidates[258];
    size_t length = 0, matches = 0, i;
    uint32_t types;

    if (wall < -QJS_TZ_WALL_LIMIT || wall > QJS_TZ_WALL_LIMIT)
        return QJS_TZ_RANGE;
    /* There are at most 256 TZif types and two footer offsets. Sorting
       unique offsets descending visits all matching epochs ascending,
       including histories with three or more simultaneous candidates. */
    types = s->type_count + (s->future ? 2 : 0);
    for (i = 0; i < types; i++) {
        int32_t offset = i < s->type_count ? type_offset(s, i)
            : i == s->type_count           ? s->standard_offset
                                           : s->daylight_offset;
        size_t at = 0;

        while (at < length && offsets[at] > offset)
            at++;
        if (at < length && offsets[at] == offset)
            continue;
        memmove(offsets + at + 1, offsets + at,
                (length - at) * sizeof(*offsets));
        offsets[at] = offset;
        length++;
    }
    for (i = 0; i < length; i++) {
        int64_t epoch = wall - offsets[i];
        int32_t actual;
        int error = qjs_tzif_offset(s, epoch, &actual);

        if (error)
            return error;
        if (actual == offsets[i])
            candidates[matches++] = epoch;
    }
    if (matches) {
        for (i = 0; i < matches; i++)
            visit(opaque, candidates[i]);
        *count = matches;
        return QJS_TZ_OK;
    }
    {
        int64_t cursor = wall - 86400, transition;
        int found, error;

        for (;;) {
            int32_t before, after;

            error = qjs_tzif_transition(s, cursor, 1, 0, &transition, &found);
            if (error)
                return error;
            if (!found || transition > wall + 86400)
                break;
            if (qjs_tzif_offset(s, transition - 1, &before) ||
                qjs_tzif_offset(s, transition, &after))
                return QJS_TZ_DATA;
            if (before < after && wall >= transition + before &&
                wall < transition + after) {
                *count = 0;
                *gap_before = before;
                *gap_after = after;
                *gap_end = transition;
                return QJS_TZ_OK;
            }
            cursor = transition;
        }
    }
    return QJS_TZ_DATA;
}
