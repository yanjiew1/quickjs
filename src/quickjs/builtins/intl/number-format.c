/* Native Intl.NumberFormat. ECMA-402 7ae78cf (2026-10-06).
 * All JavaScript observable algorithms are C; ICU provides locale data/rendering. */
#include "intl-number.h"
#include <unicode/ucurr.h>
#include <unicode/uformattedvalue.h>
#include <unicode/unum.h>

static const char *const styles[] = { "decimal", "percent", "currency", "unit" };
static const char *const notations[] = { "standard", "scientific", "engineering", "compact" };
static const char *const compact_displays[] = { "short", "long" };
static const char *const currency_displays[] = { "code", "symbol", "narrowSymbol", "name" };
static const char *const currency_signs[] = { "standard", "accounting" };
static const char *const unit_displays[] = { "short", "narrow", "long" };
static const char *const signs[] = { "auto", "never", "always", "exceptZero", "negative" };
static const char *const groups[] = { "min2", "auto", "always", "true", "false" };
static const char *const matchers[] = { "lookup", "best fit" };

typedef struct JSIntlNumberFormat {
    JSIntlResolvedLocale locale;
    JSIntlDigitOptions digits;
    char *currency, *unit;
    int style, notation, compact_display, currency_display, currency_sign;
    int unit_display, use_grouping, sign_display;
    UNumberFormatter *formatter;
    UNumberRangeFormatter *range_formatter;
    JSValue bound_format;
} JSIntlNumberFormat;

typedef struct UnitType { const char *name, *type; } UnitType;
static const UnitType units[] = {
    {"acre","area"}, {"bit","digital"}, {"byte","digital"},
    {"celsius","temperature"}, {"centimeter","length"}, {"day","duration"},
    {"degree","angle"}, {"fahrenheit","temperature"}, {"fluid-ounce","volume"},
    {"foot","length"}, {"gallon","volume"}, {"gigabit","digital"},
    {"gigabyte","digital"}, {"gram","mass"}, {"hectare","area"},
    {"hour","duration"}, {"inch","length"}, {"kilobit","digital"},
    {"kilobyte","digital"}, {"kilogram","mass"}, {"kilometer","length"},
    {"liter","volume"}, {"megabit","digital"}, {"megabyte","digital"},
    {"meter","length"}, {"microsecond","duration"}, {"mile","length"},
    {"mile-scandinavian","length"}, {"milliliter","volume"},
    {"millimeter","length"}, {"millisecond","duration"}, {"minute","duration"},
    {"month","duration"}, {"nanosecond","duration"}, {"ounce","mass"},
    {"percent","concentr"}, {"petabyte","digital"}, {"pound","mass"},
    {"second","duration"}, {"stone","mass"}, {"terabit","digital"},
    {"terabyte","digital"}, {"week","duration"}, {"yard","length"},
    {"year","duration"}
};

static const UnitType *unit_type(const char *name, size_t length)
{
    int i;
    for (i = 0; i < countof(units); i++)
        if (strlen(units[i].name) == length && !memcmp(units[i].name, name, length))
            return &units[i];
    return NULL;
}

int js_intl_number_unit_valid(const char *name)
{
    const char *per = strstr(name, "-per-");
    if (!per) return unit_type(name, strlen(name)) != NULL;
    return unit_type(name, per - name) && unit_type(per + 5, strlen(per + 5));
}

static void number_free(JSRuntime *rt, JSIntlNumberFormat *s)
{
    int i;
    if (!s) return;
    unumf_close(s->formatter);
    unumrf_close(s->range_formatter);
    js_free_rt(rt, s->locale.locale); js_free_rt(rt, s->locale.data_locale);
    js_free_rt(rt, s->locale.icu_locale);
    for (i = 0; i < s->locale.key_count; i++) js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s->currency); js_free_rt(rt, s->unit);
    JS_FreeValueRT(rt, s->bound_format);
    js_free_rt(rt, s);
}

static void number_finalizer(JSRuntime *rt, JSValue value)
{
    number_free(rt, JS_GetOpaque(value, JS_CLASS_INTL_NUMBER_FORMAT));
}

static void number_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark)
{
    JSIntlNumberFormat *s = JS_GetOpaque(value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (s) {
        JS_MarkValue(rt, s->bound_format, mark);
    }
}

static int unicode_type_valid(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    int length = 0;
    for (;;) {
        if (*p == '-' || !*p) {
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

static int use_grouping_option(JSContext *ctx, JSValueConst options,
                               int fallback, int *result)
{
    JSValue value = JS_GetPropertyStr(ctx, options, "useGrouping");
    const char *text;
    size_t length;
    int i, truth, ret = -1;
    if (JS_IsException(value)) return -1;
    if (JS_IsUndefined(value)) { *result = fallback; ret = 0; goto done; }
    truth = JS_ToBool(ctx, value);
    if (truth < 0) goto done;
    if (!truth) { *result = -1; ret = 0; goto done; }
    if (JS_IsBool(value)) { *result = 2; ret = 0; goto done; }
    text = JS_ToCStringLen(ctx, &length, value);
    if (!text) goto done;
    for (i = 0; i < countof(groups); i++)
        if (length == strlen(groups[i]) && !memcmp(text, groups[i], length)) break;
    JS_FreeCString(ctx, text);
    if (i == countof(groups)) { JS_ThrowRangeError(ctx, "invalid useGrouping"); goto done; }
    *result = i >= 3 ? fallback : i;
    ret = 0;
 done:
    JS_FreeValue(ctx, value);
    return ret;
}

static int number_unit_options(JSContext *ctx, JSValueConst options,
                               JSIntlNumberFormat *s)
{
    char *currency = NULL, *unit = NULL;
    int i, currency_display, currency_sign, unit_display, ret = -1;
    if (js_intl_get_string_option(ctx, options, "style", styles, countof(styles),
                                  0, &s->style) < 0 ||
        js_intl_get_string_option_alloc(ctx, options, "currency", &currency) < 0)
        goto done;
    if (!currency) {
        if (s->style == 2) { JS_ThrowTypeError(ctx, "currency is required"); goto done; }
    } else {
        if (strlen(currency) != 3) { JS_ThrowRangeError(ctx, "invalid currency"); goto done; }
        for (i = 0; i < 3; i++) {
            if (currency[i] >= 'a' && currency[i] <= 'z') currency[i] -= 'a' - 'A';
            if (currency[i] < 'A' || currency[i] > 'Z') {
                JS_ThrowRangeError(ctx, "invalid currency"); goto done;
            }
        }
    }
    if (js_intl_get_string_option(ctx, options, "currencyDisplay", currency_displays,
                                  countof(currency_displays), 1, &currency_display) < 0 ||
        js_intl_get_string_option(ctx, options, "currencySign", currency_signs,
                                  countof(currency_signs), 0, &currency_sign) < 0 ||
        js_intl_get_string_option_alloc(ctx, options, "unit", &unit) < 0)
        goto done;
    if (!unit) {
        if (s->style == 3) { JS_ThrowTypeError(ctx, "unit is required"); goto done; }
    } else if (!js_intl_number_unit_valid(unit)) {
        JS_ThrowRangeError(ctx, "invalid unit"); goto done;
    }
    if (js_intl_get_string_option(ctx, options, "unitDisplay", unit_displays,
                                  countof(unit_displays), 0, &unit_display) < 0)
        goto done;
    if (s->style == 2) {
        s->currency = currency; currency = NULL;
        s->currency_display = currency_display; s->currency_sign = currency_sign;
    }
    if (s->style == 3) {
        s->unit = unit; unit = NULL; s->unit_display = unit_display;
    }
    ret = 0;
 done:
    js_free(ctx, currency); js_free(ctx, unit);
    return ret;
}

static int number_skeleton(JSContext *ctx, JSIntlNumberFormat *s, DynBuf *b)
{
    static const char *const notation_tokens[] = {
        "", "scientific ", "engineering ", "compact-short "
    };
    static const char *const group_tokens[] = { "group-min2 ", "group-auto ", "group-on-aligned " };
    static const char *const sign_tokens[] = {
        "sign-auto ", "sign-never ", "sign-always ", "sign-except-zero ", "sign-negative "
    };
    static const char *const accounting_tokens[] = {
        "sign-accounting ", "sign-never ", "sign-accounting-always ",
        "sign-accounting-except-zero ", "sign-accounting-negative "
    };
    if (s->style == 1) dbuf_putstr(b, "percent scale/100 ");
    if (s->style == 2) {
        static const char *const widths[] = { "iso-code", "short", "narrow", "full-name" };
        dbuf_printf(b, "currency/%s unit-width-%s ", s->currency, widths[s->currency_display]);
    }
    if (s->style == 3) {
        const char *per = strstr(s->unit, "-per-");
        const UnitType *numerator = unit_type(s->unit, per ? (size_t)(per - s->unit) : strlen(s->unit));
        static const char *const widths[] = { "short", "narrow", "full-name" };
        dbuf_printf(b, "measure-unit/%s-%s ", numerator->type, numerator->name);
        if (per) {
            const UnitType *denominator = unit_type(per + 5, strlen(per + 5));
            dbuf_printf(b, "per-measure-unit/%s-%s ", denominator->type, denominator->name);
        }
        dbuf_printf(b, "unit-width-%s ", widths[s->unit_display]);
    }
    if (s->notation == 3 && s->compact_display == 1) dbuf_putstr(b, "compact-long ");
    else dbuf_putstr(b, notation_tokens[s->notation]);
    if (js_intl_digit_skeleton(ctx, b, &s->digits) < 0) return -1;
    dbuf_putstr(b, s->use_grouping < 0 ? "group-off " : group_tokens[s->use_grouping]);
    dbuf_putstr(b, s->style == 2 && s->currency_sign ? accounting_tokens[s->sign_display] :
                                                  sign_tokens[s->sign_display]);
    return dbuf_error(b) ? (JS_ThrowOutOfMemory(ctx), -1) : 0;
}

static int number_initialize(JSContext *ctx, JSIntlNumberFormat *s,
                             JSValueConst locales, JSValueConst options_arg)
{
    JSIntlLocaleList requested = {0};
    JSValue options = JS_UNDEFINED;
    char *numbering_system = NULL;
    JSIntlResolutionKey key = { "nu", NULL, FALSE };
    DynBuf skeleton;
    UChar *utf16 = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int matcher, mnfd = 0, mxfd = 3, ret = -1;
    js_dbuf_init(ctx, &skeleton);
    if (js_intl_canonicalize_locale_list(ctx, locales, &requested) < 0) goto done;
    options = js_intl_coerce_options(ctx, options_arg);
    if (JS_IsException(options)) goto done;
    if (js_intl_get_string_option(ctx, options, "localeMatcher", matchers,
                                  countof(matchers), 1, &matcher) < 0 ||
        js_intl_get_string_option_alloc(ctx, options, "numberingSystem", &numbering_system) < 0)
        goto done;
    if (numbering_system && !unicode_type_valid(numbering_system)) {
        JS_ThrowRangeError(ctx, "invalid numberingSystem"); goto done;
    }
    key.option = numbering_system;
    if (js_intl_resolve_locale(ctx, JS_INTL_NUMBER_FORMAT, &requested, matchers[matcher],
                               &key, 1, &s->locale) < 0 ||
        number_unit_options(ctx, options, s) < 0 ||
        js_intl_get_string_option(ctx, options, "notation", notations,
                                  countof(notations), 0, &s->notation) < 0)
        goto done;
    if (s->style == 2 && s->notation == 0) {
        UChar currency[4];
        int i;
        for (i = 0; i < 4; i++) currency[i] = s->currency[i];
        mnfd = mxfd = ucurr_getDefaultFractionDigits(currency, &status);
        if (js_intl_icu_error(ctx, status, "currency digits") < 0) goto done;
    } else if (s->style == 1) mxfd = 0;
    if (js_intl_set_digit_options(ctx, options, mnfd, mxfd, s->notation == 3, &s->digits) < 0 ||
        js_intl_get_string_option(ctx, options, "compactDisplay", compact_displays,
                                  countof(compact_displays), 0, &s->compact_display) < 0 ||
        use_grouping_option(ctx, options, s->notation == 3 ? 0 : 1, &s->use_grouping) < 0 ||
        js_intl_get_string_option(ctx, options, "signDisplay", signs,
                                  countof(signs), 0, &s->sign_display) < 0 ||
        number_skeleton(ctx, s, &skeleton) < 0)
        goto done;
    if (skeleton.size > INT32_MAX) { JS_ThrowOutOfMemory(ctx); goto done; }
    utf16 = js_intl_alloc_uchar(ctx, (int32_t)skeleton.size);
    if (!utf16) goto done;
    for (size_t i = 0; i < skeleton.size; i++) utf16[i] = skeleton.buf[i];
    s->formatter = unumf_openForSkeletonAndLocale(utf16, (int32_t)skeleton.size,
                                                  s->locale.icu_locale, &status);
    if (js_intl_icu_error(ctx, status, "number formatter") < 0) goto done;
    s->range_formatter = unumrf_openForSkeletonWithCollapseAndIdentityFallback(
        utf16, (int32_t)skeleton.size, UNUM_RANGE_COLLAPSE_AUTO,
        UNUM_IDENTITY_FALLBACK_APPROXIMATELY, s->locale.icu_locale, NULL, &status);
    if (js_intl_icu_error(ctx, status, "number range formatter") < 0) goto done;
    ret = 0;
 done:
    dbuf_free(&skeleton); js_free(ctx, utf16); js_free(ctx, numbering_system);
    JS_FreeValue(ctx, options); js_intl_locale_list_free(ctx, &requested);
    return ret;
}

static JSValue number_constructor(JSContext *ctx, JSValueConst this_value,
                                 JSValueConst new_target, int argc, JSValueConst *argv)
{
    JSValue object;
    JSValueConst target;
    JSIntlNumberFormat *s;
#ifdef CONFIG_INTL_LEGACY
    JSValueConst symbol;
    JSAtom atom;
    int chain;
#else
    (void)this_value;
#endif
    target = JS_IsUndefined(new_target) ? js_intl_constructor(ctx, JS_CLASS_INTL_NUMBER_FORMAT) : new_target;
    object = js_intl_new_object(ctx, target, JS_CLASS_INTL_NUMBER_FORMAT);
    if (JS_IsException(object)) return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) { JS_FreeValue(ctx, object); return JS_EXCEPTION; }
    s->bound_format = JS_UNDEFINED;
    JS_SetOpaque(object, s);
    if (number_initialize(ctx, s, argc > 0 ? argv[0] : JS_UNDEFINED,
                          argc > 1 ? argv[1] : JS_UNDEFINED) < 0)
        goto fail;
#ifdef CONFIG_INTL_LEGACY
    if (JS_IsUndefined(new_target)) {
        chain = JS_OrdinaryIsInstanceOf(ctx, this_value,
                                        js_intl_constructor(ctx, JS_CLASS_INTL_NUMBER_FORMAT));
        if (chain < 0) goto fail;
        if (chain) {
            symbol = js_intl_fallback_symbol(ctx);
            atom = JS_ValueToAtom(ctx, symbol);
            if (atom == JS_ATOM_NULL) goto fail;
            chain = JS_DefinePropertyValue(ctx, this_value, atom, JS_DupValue(ctx, object),
                                           JS_PROP_THROW);
            JS_FreeAtom(ctx, atom);
            if (chain < 0) goto fail;
            JS_FreeValue(ctx, object);
            return JS_DupValue(ctx, this_value);
        }
    }
#endif
    return object;
 fail:
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue number_unwrap(JSContext *ctx, JSValueConst value)
{
#ifdef CONFIG_INTL_LEGACY
    int instance;
    JSAtom atom;
    JSValue result;
    if (JS_GetOpaque(value, JS_CLASS_INTL_NUMBER_FORMAT)) return JS_DupValue(ctx, value);
    instance = JS_OrdinaryIsInstanceOf(ctx, value,
                                      js_intl_constructor(ctx, JS_CLASS_INTL_NUMBER_FORMAT));
    if (instance < 0) return JS_EXCEPTION;
    if (instance) {
        atom = JS_ValueToAtom(ctx, js_intl_fallback_symbol(ctx));
        if (atom == JS_ATOM_NULL) return JS_EXCEPTION;
        result = JS_GetProperty(ctx, value, atom);
        JS_FreeAtom(ctx, atom);
        return result;
    }
#endif
    return JS_DupValue(ctx, value);
}

static const char *field_type(int field, const JSIntlMathematicalValue *mv)
{
    switch (field) {
    case UNUM_INTEGER_FIELD:
        if (mv && mv->kind == JS_INTL_MV_NAN) return "nan";
        if (mv && (mv->kind == JS_INTL_MV_POSITIVE_INFINITY ||
                   mv->kind == JS_INTL_MV_NEGATIVE_INFINITY)) return "infinity";
        return "integer";
    case UNUM_FRACTION_FIELD: return "fraction";
    case UNUM_DECIMAL_SEPARATOR_FIELD: return "decimal";
    case UNUM_EXPONENT_SYMBOL_FIELD: return "exponentSeparator";
    case UNUM_EXPONENT_SIGN_FIELD: return "exponentMinusSign";
    case UNUM_EXPONENT_FIELD: return "exponentInteger";
    case UNUM_GROUPING_SEPARATOR_FIELD: return "group";
    case UNUM_CURRENCY_FIELD: return "currency";
    case UNUM_PERCENT_FIELD: return "percentSign";
    case UNUM_SIGN_FIELD:
        return mv && (mv->kind == JS_INTL_MV_NEGATIVE_INFINITY ||
                       mv->kind == JS_INTL_MV_NEGATIVE_ZERO ||
                       (mv->decimal && mv->decimal[0] == '-')) ? "minusSign" : "plusSign";
    case UNUM_MEASURE_UNIT_FIELD: return "unit";
    case UNUM_COMPACT_FIELD: return "compact";
    case UNUM_APPROXIMATELY_SIGN_FIELD: return "approximatelySign";
    default: return "literal";
    }
}

/* ICU fields may overlap (integer includes grouping). Classify each UTF16
   code unit, giving specific number fields priority over integer envelopes. */
JSValue js_intl_number_parts(JSContext *ctx, const UFormattedValue *formatted,
                            const JSIntlMathematicalValue *x,
                            const JSIntlMathematicalValue *y, int range)
{
    UErrorCode status = U_ZERO_ERROR;
    UConstrainedFieldPosition *position = NULL;
    const UChar *text;
    int32_t length, start, limit, category, field, at, end;
    int *fields = NULL, *sources = NULL;
    JSValue result = JS_UNDEFINED;
    uint32_t index = 0;
    text = ufmtval_getString(formatted, &length, &status);
    if (js_intl_icu_error(ctx, status, "number parts text") < 0) goto fail;
    if (length < 0) {
        JS_ThrowInternalError(ctx, "invalid ICU number text length"); goto fail;
    }
    if ((size_t)length > SIZE_MAX / sizeof(*fields)) {
        JS_ThrowOutOfMemory(ctx); goto fail;
    }
    if (length > 0) {
        fields = js_malloc(ctx, sizeof(*fields) * (size_t)length);
        sources = js_malloc(ctx, sizeof(*sources) * (size_t)length);
        if (!fields || !sources) goto fail;
        for (at = 0; at < length; at++) { fields[at] = -1; sources[at] = 2; }
    }
    position = ucfpos_open(&status);
    if (js_intl_icu_error(ctx, status, "number parts cursor") < 0) goto fail;
    while (ufmtval_nextPosition(formatted, position, &status)) {
        category = ucfpos_getCategory(position, &status);
        field = ucfpos_getField(position, &status);
        ucfpos_getIndexes(position, &start, &limit, &status);
        if (U_FAILURE(status)) break;
        if (start < 0 || limit < start || limit > length) {
            JS_ThrowInternalError(ctx, "invalid ICU number part span"); goto fail;
        }
        for (at = start; at < limit; at++) {
            if (category == UFIELD_CATEGORY_NUMBER &&
                (field != UNUM_INTEGER_FIELD || fields[at] < 0)) fields[at] = field;
            if (category == UFIELD_CATEGORY_NUMBER_RANGE_SPAN) sources[at] = field;
        }
    }
    if (js_intl_icu_error(ctx, status, "number parts fields") < 0) goto fail;
    result = JS_NewArray(ctx);
    if (JS_IsException(result)) goto fail;
    for (at = 0; at < length; at = end) {
        const char *source = sources[at] == 0 ? "startRange" :
                             sources[at] == 1 ? "endRange" : "shared";
        const JSIntlMathematicalValue *mv = sources[at] == 1 ? y : x;
        JSValue source_value = range ? JS_NewString(ctx, source) : JS_UNDEFINED;
        for (end = at + 1; end < length && fields[end] == fields[at] &&
             (!range || sources[end] == sources[at]); end++);
        if (JS_IsException(source_value) ||
            js_intl_add_part_uchar(ctx, result, index++, field_type(fields[at], mv),
                                   text + at, end - at, range ? "source" : NULL,
                                   source_value) < 0) {
            JS_FreeValue(ctx, source_value); goto fail;
        }
        JS_FreeValue(ctx, source_value);
    }
    ucfpos_close(position); js_free(ctx, fields); js_free(ctx, sources);
    return result;
 fail:
    ucfpos_close(position); js_free(ctx, fields); js_free(ctx, sources);
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

/* ICU shares percent patterns between the unit and percent services and
   uses measure-unit fields for compact percent data. Restore ECMA-402
   field names from the service style, independently of ICU pattern choice. */
static JSValue number_style_parts(JSContext *ctx, JSIntlNumberFormat *s,
                                   const UFormattedValue *formatted,
                                   const JSIntlMathematicalValue *x,
                                   const JSIntlMathematicalValue *y, int range)
{
    JSValue result = js_intl_number_parts(ctx, formatted, x, y, range);
    uint32_t length, i;
    JSValue length_value;
    if (JS_IsException(result) || (s->style != 1 && s->style != 3)) return result;
    length_value = JS_GetPropertyStr(ctx, result, "length");
    if (JS_IsException(length_value) || JS_ToUint32(ctx, &length, length_value) < 0) {
        JS_FreeValue(ctx, length_value); goto fail;
    }
    JS_FreeValue(ctx, length_value);
    for (i = 0; i < length; i++) {
        JSValue part = JS_GetPropertyUint32(ctx, result, i), type;
        const char *text;
        int replace;
        if (JS_IsException(part)) goto fail;
        type = JS_GetPropertyStr(ctx, part, "type");
        if (JS_IsException(type)) { JS_FreeValue(ctx, part); goto fail; }
        text = JS_ToCString(ctx, type);
        JS_FreeValue(ctx, type);
        if (!text) { JS_FreeValue(ctx, part); goto fail; }
        replace = (s->style == 3 && !strcmp(text, "percentSign")) ||
                  (s->style == 1 && !strcmp(text, "unit"));
        JS_FreeCString(ctx, text);
        if (replace && js_intl_define_string(ctx, part, "type", s->style == 3 ? "unit" : "percentSign") < 0) {
            JS_FreeValue(ctx, part); goto fail;
        }
        JS_FreeValue(ctx, part);
    }
    return result;
 fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue number_format_mv(JSContext *ctx, JSIntlNumberFormat *s,
                               const JSIntlMathematicalValue *mv, int parts)
{
    UErrorCode status = U_ZERO_ERROR;
    UFormattedNumber *formatted = unumf_openResult(&status);
    JSValue result = JS_EXCEPTION;
    const UFormattedValue *value;
    int32_t length;
    const UChar *text;
    if (js_intl_icu_error(ctx, status, "number result") < 0) goto done;
    if (js_intl_format_mathematical_value(ctx, s->formatter, mv, formatted, &status) < 0) goto done;
    value = unumf_resultAsValue(formatted, &status);
    if (js_intl_icu_error(ctx, status, "number result value") < 0) goto done;
    if (parts) result = number_style_parts(ctx, s, value, mv, NULL, 0);
    else {
        text = ufmtval_getString(value, &length, &status);
        if (js_intl_icu_error(ctx, status, "number result string") < 0) goto done;
        result = js_intl_from_uchar(ctx, text, length);
    }
 done:
    unumf_closeResult(formatted);
    return result;
}

static JSValue number_bound_format(JSContext *ctx, JSValueConst this_value,
                                   int argc, JSValueConst *argv, int magic,
                                   JSValue *data)
{
    JSIntlNumberFormat *s = JS_GetOpaque(data[0], JS_CLASS_INTL_NUMBER_FORMAT);
    JSIntlMathematicalValue mv;
    JSValue result;
    if (js_intl_to_mathematical_value(ctx, argc ? argv[0] : JS_UNDEFINED, &mv) < 0)
        return JS_EXCEPTION;
    result = number_format_mv(ctx, s, &mv, 0);
    js_intl_free_mathematical_value(ctx, &mv);
    return result;
}

static JSValue number_get_format(JSContext *ctx, JSValueConst this_value)
{
    JSValue value = number_unwrap(ctx, this_value), function;
    JSIntlNumberFormat *s;
    if (JS_IsException(value)) return value;
    s = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (!s) { JS_FreeValue(ctx, value); return JS_EXCEPTION; }
    if (JS_IsUndefined(s->bound_format)) {
        JSValueConst capture = value;
        function = js_intl_new_c_function_data(ctx, number_bound_format, 1, 0, 1, &capture);
        if (JS_IsException(function)) { JS_FreeValue(ctx, value); return function; }
        s->bound_format = function;
    }
    JS_FreeValue(ctx, value);
    return JS_DupValue(ctx, s->bound_format);
}

static JSValue number_format_parts(JSContext *ctx, JSValueConst this_value,
                                   int argc, JSValueConst *argv)
{
    JSIntlNumberFormat *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_NUMBER_FORMAT);
    JSIntlMathematicalValue mv;
    JSValue result;
    if (!s) return JS_EXCEPTION;
    if (js_intl_to_mathematical_value(ctx, argc ? argv[0] : JS_UNDEFINED, &mv) < 0)
        return JS_EXCEPTION;
    result = number_format_mv(ctx, s, &mv, 1);
    js_intl_free_mathematical_value(ctx, &mv);
    return result;
}

/* PartitionNumberRangePattern compares final localized strings, rather
   than the signed DecimalQuantity identity used by ICU. In particular,
   hidden signs can make distinct signed quantities produce equal text. */
static int number_rendered_equal(JSContext *ctx, JSIntlNumberFormat *s,
                                  const JSIntlMathematicalValue *x,
                                  const JSIntlMathematicalValue *y, int *equal)
{
    UErrorCode status = U_ZERO_ERROR;
    UFormattedNumber *first = NULL, *second = NULL;
    const UFormattedValue *first_value, *second_value;
    const UChar *first_text, *second_text;
    int32_t first_length, second_length;
    int result = -1;
    *equal = 0;
    first = unumf_openResult(&status);
    second = unumf_openResult(&status);
    if (js_intl_icu_error(ctx, status, "range equality results") < 0 ||
        js_intl_format_mathematical_value(ctx, s->formatter, x, first, &status) < 0 ||
        js_intl_format_mathematical_value(ctx, s->formatter, y, second, &status) < 0)
        goto done;
    first_value = unumf_resultAsValue(first, &status);
    second_value = unumf_resultAsValue(second, &status);
    if (js_intl_icu_error(ctx, status, "range equality values") < 0) goto done;
    first_text = ufmtval_getString(first_value, &first_length, &status);
    second_text = ufmtval_getString(second_value, &second_length, &status);
    if (js_intl_icu_error(ctx, status, "range equality strings") < 0) goto done;
    if (first_length < 0 || second_length < 0) {
        JS_ThrowInternalError(ctx, "invalid ICU range equality text length"); goto done;
    }
    if ((size_t)first_length > SIZE_MAX / sizeof(*first_text)) {
        JS_ThrowOutOfMemory(ctx); goto done;
    }
    *equal = first_length == second_length &&
             (!first_length || !memcmp(first_text, second_text,
                                        (size_t)first_length * sizeof(*first_text)));
    result = 0;
 done:
    unumf_closeResult(first); unumf_closeResult(second);
    return result;
}

static JSValue number_format_range(JSContext *ctx, JSValueConst this_value,
                                  int argc, JSValueConst *argv, int parts)
{
    JSIntlNumberFormat *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_NUMBER_FORMAT);
    JSIntlMathematicalValue x = {0}, y = {0};
    UErrorCode status = U_ZERO_ERROR;
    UFormattedNumberRange *formatted = NULL;
    const UFormattedValue *value;
    JSValue result = JS_EXCEPTION;
    const UChar *text;
    int32_t length;
    int equal;
    if (!s) return result;
    if (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_ThrowTypeError(ctx, "range requires two values");
    if (js_intl_to_mathematical_value(ctx, argv[0], &x) < 0 ||
        js_intl_to_mathematical_value(ctx, argv[1], &y) < 0) goto done;
    if (x.kind == JS_INTL_MV_NAN || y.kind == JS_INTL_MV_NAN) {
        JS_ThrowRangeError(ctx, "NaN range endpoint"); goto done;
    }
    if (number_rendered_equal(ctx, s, &x, &y, &equal) < 0) goto done;
    formatted = unumrf_openResult(&status);
    /* Equal strings use the first endpoint twice, ensuring ICU's
       approximation pattern retains its parts and marks all sources shared. */
    if (js_intl_icu_error(ctx, status, "number range result") < 0 ||
        js_intl_format_mathematical_range(ctx, s->range_formatter, &x,
                                         equal ? &x : &y,
                                         formatted, &status) < 0) goto done;
    value = unumrf_resultAsValue(formatted, &status);
    if (js_intl_icu_error(ctx, status, "number range value") < 0) goto done;
    if (parts) result = number_style_parts(ctx, s, value, &x, &y, 1);
    else {
        text = ufmtval_getString(value, &length, &status);
        if (js_intl_icu_error(ctx, status, "number range string") < 0) goto done;
        result = js_intl_from_uchar(ctx, text, length);
    }
 done:
    unumrf_closeResult(formatted);
    js_intl_free_mathematical_value(ctx, &x); js_intl_free_mathematical_value(ctx, &y);
    return result;
}

static JSValue number_resolved_options(JSContext *ctx, JSValueConst this_value,
                                       int argc, JSValueConst *argv)
{
    JSValue value = number_unwrap(ctx, this_value), result;
    JSIntlNumberFormat *s;
    JSIntlDigitOptions *d;
    static const char *const modes[] = {
        "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor", "halfExpand", "halfTrunc", "halfEven"
    };
    static const char *const priorities[] = { "auto", "morePrecision", "lessPrecision" };
    if (JS_IsException(value)) return value;
    s = JS_GetOpaque2(ctx, value, JS_CLASS_INTL_NUMBER_FORMAT);
    if (!s) { JS_FreeValue(ctx, value); return JS_EXCEPTION; }
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) { JS_FreeValue(ctx, value); return result; }
    d = &s->digits;
#define STRING(k,v) do { if (js_intl_define_string(ctx, result, k, v) < 0) goto fail; } while (0)
#define INT(k,v) do { if (js_intl_define_int(ctx, result, k, v) < 0) goto fail; } while (0)
    STRING("locale", s->locale.locale); STRING("numberingSystem", s->locale.values[0]);
    STRING("style", styles[s->style]);
    if (s->style == 2) {
        STRING("currency", s->currency); STRING("currencyDisplay", currency_displays[s->currency_display]);
        STRING("currencySign", currency_signs[s->currency_sign]);
    }
    if (s->style == 3) { STRING("unit", s->unit); STRING("unitDisplay", unit_displays[s->unit_display]); }
    INT("minimumIntegerDigits", d->minimum_integer_digits);
    if (d->minimum_fraction_digits >= 0) {
        INT("minimumFractionDigits", d->minimum_fraction_digits); INT("maximumFractionDigits", d->maximum_fraction_digits);
    }
    if (d->minimum_significant_digits >= 0) {
        INT("minimumSignificantDigits", d->minimum_significant_digits); INT("maximumSignificantDigits", d->maximum_significant_digits);
    }
    if (s->use_grouping < 0) {
        if (js_intl_define_bool(ctx, result, "useGrouping", FALSE) < 0) goto fail;
    } else STRING("useGrouping", groups[s->use_grouping]);
    STRING("notation", notations[s->notation]);
    if (s->notation == 3) STRING("compactDisplay", compact_displays[s->compact_display]);
    STRING("signDisplay", signs[s->sign_display]); INT("roundingIncrement", d->rounding_increment);
    STRING("roundingMode", modes[d->rounding_mode]); STRING("roundingPriority", priorities[d->rounding_priority]);
    STRING("trailingZeroDisplay", d->trailing_zero_display ? "stripIfInteger" : "auto");
#undef STRING
#undef INT
    JS_FreeValue(ctx, value);
    return result;
 fail:
    JS_FreeValue(ctx, result); JS_FreeValue(ctx, value);
    return JS_EXCEPTION;
}

static JSValue number_supported_locales(JSContext *ctx, JSValueConst this_value,
                                        int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_NUMBER_FORMAT,
                                     argc ? argv[0] : JS_UNDEFINED,
                                     argc > 1 ? argv[1] : JS_UNDEFINED);
}

JSValue js_intl_number_format_value(JSContext *ctx, JSValueConst locales,
                                   JSValueConst options,
                                   const JSIntlMathematicalValue *value, int parts)
{
    JSIntlNumberFormat *s;
    JSValue result = JS_EXCEPTION;
    if (js_intl_ensure_service(ctx, JS_INTL_NUMBER_FORMAT) < 0) return result;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) return result;
    s->bound_format = JS_UNDEFINED;
    if (number_initialize(ctx, s, locales, options) == 0)
        result = number_format_mv(ctx, s, value, parts);
    number_free(ctx->rt, s);
    return result;
}

JSValue js_intl_number_to_locale_string(JSContext *ctx, JSValueConst number,
                                       JSValueConst locales, JSValueConst options)
{
    JSIntlMathematicalValue value;
    JSValue result;
    if (js_intl_to_mathematical_value(ctx, number, &value) < 0) return JS_EXCEPTION;
    result = js_intl_number_format_value(ctx, locales, options, &value, 0);
    js_intl_free_mathematical_value(ctx, &value);
    return result;
}

static const JSCFunctionListEntry number_static_functions[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, number_supported_locales),
};
static const JSCFunctionListEntry number_prototype_functions[] = {
    JS_CGETSET_DEF("format", number_get_format, NULL),
    JS_CFUNC_DEF("formatToParts", 1, number_format_parts),
    JS_CFUNC_MAGIC_DEF("formatRange", 2, number_format_range, 0),
    JS_CFUNC_MAGIC_DEF("formatRangeToParts", 2, number_format_range, 1),
    JS_CFUNC_DEF("resolvedOptions", 0, number_resolved_options),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.NumberFormat", JS_PROP_CONFIGURABLE),
};
static const JSClassDef number_class = {
    "NumberFormat", .finalizer = number_finalizer, .gc_mark = number_mark,
};

int js_intl_init_number_format(JSContext *ctx, JSValueConst intl)
{
    JSCFunctionType function;
    function.constructor_or_func_receiver = number_constructor;
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_NUMBER_FORMAT,
        &number_class, "NumberFormat", function.generic, 0,
        JS_CFUNC_constructor_or_func_receiver,
        number_static_functions, countof(number_static_functions),
        number_prototype_functions, countof(number_prototype_functions));
}
