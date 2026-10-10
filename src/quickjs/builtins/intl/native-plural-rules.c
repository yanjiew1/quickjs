/* Native PluralRules frontend. Copyright (c) 2026 Yan-Jie Wang.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, reviewed 2026-10-09.
 * sec-resolveplural, sec-resolvepluralrange, sec-pluralruleselect. */
#include "intl-service-common.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "intl-number.h"
#include "../../../intl/provider-native-plural.h"
typedef struct JSIntlPluralRules {
    char *locale;
    QJSIntlNativePlural *rules;
    QJSIntlNativeNumber *notation_formatter;
    QJSIntlAllocator allocator;
    JSIntlDigitOptions digits;
    QJSIntlDecimalOptions native_digits;
    int type, notation, compact_display;
} JSIntlPluralRules;
static const char *const plural_types[] = {"cardinal","ordinal"};
static const char *const plural_notations[] = {"standard","scientific","engineering","compact"};
static const char *const plural_compact[] = {"short","long"};
static const char *const plural_round_modes[] = {
    "ceil","floor","expand","trunc","halfCeil","halfFloor",
    "halfExpand","halfTrunc","halfEven"
};
static const char *const plural_priorities[] = {"auto","morePrecision","lessPrecision"};
static void js_intl_plural_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlPluralRules *s=JS_GetOpaque(obj,JS_CLASS_INTL_PLURAL_RULES);
    if (s) {
        qjs_intl_native_number_close(s->notation_formatter);
        qjs_intl_native_plural_close(s->rules);
        js_free_rt(rt,s->locale);js_free_rt(rt,s);
    }
}
static JSValue js_intl_plural_constructor(JSContext *ctx,JSValueConst target,
                                        int argc,JSValueConst *argv)
{
    JSValue obj=JS_UNDEFINED,options=JS_UNDEFINED;
    JSIntlLocaleList requested={0};
    JSIntlResolvedLocale resolved={0};
    JSIntlPluralRules *s;
    QJSIntlProvider *provider;
    QJSIntlPluralOptions o;
    QJSIntlStatus status;
    int matcher;
    if (JS_IsUndefined(target)) return JS_ThrowTypeError(ctx,"Intl.PluralRules requires new");
    obj=js_intl_new_object(ctx,target,JS_CLASS_INTL_PLURAL_RULES);
    if (JS_IsException(obj)) goto fail;
    s=js_mallocz(ctx,sizeof(*s));if(!s)goto fail;
    JS_SetOpaque(obj,s);
    if (js_intl_service_options(ctx,argc?argv[0]:JS_UNDEFINED,
            argc>1?argv[1]:JS_UNDEFINED,1,&requested,&options,&matcher)<0 ||
        js_intl_resolve_locale(ctx,JS_INTL_PLURAL_RULES,&requested,
            js_intl_matchers[matcher],NULL,0,&resolved)<0 ||
        js_intl_get_string_option(ctx,options,"type",plural_types,2,0,&s->type)<0 ||
        js_intl_get_string_option(ctx,options,"notation",plural_notations,4,0,&s->notation)<0 ||
        js_intl_get_string_option(ctx,options,"compactDisplay",plural_compact,2,0,&s->compact_display)<0 ||
        js_intl_set_digit_options(ctx,options,0,3,s->notation==3,&s->digits)<0) goto fail;
    provider=js_intl_native_provider(ctx);if(!provider)goto fail;
    s->allocator=*qjs_intl_native_provider_allocator(provider);
    js_intl_native_digit_options(&s->digits,&s->native_digits);
    o.type=(QJSIntlPluralType)s->type;o.digits=s->native_digits;
    status=qjs_intl_native_provider_plural_open(provider,
        (QJSIntlBytes){resolved.data_locale,strlen(resolved.data_locale)},&o,&s->rules);
    if(js_intl_native_error(ctx,status,"PluralRules"))goto fail;
    if (s->notation) {
        QJSIntlNumberOptions number={0};
        number.notation=(QJSIntlNumberNotation)s->notation;
        number.compact_display=(QJSIntlCompactDisplay)s->compact_display;
        number.digits=s->native_digits;number.maximum_output_length=1048576;
        /* Only immutable notation data is used; inputs are never re-rounded. */
        status=qjs_intl_native_provider_number_open(provider,
            (QJSIntlBytes){resolved.data_locale,strlen(resolved.data_locale)},
            (QJSIntlBytes){"latn",4},&number,&s->notation_formatter);
        if(js_intl_native_error(ctx,status,"PluralRules notation"))goto fail;
    }
    s->locale=resolved.locale;resolved.locale=NULL;
    JS_FreeValue(ctx,options);js_intl_locale_list_free(ctx,&requested);
    js_intl_resolved_locale_free(ctx,&resolved);return obj;
fail:
    JS_FreeValue(ctx,obj);JS_FreeValue(ctx,options);
    js_intl_locale_list_free(ctx,&requested);js_intl_resolved_locale_free(ctx,&resolved);
    return JS_EXCEPTION;
}
/* Raw unsigned text controls equality and n/i/v/w/f/t. The selected notation
 * contributes c/e only, matching the existing pinned ICU frontend boundary. */
static QJSIntlStatus plural_value(JSIntlPluralRules *s,const QJSIntlMathematicalValue *v,
                                 QJSIntlDecimalResult *raw,QJSIntlPluralCategory *category)
{
    QJSIntlDecimal exact={0};
    QJSIntlStatus status;
    int32_t exponent=0;
    *category=QJS_INTL_PLURAL_OTHER;
    if(v->kind!=QJS_INTL_FINITE && v->kind!=QJS_INTL_NEGATIVE_ZERO)return QJS_INTL_OK;
    status=qjs_intl_decimal_from_value(&s->allocator,v,&exact);
    if(status!=QJS_INTL_OK)goto done;
    status=qjs_intl_decimal_format_numeric(&s->allocator,&exact,&s->native_digits,raw);
    if(status!=QJS_INTL_OK)goto done;
    if(s->notation_formatter) {
        status=qjs_intl_native_number_notation_exponent(s->notation_formatter,raw,&exponent);
        if(status!=QJS_INTL_OK)goto done;
    }
    {
        JSIntlPluralOperands operands={raw->text,raw->length,exponent};
        status=qjs_intl_native_plural_select_operands(s->rules,&operands,category);
    }
done:
    qjs_intl_decimal_clear(&s->allocator,&exact);return status;
}
static JSValue js_intl_plural_select(JSContext *ctx,JSValueConst receiver,int argc,JSValueConst *argv)
{
    JSIntlPluralRules *s=JS_GetOpaque2(ctx,receiver,JS_CLASS_INTL_PLURAL_RULES);
    JSIntlMathematicalValue mv={0};
    QJSIntlMathematicalValue value;
    QJSIntlDecimalResult raw={0};
    QJSIntlPluralCategory category;
    QJSIntlStatus status;
    JSValue result=JS_EXCEPTION;
    if(!s)return result;
    if(js_intl_to_mathematical_value(ctx,argc?argv[0]:JS_UNDEFINED,&mv)<0)goto done;
    value=js_intl_native_mathematical_value(&mv);
    status=plural_value(s,&value,&raw,&category);
    if(!js_intl_native_error(ctx,status,"PluralRules.select"))
        result=JS_NewString(ctx,qjs_intl_plural_category_name(category));
done:
    qjs_intl_decimal_result_clear(&s->allocator,&raw);
    js_intl_free_mathematical_value(ctx,&mv);return result;
}
static JSValue js_intl_plural_select_range(JSContext *ctx,JSValueConst receiver,
                                          int argc,JSValueConst *argv)
{
    JSIntlPluralRules *s=JS_GetOpaque2(ctx,receiver,JS_CLASS_INTL_PLURAL_RULES);
    JSIntlMathematicalValue x={0},y={0};
    QJSIntlMathematicalValue a,b;
    QJSIntlDecimalResult first={0},second={0};
    QJSIntlPluralCategory ac,bc,category;
    QJSIntlStatus status;
    JSValue result=JS_EXCEPTION;
    int af,bf,equal;
    if(!s)return result;
    if(argc<2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_ThrowTypeError(ctx,"PluralRules.selectRange requires two values");
    if(js_intl_to_mathematical_value(ctx,argv[0],&x)<0 ||
       js_intl_to_mathematical_value(ctx,argv[1],&y)<0)goto done;
    a=js_intl_native_mathematical_value(&x);b=js_intl_native_mathematical_value(&y);
    if(a.kind==QJS_INTL_NAN || b.kind==QJS_INTL_NAN) {
        JS_ThrowRangeError(ctx,"PluralRules.selectRange does not accept NaN");goto done;
    }
    status=plural_value(s,&a,&first,&ac);
    if(js_intl_native_error(ctx,status,"PluralRules.selectRange"))goto done;
    status=plural_value(s,&b,&second,&bc);
    if(js_intl_native_error(ctx,status,"PluralRules.selectRange"))goto done;
    af=a.kind==QJS_INTL_FINITE || a.kind==QJS_INTL_NEGATIVE_ZERO;
    bf=b.kind==QJS_INTL_FINITE || b.kind==QJS_INTL_NEGATIVE_ZERO;
    equal=af && bf ? first.length==second.length &&
        !memcmp(first.text,second.text,first.length) : !af && !bf && a.kind==b.kind;
    if(equal)category=ac;
    else {
        status=qjs_intl_native_plural_range_categories(s->rules,ac,bc,&category);
        if(js_intl_native_error(ctx,status,"PluralRules.selectRange categories"))goto done;
    }
    result=JS_NewString(ctx,qjs_intl_plural_category_name(category));
done:
    qjs_intl_decimal_result_clear(&s->allocator,&first);
    qjs_intl_decimal_result_clear(&s->allocator,&second);
    js_intl_free_mathematical_value(ctx,&x);js_intl_free_mathematical_value(ctx,&y);
    return result;
}
static JSValue js_intl_plural_resolved(JSContext *ctx,JSValueConst receiver,
                                      int argc,JSValueConst *argv)
{
    JSIntlPluralRules *s=JS_GetOpaque2(ctx,receiver,JS_CLASS_INTL_PLURAL_RULES);
    QJSIntlPluralCategory values[QJS_INTL_PLURAL_CATEGORY_COUNT];
    size_t count,i;
    QJSIntlStatus status;
    JSValue result=JS_UNDEFINED,categories=JS_UNDEFINED;
    if(!s)return JS_EXCEPTION;
    status=qjs_intl_native_plural_categories(s->rules,values,&count);
    if(js_intl_native_error(ctx,status,"PluralRules categories"))goto fail;
    result=JS_NewObject(ctx);categories=JS_NewArray(ctx);
    if(JS_IsException(result)||JS_IsException(categories))goto fail;
    for(i=0;i<count;i++) {
        JSValue category=JS_NewString(ctx,qjs_intl_plural_category_name(values[i]));
        if(JS_IsException(category))goto fail;
        if(JS_DefinePropertyValueUint32(ctx,categories,i,category,JS_PROP_C_W_E)<0)goto fail;
    }
    if(js_intl_define_string(ctx,result,"locale",s->locale)<0 ||
       js_intl_define_string(ctx,result,"type",plural_types[s->type])<0 ||
       js_intl_define_string(ctx,result,"notation",plural_notations[s->notation])<0 ||
       (s->notation==3 && js_intl_define_string(ctx,result,"compactDisplay",plural_compact[s->compact_display])<0) ||
       js_intl_define_int(ctx,result,"minimumIntegerDigits",s->digits.minimum_integer_digits)<0)goto fail;
#define PR_DIGIT(property,field) \
    if(s->digits.field>=0 && js_intl_define_int(ctx,result,property,s->digits.field)<0)goto fail
    PR_DIGIT("minimumFractionDigits",minimum_fraction_digits);
    PR_DIGIT("maximumFractionDigits",maximum_fraction_digits);
    PR_DIGIT("minimumSignificantDigits",minimum_significant_digits);
    PR_DIGIT("maximumSignificantDigits",maximum_significant_digits);
#undef PR_DIGIT
    if(JS_DefinePropertyValueStr(ctx,result,"pluralCategories",categories,JS_PROP_C_W_E)<0) {
        categories=JS_UNDEFINED;goto fail;
    }
    categories=JS_UNDEFINED;
    if(js_intl_define_int(ctx,result,"roundingIncrement",s->digits.rounding_increment)<0 ||
       js_intl_define_string(ctx,result,"roundingMode",plural_round_modes[s->digits.rounding_mode])<0 ||
       js_intl_define_string(ctx,result,"roundingPriority",plural_priorities[s->digits.rounding_priority])<0 ||
       js_intl_define_string(ctx,result,"trailingZeroDisplay",s->digits.trailing_zero_display?"stripIfInteger":"auto")<0)goto fail;
    return result;
fail:
    JS_FreeValue(ctx,categories);JS_FreeValue(ctx,result);return JS_EXCEPTION;
}
static JSValue js_intl_plural_supported(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_PLURAL_RULES,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_plural_class = {
    "Intl.PluralRules", .finalizer = js_intl_plural_finalizer,
};
static const JSCFunctionListEntry js_intl_plural_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_plural_supported),
};
static const JSCFunctionListEntry js_intl_plural_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_plural_resolved),
    JS_CFUNC_DEF("select", 1, js_intl_plural_select),
    JS_CFUNC_DEF("selectRange", 2, js_intl_plural_select_range),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.PluralRules", JS_PROP_CONFIGURABLE),
};
int js_intl_init_plural_rules(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_PLURAL_RULES,
        &js_intl_plural_class, "PluralRules", js_intl_plural_constructor, 0,
        JS_CFUNC_constructor, js_intl_plural_static, countof(js_intl_plural_static),
        js_intl_plural_prototype, countof(js_intl_plural_prototype));
}
#endif
