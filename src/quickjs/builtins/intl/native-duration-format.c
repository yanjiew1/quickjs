/* Native Intl.DurationFormat. Copyright (c) 2026 Yan-Jie Wang.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd; reviewed 2026-10-09.
 * Temporal e8cc03f Stage4 private slots and native duration parser retained.
 * Observable validation/options and exact integer serialization are C. */
#include "intl-number.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../../intl/provider-native-duration.h"
#ifdef CONFIG_TEMPORAL
#include "../temporal/temporal-internal.h"
#endif
#include "../../../temporal/duration.h"

static const char *const duration_units[] = {
    "years", "months", "weeks", "days", "hours", "minutes", "seconds",
    "milliseconds", "microseconds", "nanoseconds"
};
static const char *const number_units[] = {
    "year", "month", "week", "day", "hour", "minute", "second",
    "millisecond", "microsecond", "nanosecond"
};
static const char *const unit_styles[] = { "long", "short", "narrow", "numeric", "2-digit" };
static const char *const base_styles[] = { "long", "short", "narrow", "digital" };
static const char *const displays[] = { "auto", "always" };
static const char *const duration_matchers[] = { "lookup", "best fit" };
#define FRACTIONAL 5

typedef struct JSIntlDurationFormat {
    JSIntlResolvedLocale locale;
    int style, unit_style[10], unit_display[10], fractional_digits;
    int two_digit_hours;
    QJSIntlNativeDuration *formatter;
    QJSIntlAllocator allocator;
} JSIntlDurationFormat;

typedef struct DurationRecord { double value[10]; int sign; } DurationRecord;
/* Four base-2^32 limbs are ample: valid duration totals are <2^53 *10^9.
   All inputs are integral binary64. Extracting their mantissa and shifting
   gives exact integers even beyond Number.MAX_SAFE_INTEGER. */
typedef struct DurationUInt { uint32_t limb[4]; } DurationUInt;

static int duration_uint_zero(const DurationUInt *n)
{
    return !(n->limb[0] | n->limb[1] | n->limb[2] | n->limb[3]);
}

static void duration_uint_from_double(DurationUInt *n, double value)
{
    double fraction;
    uint64_t mantissa;
    int exponent, bit;
    memset(n, 0, sizeof(*n));
    if (!value) return;
    fraction = frexp(fabs(value), &exponent);
    mantissa = (uint64_t)ldexp(fraction, 53);
    exponent -= 53;
    if (exponent < 0) { mantissa >>= -exponent; exponent = 0; }
    for (bit = 0; bit < 53; bit++) {
        int position = exponent + bit;
        if (position < 128 && (mantissa & ((uint64_t)1 << bit)))
            n->limb[position / 32] |= (uint32_t)1 << (position % 32);
    }
}

static void duration_uint_multiply(DurationUInt *n, uint32_t factor)
{
    uint64_t carry = 0;
    int i;
    for (i = 0; i < 4; i++) {
        carry += (uint64_t)n->limb[i] * factor;
        n->limb[i] = (uint32_t)carry; carry >>= 32;
    }
    assert(carry == 0);
}

static void duration_uint_add(DurationUInt *n, const DurationUInt *value)
{
    uint64_t carry = 0;
    int i;
    for (i = 0; i < 4; i++) {
        carry += (uint64_t)n->limb[i] + value->limb[i];
        n->limb[i] = (uint32_t)carry; carry >>= 32;
    }
    assert(carry == 0);
}

static int duration_uint_compare(const DurationUInt *a, const DurationUInt *b)
{
    int i;
    for (i = 3; i >= 0; i--)
        if (a->limb[i] != b->limb[i]) return a->limb[i] < b->limb[i] ? -1 : 1;
    return 0;
}

static unsigned duration_uint_divide10(DurationUInt *n)
{
    uint64_t remainder = 0, value;
    int i;
    for (i = 3; i >= 0; i--) {
        value = (remainder << 32) | n->limb[i];
        n->limb[i] = (uint32_t)(value / 10); remainder = value % 10;
    }
    return (unsigned)remainder;
}

/* Temporal slots are immutable. A native record is sufficient for
   PartitionDurationFormatPattern; no temporary JavaScript object is exposed. */
static void duration_native_record(const QJSTemporalDuration *data,
                                    DurationRecord *record)
{
    int unit;
    memset(record, 0, sizeof(*record));
    for (unit = 0; unit < 10; unit++) {
        record->value[unit] = data->fields[unit];
        if (!record->sign && data->fields[unit])
            record->sign = data->fields[unit] < 0 ? -1 : 1;
    }
}

static int duration_record(JSContext *ctx, JSValueConst input, DurationRecord *record)
{
    static const int order[] = {3,4,8,7,5,1,9,6,2,0};
    /* Loose binary upper bounds allow exact comparison at the actual limit. */
    static const int bound_exponent[] = {0,0,0,37,42,48,54,64,74,84};
    static const uint32_t seconds_scale[] = {86400,3600,60,1};
    DurationUInt total = {{0}}, limit, component;
    int i, unit, any = 0;
    memset(record, 0, sizeof(*record));
    /* Stage4 ToTemporalDuration: native slots precede property Gets; ISO
       strings use the shared native parser and duration validity operation. */
#ifdef CONFIG_TEMPORAL
    {
        const JSTemporalDurationData *native =
            JS_GetOpaque(input, JS_CLASS_TEMPORAL_DURATION);
        if (native) {
            duration_native_record(native, record);
            return 0;
        }
    }
#endif
    if (JS_IsString(input)) {
        QJSTemporalDuration parsed;
        const char *text;
        size_t length;
        int invalid;
        text = JS_ToCStringLen(ctx, &length, input);
        if (!text) return -1;
        invalid = qjs_temporal_parse_duration(&parsed, text, length);
        JS_FreeCString(ctx, text);
        if (invalid || !qjs_temporal_duration_is_valid(&parsed))
            return (JS_ThrowRangeError(ctx, "invalid Temporal duration string"), -1);
        duration_native_record(&parsed, record);
        return 0;
    }
    if (!JS_IsObject(input))
        return (JS_ThrowTypeError(ctx, "duration must be an object or string"), -1);
    for (i = 0; i < countof(order); i++) {
        JSValue value;
        double number;
        unit = order[i];
        value = JS_GetPropertyStr(ctx, input, duration_units[unit]);
        if (JS_IsException(value)) return -1;
        if (!JS_IsUndefined(value)) {
            any = 1;
            if (JS_ToFloat64(ctx, &number, value) < 0) { JS_FreeValue(ctx, value); return -1; }
            if (!isfinite(number) || trunc(number) != number) {
                JS_FreeValue(ctx, value);
                return (JS_ThrowRangeError(ctx, "duration fields must be finite integers"), -1);
            }
            record->value[unit] = number == 0 ? 0 : number;
        }
        JS_FreeValue(ctx, value);
    }
    if (!any) return (JS_ThrowTypeError(ctx, "duration requires a field"), -1);
    for (unit = 0; unit < 10; unit++) {
        double number = record->value[unit];
        int sign = (number > 0) - (number < 0);
        if (sign && record->sign && sign != record->sign)
            return (JS_ThrowRangeError(ctx, "duration fields have mixed signs"), -1);
        if (sign) record->sign = sign;
        if (unit < 3) {
            if (fabs(number) >= 4294967296.0)
                return (JS_ThrowRangeError(ctx, "calendar duration field exceeds limit"), -1);
        } else {
            if (fabs(number) >= ldexp(1, bound_exponent[unit]))
                return (JS_ThrowRangeError(ctx, "duration exceeds seconds limit"), -1);
            duration_uint_from_double(&component, number);
            if (unit <= 6) {
                duration_uint_multiply(&component, seconds_scale[unit - 3]);
                duration_uint_multiply(&component, 1000000000);
            } else if (unit == 7) duration_uint_multiply(&component, 1000000);
            else if (unit == 8) duration_uint_multiply(&component, 1000);
            duration_uint_add(&total, &component);
        }
    }
    duration_uint_from_double(&limit, 9007199254740992.0);
    duration_uint_multiply(&limit, 1000000000);
    if (duration_uint_compare(&total, &limit) >= 0)
        return (JS_ThrowRangeError(ctx, "duration exceeds seconds limit"), -1);
    return 0;
}

static void duration_free(JSRuntime *rt, JSIntlDurationFormat *s)
{
    int i;
    if (!s) return;
    qjs_intl_native_duration_close(s->formatter);
    js_free_rt(rt, s->locale.locale);
    js_free_rt(rt, s->locale.data_locale);
    for (i = 0; i < s->locale.key_count; i++) js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s);
}

static void duration_finalizer(JSRuntime *rt, JSValue value)
{
    duration_free(rt, JS_GetOpaque(value, JS_CLASS_INTL_DURATION_FORMAT));
}

static int duration_options(JSContext *ctx, JSIntlDurationFormat *s,
                            JSValueConst options)
{
    int unit, previous = -1;
    for (unit = 0; unit < 10; unit++) {
        char display_property[32];
        int style, display_default = 1, count = unit < 4 ? 3 : unit < 7 ? 5 : 4;
        if (js_intl_get_string_option(ctx, options, duration_units[unit], unit_styles,
                                      count, -1, &style) < 0) return -1;
        if (style < 0) {
            if (s->style == 3) { style = unit < 4 ? 1 : 3; display_default = unit >= 4 && unit <= 6; }
            else if (previous >= 3) { style = 3; display_default = unit == 5 || unit == 6; }
            else { style = s->style; display_default = 0; }
        }
        if (style == 3 && unit >= 7) { style = FRACTIONAL; display_default = 0; }
        snprintf(display_property, sizeof(display_property), "%sDisplay", duration_units[unit]);
        if (js_intl_get_string_option(ctx, options, display_property, displays,
                                      countof(displays), display_default,
                                      &s->unit_display[unit]) < 0) return -1;
        if ((style == FRACTIONAL && s->unit_display[unit]) ||
            (previous == FRACTIONAL && style != FRACTIONAL) ||
            ((previous == 3 || previous == 4) && style < 3))
            return (JS_ThrowRangeError(ctx, "inconsistent duration unit options"), -1);
        if (unit == 4 && s->two_digit_hours) style = 4;
        if ((unit == 5 || unit == 6) && (previous == 3 || previous == 4)) style = 4;
        s->unit_style[unit] = style;
        if (unit >= 4 && unit <= 8) previous = style;
    }
    return js_intl_get_number_option(ctx, options, "fractionalDigits", 0, 9, -1,
                                     &s->fractional_digits);
}

static int duration_unicode_type_valid(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    int length = 0;
    for (;;) {
        if (!*p || *p == '-') {
            if (length < 3 || length > 8) return 0;
            if (!*p) return 1;
            length = 0;
        } else {
            if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9'))) return 0;
            length++;
        }
        p++;
    }
}

static void duration_native_options(const JSIntlDurationFormat *s,
                                     QJSIntlDurationOptions *o)
{
    int i;
    memset(o, 0, sizeof(*o));
    o->style = (QJSIntlDurationStyle)s->style;
    o->fractional_digits = s->fractional_digits;
    o->maximum_output_length = JS_STRING_LEN_MAX;
    for (i = 0; i < 10; i++) {
        o->units[i].style = (QJSIntlDurationUnitStyle)s->unit_style[i];
        o->units[i].always = s->unit_display[i];
    }
}

static JSValue duration_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    JSIntlDurationFormat *s;
    JSValue object, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolutionKey key = {"nu", NULL, FALSE};
    char *numbering_system = NULL;
    QJSIntlProvider *provider;
    QJSIntlDurationOptions native_options;
    QJSIntlStatus status;
    uint8_t two;
    int matcher;
    if (JS_IsUndefined(new_target)) return JS_ThrowTypeError(ctx, "DurationFormat requires new");
    object = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_DURATION_FORMAT);
    if (JS_IsException(object)) return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) { JS_FreeValue(ctx, object); return JS_EXCEPTION; }
    JS_SetOpaque(object, s);
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested) < 0) goto fail;
    options = js_intl_get_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(options)) goto fail;
    if (js_intl_get_string_option(ctx, options, "localeMatcher", duration_matchers,
                                  countof(duration_matchers), 1, &matcher) < 0 ||
        js_intl_get_string_option_alloc(ctx, options, "numberingSystem", &numbering_system) < 0)
        goto fail;
    if (numbering_system && !duration_unicode_type_valid(numbering_system)) {
        JS_ThrowRangeError(ctx, "invalid numberingSystem"); goto fail;
    }
    key.option = numbering_system;
    if (js_intl_resolve_locale(ctx, JS_INTL_DURATION_FORMAT, &requested,
                               duration_matchers[matcher], &key, 1, &s->locale) < 0) goto fail;
    provider = js_intl_native_provider(ctx);
    if (!provider) goto fail;
    s->allocator = *qjs_intl_native_provider_allocator(provider);
    status = qjs_intl_native_provider_duration_clock(provider,
        (QJSIntlBytes){s->locale.data_locale, strlen(s->locale.data_locale)},
        (QJSIntlBytes){s->locale.values[0], strlen(s->locale.values[0])}, &two);
    if (js_intl_native_error(ctx, status, "DurationFormat clock data")) goto fail;
    s->two_digit_hours = two;
    if (js_intl_get_string_option(ctx, options, "style", base_styles,
                                  countof(base_styles), 1, &s->style) < 0 ||
        duration_options(ctx, s, options) < 0) goto fail;
    duration_native_options(s, &native_options);
    status = qjs_intl_native_provider_duration_open(provider,
        (QJSIntlBytes){s->locale.data_locale, strlen(s->locale.data_locale)},
        (QJSIntlBytes){s->locale.values[0], strlen(s->locale.values[0])},
        &native_options, &s->formatter);
    if (js_intl_native_error(ctx, status, "DurationFormat")) goto fail;
    JS_FreeValue(ctx, options); js_free(ctx, numbering_system);
    js_intl_locale_list_free(ctx, &requested);
    return object;
 fail:
    JS_FreeValue(ctx, options); js_free(ctx, numbering_system);
    js_intl_locale_list_free(ctx, &requested); JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

/* The mathematical integer represented by a binary Number is serialized
 * exactly. Shortest Number strings lose precision above MAX_SAFE_INTEGER.
 * Duration bounds imply magnitudes <2^84, well within four limbs and 48 bytes.
 * Stack slices are borrowed only for the synchronous native format call. */
static void duration_backend_record(const DurationRecord *record,
                                    QJSIntlDurationRecord *native,
                                    char storage[10][48])
{
    int unit;
    memset(native, 0, sizeof(*native));
    native->sign = record->sign;
    for (unit = 0; unit < 10; unit++) {
        DurationUInt magnitude;
        char reverse[48];
        int length = 0, i;
        duration_uint_from_double(&magnitude, record->value[unit]);
        do { reverse[length++] = '0' + duration_uint_divide10(&magnitude); }
        while (!duration_uint_zero(&magnitude));
        for (i = 0; i < length; i++) storage[unit][i] = reverse[length - 1 - i];
        storage[unit][length] = 0;
        native->magnitudes[unit] = (QJSIntlBytes){storage[unit], (size_t)length};
    }
}

static const char *duration_part_name(QJSIntlPartType type)
{
    switch (type) {
    case QJS_INTL_PART_LITERAL: return "literal";
    case QJS_INTL_PART_INTEGER: return "integer";
    case QJS_INTL_PART_FRACTION: return "fraction";
    case QJS_INTL_PART_GROUP: return "group";
    case QJS_INTL_PART_DECIMAL: return "decimal";
    case QJS_INTL_PART_PLUS_SIGN: return "plusSign";
    case QJS_INTL_PART_MINUS_SIGN: return "minusSign";
    case QJS_INTL_PART_UNIT: return "unit";
    default: return NULL;
    }
}

static int duration_part_unit(QJSIntlBytes unit)
{
    int i;
    for (i = 0; i < 10; i++)
        if (unit.data && unit.length == strlen(number_units[i]) &&
            !memcmp(unit.data, number_units[i], unit.length)) return i;
    return -1;
}

static JSValue duration_format_record(JSContext *ctx, JSIntlDurationFormat *s,
                                       const DurationRecord *record, int parts)
{
    QJSIntlDurationRecord native;
    QJSIntlFormatted formatted = {0};
    char storage[10][48];
    JSValue result = JS_EXCEPTION;
    size_t i, cursor = 0;
    QJSIntlStatus status;
    static const uint16_t empty[] = {0};
    duration_backend_record(record, &native, storage);
    status = qjs_intl_native_duration_format(s->formatter, &native, &formatted);
    if (js_intl_native_error(ctx, status, "DurationFormat.format")) goto done;
    if (formatted.length > JS_STRING_LEN_MAX || formatted.length > INT32_MAX ||
        (formatted.length && !formatted.text) ||
        (formatted.part_count && !formatted.parts) || formatted.part_count > UINT32_MAX) {
        JS_ThrowOutOfMemory(ctx); goto done;
    }
    for (i = 0; i < formatted.part_count; i++) {
        const QJSIntlPart *part = &formatted.parts[i];
        if (part->start != cursor || part->end < part->start || part->end > formatted.length ||
            part->source != QJS_INTL_SOURCE_SINGLE || !duration_part_name(part->type) ||
            (part->unit.length && duration_part_unit(part->unit) < 0) ||
            (!part->unit.length && part->type != QJS_INTL_PART_LITERAL)) {
            JS_ThrowInternalError(ctx, "invalid native DurationFormat parts"); goto done;
        }
        cursor = part->end;
    }
    if (cursor != formatted.length) {
        JS_ThrowInternalError(ctx, "incomplete native DurationFormat parts"); goto done;
    }
    if (!parts) {
        result = js_intl_from_utf16(ctx, formatted.text ? formatted.text : empty,
                                    (int32_t)formatted.length);
        goto done;
    }
    result = JS_NewArray(ctx);
    if (JS_IsException(result)) goto done;
    for (i = 0; i < formatted.part_count; i++) {
        const QJSIntlPart *part = &formatted.parts[i];
        JSValue value = js_intl_from_utf16(ctx, formatted.text ? formatted.text + part->start : empty,
                                          (int32_t)(part->end - part->start));
        JSValue unit = JS_UNDEFINED;
        int failure;
        if (JS_IsException(value)) goto fail;
        if (part->unit.length) {
            unit = JS_NewString(ctx, number_units[duration_part_unit(part->unit)]);
            if (JS_IsException(unit)) { JS_FreeValue(ctx, value); goto fail; }
        }
        failure = js_intl_add_part(ctx, result, (uint32_t)i, duration_part_name(part->type), value,
                                    part->unit.length ? "unit" : NULL, unit);
        JS_FreeValue(ctx, value); JS_FreeValue(ctx, unit);
        if (failure < 0) goto fail;
    }
    goto done;
 fail:
    JS_FreeValue(ctx, result); result = JS_EXCEPTION;
 done:
    qjs_intl_native_duration_result_clear(&s->allocator, &formatted);
    return result;
}

static JSValue duration_format(JSContext *ctx, JSValueConst this_value,
                               int argc, JSValueConst *argv, int parts)
{
    JSIntlDurationFormat *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_DURATION_FORMAT);
    DurationRecord record;
    if (!s) return JS_EXCEPTION;
    if (duration_record(ctx, argc ? argv[0] : JS_UNDEFINED, &record) < 0)
        return JS_EXCEPTION;
    return duration_format_record(ctx, s, &record, parts);
}

/* Called by the shared native Temporal locale dispatcher. Brand first,
   construct the retained Intl intrinsic, then partition the original slots. */
#ifdef CONFIG_TEMPORAL
JSValue js_intl_temporal_duration_to_locale_string(JSContext *ctx,
                                                   JSValueConst temporal_duration,
                                                   JSValueConst locales,
                                                   JSValueConst options)
{
    JSTemporalDurationData *data = JS_GetOpaque2(ctx, temporal_duration,
                                                JS_CLASS_TEMPORAL_DURATION);
    DurationRecord record;
    JSValue formatter, result;
    JSValueConst arguments[2] = { locales, options };
    JSIntlDurationFormat *s;
    if (!data) return JS_EXCEPTION;
    duration_native_record(data, &record);
    if (js_intl_ensure_service(ctx, JS_INTL_DURATION_FORMAT) < 0)
        return JS_EXCEPTION;
    formatter = JS_CallConstructor(ctx,
                    js_intl_constructor(ctx, JS_CLASS_INTL_DURATION_FORMAT),
                    2, arguments);
    if (JS_IsException(formatter)) return formatter;
    s = JS_GetOpaque2(ctx, formatter, JS_CLASS_INTL_DURATION_FORMAT);
    if (!s) { JS_FreeValue(ctx, formatter); return JS_EXCEPTION; }
    result = duration_format_record(ctx, s, &record, 0);
    JS_FreeValue(ctx, formatter);
    return result;
}
#endif

static JSValue duration_resolved_options(JSContext *ctx, JSValueConst this_value,
                                         int argc, JSValueConst *argv)
{
    JSIntlDurationFormat *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_DURATION_FORMAT);
    JSValue result;
    int i;
    if (!s) return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale.locale) < 0 ||
        js_intl_define_string(ctx, result, "numberingSystem", s->locale.values[0]) < 0 ||
        js_intl_define_string(ctx, result, "style", base_styles[s->style]) < 0) goto fail;
    for (i = 0; i < 10; i++) {
        char property[32];
        const char *style = s->unit_style[i] == FRACTIONAL ? "numeric" : unit_styles[s->unit_style[i]];
        snprintf(property, sizeof(property), "%sDisplay", duration_units[i]);
        if (js_intl_define_string(ctx, result, duration_units[i], style) < 0 ||
            js_intl_define_string(ctx, result, property, displays[s->unit_display[i]]) < 0) goto fail;
    }
    if (s->fractional_digits >= 0 && js_intl_define_int(ctx, result, "fractionalDigits", s->fractional_digits) < 0)
        goto fail;
    return result;
 fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue duration_supported_locales(JSContext *ctx, JSValueConst this_value,
                                          int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_DURATION_FORMAT,
                                     argc ? argv[0] : JS_UNDEFINED,
                                     argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSCFunctionListEntry duration_static_functions[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, duration_supported_locales),
};
static const JSCFunctionListEntry duration_prototype_functions[] = {
    JS_CFUNC_MAGIC_DEF("format", 1, duration_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 1, duration_format, 1),
    JS_CFUNC_DEF("resolvedOptions", 0, duration_resolved_options),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.DurationFormat", JS_PROP_CONFIGURABLE),
};
static const JSClassDef duration_class = { "DurationFormat", .finalizer = duration_finalizer };

int js_intl_init_duration_format(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_DURATION_FORMAT,
        &duration_class, "DurationFormat", duration_constructor, 0, JS_CFUNC_constructor,
        duration_static_functions, countof(duration_static_functions),
        duration_prototype_functions, countof(duration_prototype_functions));
}

#endif
