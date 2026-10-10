/* Native Intl.NumberFormat. ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd; reviewed 2026-10-09.
 * All JavaScript observable algorithms are C; generated CLDR supplies native data. */
#include "intl-number.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)

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
    QJSIntlNativeNumber *formatter;
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
    qjs_intl_native_number_close(s->formatter);
    js_free_rt(rt, s->locale.locale); js_free_rt(rt, s->locale.data_locale);
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

static int number_initialize(JSContext *ctx, JSIntlNumberFormat *s,
                             JSValueConst locales, JSValueConst options_arg)
{
    JSIntlLocaleList requested = {0};
    JSValue options = JS_UNDEFINED;
    char *numbering_system = NULL;
    JSIntlResolutionKey key = { "nu", NULL, FALSE };
    QJSIntlNumberOptions native = {0};
    QJSIntlProvider *provider;
    QJSIntlStatus status;
    unsigned int currency_digits;
    int matcher, mnfd = 0, mxfd = 3, ret = -1;
    static const QJSIntlNumberGrouping grouping[] = { QJS_INTL_NUMBER_GROUP_MIN2,
        QJS_INTL_NUMBER_GROUP_AUTO, QJS_INTL_NUMBER_GROUP_ALWAYS };
    static const QJSIntlCurrencyDisplay currency[] = { QJS_INTL_CURRENCY_CODE,
        QJS_INTL_CURRENCY_SYMBOL, QJS_INTL_CURRENCY_NARROW_SYMBOL, QJS_INTL_CURRENCY_NAME };
    static const QJSIntlUnitDisplay unit[] = { QJS_INTL_UNIT_SHORT,
        QJS_INTL_UNIT_NARROW, QJS_INTL_UNIT_LONG };
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
    provider = js_intl_native_provider(ctx);
    if (!provider) goto done;
    if (s->style == 2 && s->notation == 0) {
        status = qjs_intl_native_provider_number_currency_digits(provider,
            (QJSIntlBytes){s->currency, strlen(s->currency)}, &currency_digits);
        if (js_intl_native_error(ctx, status, "CurrencyDigits")) goto done;
        mnfd = mxfd = currency_digits;
    } else if (s->style == 1) mxfd = 0;
    if (js_intl_set_digit_options(ctx, options, mnfd, mxfd, s->notation == 3, &s->digits) < 0 ||
        js_intl_get_string_option(ctx, options, "compactDisplay", compact_displays,
                                  countof(compact_displays), 0, &s->compact_display) < 0 ||
        use_grouping_option(ctx, options, s->notation == 3 ? 0 : 1, &s->use_grouping) < 0 ||
        js_intl_get_string_option(ctx, options, "signDisplay", signs,
                                  countof(signs), 0, &s->sign_display) < 0)
        goto done;
    native.style = (QJSIntlNumberStyle)s->style;
    native.notation = (QJSIntlNumberNotation)s->notation;
    native.sign_display = (QJSIntlNumberSign)s->sign_display;
    native.grouping = s->use_grouping < 0 ? QJS_INTL_NUMBER_GROUP_OFF : grouping[s->use_grouping];
    native.currency_display = currency[s->currency_display];
    native.currency_accounting = s->currency_sign;
    native.unit_display = unit[s->unit_display];
    native.compact_display = s->compact_display ? QJS_INTL_COMPACT_LONG : QJS_INTL_COMPACT_SHORT;
    native.currency = (QJSIntlBytes){s->currency, s->currency ? strlen(s->currency) : 0};
    native.unit = (QJSIntlBytes){s->unit, s->unit ? strlen(s->unit) : 0};
    js_intl_native_digit_options(&s->digits, &native.digits);
    native.maximum_output_length = JS_STRING_LEN_MAX;
    status = qjs_intl_native_provider_number_open(provider,
        (QJSIntlBytes){s->locale.data_locale, strlen(s->locale.data_locale)},
        (QJSIntlBytes){s->locale.values[0], strlen(s->locale.values[0])}, &native, &s->formatter);
    if (js_intl_native_error(ctx, status, "NumberFormat")) goto done;
    ret = 0;
 done:
    js_free(ctx, numbering_system);
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

static const char *number_part_type(QJSIntlPartType type)
{
    switch (type) {
    case QJS_INTL_PART_LITERAL: return "literal";
    case QJS_INTL_PART_INTEGER: return "integer";
    case QJS_INTL_PART_FRACTION: return "fraction";
    case QJS_INTL_PART_GROUP: return "group";
    case QJS_INTL_PART_DECIMAL: return "decimal";
    case QJS_INTL_PART_PLUS_SIGN: return "plusSign";
    case QJS_INTL_PART_MINUS_SIGN: return "minusSign";
    case QJS_INTL_PART_PERCENT_SIGN: return "percentSign";
    case QJS_INTL_PART_CURRENCY: return "currency";
    case QJS_INTL_PART_UNIT: return "unit";
    case QJS_INTL_PART_COMPACT: return "compact";
    case QJS_INTL_PART_EXPONENT_INTEGER: return "exponentInteger";
    case QJS_INTL_PART_EXPONENT_SEPARATOR: return "exponentSeparator";
    case QJS_INTL_PART_EXPONENT_MINUS_SIGN: return "exponentMinusSign";
    case QJS_INTL_PART_NAN: return "nan";
    case QJS_INTL_PART_INFINITY: return "infinity";
    case QJS_INTL_PART_APPROXIMATELY_SIGN: return "approximatelySign";
    default: return NULL;
    }
}
static JSValue number_result(JSContext *ctx, const QJSIntlFormatted *f, int parts, int range)
{
    static const uint16_t empty[] = {0};
    JSValue out = JS_EXCEPTION;
    size_t i, cursor = 0;
    if (f->length > JS_STRING_LEN_MAX || f->length > INT32_MAX ||
        f->part_count > UINT32_MAX || (f->length && !f->text) ||
        (f->part_count && !f->parts)) return JS_ThrowOutOfMemory(ctx);
    for (i = 0; i < f->part_count; i++) {
        const QJSIntlPart *p = &f->parts[i];
        if (p->start != cursor || p->end <= p->start || p->end > f->length ||
            !number_part_type(p->type) ||
            (range ? p->source < QJS_INTL_SOURCE_START_RANGE || p->source > QJS_INTL_SOURCE_SHARED :
                     p->source != QJS_INTL_SOURCE_SINGLE))
            return JS_ThrowInternalError(ctx, "invalid native NumberFormat parts");
        cursor = p->end;
    }
    if (cursor != f->length) return JS_ThrowInternalError(ctx, "incomplete native NumberFormat parts");
    if (!parts) return js_intl_from_utf16(ctx, f->text ? f->text : empty, (int32_t)f->length);
    out = JS_NewArray(ctx);
    if (JS_IsException(out)) return out;
    for (i = 0; i < f->part_count; i++) {
        const QJSIntlPart *p = &f->parts[i];
        JSValue text = js_intl_from_utf16(ctx, f->text + p->start, (int32_t)(p->end - p->start));
        JSValue source = range ? JS_NewString(ctx, p->source == QJS_INTL_SOURCE_START_RANGE ?
            "startRange" : p->source == QJS_INTL_SOURCE_END_RANGE ? "endRange" : "shared") : JS_UNDEFINED;
        int error = JS_IsException(text) || JS_IsException(source) ||
            js_intl_add_part(ctx, out, (uint32_t)i, number_part_type(p->type), text,
                             range ? "source" : NULL, source) < 0;
        JS_FreeValue(ctx, text); JS_FreeValue(ctx, source);
        if (error) { JS_FreeValue(ctx, out); return JS_EXCEPTION; }
    }
    return out;
}
static JSValue number_format_mv(JSContext *ctx, JSIntlNumberFormat *s,
                               const JSIntlMathematicalValue *mv, int parts)
{
    QJSIntlMathematicalValue input = js_intl_native_mathematical_value(mv);
    QJSIntlFormatted formatted = {0};
    QJSIntlStatus status = qjs_intl_native_number_format(s->formatter, &input, &formatted);
    JSValue result = JS_EXCEPTION;
    if (!js_intl_native_error(ctx, status, "NumberFormat.format"))
        result = number_result(ctx, &formatted, parts, 0);
    qjs_intl_native_number_result_clear(s->formatter, &formatted);
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

static JSValue number_format_range(JSContext *ctx, JSValueConst this_value,
                                  int argc, JSValueConst *argv, int parts)
{
    JSIntlNumberFormat *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_NUMBER_FORMAT);
    JSIntlMathematicalValue x = {0}, y = {0};
    QJSIntlMathematicalValue first, second;
    QJSIntlFormatted formatted = {0};
    QJSIntlStatus status;
    JSValue result = JS_EXCEPTION;
    if (!s) return result;
    if (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_ThrowTypeError(ctx, "range requires two values");
    if (js_intl_to_mathematical_value(ctx, argv[0], &x) < 0 ||
        js_intl_to_mathematical_value(ctx, argv[1], &y) < 0) goto done;
    if (x.kind == JS_INTL_MV_NAN || y.kind == JS_INTL_MV_NAN) {
        JS_ThrowRangeError(ctx, "NaN range endpoint"); goto done;
    }
    first = js_intl_native_mathematical_value(&x); second = js_intl_native_mathematical_value(&y);
    status = qjs_intl_native_number_format_range(s->formatter, &first, &second, &formatted);
    if (!js_intl_native_error(ctx, status, "NumberFormat.formatRange"))
        result = number_result(ctx, &formatted, parts, 1);
 done:
    qjs_intl_native_number_result_clear(s->formatter, &formatted);
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

#endif
