/* Native RelativeTimeFormat frontend. Copyright (c) 2026 Yan-Jie Wang.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, reviewed 2026-10-09.
 * sec-PartitionRelativeTimePattern, sec-intl.relativetimeformat. */
#include "intl-service-common.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../../intl/provider-native-relative.h"
#include "../../../intl/number-decimal.h"
typedef struct JSIntlRelativeTimeFormat {
    char *locale,*numbering_system;
    QJSIntlNativeRelative *formatter;
    QJSIntlNativeNumber *number;
    QJSIntlAllocator allocator;
    int style,numeric;
} JSIntlRelativeTimeFormat;
static const char *const relative_numeric[]={"always","auto"};
static void js_intl_relative_finalizer(JSRuntime *rt,JSValue obj)
{
    JSIntlRelativeTimeFormat *s=JS_GetOpaque(obj,JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    if(s) {
        /* The relative handle borrows its bridge; close it before Number. */
        qjs_intl_native_relative_close(s->formatter);
        qjs_intl_native_number_close(s->number);
        js_free_rt(rt,s->locale);js_free_rt(rt,s->numbering_system);js_free_rt(rt,s);
    }
}
static QJSIntlStatus relative_number(void *opaque,const QJSIntlDecimalResult *raw,
                                     QJSIntlFormatted *out)
{
    JSIntlRelativeTimeFormat *s=opaque;
    return qjs_intl_native_number_format_rounded(s->number,raw,out);
}
static void relative_number_clear(void *opaque,QJSIntlFormatted *out)
{
    JSIntlRelativeTimeFormat *s=opaque;
    qjs_intl_native_number_result_clear(s->number,out);
}
static JSValue js_intl_relative_constructor(JSContext *ctx,JSValueConst target,
                                           int argc,JSValueConst *argv)
{
    JSValue obj=JS_UNDEFINED,options=JS_UNDEFINED;
    JSIntlLocaleList requested={0};JSIntlResolvedLocale resolved={0};
    JSIntlResolutionKey key={"nu",NULL,FALSE};
    JSIntlRelativeTimeFormat *s;
    QJSIntlProvider *provider;
    QJSIntlNumberOptions number={0};
    QJSIntlRelativeOptions relative;
    QJSIntlRelativeNumberBridge bridge;
    QJSIntlStatus status;
    char *numbering=NULL;
    int matcher;
    if(JS_IsUndefined(target))return JS_ThrowTypeError(ctx,"Intl.RelativeTimeFormat requires new");
    obj=js_intl_new_object(ctx,target,JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    if(JS_IsException(obj))goto fail;
    s=js_mallocz(ctx,sizeof(*s));if(!s)goto fail;JS_SetOpaque(obj,s);
    if(js_intl_service_options(ctx,argc?argv[0]:JS_UNDEFINED,
        argc>1?argv[1]:JS_UNDEFINED,1,&requested,&options,&matcher)<0 ||
       js_intl_get_string_option_alloc(ctx,options,"numberingSystem",&numbering)<0)goto fail;
    if(numbering && !js_intl_unicode_type(numbering)) {
        JS_ThrowRangeError(ctx,"invalid numberingSystem");goto fail;
    }
    key.option=numbering;
    if(js_intl_resolve_locale(ctx,JS_INTL_RELATIVE_TIME_FORMAT,&requested,
        js_intl_matchers[matcher],&key,1,&resolved)<0 ||
       js_intl_get_string_option(ctx,options,"style",js_intl_styles,3,0,&s->style)<0 ||
       js_intl_get_string_option(ctx,options,"numeric",relative_numeric,2,0,&s->numeric)<0)goto fail;
    provider=js_intl_native_provider(ctx);if(!provider)goto fail;
    s->allocator=*qjs_intl_native_provider_allocator(provider);
    number.grouping=QJS_INTL_NUMBER_GROUP_AUTO;
    number.digits.minimum_integer_digits=1;number.digits.maximum_fraction_digits=3;
    number.digits.minimum_significant_digits=1;number.digits.maximum_significant_digits=21;
    number.digits.rounding_increment=1;number.digits.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;
    number.digits.maximum_output_length=number.maximum_output_length=1048576;
    status=qjs_intl_native_provider_number_open(provider,
        (QJSIntlBytes){resolved.data_locale,strlen(resolved.data_locale)},
        (QJSIntlBytes){resolved.values[0],strlen(resolved.values[0])},&number,&s->number);
    if(js_intl_native_error(ctx,status,"RelativeTimeFormat number"))goto fail;
    relative.style=(QJSIntlRelativeStyle)s->style;
    relative.numeric=(QJSIntlRelativeNumeric)s->numeric;
    relative.maximum_output_length=1048576;
    bridge=(QJSIntlRelativeNumberBridge){s,relative_number,relative_number_clear};
    status=qjs_intl_native_provider_relative_open(provider,
        (QJSIntlBytes){resolved.data_locale,strlen(resolved.data_locale)},
        &relative,&bridge,&s->formatter);
    if(js_intl_native_error(ctx,status,"RelativeTimeFormat"))goto fail;
    s->locale=resolved.locale;resolved.locale=NULL;
    s->numbering_system=resolved.values[0];resolved.values[0]=NULL;
    JS_FreeValue(ctx,options);js_free(ctx,numbering);
    js_intl_locale_list_free(ctx,&requested);js_intl_resolved_locale_free(ctx,&resolved);
    return obj;
fail:
    JS_FreeValue(ctx,obj);JS_FreeValue(ctx,options);js_free(ctx,numbering);
    js_intl_locale_list_free(ctx,&requested);js_intl_resolved_locale_free(ctx,&resolved);
    return JS_EXCEPTION;
}
static const char *relative_part_name(QJSIntlPartType type)
{
    switch(type) {
    case QJS_INTL_PART_LITERAL:return "literal";
    case QJS_INTL_PART_INTEGER:return "integer";
    case QJS_INTL_PART_FRACTION:return "fraction";
    case QJS_INTL_PART_DECIMAL:return "decimal";
    case QJS_INTL_PART_GROUP:return "group";
    case QJS_INTL_PART_PLUS_SIGN:return "plusSign";
    case QJS_INTL_PART_MINUS_SIGN:return "minusSign";
    default:return NULL;
    }
}
static JSValue js_intl_relative_format(JSContext *ctx,JSValueConst receiver,
                                      int argc,JSValueConst *argv,int parts)
{
    JSIntlRelativeTimeFormat *s=JS_GetOpaque2(ctx,receiver,JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    QJSIntlNumberDecimal number={0};QJSIntlFormatted out={0};
    QJSIntlRelativeUnit unit;
    QJSIntlStatus status;
    uint16_t *unit_text=NULL;
    char unit_ascii[9];
    int32_t length;
    size_t i,cursor=0;
    double input;
    JSValue result=JS_EXCEPTION,unit_value=JS_UNDEFINED;
    if(!s)return result;
    if(JS_ToFloat64(ctx,&input,argc?argv[0]:JS_UNDEFINED)<0 ||
       js_intl_to_utf16(ctx,argc>1?argv[1]:JS_UNDEFINED,&unit_text,&length)<0)goto done;
    /* ToString(unit) is observable before finite-value and syntax errors. */
    if(!isfinite(input)) {
        JS_ThrowRangeError(ctx,"relative time value must be finite");goto done;
    }
    if(length<1 || length>8)goto invalid_unit;
    for(i=0;i<(size_t)length;i++) {
        if(!unit_text[i] || unit_text[i]>127)goto invalid_unit;
        unit_ascii[i]=(char)unit_text[i];
    }
    status=qjs_intl_native_relative_unit((QJSIntlBytes){unit_ascii,(size_t)length},&unit);
    if(status!=QJS_INTL_OK)goto invalid_unit;
    status=qjs_intl_number_to_real_decimal(&s->allocator,input,&number);
    if(js_intl_native_error(ctx,status,"RelativeTimeFormat exact Number"))goto done;
    if(input==0 && number.number_negative)number.value.kind=QJS_INTL_NEGATIVE_ZERO;
    status=qjs_intl_native_relative_format(s->formatter,&number.value,unit,&out);
    if(js_intl_native_error(ctx,status,"RelativeTimeFormat.format"))goto done;
    if(out.length>JS_STRING_LEN_MAX || out.length>INT32_MAX || out.part_count>UINT32_MAX ||
       (out.length && !out.text) || (out.part_count && !out.parts)) {
        JS_ThrowOutOfMemory(ctx);goto done;
    }
    /* Reject bad provider spans before copying any result into JavaScript. */
    for(i=0;i<out.part_count;i++) {
        QJSIntlPart *p=&out.parts[i];
        if(p->start!=cursor || p->end<=p->start || p->end>out.length ||
           !relative_part_name(p->type) || (p->unit.length &&
           (!p->unit.data || p->unit.length!=strlen(qjs_intl_native_relative_unit_name(unit)) ||
            memcmp(p->unit.data,qjs_intl_native_relative_unit_name(unit),p->unit.length)))) {
            JS_ThrowInternalError(ctx,"invalid native relative time parts");goto done;
        }
        cursor=p->end;
    }
    if(cursor!=out.length) {
        JS_ThrowInternalError(ctx,"incomplete native relative time parts");goto done;
    }
    if(!parts) { result=js_intl_from_utf16(ctx,out.text,out.length);goto done; }
    result=JS_NewArray(ctx);unit_value=JS_NewString(ctx,qjs_intl_native_relative_unit_name(unit));
    if(JS_IsException(result)||JS_IsException(unit_value))goto fail;
    for(i=0;i<out.part_count;i++) {
        QJSIntlPart *p=&out.parts[i];
        JSValue text=js_intl_from_utf16(ctx,out.text+p->start,p->end-p->start);
        int added;
        if(JS_IsException(text))goto fail;
        added=js_intl_add_part(ctx,result,(uint32_t)i,relative_part_name(p->type),text,
            p->unit.length?"unit":NULL,unit_value);
        JS_FreeValue(ctx,text);
        if(added<0)goto fail;
    }
    goto done;
invalid_unit:
    JS_ThrowRangeError(ctx,"invalid relative time unit");goto done;
fail:
    JS_FreeValue(ctx,result);result=JS_EXCEPTION;
done:
    JS_FreeValue(ctx,unit_value);js_free(ctx,unit_text);
    qjs_intl_number_decimal_clear(&s->allocator,&number);
    qjs_intl_native_relative_result_clear(&s->allocator,&out);return result;
}
static JSValue js_intl_relative_resolved(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSIntlRelativeTimeFormat *s = JS_GetOpaque2(ctx, this_val,
                                               JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    JSValue result;
    if (!s) return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale) < 0 ||
        js_intl_define_string(ctx, result, "style", js_intl_styles[s->style]) < 0 ||
        js_intl_define_string(ctx, result, "numeric", relative_numeric[s->numeric]) < 0 ||
        js_intl_define_string(ctx, result, "numberingSystem", s->numbering_system) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}
static JSValue js_intl_relative_supported(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_RELATIVE_TIME_FORMAT,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_relative_class = {
    "Intl.RelativeTimeFormat", .finalizer = js_intl_relative_finalizer,
};
static const JSCFunctionListEntry js_intl_relative_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_relative_supported),
};
static const JSCFunctionListEntry js_intl_relative_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_relative_resolved),
    JS_CFUNC_MAGIC_DEF("format", 2, js_intl_relative_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 2, js_intl_relative_format, 1),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.RelativeTimeFormat", JS_PROP_CONFIGURABLE),
};
int js_intl_init_relative_time_format(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_RELATIVE_TIME_FORMAT,
        &js_intl_relative_class, "RelativeTimeFormat", js_intl_relative_constructor, 0,
        JS_CFUNC_constructor, js_intl_relative_static, countof(js_intl_relative_static),
        js_intl_relative_prototype, countof(js_intl_relative_prototype));
}
#endif
