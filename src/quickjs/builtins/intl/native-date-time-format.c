/* Native Intl.DateTimeFormat; ECMA402 7ae78cf, reviewed 2026-10-09.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license. Source preparation only. */
#include "intl-internal.h"
#include "intl-text.h"
#include "locale-integration.h"
#include "native-date-timezone-contract.h"
#include "../../../intl/provider-native-date.h"
#ifdef CONFIG_TEMPORAL
#include "../temporal/temporal-internal.h"
#endif
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
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
    QJSIntlNativeDateBank *bank;
    QJSIntlNativeDateProviderEnvironment environment;
    QJSIntlAllocator allocator;
    int fields[DTF_FIELD_COUNT];
    int hour_cycle, date_style, time_style;
    JSValue bound_format;
} JSIntlDateTimeFormat;
static QJSIntlBytes dtf_bytes(const char *s)
{
    QJSIntlBytes b = { s, s ? strlen(s) : 0 }; return b;
}
static void dtf_destroy(JSRuntime *rt, JSIntlDateTimeFormat *s)
{
    int i;
    if (!s) return;
    qjs_intl_native_date_bank_close(s->bank);
    js_free_rt(rt, s->locale.locale); js_free_rt(rt, s->locale.data_locale);
    for (i = 0; i < s->locale.key_count; i++) js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s->time_zone); JS_FreeValueRT(rt, s->bound_format); js_free_rt(rt, s);
}
static void dtf_finalizer(JSRuntime *rt, JSValue value)
{
    dtf_destroy(rt, JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT));
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
    uint16_t *text = NULL;
    int32_t length = 0, i;
    char *ascii = NULL;
    const char *zone;
    int hours, minutes = 0;
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
        if (js_intl_to_utf16(ctx, value, &text, &length))
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
            const char *identifier = NULL, *primary = NULL;
            if (qjs_tz_resolve(ascii, length, &identifier, &primary) != QJS_TZ_OK) {
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
    if (s->time_zone[0] != '+' && s->time_zone[0] != '-' &&
        strcmp(s->time_zone, "UTC")) {
        const QJSTimeZone *snapshot;
        QJSTzProvider *provider = js_intl_native_time_zone_provider(ctx);
        int status;
        if (!provider) goto fail;
        status = qjs_tz_provider_open(provider, s->time_zone,
                                     strlen(s->time_zone), &snapshot);
        if (status != QJS_TZ_OK) {
            js_intl_native_error(ctx, status == QJS_TZ_MEMORY ? QJS_INTL_NO_MEMORY :
                status == QJS_TZ_ABSENT ? QJS_INTL_UNSUPPORTED : QJS_INTL_DATA_ERROR,
                "DateTimeFormat timeZone snapshot");
            goto fail;
        }
    }
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

/* Explicit field mappings: frontend/provider ordinals are private. */
static const unsigned int dtf_provider_fields[DTF_FIELD_COUNT] = {
    QJS_DATE_WEEKDAY, QJS_DATE_ERA, QJS_DATE_YEAR, QJS_DATE_MONTH, QJS_DATE_DAY,
    QJS_DATE_DAY_PERIOD, QJS_DATE_HOUR, QJS_DATE_MINUTE, QJS_DATE_SECOND,
    QJS_DATE_FRACTION, QJS_DATE_ZONE
};
static int dtf_provider_width(int field, int width)
{
    static const int widths[] = { QJS_DATE_TWO_DIGIT, QJS_DATE_NUMERIC,
        QJS_DATE_NARROW, QJS_DATE_SHORT, QJS_DATE_LONG };
    static const int zones[] = { QJS_DATE_ZONE_SHORT, QJS_DATE_ZONE_LONG,
        QJS_DATE_ZONE_SHORT_OFFSET, QJS_DATE_ZONE_LONG_OFFSET,
        QJS_DATE_ZONE_SHORT_GENERIC, QJS_DATE_ZONE_LONG_GENERIC };
    if (width < 0 || field == DTF_FRACTION) return width;
    return field == DTF_ZONE_NAME ? zones[width] : widths[width];
}
static int dtf_frontend_width(int field, int width)
{
    int i;
    if (width < 0 || field == DTF_FRACTION) return width;
    for (i = 0; i < (field == DTF_ZONE_NAME ? 6 : 5); i++)
        if (dtf_provider_width(field, i) == width) return i;
    return -2;
}
static QJSIntlDateRequired dtf_required(int required)
{
    switch (required) {
    case JS_INTL_DTF_DATE: return QJS_DATE_REQUIRE_DATE;
    case JS_INTL_DTF_TIME: return QJS_DATE_REQUIRE_TIME;
    default: return QJS_DATE_REQUIRE_ANY;
    }
}
static QJSIntlDateDefaults dtf_defaults(int defaults)
{
    switch (defaults) {
    case JS_INTL_DTF_DATE: return QJS_DATE_DEFAULT_DATE;
    case JS_INTL_DTF_TIME: return QJS_DATE_DEFAULT_TIME;
    default: return QJS_DATE_DEFAULT_ALL;
    }
}
static int dtf_open(JSContext *ctx, JSIntlDateTimeFormat *s, int required,
    int defaults, int matcher, int hour12, int zoned_locale)
{
    QJSIntlProvider *provider = js_intl_native_provider(ctx);
    QJSIntlDateBankOptions o; QJSIntlStatus status;
    QJSTzProvider *tz = NULL; unsigned int cycle; int i, resolved[QJS_DATE_FIELD_COUNT];
    static const unsigned int cycles[] = { QJS_DATE_H11, QJS_DATE_H12, QJS_DATE_H23, QJS_DATE_H24 };
    static const int styles[] = { QJS_DATE_FULL, QJS_DATE_STYLE_LONG, QJS_DATE_MEDIUM, QJS_DATE_STYLE_SHORT };
    if (!provider) return -1;
    if (hour12 >= 0) {
        status = qjs_intl_native_provider_date_preferred_hour_cycle(provider,
            dtf_bytes(s->locale.data_locale), &cycle);
        if (js_intl_native_error(ctx, status, "DateTimeFormat preferred hour cycle")) return -1;
        i = qjs_intl_native_date_hour_cycle(cycle, hour12);
        if (i < 0) return js_intl_native_error(ctx, QJS_INTL_DATA_ERROR, "hour cycle");
        cycle = (unsigned int)i;
    } else {
        for (i = 0; i < 4; i++) if (!strcmp(s->locale.values[1], dtf_cycles[i])) break;
        if (i == 4) return js_intl_native_error(ctx, QJS_INTL_DATA_ERROR, "resolved hour cycle");
        cycle = cycles[i];
    }
    for (i = 0; i < 4; i++) if (cycles[i] == cycle) break;
    if (i == 4) return js_intl_native_error(ctx, QJS_INTL_DATA_ERROR, "hour cycle ordinal");
    s->hour_cycle = i;
    memset(&o, 0, sizeof(o));
    o.requested.calendar = dtf_bytes(s->locale.values[0]);
    o.requested.time_zone = dtf_bytes(s->time_zone);
    o.requested.hour_cycle = cycle;
    o.requested.date_style = s->date_style < 0 ? -1 : styles[s->date_style];
    o.requested.time_style = s->time_style < 0 ? -1 : styles[s->time_style];
    o.requested.format_matcher = matcher ? QJS_DATE_BEST_FIT : QJS_DATE_BASIC;
    for (i = 0; i < DTF_FIELD_COUNT; i++)
        o.requested.fields[dtf_provider_fields[i]] = dtf_provider_width(i, s->fields[i]);
    o.number_required = dtf_required(required); o.number_defaults = dtf_defaults(defaults);
    o.instant_defaults = zoned_locale ? QJS_DATE_DEFAULT_ZONED_DATE_TIME : QJS_DATE_DEFAULT_ALL;
    if (s->time_zone[0] != '+' && s->time_zone[0] != '-' && strcmp(s->time_zone, "UTC")) {
        tz = js_intl_native_time_zone_provider(ctx); if (!tz) return -1;
    }
    s->allocator = *qjs_intl_native_provider_allocator(provider);
    status = qjs_intl_native_provider_date_open(provider, dtf_bytes(s->locale.data_locale),
        dtf_bytes(s->locale.values[2]), &o, tz, &s->environment, &s->bank);
    if (js_intl_native_error(ctx, status, "DateTimeFormat bank")) return -1;
    qjs_intl_native_date_resolved_fields(qjs_intl_native_date_bank_slot(s->bank,
        QJS_DATE_VALUE_NUMBER), resolved);
    for (i = 0; i < DTF_FIELD_COUNT; i++) {
        s->fields[i] = dtf_frontend_width(i, resolved[dtf_provider_fields[i]]);
        if (s->fields[i] == -2) return js_intl_native_error(ctx, QJS_INTL_DATA_ERROR, "resolved field");
    }
    return 0;
}
static JSValue dtf_create(JSContext *ctx, JSValueConst new_target,
                           JSValueConst locales, JSValueConst options,
                           int required, int defaults,
                           JSValueConst override_time_zone)
{
    JSValue object, opts = JS_UNDEFINED;
    JSIntlDateTimeFormat *s = NULL;
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
        char *replacement = js_intl_strdup(ctx, "islamic-tbla");
        if (!replacement) goto fail;
        js_free(ctx, s->locale.values[0]); s->locale.values[0] = replacement;
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
    if (s->date_style >= 0 || s->time_style >= 0) {
        if (explicit_fields || (required == JS_INTL_DTF_DATE && s->time_style >= 0) ||
            (required == JS_INTL_DTF_TIME && s->date_style >= 0)) {
            JS_ThrowTypeError(ctx, "conflicting date/time style and component options");
            goto fail;
        }
    }
    if (dtf_open(ctx, s, required, defaults, format_matcher, hour12,
                 !JS_IsUndefined(override_time_zone))) goto fail;
    /* Publish initialized state only after every immutable slot is prepared. */
    JS_SetOpaque(object, s); s = NULL;
    JS_FreeValue(ctx, opts);
    js_intl_locale_list_free(ctx, &requested);
    js_free(ctx, calendar);
    js_free(ctx, numbers);
    return object;
 fail:
    dtf_destroy(ctx->rt, s);
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

typedef struct DTFInput {
    QJSIntlDateValue value;
    double number; /* retain unclipped Number until range type checks */
} DTFInput;
static int dtf_input(JSContext *ctx, JSValueConst input, DTFInput *out)
{
    memset(out, 0, sizeof(*out)); out->value.kind = QJS_DATE_VALUE_NUMBER;
#ifdef CONFIG_TEMPORAL
    {
        JSTemporalPlainDateData *date;
        JSTemporalPlainDateTimeData *datetime;
        JSTemporalPlainTimeData *time;
        JSTemporalInstantData *instant;
        if ((date = JS_GetOpaque(input, JS_CLASS_TEMPORAL_PLAIN_DATE)))
            out->value.kind = QJS_DATE_VALUE_PLAIN_DATE;
        else if ((date = JS_GetOpaque(input, JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH)))
            out->value.kind = QJS_DATE_VALUE_PLAIN_YEAR_MONTH;
        else if ((date = JS_GetOpaque(input, JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY)))
            out->value.kind = QJS_DATE_VALUE_PLAIN_MONTH_DAY;
        if (date) {
            out->value.calendar = dtf_bytes(qjs_temporal_calendar_identifier(date->calendar));
            out->value.value.date = date->date; return 0;
        }
        if ((datetime = JS_GetOpaque(input, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME))) {
            out->value.kind = QJS_DATE_VALUE_PLAIN_DATE_TIME;
            out->value.calendar = dtf_bytes(qjs_temporal_calendar_identifier(datetime->calendar));
            out->value.value.datetime = datetime->datetime; return 0;
        }
        if ((time = JS_GetOpaque(input, JS_CLASS_TEMPORAL_PLAIN_TIME))) {
            out->value.kind = QJS_DATE_VALUE_PLAIN_TIME;
            out->value.value.time = *time; return 0;
        }
        if ((instant = JS_GetOpaque(input, JS_CLASS_TEMPORAL_INSTANT))) {
            out->value.kind = QJS_DATE_VALUE_INSTANT;
            out->value.value.instant = *instant; return 0;
        }
        if (JS_GetOpaque(input, JS_CLASS_TEMPORAL_ZONED_DATE_TIME)) {
            out->value.kind = QJS_DATE_VALUE_ZONED_DATE_TIME; return 0;
        }
    }
#endif
    return dtf_time_argument(ctx, input, &out->number);
}
static int dtf_input_clip(JSContext *ctx, DTFInput *input)
{
    if (input->value.kind == QJS_DATE_VALUE_NUMBER) {
        if (dtf_clip(ctx, &input->number)) return -1;
        input->value.value.number_ms = (int64_t)input->number;
    }
    return 0;
}
static int dtf_error(JSContext *ctx, QJSIntlStatus status, QJSIntlDateValueFault fault)
{
    if (status == QJS_INTL_INVALID_ARGUMENT) {
        if (fault == QJS_DATE_FAULT_CALENDAR_MISMATCH) {
            JS_ThrowRangeError(ctx, "Temporal and formatter calendars differ"); return -1;
        }
        if (fault == QJS_DATE_FAULT_INAPPLICABLE_TYPE || fault == QJS_DATE_FAULT_MIXED_TYPE) {
            JS_ThrowTypeError(ctx, "formatter does not apply to Temporal type"); return -1;
        }
    }
    return js_intl_native_error(ctx, status, "DateTimeFormat value");
}
static const char *dtf_part_type(QJSIntlPartType type)
{
    switch (type) {
    case QJS_INTL_PART_LITERAL: return "literal";
    case QJS_INTL_PART_ERA: return "era";
    case QJS_INTL_PART_YEAR: return "year";
    case QJS_INTL_PART_RELATED_YEAR: return "relatedYear";
    case QJS_INTL_PART_YEAR_NAME: return "yearName";
    case QJS_INTL_PART_MONTH: return "month";
    case QJS_INTL_PART_DAY: return "day";
    case QJS_INTL_PART_WEEKDAY: return "weekday";
    case QJS_INTL_PART_DAY_PERIOD: return "dayPeriod";
    case QJS_INTL_PART_HOUR: return "hour";
    case QJS_INTL_PART_MINUTE: return "minute";
    case QJS_INTL_PART_SECOND: return "second";
    case QJS_INTL_PART_FRACTIONAL_SECOND: return "fractionalSecond";
    case QJS_INTL_PART_TIME_ZONE_NAME: return "timeZoneName";
    default: return NULL;
    }
}
static const char *dtf_part_source(QJSIntlPartSource source)
{
    switch (source) {
    case QJS_INTL_SOURCE_START_RANGE: return "startRange";
    case QJS_INTL_SOURCE_END_RANGE: return "endRange";
    case QJS_INTL_SOURCE_SHARED: return "shared";
    default: return NULL;
    }
}
static JSValue dtf_result(JSContext *ctx, const QJSIntlFormatted *formatted,
                           int parts, int range)
{
    static const uint16_t empty[] = { 0 };
    JSValue result; size_t i, cursor = 0;
    if (formatted->length > INT32_MAX || formatted->length > JS_STRING_LEN_MAX ||
        formatted->part_count > UINT32_MAX ||
        (formatted->length && !formatted->text) ||
        (formatted->part_count && !formatted->parts))
        return JS_ThrowOutOfMemory(ctx);
    for (i = 0; i < formatted->part_count; i++) {
        const QJSIntlPart *p = &formatted->parts[i];
        if (p->start != cursor || p->end < p->start || p->end > formatted->length ||
            !dtf_part_type(p->type) || (range ? !dtf_part_source(p->source) :
                p->source != QJS_INTL_SOURCE_SINGLE))
            return JS_ThrowInternalError(ctx, "invalid native DateTimeFormat part");
        cursor = p->end;
    }
    if (cursor != formatted->length)
        return JS_ThrowInternalError(ctx, "incomplete native DateTimeFormat parts");
    /* String construction consumes the provider UTF16 directly. */
    if (!parts) return js_intl_from_utf16(ctx, formatted->text ? formatted->text : empty,
                                         (int32_t)formatted->length);
    result = JS_NewArray(ctx); if (JS_IsException(result)) return result;
    for (i = 0; i < formatted->part_count; i++) {
        const QJSIntlPart *p = &formatted->parts[i];
        JSValue value = js_intl_from_utf16(ctx, formatted->text ? formatted->text + p->start : empty,
                                          (int32_t)(p->end - p->start));
        JSValue source = JS_UNDEFINED; int failure;
        if (JS_IsException(value)) goto fail;
        if (range) {
            source = JS_NewString(ctx, dtf_part_source(p->source));
            if (JS_IsException(source)) { JS_FreeValue(ctx, value); goto fail; }
        }
        failure = js_intl_add_part(ctx, result, (uint32_t)i, dtf_part_type(p->type),
                                  value, range ? "source" : NULL, source);
        JS_FreeValue(ctx, source); JS_FreeValue(ctx, value);
        if (failure < 0) goto fail;
    }
    return result;
fail:
    JS_FreeValue(ctx, result); return JS_EXCEPTION;
}
static JSValue dtf_format_value(JSContext *ctx, JSIntlDateTimeFormat *s,
                                const QJSIntlDateValue *value, int parts)
{
    QJSIntlFormatted formatted = { 0 }; QJSIntlDateValueFault fault;
    QJSIntlStatus status; JSValue result = JS_EXCEPTION;
    status = qjs_intl_native_date_bank_format(s->bank, value, &formatted, &fault);
    if (!dtf_error(ctx, status, fault)) result = dtf_result(ctx, &formatted, parts, FALSE);
    qjs_intl_native_date_clear(&s->allocator, &formatted); return result;
}
static JSValue dtf_bound_format(JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv, int magic, JSValue *data)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque(data[0], JS_CLASS_INTL_DATE_TIME_FORMAT);
    DTFInput input;
    if (!s) return JS_ThrowTypeError(ctx, "not an Intl.DateTimeFormat object");
    if (dtf_input(ctx, argc ? argv[0] : JS_UNDEFINED, &input) || dtf_input_clip(ctx, &input))
        return JS_EXCEPTION;
    return dtf_format_value(ctx, s, &input.value, FALSE);
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
    DTFInput input;
    if (!s) return JS_EXCEPTION;
    if (dtf_input(ctx, argc ? argv[0] : JS_UNDEFINED, &input) || dtf_input_clip(ctx, &input))
        return JS_EXCEPTION;
    return dtf_format_value(ctx, s, &input.value, TRUE);
}
static JSValue dtf_format_range(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int parts)
{
    JSIntlDateTimeFormat *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DATE_TIME_FORMAT);
    DTFInput start, end; QJSIntlFormatted formatted = { 0 }; QJSIntlDateValueFault fault;
    QJSIntlStatus status; JSValue result = JS_EXCEPTION;
    if (!s) return JS_EXCEPTION;
    if (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_ThrowTypeError(ctx, "DateTimeFormat range endpoint is undefined");
    /* Convert both arguments in order before any Number clipping. */
    if (dtf_input(ctx, argv[0], &start) || dtf_input(ctx, argv[1], &end)) return JS_EXCEPTION;
    if (start.value.kind != end.value.kind)
        return JS_ThrowTypeError(ctx, "Temporal range types differ");
    if (dtf_input_clip(ctx, &start) || dtf_input_clip(ctx, &end)) return JS_EXCEPTION;
    status = qjs_intl_native_date_bank_range(s->bank, &start.value, &end.value, &formatted, &fault);
    if (!dtf_error(ctx, status, fault)) result = dtf_result(ctx, &formatted, parts, TRUE);
    qjs_intl_native_date_clear(&s->allocator, &formatted); return result;
}
JSValue js_intl_date_format(JSContext *ctx, double time, JSValueConst locales,
                            JSValueConst options, int required, int defaults)
{
    JSValue object, result; JSIntlDateTimeFormat *s; DTFInput input;
    if (js_intl_ensure_service(ctx, JS_INTL_DATE_TIME_FORMAT)) return JS_EXCEPTION;
    object = dtf_create(ctx, js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT),
                        locales, options, required, defaults, JS_UNDEFINED);
    if (JS_IsException(object)) return object;
    s = JS_GetOpaque(object, JS_CLASS_INTL_DATE_TIME_FORMAT);
    memset(&input, 0, sizeof(input)); input.value.kind = QJS_DATE_VALUE_NUMBER; input.number = time;
    result = dtf_input_clip(ctx, &input) ? JS_EXCEPTION : dtf_format_value(ctx, s, &input.value, FALSE);
    JS_FreeValue(ctx, object); return result;
}
#ifdef CONFIG_TEMPORAL
JSValue js_intl_temporal_duration_to_locale_string(JSContext *, JSValueConst,
                                                  JSValueConst, JSValueConst);
JSValue js_intl_temporal_to_locale_string(JSContext *ctx, JSValueConst this_val,
                                          JSValueConst locales, JSValueConst options)
{
    DTFInput input; JSTemporalZonedDateTimeData *zoned = NULL;
    JSIntlDateTimeFormat *s; JSValue formatter, result;
    int required, defaults;
    if (JS_GetOpaque(this_val, JS_CLASS_TEMPORAL_DURATION))
        return js_intl_temporal_duration_to_locale_string(ctx, this_val, locales, options);
    /* This entry is called only by branded Temporal methods, never ToNumber. */
    if (dtf_input(ctx, this_val, &input)) return JS_EXCEPTION;
    switch (input.value.kind) {
    case QJS_DATE_VALUE_PLAIN_DATE:
    case QJS_DATE_VALUE_PLAIN_YEAR_MONTH:
    case QJS_DATE_VALUE_PLAIN_MONTH_DAY: required = defaults = JS_INTL_DTF_DATE; break;
    case QJS_DATE_VALUE_PLAIN_TIME: required = defaults = JS_INTL_DTF_TIME; break;
    case QJS_DATE_VALUE_PLAIN_DATE_TIME:
    case QJS_DATE_VALUE_INSTANT: required = JS_INTL_DTF_ANY; defaults = JS_INTL_DTF_ALL; break;
    case QJS_DATE_VALUE_ZONED_DATE_TIME:
        zoned = JS_GetOpaque(this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        required = JS_INTL_DTF_ANY; defaults = JS_INTL_DTF_ALL; break;
    default: return JS_ThrowTypeError(ctx, "unrecognized Temporal formatting brand");
    }
    if (js_intl_ensure_service(ctx, JS_INTL_DATE_TIME_FORMAT)) return JS_EXCEPTION;
    formatter = dtf_create(ctx, js_intl_constructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT),
        locales, options, required, defaults, zoned ? zoned->time_zone.identifier : JS_UNDEFINED);
    if (JS_IsException(formatter)) return formatter;
    s = JS_GetOpaque(formatter, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (zoned) {
        if (zoned->calendar != QJS_TEMPORAL_CAL_ISO8601 &&
            strcmp(qjs_temporal_calendar_identifier(zoned->calendar), s->locale.values[0])) {
            result = JS_ThrowRangeError(ctx, "ZonedDateTime calendars differ"); goto done;
        }
        memset(&input, 0, sizeof(input)); input.value.kind = QJS_DATE_VALUE_INSTANT;
        input.value.value.instant = zoned->epoch_nanoseconds;
    }
    result = dtf_format_value(ctx, s, &input.value, FALSE);
done:
    JS_FreeValue(ctx, formatter); return result;
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
