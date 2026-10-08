/* Native Intl.DurationFormat. ECMA-402 7ae78cf (2026-10-06).
 * Stage 4 Temporal e8cc03f (2026-07-27), retrieved 2026-10-08.
 * Duration validation/composition is implemented in C, with ICU locale data. */
#include "intl-number.h"
#ifdef CONFIG_TEMPORAL
#include "../temporal/temporal-internal.h"
#endif
#include "../../../temporal/duration.h"
#include <unicode/udata.h>
#include <unicode/ures.h>
#include <unicode/ulistformatter.h>
#include <unicode/uformattedvalue.h>

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
    UChar *hour_minute_separator, *minute_second_separator;
    int32_t hour_minute_length, minute_second_length;
    int two_digit_hours;
    UListFormatter *list_formatter;
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

static int duration_value(JSContext *ctx, JSIntlDurationFormat *s,
                          const DurationRecord *record, int unit,
                          int show_sign, JSIntlMathematicalValue *mv, int *nonzero)
{
    DurationUInt value, component;
    char reverse[48], decimal[64];
    int fraction = 0, length = 0, output = 0, i;
    memset(mv, 0, sizeof(*mv));
    duration_uint_from_double(&value, record->value[unit]);
    if (unit >= 6 && unit < 9 && s->unit_style[unit + 1] == FRACTIONAL) {
        /* First fractional unit defines the scale; subsequent fractional
           units are exactly 1000 times smaller, independent of unit name. */
        for (i = unit + 1; i < 10 && s->unit_style[i] == FRACTIONAL; i++) {
            duration_uint_multiply(&value, 1000);
            duration_uint_from_double(&component, record->value[i]);
            duration_uint_add(&value, &component);
            fraction += 3;
        }
    }
    *nonzero = !duration_uint_zero(&value);
    if (!*nonzero && show_sign && record->sign < 0) {
        mv->kind = JS_INTL_MV_NEGATIVE_ZERO;
        return 0;
    }
    do { reverse[length++] = '0' + duration_uint_divide10(&value); }
    while (!duration_uint_zero(&value));
    while (length <= fraction) reverse[length++] = '0';
    if (record->sign < 0) decimal[output++] = '-';
    for (i = length - 1; i >= 0; i--) {
        decimal[output++] = reverse[i];
        if (i == fraction && fraction) decimal[output++] = '.';
    }
    decimal[output] = 0;
    mv->decimal = js_intl_strdup(ctx, decimal);
    if (!mv->decimal) return -1;
    mv->length = output;
    return 0;
}

static void duration_free(JSRuntime *rt, JSIntlDurationFormat *s)
{
    int i;
    if (!s) return;
    ulistfmt_close(s->list_formatter);
    js_free_rt(rt, s->locale.locale); js_free_rt(rt, s->locale.data_locale);
    js_free_rt(rt, s->locale.icu_locale);
    for (i = 0; i < s->locale.key_count; i++) js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s->hour_minute_separator); js_free_rt(rt, s->minute_second_separator);
    js_free_rt(rt, s);
}

static void duration_finalizer(JSRuntime *rt, JSValue value)
{
    duration_free(rt, JS_GetOpaque(value, JS_CLASS_INTL_DURATION_FORMAT));
}

/* Pattern fields contain quoted literals. Preserve UTF16 literals verbatim,
   unquote doubled apostrophes, and detect the hour field's minimum width. */
static int duration_separator(JSContext *ctx, const UChar *pattern, int32_t length,
                              UChar first, UChar second, UChar **result,
                              int32_t *result_length, int *first_width)
{
    UChar *buffer;
    int32_t at, count = 0;
    int quoted = 0, phase = 0, width = 0;
    buffer = js_intl_alloc_uchar(ctx, length);
    if (!buffer) return -1;
    for (at = 0; at < length; at++) {
        UChar c = pattern[at];
        if (c == '\'') {
            if (at + 1 < length && pattern[at + 1] == '\'') {
                if (phase == 1) buffer[count++] = '\'';
                at++;
            } else quoted = !quoted;
            continue;
        }
        if (!quoted && c == first && phase == 0) { width++; continue; }
        if (!quoted && c == second && width) { phase = 2; break; }
        if (width) phase = 1;
        if (phase == 1) buffer[count++] = c;
    }
    if (phase != 2 || !width) {
        js_free(ctx, buffer);
        return (JS_ThrowInternalError(ctx, "invalid ICU duration pattern"), -1);
    }
    *result = buffer; *result_length = count;
    if (first_width) *first_width = width;
    return 0;
}

static int duration_digital_data(JSContext *ctx, JSIntlDurationFormat *s)
{
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *bundle = NULL, *duration = NULL;
    const UChar *pattern;
    int32_t length;
    int width, ret = -1;
    /* Public ures C API, ICU data package naming from public udata.h. */
    bundle = ures_open(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "unit",
                       s->locale.icu_locale, &status);
    duration = ures_getByKey(bundle, "durationUnits", NULL, &status);
    if (js_intl_icu_error(ctx, status, "duration locale patterns") < 0) goto done;
    /* hms is present in every pinned ICU78.3 durationUnits table. Using
       this one pattern preserves hours width in locales whose hm or ms
       entries inherit separately (e.g. tk, es_CL and mn). */
    pattern = ures_getStringByKey(duration, "hms", &length, &status);
    if (js_intl_icu_error(ctx, status, "duration digital pattern") < 0 ||
        duration_separator(ctx, pattern, length, 'h', 'm', &s->hour_minute_separator,
                            &s->hour_minute_length, &width) < 0 ||
        duration_separator(ctx, pattern, length, 'm', 's', &s->minute_second_separator,
                            &s->minute_second_length, NULL) < 0) goto done;
    s->two_digit_hours = width >= 2;
    ret = 0;
 done:
    ures_close(duration); ures_close(bundle);
    return ret;
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

static JSValue duration_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    JSIntlDurationFormat *s;
    JSValue object, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolutionKey key = {"nu", NULL, FALSE};
    char *numbering_system = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int matcher;
    static const UListFormatterWidth widths[] = { ULISTFMT_WIDTH_WIDE,
        ULISTFMT_WIDTH_SHORT, ULISTFMT_WIDTH_NARROW, ULISTFMT_WIDTH_SHORT };
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
                               duration_matchers[matcher], &key, 1, &s->locale) < 0 ||
        duration_digital_data(ctx, s) < 0 ||
        js_intl_get_string_option(ctx, options, "style", base_styles,
                                  countof(base_styles), 1, &s->style) < 0 ||
        duration_options(ctx, s, options) < 0) goto fail;
    s->list_formatter = ulistfmt_openForType(s->locale.icu_locale, ULISTFMT_TYPE_UNITS,
                                             widths[s->style], &status);
    if (js_intl_icu_error(ctx, status, "duration list formatter") < 0) goto fail;
    JS_FreeValue(ctx, options); js_free(ctx, numbering_system);
    js_intl_locale_list_free(ctx, &requested);
    return object;
 fail:
    JS_FreeValue(ctx, options); js_free(ctx, numbering_system);
    js_intl_locale_list_free(ctx, &requested); JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static int duration_append_parts(JSContext *ctx, JSValueConst result,
                                  uint32_t *index, JSValueConst group,
                                  const char *unit)
{
    uint32_t count, i;
    JSValue length = JS_GetPropertyStr(ctx, group, "length");
    if (JS_IsException(length)) return -1;
    if (JS_ToUint32(ctx, &count, length) < 0) { JS_FreeValue(ctx, length); return -1; }
    JS_FreeValue(ctx, length);
    for (i = 0; i < count; i++) {
        JSValue part = JS_GetPropertyUint32(ctx, group, i);
        if (JS_IsException(part)) return -1;
        if (unit && js_intl_define_string(ctx, part, "unit", unit) < 0) {
            JS_FreeValue(ctx, part); return -1;
        }
        if (JS_DefinePropertyValueUint32(ctx, result, (*index)++, part, JS_PROP_C_W_E) < 0) return -1;
    }
    return 0;
}

static JSValue duration_number_parts(JSContext *ctx, JSIntlDurationFormat *s,
                                     const DurationRecord *record, int unit,
                                     int numeric, int show_sign, int *nonzero)
{
    JSValue options = JS_NewObjectProto(ctx, JS_NULL), locale, parts = JS_EXCEPTION;
    JSIntlMathematicalValue mv = {0};
    int fractional = unit >= 6 && unit < 9 && s->unit_style[unit + 1] == FRACTIONAL;
    if (JS_IsException(options)) return options;
    locale = JS_NewString(ctx, s->locale.locale);
    if (JS_IsException(locale)) goto done;
    if (js_intl_define_string(ctx, options, "numberingSystem", s->locale.values[0]) < 0) goto done;
    if (!show_sign && js_intl_define_string(ctx, options, "signDisplay", "never") < 0) goto done;
    if (numeric) {
        if (js_intl_define_bool(ctx, options, "useGrouping", FALSE) < 0) goto done;
        if (s->unit_style[unit] == 4 && js_intl_define_int(ctx, options, "minimumIntegerDigits", 2) < 0) goto done;
    } else {
        if (js_intl_define_string(ctx, options, "style", "unit") < 0 ||
            js_intl_define_string(ctx, options, "unit", number_units[unit]) < 0 ||
            js_intl_define_string(ctx, options, "unitDisplay", unit_styles[s->unit_style[unit]]) < 0)
            goto done;
    }
    if ((numeric && unit == 6) || fractional) {
        if (js_intl_define_int(ctx, options, "minimumFractionDigits", s->fractional_digits < 0 ? 0 : s->fractional_digits) < 0 ||
            js_intl_define_int(ctx, options, "maximumFractionDigits", s->fractional_digits < 0 ? 9 : s->fractional_digits) < 0 ||
            js_intl_define_string(ctx, options, "roundingMode", "trunc") < 0) goto done;
    }
    if (duration_value(ctx, s, record, unit, show_sign, &mv, nonzero) < 0) goto done;
    parts = js_intl_number_format_value(ctx, locale, options, &mv, 1);
 done:
    js_intl_free_mathematical_value(ctx, &mv);
    JS_FreeValue(ctx, locale); JS_FreeValue(ctx, options);
    return parts;
}

static JSValue duration_numeric_parts(JSContext *ctx, JSIntlDurationFormat *s,
                                      const DurationRecord *record, int first,
                                      int show_sign)
{
    JSValue result = JS_NewArray(ctx);
    uint32_t index = 0;
    int hour = first == 4 && (record->value[4] != 0 || s->unit_display[4]);
    int second = record->value[6] != 0 || s->unit_display[6];
    int minute, unit, nonzero;
    /* Fractional input can make seconds nonzero before truncation. */
    for (unit = 7; unit < 10 && s->unit_style[unit] == FRACTIONAL; unit++)
        if (record->value[unit]) second = 1;
    minute = first <= 5 && ((hour && second) || record->value[5] != 0 || s->unit_display[5]);
    if (JS_IsException(result)) return result;
    for (unit = first; unit <= 6; unit++) {
        JSValue group;
        if ((unit == 4 && !hour) || (unit == 5 && !minute) || (unit == 6 && !second)) continue;
        if ((unit == 5 && hour) || (unit == 6 && minute)) {
            UChar *separator = unit == 5 ? s->hour_minute_separator : s->minute_second_separator;
            int32_t length = unit == 5 ? s->hour_minute_length : s->minute_second_length;
            if (js_intl_add_part_uchar(ctx, result, index++, "literal", separator, length, NULL, JS_UNDEFINED) < 0)
                goto fail;
        }
        group = duration_number_parts(ctx, s, record, unit, 1, show_sign, &nonzero);
        if (JS_IsException(group)) goto fail;
        if (duration_append_parts(ctx, result, &index, group, number_units[unit]) < 0) {
            JS_FreeValue(ctx, group); goto fail;
        }
        JS_FreeValue(ctx, group); show_sign = 0;
    }
    return result;
 fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue duration_parts_string(JSContext *ctx, JSValueConst parts)
{
    JSValue length, result = JS_NewString(ctx, "");
    uint32_t count, i;
    if (JS_IsException(result)) return result;
    length = JS_GetPropertyStr(ctx, parts, "length");
    if (JS_IsException(length) || JS_ToUint32(ctx, &count, length) < 0) {
        JS_FreeValue(ctx, length); JS_FreeValue(ctx, result); return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, length);
    for (i = 0; i < count; i++) {
        JSValue part = JS_GetPropertyUint32(ctx, parts, i), value;
        if (JS_IsException(part)) { JS_FreeValue(ctx, result); return JS_EXCEPTION; }
        value = JS_GetPropertyStr(ctx, part, "value");
        JS_FreeValue(ctx, part);
        if (JS_IsException(value)) { JS_FreeValue(ctx, result); return JS_EXCEPTION; }
        result = JS_ConcatString(ctx, result, value);
        if (JS_IsException(result)) return result;
    }
    return result;
}

static JSValue duration_list_parts(JSContext *ctx, JSIntlDurationFormat *s,
                                   JSValue *groups, int count)
{
    UChar *strings[10] = {0};
    int32_t lengths[10] = {0}, length, start, end, field, category, at;
    const UChar *text;
    UErrorCode status = U_ZERO_ERROR;
    UFormattedList *formatted = NULL;
    UConstrainedFieldPosition *cursor = NULL;
    const UFormattedValue *value;
    JSValue result = JS_EXCEPTION;
    uint32_t index = 0;
    int i;
    /* ICU span fields carry element indices even when element strings equal. */
    for (i = 0; i < count; i++) {
        JSValue string = duration_parts_string(ctx, groups[i]);
        if (JS_IsException(string)) goto done;
        if (js_intl_to_uchar(ctx, string, &strings[i], &lengths[i]) < 0) {
            JS_FreeValue(ctx, string); goto done;
        }
        JS_FreeValue(ctx, string);
    }
    result = JS_NewArray(ctx);
    if (JS_IsException(result)) goto done;
    if (!count) goto done;
    formatted = ulistfmt_openResult(&status);
    if (js_intl_icu_error(ctx, status, "duration list result") < 0) goto fail;
    ulistfmt_formatStringsToResult(s->list_formatter, (const UChar *const *)strings,
                                   lengths, count, formatted, &status);
    value = ulistfmt_resultAsValue(formatted, &status);
    text = ufmtval_getString(value, &length, &status);
    cursor = ucfpos_open(&status);
    ucfpos_constrainCategory(cursor, UFIELD_CATEGORY_LIST_SPAN, &status);
    if (js_intl_icu_error(ctx, status, "duration list fields") < 0) goto fail;
    at = 0;
    while (ufmtval_nextPosition(value, cursor, &status)) {
        category = ucfpos_getCategory(cursor, &status);
        field = ucfpos_getField(cursor, &status);
        ucfpos_getIndexes(cursor, &start, &end, &status);
        if (U_FAILURE(status)) break;
        if (category != UFIELD_CATEGORY_LIST_SPAN || field < 0 || field >= count ||
            start < at || end < start || end > length) {
            JS_ThrowInternalError(ctx, "invalid ICU duration list span"); goto fail;
        }
        if (start > at && js_intl_add_part_uchar(ctx, result, index++, "literal",
                                                text + at, start - at, NULL, JS_UNDEFINED) < 0) goto fail;
        if (duration_append_parts(ctx, result, &index, groups[field], NULL) < 0) goto fail;
        at = end;
    }
    if (js_intl_icu_error(ctx, status, "duration list spans") < 0) goto fail;
    if (at < length && js_intl_add_part_uchar(ctx, result, index++, "literal", text + at,
                                             length - at, NULL, JS_UNDEFINED) < 0) goto fail;
    goto done;
 fail:
    JS_FreeValue(ctx, result); result = JS_EXCEPTION;
 done:
    ucfpos_close(cursor); ulistfmt_closeResult(formatted);
    for (i = 0; i < 10; i++) js_free(ctx, strings[i]);
    return result;
}

static JSValue duration_format_record(JSContext *ctx, JSIntlDurationFormat *s,
                                       const DurationRecord *record, int parts)
{
    JSValue groups[10], result = JS_EXCEPTION;
    int count = 0, unit, show_sign = 1;
    for (unit = 0; unit < 10; unit++) {
        JSValue group;
        int nonzero, stop = unit >= 6 && unit < 9 && s->unit_style[unit + 1] == FRACTIONAL;
        if (s->unit_style[unit] == 3 || s->unit_style[unit] == 4) {
            JSValue length;
            uint32_t n;
            group = duration_numeric_parts(ctx, s, record, unit, show_sign);
            if (JS_IsException(group)) goto done;
            length = JS_GetPropertyStr(ctx, group, "length");
            if (JS_IsException(length) || JS_ToUint32(ctx, &n, length) < 0) {
                JS_FreeValue(ctx, length); JS_FreeValue(ctx, group); goto done;
            }
            JS_FreeValue(ctx, length);
            if (n) groups[count++] = group;
            else JS_FreeValue(ctx, group);
            break;
        }
        /* Test exact value before rounding; subsecond values can contribute
           to an otherwise zero textual field. */
        nonzero = record->value[unit] != 0;
        if (stop) for (int i = unit + 1; i < 10 && s->unit_style[i] == FRACTIONAL; i++)
            if (record->value[i]) nonzero = 1;
        if (nonzero || s->unit_display[unit]) {
            JSValue numbered;
            uint32_t index = 0;
            numbered = duration_number_parts(ctx, s, record, unit, 0, show_sign, &nonzero);
            if (JS_IsException(numbered)) goto done;
            group = JS_NewArray(ctx);
            if (JS_IsException(group) || duration_append_parts(ctx, group, &index, numbered, number_units[unit]) < 0) {
                JS_FreeValue(ctx, numbered); JS_FreeValue(ctx, group); goto done;
            }
            JS_FreeValue(ctx, numbered);
            groups[count++] = group; show_sign = 0;
        }
        if (stop) break;
    }
    result = duration_list_parts(ctx, s, groups, count);
    if (!parts && !JS_IsException(result)) {
        JSValue string = duration_parts_string(ctx, result);
        JS_FreeValue(ctx, result); result = string;
    }
 done:
    for (unit = 0; unit < count; unit++) JS_FreeValue(ctx, groups[unit]);
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
