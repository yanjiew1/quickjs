/* Intl.ListFormat formatting, locale resolution and native ownership.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../../internal/atom.h"
#include "../../internal/object.h"
#include "../../internal/runtime.h"
#include "../../internal/string.h"
#include "../../value/conversion.h"
#include "../intl.h"

typedef struct JSIntlListFormat {
    QJSIntlListFormatter *formatter;
    char *locale;
    int type, style;
} JSIntlListFormat;

static const char *const list_types[] = {"conjunction", "disjunction", "unit"};
static const char *const list_styles[] = {"long", "short", "narrow"};

static QJSIntlListService *list_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_list) {
        const QJSIntlData *data = js_intl_data(ctx);
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlListService *service;
        QJSIntlStatus status;

        if (!data)
            return NULL;
        status = qjs_intl_list_open(&allocator, data, &service);
        if (js_intl_error(ctx, status, "list initialization"))
            return NULL;
        rt->intl_list = service;
    }
    return rt->intl_list;
}

static void list_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlListFormat *state = JS_GetOpaque(value, JS_CLASS_INTL_LIST_FORMAT);
    if (state) {
        qjs_intl_list_formatter_close(state->formatter);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state);
    }
}

static JSValue list_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    JSValue object, options = JS_UNDEFINED;
    QJSIntlListService *service;
    const QJSIntlBytes *available;
    size_t count, locale;
    int type, style;
    JSIntlListFormat *state;
    QJSIntlStatus status;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.ListFormat requires new");
    object =
        js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_LIST_FORMAT);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED,
                                         &requested))
        goto fail;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, FALSE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0)
        goto fail;
    service = list_service(ctx);
    if (!service ||
        js_intl_error(ctx, qjs_intl_list_available(service, &available, &count),
                      "list locales") ||
        js_intl_resolve_locale_index(ctx, available, count, &requested, &locale))
        goto fail;
    type = js_intl_string_option(ctx, options, "type", list_types, 3, 0);
    if (type < 0)
        goto fail;
    style = js_intl_string_option(ctx, options, "style", list_styles, 3, 0);
    if (style < 0)
        goto fail;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto fail;
    JS_SetOpaque(object, state);
    state->type = type;
    state->style = style;
    state->locale = js_strndup(ctx, available[locale].data, available[locale].length);
    if (!state->locale)
        goto fail;
    status = qjs_intl_list_formatter_open(service, locale, (QJSIntlListType)type,
                                          (QJSIntlListStyle)style, &state->formatter);
    if (js_intl_error(ctx, status, "ListFormat"))
        goto fail;
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return object;
fail:
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(ctx->rt, &requested);
    return JS_EXCEPTION;
}

static JSValue list_resolved(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv)
{
    JSIntlListFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LIST_FORMAT);
    JSValue result;
    const char *names[] = {"locale", "type", "style"};
    const char *values[3];
    size_t i;
    if (!state)
        return JS_EXCEPTION;
    values[0] = state->locale;
    values[1] = list_types[state->type];
    values[2] = list_styles[state->style];
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    for (i = 0; i < 3; i++) {
        JSValue value = JS_NewString(ctx, values[i]);
        if (JS_IsException(value) ||
            JS_DefinePropertyValueStr(ctx, result, names[i], value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
            JS_FreeValue(ctx, result);
            return JS_EXCEPTION;
        }
    }
    return result;
}

static JSValue list_supported(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv)
{
    QJSIntlListService *service = list_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service ||
        js_intl_error(ctx, qjs_intl_list_available(service, &available, &count),
                      "list locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                  argc > 1 ? argv[1] : JS_UNDEFINED);
}

/* Retain strings while consuming the iterator, then borrow their flat storage.
   In particular, do not coerce iterator values or close after IteratorStepValue
   itself fails. A non-string value instead requires IteratorClose. */
typedef struct JSIntlStringList {
    JSValue *values;
    QJSIntlUTF16 *items;
    uint16_t *narrow;
    size_t count;
} JSIntlStringList;

static void string_list_clear(JSRuntime *rt, JSIntlStringList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++)
        JS_FreeValueRT(rt, list->values[i]);
    js_free_rt(rt, list->values);
    js_free_rt(rt, list->items);
    js_free_rt(rt, list->narrow);
}

static int string_list_read(JSContext *ctx, JSValueConst input, JSIntlStringList *list)
{
    JSValue iterator = JS_UNDEFINED, next = JS_UNDEFINED;
    size_t capacity = 0, narrow_length = 0, offset = 0, i;
    int result = -1;
    if (JS_IsUndefined(input))
        return 0;
    iterator = JS_GetIterator(ctx, input, FALSE);
    if (JS_IsException(iterator))
        goto done;
    next = JS_GetProperty(ctx, iterator, JS_ATOM_next);
    if (JS_IsException(next))
        goto done;
    for (;;) {
        BOOL finished;
        JSValue value = JS_IteratorNext(ctx, iterator, next, &finished);
        if (JS_IsException(value))
            goto done;
        if (finished)
            break;
        if (!JS_IsString(value)) {
            JS_FreeValue(ctx, value);
            JS_ThrowTypeError(ctx, "ListFormat values must be strings");
            goto close;
        }
        if (list->count == capacity) {
            size_t limit = SIZE_MAX / sizeof(*list->values);
            size_t grown = capacity ? capacity + capacity / 2 : 8;
            JSValue *values;
            if (grown <= capacity || grown > limit) {
                JS_FreeValue(ctx, value);
                JS_ThrowOutOfMemory(ctx);
                goto close;
            }
            values = js_realloc(ctx, list->values, grown * sizeof(*values));
            if (!values) {
                JS_FreeValue(ctx, value);
                goto close;
            }
            list->values = values;
            capacity = grown;
        }
        list->values[list->count++] = value;
    }
    if (!list->count) {
        result = 0;
        goto done;
    }
    /* Flatten only after iteration has completed; retained strings cannot run
       user conversion hooks. Keep all flat strings alive through formatting. */
    for (i = 0; i < list->count; i++) {
        JSString *string;
        list->values[i] = JS_ToStringFree(ctx, list->values[i]);
        if (JS_IsException(list->values[i]))
            goto done;
        string = JS_VALUE_GET_STRING(list->values[i]);
        if (!string->is_wide_char) {
            if (string->len > SIZE_MAX / sizeof(uint16_t) - narrow_length) {
                JS_ThrowOutOfMemory(ctx);
                goto done;
            }
            narrow_length += string->len;
        }
    }
    if (list->count > SIZE_MAX / sizeof(*list->items)) {
        JS_ThrowOutOfMemory(ctx);
        goto done;
    }
    list->items = js_malloc(ctx, list->count * sizeof(*list->items));
    if (!list->items)
        goto done;
    if (narrow_length) {
        list->narrow = js_malloc(ctx, narrow_length * sizeof(*list->narrow));
        if (!list->narrow)
            goto done;
    }
    for (i = 0; i < list->count; i++) {
        JSString *string = JS_VALUE_GET_STRING(list->values[i]);
        list->items[i].length = string->len;
        if (string->is_wide_char) {
            list->items[i].data = string->u.str16;
        } else {
            size_t j;
            list->items[i].data = list->narrow ? list->narrow + offset : NULL;
            for (j = 0; j < string->len; j++)
                list->narrow[offset + j] = string->u.str8[j];
            offset += string->len;
        }
    }
    result = 0;
    goto done;
close:
    JS_IteratorClose(ctx, iterator, TRUE);
done:
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iterator);
    return result;
}

static int valid_list_result(const QJSIntlFormatted *formatted)
{
    size_t i, end = 0;
    if ((formatted->length && !formatted->text) ||
        (formatted->part_count && !formatted->parts))
        return 0;
    for (i = 0; i < formatted->part_count; i++) {
        const QJSIntlPart *part = &formatted->parts[i];
        if ((part->type != QJS_INTL_PART_LITERAL &&
             part->type != QJS_INTL_PART_ELEMENT) ||
            part->start != end || part->end < end || part->end > formatted->length)
            return 0;
        end = part->end;
    }
    return end == formatted->length;
}

static JSValue list_parts(JSContext *ctx, const QJSIntlFormatted *formatted)
{
    JSValue array;
    size_t i;
    if (formatted->part_count > UINT32_MAX)
        return JS_ThrowRangeError(ctx, "too many ListFormat parts");
    array = JS_NewArray(ctx);
    if (JS_IsException(array))
        return array;
    for (i = 0; i < formatted->part_count; i++) {
        const QJSIntlPart *part = &formatted->parts[i];
        JSValue object = JS_NewObject(ctx), value;
        if (JS_IsException(object))
            goto fail;
        value = JS_NewString(ctx, part->type == QJS_INTL_PART_ELEMENT ? "element"
                                                                      : "literal");
        if (JS_IsException(value) ||
            JS_DefinePropertyValueStr(ctx, object, "type", value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto part_fail;
        value = JS_NewStringUTF16(
            ctx, formatted->text ? formatted->text + part->start : NULL,
            part->end - part->start);
        if (JS_IsException(value) ||
            JS_DefinePropertyValueStr(ctx, object, "value", value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto part_fail;
        if (JS_DefinePropertyValueUint32(ctx, array, (uint32_t)i, object,
                                         JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
        continue;
    part_fail:
        JS_FreeValue(ctx, object);
        goto fail;
    }
    return array;
fail:
    JS_FreeValue(ctx, array);
    return JS_EXCEPTION;
}

static JSValue list_format(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv, int parts)
{
    JSIntlListFormat *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LIST_FORMAT);
    JSIntlStringList strings = {0};
    QJSIntlFormatted formatted = {0};
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    JSValue result = JS_EXCEPTION;
    if (!state)
        return result;
    if (string_list_read(ctx, argc ? argv[0] : JS_UNDEFINED, &strings))
        goto done;
    if (js_intl_error(ctx,
                      qjs_intl_list_format(state->formatter, strings.items,
                                           strings.count, &formatted),
                      "list formatting"))
        goto done;
    if (!valid_list_result(&formatted)) {
        JS_ThrowInternalError(ctx, "invalid Intl list result");
        goto done;
    }
    result = parts ? list_parts(ctx, &formatted)
                   : JS_NewStringUTF16(ctx, formatted.text, formatted.length);
done:
    qjs_intl_list_result_clear(&allocator, &formatted);
    string_list_clear(ctx->rt, &strings);
    return result;
}

static const JSCFunctionListEntry list_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, list_supported),
};
static const JSCFunctionListEntry list_properties[] = {
    JS_CFUNC_MAGIC_DEF("format", 1, list_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 1, list_format, 1),
    JS_CFUNC_DEF("resolvedOptions", 0, list_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.ListFormat", JS_PROP_CONFIGURABLE),
};

int js_intl_init_list_format(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definition = {
        .class_name = "Intl.ListFormat",
        .finalizer = list_finalizer,
    };
    JSValue constructor;
    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_INTL_LIST_FORMAT) &&
        JS_NewClass(ctx->rt, JS_CLASS_INTL_LIST_FORMAT, &definition) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(
        ctx, JS_CLASS_INTL_LIST_FORMAT, "ListFormat", list_constructor, 0,
        JS_CFUNC_constructor, 0, JS_UNDEFINED, list_statics, countof(list_statics),
        list_properties, countof(list_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    return JS_DefinePropertyValueStr(ctx, namespace_object, "ListFormat", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE |
                                         JS_PROP_THROW) < 0
               ? -1
               : 0;
}
