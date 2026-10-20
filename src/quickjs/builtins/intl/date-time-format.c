/* Intl.DateTimeFormat observable binding and formatter lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../date.h"
#include "../../../intl/datetime/datetime.h"
#include "../../internal/atom.h"
#include "../../internal/default-time-zone.h"
#include "../../internal/function.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"
#include "../../internal/string.h"
#include <math.h>
#ifdef CONFIG_TEMPORAL
#include "../temporal/temporal-internal.h"
#include "../../../calendar/calendar.h"
#endif

static const char *const hour_cycles[] = {"h11", "h12", "h23", "h24"};
static const char *const styles[] = {"full", "long", "medium", "short"};
static const char *const numeric_widths[] = {"2-digit", "numeric"};
static const char *const text_widths[] = {"narrow", "short", "long"};
static const char *const month_widths[] = {"2-digit", "numeric", "narrow", "short", "long"};
static const char *const zone_widths[] = {"short", "long", "shortOffset", "longOffset", "shortGeneric", "longGeneric"};
static const char *const component_names[] = {
    "weekday", "era", "year", "month", "day", "dayPeriod", "hour", "minute",
    "second", "fractionalSecondDigits", "timeZoneName",
};

typedef struct JSIntlDateTimeFormat {
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlDateTimeOptions options;
    QJSIntlDateTimeComponents components;
    /* Resolved facts borrow immutable storage retained by formatter. */
    QJSIntlBytes calendar, system;
    const char *zone;
    char *locale;
    JSValue bound_format;
} JSIntlDateTimeFormat;

typedef struct DateCapabilities {
    const QJSIntlDateTimeService *service;
    size_t locale;
} DateCapabilities;

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static QJSIntlStatus calendar_supported(void *opaque, QJSIntlBytes value, int *out)
{
    DateCapabilities *capabilities = opaque;
    return qjs_intl_datetime_calendar_supported(capabilities->service, capabilities->locale, value, out);
}

static QJSIntlStatus system_supported(void *opaque, QJSIntlBytes value, int *out)
{
    DateCapabilities *capabilities = opaque;
    return qjs_intl_datetime_system_supported(capabilities->service, capabilities->locale, value, out);
}

static QJSIntlStatus hour_supported(void *opaque, QJSIntlBytes value, int *out)
{
    size_t i;
    (void)opaque;
    *out = !value.data;
    for (i = 0; i < countof(hour_cycles); i++)
        if (value.length == 3 && !memcmp(value.data, hour_cycles[i], 3))
            *out = 1;
    return QJS_INTL_OK;
}

static QJSIntlDateTimeService *datetime_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_datetime) {
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlDateTimeService *service;
        if (js_intl_error(ctx, qjs_intl_datetime_open(&allocator, &service), "date/time initialization"))
            return NULL;
        rt->intl_datetime = service;
    }
    return rt->intl_datetime;
}

static void free_state(JSRuntime *rt, JSIntlDateTimeFormat *state)
{
    if (state) {
        qjs_intl_datetime_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        JS_FreeValueRT(rt, state->bound_format);
        js_free_rt(rt, state);
    }
}

static void datetime_finalizer(JSRuntime *rt, JSValue value)
{
    free_state(rt, JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT));
}

static void datetime_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark)
{
    JSIntlDateTimeFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (state)
        JS_MarkValue(rt, state->bound_format, mark);
}

static int timezone_option(JSContext *ctx, JSValueConst options, const char *forced,
                           QJSTimeZoneIdentifier *out)
{
    JSValue value = JS_GetPropertyStr(ctx, options, "timeZone");
    const char *text;
    size_t length;
    int error;
    if (JS_IsException(value))
        return -1;
    if (forced) {
        if (!JS_IsUndefined(value)) {
            JS_FreeValue(ctx, value);
            JS_ThrowTypeError(ctx, "timeZone is not permitted for this Temporal locale method");
            return -1;
        }
        error = qjs_timezone_parse_identifier(out, forced, strlen(forced));
    } else if (JS_IsUndefined(value)) {
        error = js_default_time_zone_get(ctx, out);
    } else {
        text = JS_ToCStringLen(ctx, &length, value);
        JS_FreeValue(ctx, value);
        if (!text)
            return -1;
        error = qjs_timezone_parse_identifier(out, text, length);
        JS_FreeCString(ctx, text);
    }
    if (!error)
        return 0;
    if (error == QJS_TZ_MEMORY)
        JS_ThrowOutOfMemory(ctx);
    else if (error == QJS_TZ_DATA)
        JS_ThrowInternalError(ctx, "invalid timezone data");
    else
        JS_ThrowRangeError(ctx, "invalid timeZone");
    return -1;
}

static int component_options(JSContext *ctx, JSValueConst options, QJSIntlDateTimeOptions *out)
{
    static const char *const matchers[] = {"basic", "best fit"};
    size_t i;
    int value, present = 0;
    for (i = 0; i < countof(component_names); i++) {
        const char *const *choices = numeric_widths;
        size_t count = countof(numeric_widths);
        int base = QJS_INTL_DATETIME_TWO_DIGIT;
        if (i == QJS_INTL_DATETIME_FRACTION) {
            if (js_intl_get_number_option(ctx, options, component_names[i], 1, 3, 0, &value))
                return -1;
            out->components.values[i] = value;
            present |= value != 0;
            continue;
        }
        if (i == QJS_INTL_DATETIME_WEEKDAY || i == QJS_INTL_DATETIME_ERA || i == QJS_INTL_DATETIME_DAY_PERIOD) {
            choices = text_widths;
            count = countof(text_widths);
            base = QJS_INTL_DATETIME_NARROW;
        } else if (i == QJS_INTL_DATETIME_MONTH) {
            choices = month_widths;
            count = countof(month_widths);
        } else if (i == QJS_INTL_DATETIME_ZONE) {
            choices = zone_widths;
            count = countof(zone_widths);
            base = QJS_INTL_DATETIME_ZONE_SHORT;
        }
        value = js_intl_string_option(ctx, options, component_names[i], choices, count, count);
        if (value < 0)
            return -1;
        if ((size_t)value != count) {
            out->components.values[i] = base + value;
            present = 1;
        }
    }
    value = js_intl_string_option(ctx, options, "formatMatcher", matchers, countof(matchers), 1);
    if (value < 0)
        return -1;
    out->best_fit = value;
    value = js_intl_string_option(ctx, options, "dateStyle", styles, countof(styles), countof(styles));
    if (value < 0)
        return -1;
    out->date_style = value == countof(styles) ? 0 : value + 1;
    value = js_intl_string_option(ctx, options, "timeStyle", styles, countof(styles), countof(styles));
    if (value < 0)
        return -1;
    out->time_style = value == countof(styles) ? 0 : value + 1;
    if ((out->date_style || out->time_style) && present) {
        JS_ThrowTypeError(ctx, "dateStyle/timeStyle cannot be combined with component options");
        return -1;
    }
    if ((out->required == QJS_INTL_DATE_REQUIRE_DATE && out->time_style) ||
        (out->required == QJS_INTL_DATE_REQUIRE_TIME && out->date_style)) {
        JS_ThrowTypeError(ctx, "date/time style conflicts with the locale method");
        return -1;
    }
    return 0;
}

/* CreateDateTimeFormat: complete observable option reads in specification
   order; the backend receives only resolved values and cannot call user code. */
static JSIntlDateTimeFormat *make_state(JSContext *ctx, JSValueConst locales, JSValueConst options_value,
                                       QJSIntlDateRequired required, QJSIntlDateDefaults defaults,
                                       const char *forced_zone)
{
    JSIntlLocaleList requested = {0};
    QJSIntlResolvedLocale resolved = {0};
    QJSIntlLocaleResolveKey keys[3] = {0};
    QJSIntlLocaleMatch match;
    QJSIntlDateTimeService *service;
    QJSIntlDateHourPreferences hours;
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSTimeZoneIdentifier zone;
    DateCapabilities capabilities;
    const QJSIntlBytes *available;
    QJSIntlBytes calendar, system;
    const char *owned_calendar = NULL, *owned_system = NULL;
    JSIntlDateTimeFormat *state = NULL, *result = NULL;
    JSValue options = JS_UNDEFINED, value;
    size_t count, i;
    int hour12 = -1, cycle;
    if (js_intl_canonicalize_locale_list(ctx, locales, &requested))
        goto done;
    options = js_intl_options_object(ctx, options_value, TRUE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0 ||
        js_intl_unicode_type_option(ctx, options, "calendar", &calendar, &owned_calendar) ||
        js_intl_unicode_type_option(ctx, options, "numberingSystem", &system, &owned_system))
        goto done;
    value = JS_GetPropertyStr(ctx, options, "hour12");
    if (JS_IsException(value))
        goto done;
    if (!JS_IsUndefined(value)) {
        hour12 = JS_ToBool(ctx, value);
        if (hour12 < 0) {
            JS_FreeValue(ctx, value);
            goto done;
        }
    }
    JS_FreeValue(ctx, value);
    cycle = js_intl_string_option(ctx, options, "hourCycle", hour_cycles, countof(hour_cycles), countof(hour_cycles));
    if (cycle < 0)
        goto done;
    service = datetime_service(ctx);
    if (!service || js_intl_error(ctx, qjs_intl_datetime_available(service, &available, &count), "date/time locales") ||
        js_intl_resolve_locale_match(ctx, available, count, &requested, &match))
        goto done;
    capabilities = (DateCapabilities){service, match.available_index};
    memcpy(keys[0].key, "ca", 2);
    memcpy(keys[1].key, "hc", 2);
    memcpy(keys[2].key, "nu", 2);
    keys[0].option = calendar;
    /* hourCycle is still read and validated above when hour12 overrides it.
       Explicit null removes an accepted hc extension during joint resolution. */
    keys[1].option = hour12 >= 0 || cycle == countof(hour_cycles) ? (QJSIntlBytes){0} : bytes(hour_cycles[cycle]);
    keys[1].option_is_null = hour12 >= 0;
    keys[2].option = system;
    keys[0].supported = calendar_supported;
    keys[1].supported = hour_supported;
    keys[2].supported = system_supported;
    for (i = 0; i < countof(keys); i++)
        keys[i].opaque = &capabilities;
    if (js_intl_error(ctx, qjs_intl_datetime_default_calendar(service, match.available_index, &keys[0].default_value),
                      "default calendar") ||
        js_intl_error(ctx, qjs_intl_datetime_default_system(service, match.available_index, &keys[2].default_value),
                      "default numbering system") ||
        js_intl_error(ctx, qjs_intl_datetime_hour_preferences(service, match.available_index, &hours), "hour preferences"))
        goto done;
    {
        QJSIntlLocaleService *identity = js_intl_locale_service(ctx);
        if (!identity || js_intl_error(ctx, qjs_intl_resolve_locale_keys(identity, &allocator,
                           available[match.available_index], match.extension, keys, countof(keys), &resolved),
                           "date/time locale resolution"))
            goto done;
    }
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto done;
    state->bound_format = JS_UNDEFINED;
    state->locale = resolved.locale;
    resolved.locale = NULL;
    state->options.hour_cycle = hour12 == 1 ? hours.twelve : hour12 == 0 ? hours.twenty_four : hours.preferred;
    if (hour12 < 0 && resolved.values[1]) {
        for (i = 0; i < countof(hour_cycles); i++)
            if (!strcmp(resolved.values[1], hour_cycles[i]))
                state->options.hour_cycle = i;
    }
    state->options.required = required;
    state->options.defaults = defaults;
    state->options.forced_zone = forced_zone != NULL;
    state->options.maximum_output_length = JS_STRING_LEN_MAX;
    if (timezone_option(ctx, options, forced_zone, &zone) || component_options(ctx, options, &state->options))
        goto done;
    if (js_intl_error(ctx, qjs_intl_datetime_formatter_open(service, match.available_index,
                        bytes(resolved.values[0]), bytes(resolved.values[2]), &zone, &state->options,
                        &state->formatter), "DateTimeFormat") ||
        js_intl_error(ctx, qjs_intl_datetime_formatter_info(state->formatter, &state->calendar,
                        &state->system, &state->zone, &state->components), "date/time options"))
        goto done;
    result = state;
    state = NULL;
done:
    free_state(ctx->rt, state);
    qjs_intl_resolved_locale_clear(&resolved);
    JS_FreeValue(ctx, options);
    JS_FreeCString(ctx, owned_calendar);
    JS_FreeCString(ctx, owned_system);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return result;
}

static JSValue datetime_constructor(JSContext *ctx, JSValueConst active_function,
                                    JSValueConst this_val, int argc, JSValueConst *argv, int flags)
{
    BOOL construct = (flags & JS_CALL_FLAG_CONSTRUCTOR) != 0;
    JSValue object = js_intl_create_from_constructor(ctx, construct ? this_val : active_function,
                                                     JS_CLASS_INTL_DATE_TIME_FORMAT);
    JSIntlDateTimeFormat *state;
    if (JS_IsException(object))
        return object;
    state = make_state(ctx, argc ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED,
                         QJS_INTL_DATE_REQUIRE_ANY, QJS_INTL_DATE_DEFAULT_DATE, NULL);
    if (!state)
        goto fail;
    JS_SetOpaque(object, state);
    if (!construct) {
        int instance = JS_OrdinaryIsInstanceOf(ctx, this_val, ctx->intl_datetime_ctor);
        if (instance < 0)
            goto fail;
        if (instance) {
            JSAtom key = JS_ValueToAtom(ctx, ctx->intl_fallback_symbol);
            int status;
            if (key == JS_ATOM_NULL)
                goto fail;
            status = JS_DefinePropertyValue(ctx, this_val, key, object, JS_PROP_THROW);
            JS_FreeAtom(ctx, key);
            if (status < 0)
                return JS_EXCEPTION;
            return JS_DupValue(ctx, this_val);
        }
    }
    return object;
fail:
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue unwrap(JSContext *ctx, JSValueConst value)
{
    JSAtom key;
    int instance;
    JSValue result;
    if (!JS_IsObject(value))
        return JS_ThrowTypeError(ctx, "DateTimeFormat receiver must be an object");
    if (JS_GetOpaque(value, JS_CLASS_INTL_DATE_TIME_FORMAT))
        return JS_DupValue(ctx, value);
    instance = JS_OrdinaryIsInstanceOf(ctx, value, ctx->intl_datetime_ctor);
    if (instance < 0)
        return JS_EXCEPTION;
    if (!instance)
        return JS_DupValue(ctx, value);
    key = JS_ValueToAtom(ctx, ctx->intl_fallback_symbol);
    if (key == JS_ATOM_NULL)
        return JS_EXCEPTION;
    result = JS_GetProperty(ctx, value, key);
    JS_FreeAtom(ctx, key);
    return result;
}

typedef struct DateInput {
    QJSIntlDateValueKind kind; /* KIND_COUNT denotes a direct ZonedDateTime. */
    double number;
#ifdef CONFIG_TEMPORAL
    QJSTemporalEpochNs epoch;
    QJSTemporalISODateTime plain;
    QJSCalendarId calendar;
#endif
} DateInput;

/* ToDateTimeFormattable does coercion only. Delay TimeClip, calendar checks and
   record errors until both range arguments have completed their coercions. */
static int read_input(JSContext *ctx, JSValueConst value, DateInput *out)
{
    memset(out, 0, sizeof(*out));
#ifdef CONFIG_TEMPORAL
    JSClassID class_id = JS_IsObject(value) ? JS_VALUE_GET_OBJ(value)->class_id : 0;
    switch (class_id) {
    case JS_CLASS_TEMPORAL_INSTANT: {
        const JSTemporalInstantData *data = JS_GetOpaque(value, JS_CLASS_TEMPORAL_INSTANT);
        if (!data)
            goto invalid;
        out->kind = QJS_INTL_DATE_VALUE_INSTANT;
        out->epoch = *data;
        return 0;
    }
    case JS_CLASS_TEMPORAL_PLAIN_DATE:
    case JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH:
    case JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY: {
        const JSTemporalPlainDateData *data = JS_GetOpaque(value, class_id);
        if (!data)
            goto invalid;
        out->kind = class_id == JS_CLASS_TEMPORAL_PLAIN_DATE ? QJS_INTL_DATE_VALUE_PLAIN_DATE :
                    class_id == JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH ? QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH :
                    QJS_INTL_DATE_VALUE_PLAIN_MONTH_DAY;
        out->calendar = data->calendar;
        out->plain.date = data->date;
        out->plain.time.hour = 12; /* ValueFormatRecord uses the reference date at noon. */
        return 0;
    }
    case JS_CLASS_TEMPORAL_PLAIN_DATE_TIME: {
        const JSTemporalPlainDateTimeData *data = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME);
        if (!data)
            goto invalid;
        out->kind = QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME;
        out->calendar = data->calendar;
        out->plain = data->datetime;
        return 0;
    }
    case JS_CLASS_TEMPORAL_PLAIN_TIME: {
        const JSTemporalPlainTimeData *data = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_TIME);
        if (!data)
            goto invalid;
        out->kind = QJS_INTL_DATE_VALUE_PLAIN_TIME;
        out->plain.date = (QJSTemporalISODate){1970, 1, 1};
        out->plain.time = *data;
        return 0;
    }
    case JS_CLASS_TEMPORAL_ZONED_DATE_TIME: {
        const JSTemporalZonedDateTimeData *data = JS_GetOpaque(value, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        if (!data)
            goto invalid;
        out->kind = QJS_INTL_DATE_VALUE_KIND_COUNT;
        out->epoch = data->epoch_nanoseconds;
        out->calendar = data->calendar;
        return 0;
    }
    default:
        break;
    }
#endif
    out->kind = QJS_INTL_DATE_VALUE_DATE;
    return JS_ToFloat64(ctx, &out->number, value);
#ifdef CONFIG_TEMPORAL
invalid:
    JS_ThrowTypeError(ctx, "invalid Temporal receiver");
    return -1;
#endif
}

static int input_epoch(JSContext *ctx, const JSIntlDateTimeFormat *state, const DateInput *input,
                        int64_t *seconds, uint32_t *nanosecond)
{
    if (input->kind == QJS_INTL_DATE_VALUE_DATE) {
        int64_t milliseconds, fraction;
        if (!isfinite(input->number) || fabs(input->number) > 8640000000000000.0) {
            JS_ThrowRangeError(ctx, "date outside the TimeClip range");
            return -1;
        }
        milliseconds = (int64_t)trunc(input->number);
        *seconds = milliseconds / 1000;
        fraction = milliseconds % 1000;
        if (fraction < 0) {
            fraction += 1000;
            (*seconds)--;
        }
        *nanosecond = fraction * 1000000;
        return 0;
    }
#ifdef CONFIG_TEMPORAL
    if (input->kind == QJS_INTL_DATE_VALUE_KIND_COUNT) {
        JS_ThrowTypeError(ctx, "format ZonedDateTime through its toLocaleString method");
        return -1;
    }
    if (input->kind == QJS_INTL_DATE_VALUE_INSTANT) {
        QJSTemporalEpochNs quotient;
        uint64_t fraction;
        if (qjs_temporal_epoch_ns_divide(&quotient, &fraction, input->epoch, UINT64_C(1000000000)) ||
            qjs_temporal_epoch_ns_to_int64(seconds, quotient)) {
            JS_ThrowRangeError(ctx, "invalid Temporal instant");
            return -1;
        }
        *nanosecond = fraction;
        return 0;
    }
    if (input->kind != QJS_INTL_DATE_VALUE_PLAIN_TIME) {
        const char *calendar = qjs_calendar_identifier(input->calendar);
        BOOL permit_iso = input->kind == QJS_INTL_DATE_VALUE_PLAIN_DATE ||
                          input->kind == QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME;
        if (!(permit_iso && input->calendar == QJS_CAL_ISO8601) &&
            (state->calendar.length != strlen(calendar) ||
             memcmp(state->calendar.data, calendar, state->calendar.length))) {
            JS_ThrowRangeError(ctx, "Temporal calendar differs from DateTimeFormat calendar");
            return -1;
        }
    }
    {
        int64_t day;
        const QJSTemporalISOTime *time = &input->plain.time;
        if (qjs_iso_date_to_days(&day, input->plain.date)) {
            JS_ThrowRangeError(ctx, "invalid Temporal date");
            return -1;
        }
        *seconds = day * 86400 + (time->hour * 60 + time->minute) * 60 + time->second;
        *nanosecond = (time->millisecond * 1000 + time->microsecond) * 1000 + time->nanosecond;
        return 0;
    }
#else
    (void)state;
    JS_ThrowInternalError(ctx, "unexpected Temporal input in a disabled build");
    return -1;
#endif
}

static const char *const part_names[QJS_INTL_PART_COUNT] = {
    [QJS_INTL_PART_LITERAL] = "literal",
    [QJS_INTL_PART_ERA] = "era",
    [QJS_INTL_PART_YEAR] = "year",
    [QJS_INTL_PART_RELATED_YEAR] = "relatedYear",
    [QJS_INTL_PART_YEAR_NAME] = "yearName",
    [QJS_INTL_PART_MONTH] = "month",
    [QJS_INTL_PART_DAY] = "day",
    [QJS_INTL_PART_WEEKDAY] = "weekday",
    [QJS_INTL_PART_DAY_PERIOD] = "dayPeriod",
    [QJS_INTL_PART_HOUR] = "hour",
    [QJS_INTL_PART_MINUTE] = "minute",
    [QJS_INTL_PART_SECOND] = "second",
    [QJS_INTL_PART_FRACTIONAL_SECOND] = "fractionalSecond",
    [QJS_INTL_PART_TIME_ZONE_NAME] = "timeZoneName",
};

static JSValue format_input(JSContext *ctx, const JSIntlDateTimeFormat *state,
                            const DateInput *input, int operation)
{
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    QJSIntlFormatted formatted = {0};
    QJSIntlStatus status;
    int64_t seconds;
    uint32_t nanosecond;
    JSValue result = JS_EXCEPTION;
    if (input_epoch(ctx, state, input, &seconds, &nanosecond))
        return result;
    status = qjs_intl_datetime_format(state->formatter, input->kind, seconds, nanosecond, &formatted);
    if (status == QJS_INTL_UNSUPPORTED)
        JS_ThrowTypeError(ctx, "format options do not support this Temporal type");
    else if (!js_intl_error(ctx, status, "date/time formatting"))
        result = operation == 0 ? js_intl_format_parts(ctx, &formatted, NULL, part_names, countof(part_names)) :
                                 JS_NewStringUTF16(ctx, formatted.text, formatted.length);
    qjs_intl_formatted_clear(&allocator, &formatted);
    return result;
}

/* operation -1 returns text; 0 returns parts. */
static JSValue datetime_format(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv, int operation)
{
    JSIntlDateTimeFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_DATE_TIME_FORMAT);
    DateInput input;
    if (!state)
        return JS_EXCEPTION;
    if (!argc || JS_IsUndefined(argv[0])) {
        int64_t milliseconds;
        if (js_date_now(ctx, &milliseconds))
            return JS_EXCEPTION;
        memset(&input, 0, sizeof(input));
        input.number = milliseconds;
    } else if (read_input(ctx, argv[0], &input)) {
        return JS_EXCEPTION;
    }
    return format_input(ctx, state, &input, operation);
}

JSValue js_intl_format_date_time(JSContext *ctx, JSValueConst receiver, int argc, JSValueConst *argv,
                                 QJSIntlDateRequired required, QJSIntlDateDefaults defaults)
{
    DateInput input;
    JSIntlDateTimeFormat *state;
    JSValue result;
    const char *forced = NULL;
    if (js_intl_ensure_intrinsics(ctx) || read_input(ctx, receiver, &input))
        return JS_EXCEPTION;
#ifdef CONFIG_TEMPORAL
    if (input.kind == QJS_INTL_DATE_VALUE_KIND_COUNT) {
        const JSTemporalZonedDateTimeData *data = JS_GetOpaque(receiver, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        forced = JS_ToCString(ctx, data->time_zone.identifier);
        if (!forced)
            return JS_EXCEPTION;
    }
#endif
    state = make_state(ctx, argc ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED,
                         required, defaults, forced);
    JS_FreeCString(ctx, forced);
    if (!state)
        return JS_EXCEPTION;
#ifdef CONFIG_TEMPORAL
    if (input.kind == QJS_INTL_DATE_VALUE_KIND_COUNT) {
        const char *calendar = qjs_calendar_identifier(input.calendar);
        /* The locale method formats an internal Instant only after complete
           option processing and its receiver-calendar compatibility check. */
        if (input.calendar != QJS_CAL_ISO8601 &&
            (state->calendar.length != strlen(calendar) ||
             memcmp(state->calendar.data, calendar, state->calendar.length))) {
            free_state(ctx->rt, state);
            return JS_ThrowRangeError(ctx, "Temporal calendar differs from DateTimeFormat calendar");
        }
        input.kind = QJS_INTL_DATE_VALUE_INSTANT;
    }
#endif
    result = format_input(ctx, state, &input, -1);
    free_state(ctx->rt, state);
    return result;
}

static JSValue bound_format(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv, int magic, JSValue *data)
{
    JSContext *realm;
    (void)this_val;
    (void)magic;
    /* Data callbacks run with their caller's context; the marked constructor
       anchors the getter's intrinsic realm even for a borrowed format getter. */
    realm = JS_GetFunctionRealm(ctx, data[1]);
    return realm ? datetime_format(realm, data[0], argc, argv, -1) : JS_EXCEPTION;
}

static JSValue datetime_format_get(JSContext *ctx, JSValueConst this_val)
{
    JSValue value = unwrap(ctx, this_val), result = JS_EXCEPTION;
    JSIntlDateTimeFormat *state;
    if (JS_IsException(value))
        return value;
    state = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (!state)
        goto done;
    if (JS_IsUndefined(state->bound_format)) {
        JSValueConst data[] = {value, ctx->intl_datetime_ctor};
        JSValue function = JS_NewCFunctionData(ctx, bound_format, 1, 0, countof(data), data);
        if (JS_IsException(function))
            goto done;
        state->bound_format = function;
    }
    result = JS_DupValue(ctx, state->bound_format);
done:
    JS_FreeValue(ctx, value);
    return result;
}

static int property(JSContext *ctx, JSValueConst object, const char *name, JSValue value)
{
    if (JS_IsException(value))
        return -1;
    return JS_DefinePropertyValueStr(ctx, object, name, value, JS_PROP_C_W_E | JS_PROP_THROW);
}

static const char *component_width(size_t field, int value)
{
    if (field == QJS_INTL_DATETIME_ZONE)
        return zone_widths[value - 1];
    return month_widths[value - 1];
}

static JSValue datetime_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue value = unwrap(ctx, this_val), result = JS_EXCEPTION;
    JSIntlDateTimeFormat *state;
    size_t i;
    (void)argc;
    (void)argv;
    if (JS_IsException(value))
        return value;
    state = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_DATE_TIME_FORMAT);
    if (!state)
        goto done;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto done;
    if (property(ctx, result, "locale", JS_NewString(ctx, state->locale)) < 0 ||
        property(ctx, result, "calendar", JS_NewStringLen(ctx, state->calendar.data, state->calendar.length)) < 0 ||
        property(ctx, result, "numberingSystem", JS_NewStringLen(ctx, state->system.data, state->system.length)) < 0 ||
        property(ctx, result, "timeZone", JS_NewString(ctx, state->zone)) < 0)
        goto fail;
    if (state->components.values[QJS_INTL_DATETIME_HOUR]) {
        /* Observable options follow the ordinary Date format. Keep the
           effective cycle internally for other value-kind records. */
        QJSIntlDateHourCycle cycle = state->options.hour_cycle;
        if (property(ctx, result, "hourCycle", JS_NewString(ctx, hour_cycles[cycle])) < 0 ||
            property(ctx, result, "hour12", JS_NewBool(ctx, cycle <= QJS_INTL_DATE_H12)) < 0)
            goto fail;
    }
    if (state->options.date_style || state->options.time_style) {
        if ((state->options.date_style && property(ctx, result, "dateStyle",
                       JS_NewString(ctx, styles[state->options.date_style - 1])) < 0) ||
            (state->options.time_style && property(ctx, result, "timeStyle",
                       JS_NewString(ctx, styles[state->options.time_style - 1])) < 0))
            goto fail;
    } else {
        for (i = 0; i < countof(component_names); i++) {
            int width = state->components.values[i];
            JSValue item;
            if (!width)
                continue;
            item = i == QJS_INTL_DATETIME_FRACTION ? JS_NewInt32(ctx, width) :
                   JS_NewString(ctx, component_width(i, width));
            if (property(ctx, result, component_names[i], item) < 0)
                goto fail;
        }
    }
    goto done;
fail:
    JS_FreeValue(ctx, result);
    result = JS_EXCEPTION;
done:
    JS_FreeValue(ctx, value);
    return result;
}

static JSValue datetime_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlDateTimeService *service = datetime_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    (void)this_val;
    if (!service || js_intl_error(ctx, qjs_intl_datetime_available(service, &available, &count), "date/time locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                    argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSCFunctionListEntry datetime_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, datetime_supported),
};
static const JSCFunctionListEntry datetime_properties[] = {
    JS_CGETSET_DEF("format", datetime_format_get, NULL),
    JS_CFUNC_MAGIC_DEF("formatToParts", 1, datetime_format, 0),
    JS_CFUNC_DEF("resolvedOptions", 0, datetime_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DateTimeFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_date_time_format(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.DateTimeFormat",
        .finalizer = datetime_finalizer,
        .gc_mark = datetime_mark,
    };
    JSCFunctionType callback = {.constructor_or_func_flags = datetime_constructor};
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_DATE_TIME_FORMAT) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_DATE_TIME_FORMAT, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_DATE_TIME_FORMAT, "DateTimeFormat",
                     callback.generic, 0, JS_CFUNC_constructor_or_func_flags, 0,
                     JS_UNDEFINED, datetime_statics, countof(datetime_statics),
                     datetime_properties, countof(datetime_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    ctx->intl_datetime_ctor = JS_DupValue(ctx, constructor);
    return JS_DefinePropertyValueStr(ctx, namespace_object, "DateTimeFormat", constructor,
                                      JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
