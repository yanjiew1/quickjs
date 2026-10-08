/* Native Intl.DateTimeFormat, ECMA-402 7ae78cf (2026-10-06). */
#include "intl-internal.h"
#include "locale-integration.h"
#ifdef CONFIG_TEMPORAL
#include "../temporal/temporal-internal.h"
#include "../../../temporal/time-zone.h"
#endif
#ifdef CONFIG_INTL
#include "../../../intl/locale-data.h"
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/udateintervalformat.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uformattedvalue.h>
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <limits.h>

/* Format values use the order required by BasicFormatMatcher. */
enum { DTF_TWO_DIGIT, DTF_NUMERIC, DTF_NARROW, DTF_SHORT, DTF_LONG };
enum { DTF_WEEKDAY, DTF_ERA, DTF_YEAR, DTF_MONTH, DTF_DAY,
       DTF_DAY_PERIOD, DTF_HOUR, DTF_MINUTE, DTF_SECOND, DTF_FRACTION,
       DTF_ZONE_NAME, DTF_FIELD_COUNT };
static const char *const dtf_names[] = {
    "weekday", "era", "year", "month", "day", "dayPeriod", "hour",
    "minute", "second", "fractionalSecondDigits", "timeZoneName"
};
static const char *const dtf_widths[] = {
    "2-digit", "numeric", "narrow", "short", "long"
};
static const char *const dtf_text_widths[] = { "narrow", "short", "long" };
static const char *const dtf_numeric_widths[] = { "2-digit", "numeric" };
static const char *const dtf_zones[] = {
    "short", "long", "shortOffset", "longOffset", "shortGeneric", "longGeneric"
};
static const char *const dtf_styles[] = { "full", "long", "medium", "short" };
static const char *const dtf_cycles[] = { "h11", "h12", "h23", "h24" };
static const char *const dtf_matchers[] = { "lookup", "best fit" };
static const char *const dtf_format_matchers[] = { "basic", "best fit" };

typedef struct JSIntlDateTimeFormat {
    JSIntlResolvedLocale locale;
    char *time_zone;
    UChar *icu_zone;
    int32_t zone_length;
    UDateFormat *format;
    UDateIntervalFormat *range;
    UChar *pattern;
    int32_t pattern_length;
    UChar *skeleton;
    int32_t skeleton_length;
    int fields[DTF_FIELD_COUNT];
    int hour_cycle, date_style, time_style;
    JSValue bound_format;
#ifdef CONFIG_TEMPORAL
    struct JSIntlDateTimeFormat *temporal[6];
    int requested_fields[DTF_FIELD_COUNT];
    int matcher, prepared, is_plain, zoned_locale;
#endif
} JSIntlDateTimeFormat;

static void dtf_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    int i;
    if (!s)
        return;
#ifdef CONFIG_TEMPORAL
    for (i = 0; i < 6; i++) {
        JSIntlDateTimeFormat *t = s->temporal[i];
        if (!t) continue;
        if (t->format) udat_close(t->format);
        if (t->range) udtitvfmt_close(t->range);
        js_free_rt(rt, t->pattern);
        js_free_rt(rt, t->skeleton);
        js_free_rt(rt, t);
    }
#endif
    if (s->format)
        udat_close(s->format);
    if (s->range)
        udtitvfmt_close(s->range);
    js_free_rt(rt, s->locale.locale);
    js_free_rt(rt, s->locale.data_locale);
    js_free_rt(rt, s->locale.icu_locale);
    for (i = 0; i < s->locale.key_count; i++)
        js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s->time_zone);
    js_free_rt(rt, s->icu_zone);
    js_free_rt(rt, s->pattern);
    js_free_rt(rt, s->skeleton);
    JS_FreeValueRT(rt, s->bound_format);
    js_free_rt(rt, s);
}

static void dtf_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark_func)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (s) {
        JS_MarkValue(rt, s->bound_format, mark_func);
    }
}

static int dtf_unicode_type(JSContext *ctx, const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    int length = 0;
    if (!value)
        return 0;
    for (;;) {
        if (!*p || *p == '-') {
            if (length < 3 || length > 8)
                break;
            if (!*p)
                return 0;
            length = 0;
        } else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                   (*p >= '0' && *p <= '9')) {
            length++;
        } else {
            break;
        }
        p++;
    }
    JS_ThrowRangeError(ctx, "invalid Unicode locale type");
    return -1;
}

static int dtf_time_zone(JSContext *ctx, JSValueConst options,
                         JSIntlDateTimeFormat *s,
                         JSValueConst override_time_zone)
{
    JSValue value;
    UChar *text = NULL;
    int32_t length = 0, i;
    char *ascii = NULL;
    const char *zone;
    int hours, minutes = 0;
    BOOL offset = FALSE;
    value = JS_GetPropertyStr(ctx, options, "timeZone");
    if (JS_IsException(value))
        return -1;
    if (!JS_IsUndefined(override_time_zone)) {
        if (!JS_IsUndefined(value)) {
            JS_FreeValue(ctx, value);
            JS_ThrowTypeError(ctx, "ZonedDateTime locale options specify timeZone");
            return -1;
        }
        JS_FreeValue(ctx, value);
        value = JS_DupValue(ctx, override_time_zone);
    }
    if (JS_IsUndefined(value)) {
        zone = js_intl_default_time_zone(ctx);
        if (!zone)
            goto fail;
        s->time_zone = js_intl_strdup(ctx, zone);
    } else {
        if (js_intl_to_uchar(ctx, value, &text, &length))
            goto fail;
        ascii = js_intl_alloc_char(ctx, length);
        if (!ascii)
            goto fail;
        for (i = 0; i < length; i++) {
            if (text[i] > 127 || !text[i]) {
                JS_ThrowRangeError(ctx, "invalid time zone");
                goto fail;
            }
            ascii[i] = text[i];
        }
        ascii[length] = 0;
        if (length && (ascii[0] == '+' || ascii[0] == '-')) {
            offset = TRUE;
            if (length != 3 && length != 5 && length != 6)
                goto invalid_offset;
            if (ascii[1] < '0' || ascii[1] > '9' ||
                ascii[2] < '0' || ascii[2] > '9')
                goto invalid_offset;
            hours = (ascii[1] - '0') * 10 + ascii[2] - '0';
            if (length > 3) {
                i = length == 6 ? 4 : 3;
                if ((length == 6 && ascii[3] != ':') ||
                    ascii[i] < '0' || ascii[i] > '9' ||
                    ascii[i + 1] < '0' || ascii[i + 1] > '9')
                    goto invalid_offset;
                minutes = (ascii[i] - '0') * 10 + ascii[i + 1] - '0';
            }
            if (hours > 23 || minutes > 59)
                goto invalid_offset;
            s->time_zone = js_intl_alloc_char(ctx, 6);
            if (!s->time_zone)
                goto fail;
            /* Both zero signs canonicalize to +00:00. */
            snprintf(s->time_zone, 7, "%c%02d:%02d",
                     (hours || minutes) ? ascii[0] : '+', hours, minutes);
        } else {
            const char *identifier = intl_iana_zone_name(ascii, length);
            if (!identifier) {
                JS_ThrowRangeError(ctx, "invalid named time zone");
                goto fail;
            }
            /* Adopt Temporal's Stage 4 CreateDateTimeFormat amendment:
               retain Identifier, rather than replacing it with Primary. */
            s->time_zone = js_intl_strdup(ctx, identifier);
        }
    }
    if (!s->time_zone)
        goto fail;
    /* ICU recognizes fixed offsets as GMT+HH:MM; public slot remains +HH:MM. */
    if (!offset)
        offset = s->time_zone[0] == '+' || s->time_zone[0] == '-';
    s->zone_length = strlen(s->time_zone) + (offset ? 3 : 0);
    s->icu_zone = js_intl_alloc_uchar(ctx, s->zone_length);
    if (!s->icu_zone)
        goto fail;
    i = 0;
    if (offset) {
        s->icu_zone[i++] = 'G';
        s->icu_zone[i++] = 'M';
        s->icu_zone[i++] = 'T';
    }
    zone = s->time_zone;
    while (*zone)
        s->icu_zone[i++] = (unsigned char)*zone++;
    s->icu_zone[i] = 0;
    js_free(ctx, text);
    js_free(ctx, ascii);
    JS_FreeValue(ctx, value);
    return 0;
 invalid_offset:
    JS_ThrowRangeError(ctx, "invalid offset time zone");
 fail:
    js_free(ctx, text);
    js_free(ctx, ascii);
    JS_FreeValue(ctx, value);
    return -1;
}

static int dtf_read_fields(JSContext *ctx, JSValueConst options, int *fields,
                           BOOL *explicit_fields)
{
    int i, v;
    const char *const *values;
    int count, offset;
    *explicit_fields = FALSE;
    for (i = 0; i < DTF_FIELD_COUNT; i++) {
        offset = 0;
        if (i == DTF_FRACTION) {
            if (js_intl_get_number_option(ctx, options, dtf_names[i], 1, 3, -1, &v))
                return -1;
        } else {
            if (i == DTF_ZONE_NAME) {
                values = dtf_zones;
                count = countof(dtf_zones);
            } else if (i == DTF_WEEKDAY || i == DTF_ERA || i == DTF_DAY_PERIOD) {
                values = dtf_text_widths;
                count = countof(dtf_text_widths);
                offset = DTF_NARROW;
            } else if (i == DTF_MONTH) {
                values = dtf_widths;
                count = countof(dtf_widths);
            } else {
                values = dtf_numeric_widths;
                count = countof(dtf_numeric_widths);
            }
            if (js_intl_get_string_option(ctx, options, dtf_names[i],
                    values, count, -1, &v))
                return -1;
            if (v >= 0)
                v += offset;
        }
        fields[i] = v;
        if (v >= 0)
            *explicit_fields = TRUE;
    }
    return 0;
}

static int dtf_defaults(int *fields, int required, int defaults)
{
    int i;
    BOOL needed = TRUE;
    if (required != JS_INTL_DTF_TIME) {
        static const int date_fields[] = { DTF_WEEKDAY, DTF_YEAR, DTF_MONTH, DTF_DAY };
        for (i = 0; i < countof(date_fields); i++)
            if (fields[date_fields[i]] >= 0)
                needed = FALSE;
    }
    if (required != JS_INTL_DTF_DATE) {
        for (i = DTF_DAY_PERIOD; i <= DTF_FRACTION; i++)
            if (fields[i] >= 0)
                needed = FALSE;
    }
    if (needed && (defaults == JS_INTL_DTF_DATE || defaults == JS_INTL_DTF_ALL))
        fields[DTF_YEAR] = fields[DTF_MONTH] = fields[DTF_DAY] = DTF_NUMERIC;
    if (needed && (defaults == JS_INTL_DTF_TIME || defaults == JS_INTL_DTF_ALL))
        fields[DTF_HOUR] = fields[DTF_MINUTE] = fields[DTF_SECOND] = DTF_NUMERIC;
    return 0;
}

static int dtf_pattern_fields(const UChar *pattern, int32_t length, int *fields)
{
    int32_t i, n;
    int field, value;
    UChar ch;
    BOOL quoted = FALSE;
    for (i = 0; i < DTF_FIELD_COUNT; i++)
        fields[i] = -1;
    for (i = 0; i < length;) {
        ch = pattern[i++];
        if (ch == '\'') {
            if (i < length && pattern[i] == '\'')
                i++;
            else
                quoted = !quoted;
            continue;
        }
        if (quoted || !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')))
            continue;
        n = 1;
        while (i < length && pattern[i] == ch) {
            n++;
            i++;
        }
        switch (ch) {
        case 'G': field = DTF_ERA; value = n >= 5 ? DTF_NARROW : n == 4 ? DTF_LONG : DTF_SHORT; break;
        case 'y': case 'u': case 'r': case 'U':
            field = DTF_YEAR; value = n == 2 ? DTF_TWO_DIGIT : DTF_NUMERIC; break;
        case 'M': case 'L':
            field = DTF_MONTH;
            value = n >= 5 ? DTF_NARROW : n == 4 ? DTF_LONG : n == 3 ? DTF_SHORT : n == 2 ? DTF_TWO_DIGIT : DTF_NUMERIC;
            break;
        case 'd': field = DTF_DAY; value = n == 2 ? DTF_TWO_DIGIT : DTF_NUMERIC; break;
        case 'E': case 'e': case 'c':
            if (ch != 'E' && n < 3)
                return -1;
            field = DTF_WEEKDAY; value = n == 5 ? DTF_NARROW : n == 4 ? DTF_LONG : DTF_SHORT; break;
        case 'B': case 'b':
            field = DTF_DAY_PERIOD; value = n >= 5 ? DTF_NARROW : n == 4 ? DTF_LONG : DTF_SHORT; break;
        case 'a': continue; /* Implicit AM/PM is not a resolved dayPeriod option. */
        case 'K': case 'h': case 'H': case 'k':
            field = DTF_HOUR; value = n == 2 ? DTF_TWO_DIGIT : DTF_NUMERIC; break;
        case 'm': field = DTF_MINUTE; value = n == 2 ? DTF_TWO_DIGIT : DTF_NUMERIC; break;
        case 's': field = DTF_SECOND; value = n == 2 ? DTF_TWO_DIGIT : DTF_NUMERIC; break;
        case 'S': field = DTF_FRACTION; value = min_int(n, 3); break;
        case 'z': field = DTF_ZONE_NAME; value = n >= 4 ? 1 : 0; break;
        case 'v': field = DTF_ZONE_NAME; value = n >= 4 ? 5 : 4; break;
        case 'O': case 'X': case 'x': case 'Z':
            field = DTF_ZONE_NAME; value = n >= 4 ? 3 : 2; break;
        default: return -1;
        }
        fields[field] = value;
    }
    return 0;
}

static int dtf_basic_score(const int *requested, const int *format)
{
    int score = 0, i, a, b, delta;
    for (i = 0; i < DTF_FIELD_COUNT; i++) {
        a = requested[i];
        b = format[i];
        if (a < 0 && b >= 0)
            score -= 20;
        else if (a >= 0 && b < 0)
            score -= 120;
        else if (a == b)
            continue;
        else if (i == DTF_ZONE_NAME) {
            if (a == 0 || a == 4) {
                if (b == 2) score -= 1;
                else if (b == 3) score -= 4;
                else if ((a == 0 && b == 1) || (a == 4 && b == 5)) score -= 3;
                else score -= 120;
            } else if (a == 2 && b == 3) {
                score -= 3;
            } else if (a == 1 || a == 5) {
                if (b == 3) score -= 1;
                else if (b == 2) score -= 9;
                else if ((a == 1 && b == 0) || (a == 5 && b == 4)) score -= 8;
                else score -= 120;
            } else if (a == 3 && b == 2) {
                score -= 8;
            } else {
                score -= 120;
            }
        } else {
            delta = max_int(-2, min_int(2, b - a));
            if (delta == 2) score -= 6;
            else if (delta == 1) score -= 3;
            else if (delta == -1) score -= 6;
            else if (delta == -2) score -= 8;
        }
    }
    return score;
}

static int dtf_skeleton(JSContext *ctx, JSIntlDateTimeFormat *s)
{
    UChar tmp[96];
    int i, j, n = 0, repeat, v;
    UChar ch;
    for (i = 0; i < DTF_FIELD_COUNT; i++) {
        v = s->fields[i];
        if (v < 0)
            continue;
        switch (i) {
        case DTF_WEEKDAY: ch = 'E'; repeat = v == DTF_NARROW ? 5 : v == DTF_LONG ? 4 : 3; break;
        case DTF_ERA: ch = 'G'; repeat = v == DTF_NARROW ? 5 : v == DTF_LONG ? 4 : 3; break;
        case DTF_YEAR: ch = 'y'; repeat = v == DTF_TWO_DIGIT ? 2 : 1; break;
        case DTF_MONTH: ch = 'M'; repeat = v == DTF_NARROW ? 5 : v == DTF_LONG ? 4 : v == DTF_SHORT ? 3 : v == DTF_TWO_DIGIT ? 2 : 1; break;
        case DTF_DAY: ch = 'd'; repeat = v == DTF_TWO_DIGIT ? 2 : 1; break;
        case DTF_DAY_PERIOD: ch = 'B'; repeat = v == DTF_NARROW ? 5 : v == DTF_LONG ? 4 : 1; break;
        case DTF_HOUR: ch = "KhHk"[s->hour_cycle]; repeat = v == DTF_TWO_DIGIT ? 2 : 1; break;
        case DTF_MINUTE: ch = 'm'; repeat = v == DTF_TWO_DIGIT ? 2 : 1; break;
        case DTF_SECOND: ch = 's'; repeat = v == DTF_TWO_DIGIT ? 2 : 1; break;
        case DTF_FRACTION: ch = 'S'; repeat = v; break;
        default: ch = v < 2 ? 'z' : v < 4 ? 'O' : 'v'; repeat = v % 2 ? 4 : 1; break;
        }
        for (j = 0; j < repeat; j++)
            tmp[n++] = ch;
    }
    s->skeleton = js_intl_alloc_uchar(ctx, n);
    if (!s->skeleton)
        return -1;
    memcpy(s->skeleton, tmp, n * sizeof(UChar));
    s->skeleton[n] = 0;
    s->skeleton_length = n;
    return 0;
}

static int dtf_best_pattern(JSContext *ctx, UDateTimePatternGenerator *generator,
                             const UChar *skeleton, int32_t skeleton_length,
                             UChar **pattern, int32_t *pattern_length)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length;
    length = udatpg_getBestPatternWithOptions(generator, skeleton, skeleton_length,
                UDATPG_MATCH_HOUR_FIELD_LENGTH, NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && js_intl_icu_error(ctx, status, "date pattern"))
        return -1;
    *pattern = js_intl_alloc_uchar(ctx, length);
    if (!*pattern)
        return -1;
    status = U_ZERO_ERROR;
    *pattern_length = udatpg_getBestPatternWithOptions(generator, skeleton,
                skeleton_length, UDATPG_MATCH_HOUR_FIELD_LENGTH,
                *pattern, length + 1, &status);
    return js_intl_icu_error(ctx, status, "date pattern");
}

static int dtf_catalog_candidate(JSContext *ctx,
                                   UDateTimePatternGenerator *generator,
                                   JSIntlDateTimeFormat *s,
                                   const UChar *skeleton, int32_t length,
                                   int *best_score)
{
    UChar *candidate = NULL, *normalized;
    int32_t candidate_length, i, n = 0;
    int fields[DTF_FIELD_COUNT], score;
    normalized = js_intl_alloc_uchar(ctx, length);
    if (!normalized)
        return -1;
    /* Records carry patterns for all four hour cycles. Generate the selected
       cycle's pattern from a fixed locale catalog, independent of options. */
    for (i = 0; i < length; i++) {
        UChar ch = skeleton[i];
        if (ch == 'K' || ch == 'h' || ch == 'H' || ch == 'k' || ch == 'j')
            ch = "KhHk"[s->hour_cycle];
        if (ch == 'a' && s->hour_cycle >= 2)
            continue;
        normalized[n++] = ch;
    }
    normalized[n] = 0;
    if (dtf_best_pattern(ctx, generator, normalized, n, &candidate, &candidate_length)) {
        js_free(ctx, normalized);
        js_free(ctx, candidate);
        return -1;
    }
    js_free(ctx, normalized);
    if (!dtf_pattern_fields(candidate, candidate_length, fields)) {
        score = dtf_basic_score(s->fields, fields);
        if (score > *best_score) {
            js_free(ctx, s->pattern);
            s->pattern = candidate;
            s->pattern_length = candidate_length;
            candidate = NULL;
            *best_score = score;
        }
    }
    js_free(ctx, candidate);
    return 0;
}

static int dtf_basic_pattern(JSContext *ctx, UDateTimePatternGenerator *generator,
                              JSIntlDateTimeFormat *s)
{
    static const char *const required[] = {
        "EEEyMdjmsS", "EEEyMdjmsSS", "EEEyMdjmsSSS", "EEEyMdjms",
        "EEEyMd", "yMd", "yM", "Md", "M",
        "jmsS", "jmsSS", "jmsSSS", "jms", "jm",
        "BjmsS", "BjmsSS", "BjmsSSS", "Bjms", "Bjm", "Bj"
    };
    UEnumeration *enumeration = NULL;
    UErrorCode status = U_ZERO_ERROR;
    const UChar *skeleton;
    UChar buffer[32];
    int32_t length, i, j;
    int best_score = INT_MIN;
    for (i = 0; i < countof(required); i++) {
        length = strlen(required[i]);
        for (j = 0; j < length; j++)
            buffer[j] = required[i][j];
        if (dtf_catalog_candidate(ctx, generator, s, buffer, length, &best_score))
            return -1;
    }
    enumeration = udatpg_openSkeletons(generator, &status);
    if (js_intl_icu_error(ctx, status, "date patterns"))
        goto fail;
    while ((skeleton = uenum_unext(enumeration, &length, &status))) {
        if (dtf_catalog_candidate(ctx, generator, s, skeleton, length, &best_score))
            goto fail;
    }
    uenum_close(enumeration);
    return js_intl_icu_error(ctx, status, "date patterns");
 fail:
    if (enumeration)
        uenum_close(enumeration);
    return -1;
}

static int dtf_proleptic_calendar(JSContext *ctx, UDateFormat *format)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *calendar = ucal_clone(udat_getCalendar(format), &status);
    const char *type;
    if (js_intl_icu_error(ctx, status, "date calendar"))
        return -1;
    type = ucal_getType(calendar, &status);
    if (U_SUCCESS(status) && (!strcmp(type, "gregorian") || !strcmp(type, "iso8601")))
        ucal_setGregorianChange(calendar, -8.64e15, &status);
    if (!js_intl_icu_error(ctx, status, "date calendar"))
        udat_setCalendar(format, calendar);
    ucal_close(calendar);
    return U_FAILURE(status) ? -1 : 0;
}

static int dtf_make_formatter(JSContext *ctx, JSIntlDateTimeFormat *s, int matcher,
                               int hour12)
{
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *generator = NULL;
    UDateFormat *style = NULL;
    UChar *style_pattern = NULL;
    int32_t length;
    int i, cycle;
    char *locale = NULL;
    size_t capacity;

    generator = udatpg_open(s->locale.icu_locale, &status);
    if (js_intl_icu_error(ctx, status, "date pattern generator"))
        goto fail;
#ifdef CONFIG_TEMPORAL
    if (!s->prepared) {
#endif
    cycle = udatpg_getDefaultHourCycle(generator, &status);
    if (js_intl_icu_error(ctx, status, "date hour cycle"))
        goto fail;
    s->hour_cycle = cycle; /* UDateFormatHourCycle uses h11,h12,h23,h24 order. */
    if (s->locale.values[1]) {
        for (i = 0; i < countof(dtf_cycles); i++)
            if (!strcmp(s->locale.values[1], dtf_cycles[i]))
                s->hour_cycle = i;
    }
    if (hour12 >= 0)
        s->hour_cycle = hour12 ? (cycle == 0 ? 0 : 1) : (cycle == 3 ? 3 : 2);
    capacity = strlen(s->locale.icu_locale) + 40;
    if (capacity > INT32_MAX) {
        JS_ThrowInternalError(ctx, "locale too long");
        goto fail;
    }
    locale = js_intl_alloc_char(ctx, capacity);
    if (!locale)
        goto fail;
    strcpy(locale, s->locale.icu_locale);
    uloc_setKeywordValue("hours", dtf_cycles[s->hour_cycle], locale, capacity, &status);
    if (js_intl_icu_error(ctx, status, "date locale hour cycle"))
        goto fail;
    js_free(ctx, s->locale.icu_locale);
    s->locale.icu_locale = locale;
    locale = NULL;
    udatpg_close(generator);
    generator = udatpg_open(s->locale.icu_locale, &status);
    if (js_intl_icu_error(ctx, status, "date hour cycle generator"))
        goto fail;

#ifdef CONFIG_TEMPORAL
    }
#endif

    if (s->date_style >= 0 || s->time_style >= 0) {
        style = udat_open(s->time_style < 0 ? UDAT_NONE : (UDateFormatStyle)s->time_style,
                         s->date_style < 0 ? UDAT_NONE : (UDateFormatStyle)s->date_style,
                         s->locale.icu_locale, s->icu_zone, s->zone_length,
                         NULL, 0, &status);
        if (js_intl_icu_error(ctx, status, "date style"))
            goto fail;
        length = udat_toPattern(style, FALSE, NULL, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR && js_intl_icu_error(ctx, status, "date style pattern"))
            goto fail;
        status = U_ZERO_ERROR;
        style_pattern = js_intl_alloc_uchar(ctx, length);
        if (!style_pattern)
            goto fail;
        length = udat_toPattern(style, FALSE, style_pattern, length + 1, &status);
        if (js_intl_icu_error(ctx, status, "date style pattern"))
            goto fail;
        /* ICU style locale hours are preserved in the stored pattern. */
        s->pattern = style_pattern;
        style_pattern = NULL;
        s->pattern_length = length;
    } else {
        if (dtf_skeleton(ctx, s))
            goto fail;
        if (matcher == 0) {
            if (dtf_basic_pattern(ctx, generator, s))
                goto fail;
        } else if (dtf_best_pattern(ctx, generator, s->skeleton, s->skeleton_length,
                                    &s->pattern, &s->pattern_length)) {
            goto fail;
        }
    }
    if (dtf_pattern_fields(s->pattern, s->pattern_length, s->fields)) {
        JS_ThrowInternalError(ctx, "unsupported ICU date pattern field");
        goto fail;
    }
    js_free(ctx, s->skeleton);
    s->skeleton = NULL;
    length = udatpg_getSkeleton(generator, s->pattern, s->pattern_length, NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && js_intl_icu_error(ctx, status, "date range skeleton"))
        goto fail;
    status = U_ZERO_ERROR;
    s->skeleton = js_intl_alloc_uchar(ctx, length);
    if (!s->skeleton)
        goto fail;
    s->skeleton_length = udatpg_getSkeleton(generator, s->pattern, s->pattern_length,
                                            s->skeleton, length + 1, &status);
    if (js_intl_icu_error(ctx, status, "date range skeleton"))
        goto fail;
    s->format = udat_open(UDAT_PATTERN, UDAT_PATTERN, s->locale.icu_locale,
                          s->icu_zone, s->zone_length, s->pattern,
                          s->pattern_length, &status);
    if (js_intl_icu_error(ctx, status, "date formatter") ||
        dtf_proleptic_calendar(ctx, s->format))
        goto fail;
    s->range = udtitvfmt_open(s->locale.icu_locale, s->skeleton, s->skeleton_length,
                             s->icu_zone, s->zone_length, &status);
    if (js_intl_icu_error(ctx, status, "date interval formatter"))
        goto fail;
    if (style)
        udat_close(style);
    udatpg_close(generator);
    return 0;
 fail:
    if (generator)
        udatpg_close(generator);
    if (style)
        udat_close(style);
    js_free(ctx, style_pattern);
    js_free(ctx, locale);
    return -1;
}

#ifdef CONFIG_TEMPORAL
enum { DTF_TEMP_DATE, DTF_TEMP_YEAR_MONTH, DTF_TEMP_MONTH_DAY,
       DTF_TEMP_TIME, DTF_TEMP_DATETIME, DTF_TEMP_INSTANT,
       DTF_TEMP_ZONED };

static int dtf_temporal_kind(JSValueConst value)
{
    static const JSClassID classes[] = {
        JS_CLASS_TEMPORAL_PLAIN_DATE, JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH,
        JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY, JS_CLASS_TEMPORAL_PLAIN_TIME,
        JS_CLASS_TEMPORAL_PLAIN_DATE_TIME, JS_CLASS_TEMPORAL_INSTANT,
        JS_CLASS_TEMPORAL_ZONED_DATE_TIME
    };
    int i;
    for (i = 0; i < countof(classes); i++)
        if (JS_GetOpaque(value, classes[i]))
            return i;
    return -1;
}

static int dtf_temporal_formats(JSContext *ctx, JSIntlDateTimeFormat *s)
{
    static const UChar utc[] = { 'U', 'T', 'C', 0 };
    unsigned masks[] = {
        (1U << DTF_WEEKDAY) | (1U << DTF_YEAR) | (1U << DTF_MONTH) |
        (1U << DTF_DAY),
        (1U << DTF_YEAR) | (1U << DTF_MONTH),
        (1U << DTF_MONTH) | (1U << DTF_DAY),
        (1U << DTF_DAY_PERIOD) | (1U << DTF_HOUR) | (1U << DTF_MINUTE) |
        (1U << DTF_SECOND) | (1U << DTF_FRACTION),
        0, 0
    };
    int kind, field, any = 0;
    BOOL styles = s->date_style >= 0 || s->time_style >= 0;
    for (field = 0; field < DTF_FIELD_COUNT; field++)
        if (field != DTF_ERA && field != DTF_ZONE_NAME &&
            s->requested_fields[field] >= 0)
            any = 1;
    masks[DTF_TEMP_DATETIME] = masks[DTF_TEMP_DATE] | masks[DTF_TEMP_TIME];
    masks[DTF_TEMP_INSTANT] = (1U << DTF_FIELD_COUNT) - 1;
    for (kind = 0; kind < 6; kind++) {
        JSIntlDateTimeFormat *t;
        int present = 0;
        if (styles && ((kind <= DTF_TEMP_MONTH_DAY && s->date_style < 0) ||
                       (kind == DTF_TEMP_TIME && s->time_style < 0)))
            continue;
        for (field = 0; field < DTF_FIELD_COUNT; field++)
            if ((masks[kind] & (1U << field)) &&
                field != DTF_ERA && field != DTF_ZONE_NAME &&
                s->requested_fields[field] >= 0)
                present = 1;
        if (!styles && kind != DTF_TEMP_INSTANT && any && !present)
            continue;
        t = js_mallocz(ctx, sizeof(*t));
        if (!t)
            return -1;
        s->temporal[kind] = t;
        /* Immutable locale/zone storage is borrowed from the parent. */
        t->locale = s->locale;
        t->time_zone = s->time_zone;
        t->hour_cycle = s->hour_cycle;
        t->prepared = 1;
        t->is_plain = kind != DTF_TEMP_INSTANT;
        t->icu_zone = t->is_plain ? (UChar *)utc : s->icu_zone;
        t->zone_length = t->is_plain ? 3 : s->zone_length;
        t->date_style = t->time_style = -1;
        t->bound_format = JS_UNDEFINED;
        for (field = 0; field < DTF_FIELD_COUNT; field++) {
            BOOL keep = (masks[kind] & (1U << field)) != 0;
            if (field == DTF_ERA &&
                (kind == DTF_TEMP_DATE || kind == DTF_TEMP_YEAR_MONTH ||
                 kind == DTF_TEMP_DATETIME))
                keep = TRUE;
            t->fields[field] = keep ? (styles ? s->fields[field] :
                                      s->requested_fields[field]) : -1;
        }
        if (styles && kind == DTF_TEMP_INSTANT) {
            t->date_style = s->date_style;
            t->time_style = s->time_style;
        } else if (!styles && !present) {
            if (kind != DTF_TEMP_TIME) {
                if (kind != DTF_TEMP_MONTH_DAY)
                    t->fields[DTF_YEAR] = DTF_NUMERIC;
                t->fields[DTF_MONTH] = DTF_NUMERIC;
                if (kind != DTF_TEMP_YEAR_MONTH)
                    t->fields[DTF_DAY] = DTF_NUMERIC;
            }
            if (kind >= DTF_TEMP_TIME) {
                t->fields[DTF_HOUR] = t->fields[DTF_MINUTE] =
                t->fields[DTF_SECOND] = DTF_NUMERIC;
            }
            if (kind == DTF_TEMP_INSTANT && s->zoned_locale &&
                t->fields[DTF_ZONE_NAME] < 0)
                t->fields[DTF_ZONE_NAME] = 0;
        }
        if (dtf_make_formatter(ctx, t, s->matcher, -1))
            return -1;
    }
    return 0;
}

static int dtf_temporal_argument(JSContext *ctx, JSIntlDateTimeFormat **format,
                                  JSValueConst value, double *time)
{
    JSIntlDateTimeFormat *s = *format;
    int kind = dtf_temporal_kind(value);
    QJSTemporalISODateTime datetime;
    QJSTemporalEpochNs epoch, quotient;
    QJSTemporalCalendar calendar = QJS_TEMPORAL_CAL_ISO8601;
    uint64_t remainder;
    int64_t milliseconds;
    if (kind < 0)
        return 0;
    if (kind == DTF_TEMP_ZONED) {
        JS_ThrowTypeError(ctx, "DateTimeFormat cannot format ZonedDateTime");
        return -1;
    }
    if (kind == DTF_TEMP_INSTANT) {
        epoch = *(JSTemporalInstantData *)JS_GetOpaque(value,
                                                     JS_CLASS_TEMPORAL_INSTANT);
    } else {
        datetime.time = (QJSTemporalISOTime){ 12, 0, 0, 0, 0, 0 };
        if (kind == DTF_TEMP_DATETIME) {
            JSTemporalPlainDateTimeData *data = JS_GetOpaque(value,
                                               JS_CLASS_TEMPORAL_PLAIN_DATE_TIME);
            datetime = data->datetime;
            calendar = data->calendar;
        } else if (kind == DTF_TEMP_TIME) {
            datetime.date = (QJSTemporalISODate){ 1970, 1, 1 };
            datetime.time = *(JSTemporalPlainTimeData *)JS_GetOpaque(value,
                                                   JS_CLASS_TEMPORAL_PLAIN_TIME);
        } else {
            JSClassID id = kind == DTF_TEMP_DATE ? JS_CLASS_TEMPORAL_PLAIN_DATE :
                kind == DTF_TEMP_YEAR_MONTH ? JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH :
                                             JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY;
            JSTemporalPlainDateData *data = JS_GetOpaque(value, id);
            datetime.date = data->date;
            calendar = data->calendar;
        }
        if (kind != DTF_TEMP_TIME &&
            !((kind == DTF_TEMP_DATE || kind == DTF_TEMP_DATETIME) &&
              calendar == QJS_TEMPORAL_CAL_ISO8601) &&
            strcmp(qjs_temporal_calendar_identifier(calendar),
                    s->locale.values[0])) {
            JS_ThrowRangeError(ctx, "Temporal and formatter calendars differ");
            return -1;
        }
        if (qjs_temporal_iso_datetime_to_epoch_ns(&epoch, datetime)) {
            JS_ThrowRangeError(ctx, "invalid Temporal civil formatting value");
            return -1;
        }
    }
    if (!s->temporal[kind]) {
        JS_ThrowTypeError(ctx, "formatter options do not apply to Temporal type");
        return -1;
    }
    if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, epoch, 1000000) ||
        qjs_temporal_epoch_ns_to_int64(&milliseconds, quotient)) {
        JS_ThrowRangeError(ctx, "Temporal formatting value outside range");
        return -1;
    }
    *time = (double)milliseconds;
    *format = s->temporal[kind];
    return 1;
}
#endif

static JSValue dtf_create(JSContext *ctx, JSValueConst new_target,
                           JSValueConst locales, JSValueConst options,
                           int required, int defaults,
                           JSValueConst override_time_zone)
{
    JSValue object, opts = JS_UNDEFINED;
    JSIntlDateTimeFormat *s;
    JSIntlLocaleList requested = { 0 };
    JSIntlResolutionKey keys[3];
    char *calendar = NULL, *numbers = NULL;
    int matcher, hour12, cycle, format_matcher;
    BOOL explicit_fields;
    object = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (JS_IsException(object))
        return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    s->bound_format = JS_UNDEFINED;
    s->date_style = s->time_style = -1;
    JS_SetOpaque(object, s);
    if (js_intl_canonicalize_locale_list(ctx, locales, &requested))
        goto fail;
    opts = js_intl_coerce_options(ctx, options);
    if (JS_IsException(opts))
        goto fail;
    if (js_intl_get_string_option(ctx, opts, "localeMatcher", dtf_matchers,
                                  countof(dtf_matchers), 1, &matcher) ||
        js_intl_get_string_option_alloc(ctx, opts, "calendar", &calendar) ||
        dtf_unicode_type(ctx, calendar) ||
        js_intl_get_string_option_alloc(ctx, opts, "numberingSystem", &numbers) ||
        dtf_unicode_type(ctx, numbers) ||
        js_intl_get_bool_option(ctx, opts, "hour12", -1, &hour12) ||
        js_intl_get_string_option(ctx, opts, "hourCycle", dtf_cycles,
                                  countof(dtf_cycles), -1, &cycle))
        goto fail;
    keys[0] = (JSIntlResolutionKey){ "ca", calendar, FALSE };
    keys[1] = (JSIntlResolutionKey){ "hc", (hour12 >= 0 || cycle < 0) ? NULL : dtf_cycles[cycle], hour12 >= 0 };
    keys[2] = (JSIntlResolutionKey){ "nu", numbers, FALSE };
    if (js_intl_resolve_locale(ctx, JS_INTL_DATE_TIME_FORMAT, &requested,
                               dtf_matchers[matcher], keys, 3, &s->locale))
        goto fail;
    if (!strcmp(s->locale.values[0], "islamic")) {
        char *replacement, *icu_locale;
        size_t capacity = strlen(s->locale.icu_locale) + 40;
        UErrorCode status = U_ZERO_ERROR;
        replacement = js_intl_strdup(ctx, "islamic-tbla");
        if (!replacement)
            goto fail;
        icu_locale = js_intl_alloc_char(ctx, capacity);
        if (!icu_locale) {
            js_free(ctx, replacement);
            goto fail;
        }
        strcpy(icu_locale, s->locale.icu_locale);
        uloc_setKeywordValue("calendar", "islamic-tbla", icu_locale,
                              capacity, &status);
        if (js_intl_icu_error(ctx, status, "calendar fallback")) {
            js_free(ctx, replacement);
            js_free(ctx, icu_locale);
            goto fail;
        }
        js_free(ctx, s->locale.values[0]);
        js_free(ctx, s->locale.icu_locale);
        s->locale.values[0] = replacement;
        s->locale.icu_locale = icu_locale;
    }
    if (dtf_time_zone(ctx, opts, s, override_time_zone) ||
        dtf_read_fields(ctx, opts, s->fields, &explicit_fields) ||
        js_intl_get_string_option(ctx, opts, "formatMatcher", dtf_format_matchers,
                                  countof(dtf_format_matchers), 1, &format_matcher) ||
        js_intl_get_string_option(ctx, opts, "dateStyle", dtf_styles,
                                  countof(dtf_styles), -1, &s->date_style) ||
        js_intl_get_string_option(ctx, opts, "timeStyle", dtf_styles,
                                  countof(dtf_styles), -1, &s->time_style))
        goto fail;
#ifdef CONFIG_TEMPORAL
    memcpy(s->requested_fields, s->fields, sizeof(s->fields));
    s->matcher = format_matcher;
    s->zoned_locale = !JS_IsUndefined(override_time_zone);
#endif
    if (s->date_style >= 0 || s->time_style >= 0) {
        if (explicit_fields || (required == JS_INTL_DTF_DATE && s->time_style >= 0) ||
            (required == JS_INTL_DTF_TIME && s->date_style >= 0)) {
            JS_ThrowTypeError(ctx, "conflicting date/time style and component options");
            goto fail;
        }
    } else {
        dtf_defaults(s->fields, required, defaults);
    }
    if (dtf_make_formatter(ctx, s, format_matcher, hour12))
        goto fail;
#ifdef CONFIG_TEMPORAL
    if (dtf_temporal_formats(ctx, s))
        goto fail;
#endif
    JS_FreeValue(ctx, opts);
    js_intl_locale_list_free(ctx, &requested);
    js_free(ctx, calendar);
    js_free(ctx, numbers);
    return object;
 fail:
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, opts);
    js_intl_locale_list_free(ctx, &requested);
    js_free(ctx, calendar);
    js_free(ctx, numbers);
    return JS_EXCEPTION;
}

static JSValue dtf_constructor(JSContext *ctx, JSValueConst this_val,
                                JSValueConst new_target, int argc,
                                JSValueConst *argv)
{
    JSValue value;
    int instance;
    value = dtf_create(ctx,
        JS_IsUndefined(new_target) ? js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT) : new_target,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED, JS_INTL_DTF_ANY, JS_INTL_DTF_DATE, JS_UNDEFINED);
    if (JS_IsException(value))
        return value;
#ifdef CONFIG_INTL_LEGACY
    if (JS_IsUndefined(new_target)) {
        instance = JS_OrdinaryIsInstanceOf(ctx, this_val,
                       js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT));
        if (instance < 0)
            goto fail;
        if (instance) {
            JSAtom symbol = JS_ValueToAtom(ctx, js_intl_fallback_symbol(ctx));
            if (symbol == JS_ATOM_NULL)
                goto fail;
            instance = JS_DefinePropertyValue(ctx, this_val, symbol,
                           JS_DupValue(ctx, value), JS_PROP_THROW);
            JS_FreeAtom(ctx, symbol);
            if (instance < 0)
                goto fail;
            JS_FreeValue(ctx, value);
            return JS_DupValue(ctx, this_val);
        }
    }
#else
    (void)this_val;
    (void)instance;
#endif
    return value;
#ifdef CONFIG_INTL_LEGACY
 fail:
    JS_FreeValue(ctx, value);
    return JS_EXCEPTION;
#endif
}

static JSValue dtf_unwrap(JSContext *ctx, JSValueConst value)
{
#ifdef CONFIG_INTL_LEGACY
    int instance;
    if (!JS_IsObject(value))
        return JS_ThrowTypeError(ctx, "not an Intl.DateTimeFormat object");
    if (!JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT)) {
        instance = JS_OrdinaryIsInstanceOf(ctx, value,
                       js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT));
        if (instance < 0)
            return JS_EXCEPTION;
        if (instance) {
            JSAtom symbol = JS_ValueToAtom(ctx, js_intl_fallback_symbol(ctx));
            JSValue result;
            if (symbol == JS_ATOM_NULL)
                return JS_EXCEPTION;
            result = JS_GetProperty(ctx, value, symbol);
            JS_FreeAtom(ctx, symbol);
            return result;
        }
    }
#endif
    return JS_DupValue(ctx, value);
}

static JSValue dtf_resolved_options(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue value, result;
    JSIntlDateTimeFormat *s;
    int i;
    value = dtf_unwrap(ctx, this_val);
    if (JS_IsException(value))
        return value;
    s = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (!s) {
        JS_FreeValue(ctx, value);
        return JS_EXCEPTION;
    }
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto done;
    if (js_intl_define_string(ctx, result, "locale", s->locale.locale) ||
        js_intl_define_string(ctx, result, "calendar", s->locale.values[0]) ||
        js_intl_define_string(ctx, result, "numberingSystem", s->locale.values[2]) ||
        js_intl_define_string(ctx, result, "timeZone", s->time_zone))
        goto fail;
    if (s->fields[DTF_HOUR] >= 0 &&
        (js_intl_define_string(ctx, result, "hourCycle", dtf_cycles[s->hour_cycle]) ||
         js_intl_define_bool(ctx, result, "hour12", s->hour_cycle < 2)))
        goto fail;
    if (s->date_style < 0 && s->time_style < 0) {
        for (i = 0; i < DTF_FIELD_COUNT; i++) {
            if (s->fields[i] < 0)
                continue;
            if (i == DTF_FRACTION) {
                if (js_intl_define_int(ctx, result, dtf_names[i], s->fields[i]))
                    goto fail;
            } else if (js_intl_define_string(ctx, result, dtf_names[i],
                        i == DTF_ZONE_NAME ? dtf_zones[s->fields[i]] : dtf_widths[s->fields[i]])) {
                goto fail;
            }
        }
    }
    if ((s->date_style >= 0 && js_intl_define_string(ctx, result, "dateStyle", dtf_styles[s->date_style])) ||
        (s->time_style >= 0 && js_intl_define_string(ctx, result, "timeStyle", dtf_styles[s->time_style])))
        goto fail;
 done:
    JS_FreeValue(ctx, value);
    return result;
 fail:
    JS_FreeValue(ctx, result);
    result = JS_EXCEPTION;
    goto done;
}

static int dtf_clip(JSContext *ctx, double *time)
{
    if (!isfinite(*time) || fabs(*time) > 8.64e15) {
        JS_ThrowRangeError(ctx, "invalid time value");
        return -1;
    }
    *time = trunc(*time) + 0.0;
    return 0;
}

static int dtf_time_argument(JSContext *ctx, JSValueConst value, double *time)
{
    if (JS_IsUndefined(value)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        *time = (double)tv.tv_sec * 1000 + tv.tv_usec / 1000;
        return 0;
    }
    return JS_ToFloat64(ctx, time, value);
}

/* Leaf ICU date fields, including non-Gregorian year names and related years. */
static const char *dtf_part_type(int field)
{
    switch (field) {
    case UDAT_ERA_FIELD: return "era";
    case UDAT_YEAR_FIELD: case UDAT_EXTENDED_YEAR_FIELD: return "year";
    case UDAT_YEAR_NAME_FIELD: return "yearName";
    case UDAT_RELATED_YEAR_FIELD: return "relatedYear";
    case UDAT_MONTH_FIELD: case UDAT_STANDALONE_MONTH_FIELD: return "month";
    case UDAT_DATE_FIELD: return "day";
    case UDAT_DAY_OF_WEEK_FIELD: case UDAT_DOW_LOCAL_FIELD: case UDAT_STANDALONE_DAY_FIELD: return "weekday";
    case UDAT_HOUR_OF_DAY1_FIELD: case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD: case UDAT_HOUR0_FIELD: return "hour";
    case UDAT_MINUTE_FIELD: return "minute";
    case UDAT_SECOND_FIELD: return "second";
    case UDAT_FRACTIONAL_SECOND_FIELD: return "fractionalSecond";
    case UDAT_AM_PM_FIELD: case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
    case UDAT_FLEXIBLE_DAY_PERIOD_FIELD: return "dayPeriod";
    case UDAT_TIMEZONE_FIELD: case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD: case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD: case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD: return "timeZoneName";
    default: return "literal";
    }
}

static JSValue dtf_parts(JSContext *ctx, const UChar *text, int32_t length,
                          const int8_t *fields, const int8_t *sources)
{
    JSValue result, source = JS_UNDEFINED;
    int32_t begin = 0, end;
    uint32_t index = 0;
    const char *type;
    static const char *const source_names[] = { "shared", "startRange", "endRange" };
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        return result;
    while (begin < length) {
        type = dtf_part_type(fields[begin]);
        for (end = begin + 1; end < length; end++) {
            if (strcmp(type, dtf_part_type(fields[end])) ||
                (sources && sources[end] != sources[begin]))
                break;
        }
        if (sources) {
            source = JS_NewString(ctx, source_names[sources[begin]]);
            if (JS_IsException(source))
                goto fail;
        }
        if (js_intl_add_part_uchar(ctx, result, index++, type, text + begin,
                                    end - begin, sources ? "source" : NULL, source))
            goto fail;
        JS_FreeValue(ctx, source);
        source = JS_UNDEFINED;
        begin = end;
    }
    return result;
 fail:
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, source);
    return JS_EXCEPTION;
}

static JSValue dtf_format(JSContext *ctx, JSIntlDateTimeFormat *s,
                           double time, int parts)
{
    UErrorCode status = U_ZERO_ERROR;
    UChar *text = NULL;
    UFieldPositionIterator *iterator = NULL;
    int8_t *fields = NULL, *shared = NULL;
    int32_t length, field, begin, end, i;
    JSValue result = JS_EXCEPTION;
#ifdef CONFIG_TEMPORAL
    if (!s->is_plain && dtf_clip(ctx, &time))
#else
    if (dtf_clip(ctx, &time))
#endif
        return JS_EXCEPTION;
    length = udat_formatForFields(s->format, time, NULL, 0, NULL, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && js_intl_icu_error(ctx, status, "date formatting"))
        goto done;
    text = js_intl_alloc_uchar(ctx, length);
    if (!text)
        goto done;
    status = U_ZERO_ERROR;
    if (parts) {
        iterator = ufieldpositer_open(&status);
        if (js_intl_icu_error(ctx, status, "date parts"))
            goto done;
    }
    length = udat_formatForFields(s->format, time, text, length + 1, iterator, &status);
    if (js_intl_icu_error(ctx, status, "date formatting"))
        goto done;
    if (!parts) {
        result = js_intl_from_uchar(ctx, text, length);
        goto done;
    }
    fields = js_malloc(ctx, max_int(length, 1));
    if (!fields)
        goto done;
    memset(fields, -1, length);
    while ((field = ufieldpositer_next(iterator, &begin, &end)) >= 0) {
        if (begin < 0 || end > length || field > INT8_MAX) {
            JS_ThrowInternalError(ctx, "invalid ICU date field position");
            goto done;
        }
        for (i = begin; i < end; i++)
            fields[i] = field;
    }
    if (parts == 2) {
        shared = js_mallocz(ctx, max_int(length, 1));
        if (!shared)
            goto done;
    }
    result = dtf_parts(ctx, text, length, fields, shared);
 done:
    if (iterator)
        ufieldpositer_close(iterator);
    js_free(ctx, text);
    js_free(ctx, fields);
    js_free(ctx, shared);
    return result;
}

static JSValue dtf_bound_format(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic,
                                 JSValue *data)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque(data[0], JS_CLASS_INTL_DATE_TIME_FORMAT);
    double time;
#ifdef CONFIG_TEMPORAL
    int temporal = dtf_temporal_argument(ctx, &s,
                          argc > 0 ? argv[0] : JS_UNDEFINED, &time);
    if (temporal < 0)
        return JS_EXCEPTION;
    if (!temporal &&
#else
    if (
#endif
        dtf_time_argument(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, &time))
        return JS_EXCEPTION;
    return dtf_format(ctx, s, time, FALSE);
}

static JSValue dtf_get_format(JSContext *ctx, JSValueConst this_val)
{
    JSValue value, result;
    JSIntlDateTimeFormat *s;
    value = dtf_unwrap(ctx, this_val);
    if (JS_IsException(value))
        return value;
    s = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (!s) {
        JS_FreeValue(ctx, value);
        return JS_EXCEPTION;
    }
    if (JS_IsUndefined(s->bound_format)) {
        JSValueConst capture = value;
        result = js_intl_new_c_function_data(ctx, dtf_bound_format, 1, 0, 1, &capture);
        if (JS_IsException(result)) {
            JS_FreeValue(ctx, value);
            return result;
        }
        s->bound_format = result;
    }
    result = JS_DupValue(ctx, s->bound_format);
    JS_FreeValue(ctx, value);
    return result;
}

static JSValue dtf_format_to_parts(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DATE_TIME_FORMAT);
    double time;
    if (!s)
        return JS_EXCEPTION;
#ifdef CONFIG_TEMPORAL
    int temporal = dtf_temporal_argument(ctx, &s,
                          argc > 0 ? argv[0] : JS_UNDEFINED, &time);
    if (temporal < 0)
        return JS_EXCEPTION;
    if (!temporal &&
#else
    if (
#endif
        dtf_time_argument(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, &time))
        return JS_EXCEPTION;
    return dtf_format(ctx, s, time, TRUE);
}

static JSValue dtf_format_range(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int parts)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DATE_TIME_FORMAT);
    UErrorCode status = U_ZERO_ERROR;
    UFormattedDateInterval *interval = NULL;
    UConstrainedFieldPosition *position = NULL;
    const UFormattedValue *formatted;
    const UChar *text;
    UCalendar *first = NULL, *last = NULL;
    int8_t *fields = NULL, *sources = NULL;
    int32_t length, begin, end, category, field, i;
    double from, to;
    JSValue result = JS_EXCEPTION;
    if (!s)
        return JS_EXCEPTION;
    if (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_ThrowTypeError(ctx, "date range requires two endpoints");
#ifdef CONFIG_TEMPORAL
    {
        int first_kind = dtf_temporal_kind(argv[0]);
        int last_kind = dtf_temporal_kind(argv[1]);
        JSIntlDateTimeFormat *last_format = s;
        if (first_kind < 0 && JS_ToFloat64(ctx, &from, argv[0]))
            return JS_EXCEPTION;
        if (last_kind < 0 && JS_ToFloat64(ctx, &to, argv[1]))
            return JS_EXCEPTION;
        if (first_kind >= 0 || last_kind >= 0) {
            if (first_kind != last_kind)
                return JS_ThrowTypeError(ctx, "Temporal range types differ");
            if (dtf_temporal_argument(ctx, &s, argv[0], &from) < 0 ||
                dtf_temporal_argument(ctx, &last_format, argv[1], &to) < 0)
                return JS_EXCEPTION;
        } else if (dtf_clip(ctx, &from) || dtf_clip(ctx, &to)) {
            return JS_EXCEPTION;
        }
    }
#else
    if (JS_ToFloat64(ctx, &from, argv[0]) || JS_ToFloat64(ctx, &to, argv[1]) ||
        dtf_clip(ctx, &from) || dtf_clip(ctx, &to))
        return JS_EXCEPTION;
#endif
    if (from == to)
        return dtf_format(ctx, s, from, parts ? 2 : 0);
    interval = udtitvfmt_openResult(&status);
    first = ucal_clone(udat_getCalendar(s->format), &status);
    last = ucal_clone(udat_getCalendar(s->format), &status);
    if (js_intl_icu_error(ctx, status, "date interval calendars"))
        goto done;
    ucal_setMillis(first, from, &status);
    ucal_setMillis(last, to, &status);
    udtitvfmt_formatCalendarToResult(s->range, first, last, interval, &status);
    formatted = udtitvfmt_resultAsValue(interval, &status);
    text = ufmtval_getString(formatted, &length, &status);
    if (js_intl_icu_error(ctx, status, "date interval formatting"))
        goto done;
    if (!parts) {
        result = js_intl_from_uchar(ctx, text, length);
        goto done;
    }
    fields = js_malloc(ctx, max_int(length, 1));
    sources = js_mallocz(ctx, max_int(length, 1));
    if (!fields || !sources)
        goto done;
    memset(fields, -1, length);
    position = ucfpos_open(&status);
    if (js_intl_icu_error(ctx, status, "date interval positions"))
        goto done;
    while (ufmtval_nextPosition(formatted, position, &status)) {
        category = ucfpos_getCategory(position, &status);
        field = ucfpos_getField(position, &status);
        ucfpos_getIndexes(position, &begin, &end, &status);
        if (begin < 0 || end > length) {
            JS_ThrowInternalError(ctx, "invalid ICU date interval field position");
            goto done;
        }
        if (category == UFIELD_CATEGORY_DATE) {
            if (field > INT8_MAX) {
                JS_ThrowInternalError(ctx, "invalid ICU date interval field");
                goto done;
            }
            for (i = begin; i < end; i++)
                fields[i] = field;
        } else if (category == UFIELD_CATEGORY_DATE_INTERVAL_SPAN) {
            if (field < 0 || field > 1) {
                JS_ThrowInternalError(ctx, "invalid ICU date interval source");
                goto done;
            }
            for (i = begin; i < end; i++)
                sources[i] = field + 1;
        }
    }
    if (js_intl_icu_error(ctx, status, "date interval positions"))
        goto done;
    result = dtf_parts(ctx, text, length, fields, sources);
 done:
    if (position)
        ucfpos_close(position);
    if (interval)
        udtitvfmt_closeResult(interval);
    if (first)
        ucal_close(first);
    if (last)
        ucal_close(last);
    js_free(ctx, fields);
    js_free(ctx, sources);
    return result;
}

JSValue js_intl_date_format(JSContext *ctx, double time,
                            JSValueConst locales, JSValueConst options,
                            int required, int defaults)
{
    JSValue object, result;
    JSIntlDateTimeFormat *s;
    if (js_intl_ensure_service(ctx, JS_INTL_DATE_TIME_FORMAT))
        return JS_EXCEPTION;
    object = dtf_create(ctx, js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT),
                        locales, options, required, defaults, JS_UNDEFINED);
    if (JS_IsException(object))
        return object;
    s = JS_GetOpaque(object, JS_CLASS_INTL_DATE_TIME_FORMAT);
    result = dtf_format(ctx, s, time, FALSE);
    JS_FreeValue(ctx, object);
    return result;
}

#ifdef CONFIG_TEMPORAL
/* Native DurationFormat owner supplies this private branded bridge. */
JSValue js_intl_temporal_duration_to_locale_string(
    JSContext *ctx, JSValueConst duration, JSValueConst locales,
    JSValueConst options);

JSValue js_intl_temporal_to_locale_string(JSContext *ctx,
                                         JSValueConst this_val,
                                         JSValueConst locales,
                                         JSValueConst options)
{
    JSValue formatter, result, instant = JS_UNDEFINED;
    JSValueConst formatted = this_val;
    JSIntlDateTimeFormat *s;
    JSTemporalZonedDateTimeData *zoned = NULL;
    int kind = dtf_temporal_kind(this_val), required, defaults;
    double time;
    if (JS_GetOpaque(this_val, JS_CLASS_TEMPORAL_DURATION))
        return js_intl_temporal_duration_to_locale_string(ctx, this_val,
                                                           locales, options);
    if (kind < 0)
        return JS_ThrowTypeError(ctx, "unrecognized Temporal formatting brand");
    if (kind == DTF_TEMP_ZONED)
        zoned = JS_GetOpaque(this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    required = kind <= DTF_TEMP_MONTH_DAY ? JS_INTL_DTF_DATE :
               kind == DTF_TEMP_TIME ? JS_INTL_DTF_TIME : JS_INTL_DTF_ANY;
    defaults = required == JS_INTL_DTF_ANY ? JS_INTL_DTF_ALL : required;
    if (js_intl_ensure_service(ctx, JS_INTL_DATE_TIME_FORMAT))
        return JS_EXCEPTION;
    formatter = dtf_create(ctx,
                    js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT),
                    locales, options, required, defaults,
                    zoned ? zoned->time_zone.identifier : JS_UNDEFINED);
    if (JS_IsException(formatter))
        return formatter;
    s = JS_GetOpaque(formatter, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (zoned) {
        if (zoned->calendar != QJS_TEMPORAL_CAL_ISO8601 &&
            strcmp(qjs_temporal_calendar_identifier(zoned->calendar),
                    s->locale.values[0])) {
            result = JS_ThrowRangeError(ctx,
                                       "ZonedDateTime calendars differ");
            goto done;
        }
        instant = js_temporal_create_instant(ctx, JS_UNDEFINED,
                                             zoned->epoch_nanoseconds);
        if (JS_IsException(instant)) {
            result = JS_EXCEPTION;
            goto done;
        }
        formatted = instant;
    }
    if (dtf_temporal_argument(ctx, &s, formatted, &time) < 0)
        result = JS_EXCEPTION;
    else
        result = dtf_format(ctx, s, time, FALSE);
 done:
    JS_FreeValue(ctx, instant);
    JS_FreeValue(ctx, formatter);
    return result;
}
#endif

static JSValue dtf_supported_locales(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_DATE_TIME_FORMAT,
        argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSClassDef dtf_class = {
    "Intl.DateTimeFormat", .finalizer = dtf_finalizer, .gc_mark = dtf_mark
};
static const JSCFunctionListEntry dtf_constructor_functions[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, dtf_supported_locales),
};
static const JSCFunctionListEntry dtf_prototype_functions[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, dtf_resolved_options),
    JS_CGETSET_DEF("format", dtf_get_format, NULL),
    JS_CFUNC_DEF("formatToParts", 1, dtf_format_to_parts),
    JS_CFUNC_MAGIC_DEF("formatRange", 2, dtf_format_range, FALSE),
    JS_CFUNC_MAGIC_DEF("formatRangeToParts", 2, dtf_format_range, TRUE),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DateTimeFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_date_time_format(JSContext *ctx, JSValueConst intl)
{
    JSCFunctionType function;
    function.constructor_or_func_receiver = dtf_constructor;
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_DATE_TIME_FORMAT,
        &dtf_class, "DateTimeFormat", function.generic, 0,
        JS_CFUNC_constructor_or_func_receiver,
        dtf_constructor_functions, countof(dtf_constructor_functions),
        dtf_prototype_functions, countof(dtf_prototype_functions));
}
#endif
