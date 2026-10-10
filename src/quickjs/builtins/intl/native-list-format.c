/*
 * QuickJS native Intl services and tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/* Intl.ListFormat, ECMA402 #sec-Intl.ListFormat.
 * Spec 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-06.
 * Initial native development routing; generated CLDR data supplies patterns. */
#include "intl-service-common.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../internal/iterator.h"
typedef struct JSIntlListFormat {
    char *locale;
    QJSIntlNativeList *formatter;
    QJSIntlAllocator allocator;
    int type, style;
} JSIntlListFormat;
static const char *const list_types[] = { "conjunction", "disjunction", "unit" };
static void js_intl_list_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlListFormat *s = JS_GetOpaque(obj, JS_CLASS_INTL_LIST_FORMAT);
    if (s) {
        if (s->formatter) qjs_intl_native_list_close(s->formatter);
        js_free_rt(rt, s->locale);
        js_free_rt(rt, s);
    }
}
static JSValue js_intl_list_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolvedLocale resolved = {0};
    JSIntlListFormat *s = NULL;
    QJSIntlStatus status;
    QJSIntlProvider *provider;
    int matcher;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.ListFormat requires new");
    obj = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_LIST_FORMAT);
    if (JS_IsException(obj)) goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    JS_SetOpaque(obj, s);
    if (js_intl_service_options(ctx,
            argc > 0 ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED, 0, &requested,
                               &options, &matcher) < 0 ||
        js_intl_resolve_locale(ctx, JS_INTL_LIST_FORMAT, &requested,
            js_intl_matchers[matcher], NULL, 0, &resolved) < 0 ||
        js_intl_get_string_option(ctx, options, "type", list_types,
            countof(list_types), 0, &s->type) < 0 ||
        js_intl_get_string_option(ctx, options, "style", js_intl_styles,
            countof(js_intl_styles), 0, &s->style) < 0)
        goto fail;
    s->locale = resolved.locale;
    resolved.locale = NULL;
    provider = js_intl_native_provider(ctx);
    if (!provider) goto fail;
    s->allocator = *qjs_intl_native_provider_allocator(provider);
    /* Option enum ordinals are converted explicitly at this boundary. */
    {
        static const QJSIntlListType types[] = { QJS_INTL_LIST_CONJUNCTION,
            QJS_INTL_LIST_DISJUNCTION, QJS_INTL_LIST_UNIT };
        static const QJSIntlListStyle styles[] = { QJS_INTL_LIST_LONG,
            QJS_INTL_LIST_SHORT, QJS_INTL_LIST_NARROW };
        status = qjs_intl_native_provider_list_open(provider,
            (QJSIntlBytes){ resolved.data_locale, strlen(resolved.data_locale) },
            types[s->type], styles[s->style], &s->formatter);
    }
    if (js_intl_native_error(ctx, status, "ListFormat")) goto fail;
    JS_FreeValue(ctx, options);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return JS_EXCEPTION;
}
typedef struct JSIntlStringList {
    uint16_t **strings;
    int32_t *lengths;
    int32_t count, capacity;
} JSIntlStringList;
static void js_intl_string_list_free(JSContext *ctx, JSIntlStringList *list)
{
    int32_t i;
    for (i = 0; i < list->count; i++) js_free(ctx, list->strings[i]);
    js_free(ctx, list->strings);
    js_free(ctx, list->lengths);
}
/* IteratorStepValue abrupt completion never calls return; a non-string does.
   Each successful append transfers ownership of the exact UTF16 string. */
static int js_intl_string_list(JSContext *ctx, JSValueConst input,
                               JSIntlStringList *list)
{
    JSValue iterator = JS_UNDEFINED, next = JS_UNDEFINED, item = JS_UNDEFINED;
    uint16_t *text = NULL;
    int32_t length;
    BOOL done;
    if (JS_IsUndefined(input)) return 0;
    iterator = JS_GetIterator(ctx, input, FALSE);
    if (JS_IsException(iterator)) goto fail;
    next = JS_GetProperty(ctx, iterator, JS_ATOM_next);
    if (JS_IsException(next)) goto fail;
    for (;;) {
        item = JS_IteratorNext(ctx, iterator, next, 0, NULL, &done);
        if (JS_IsException(item)) goto fail;
        if (done) break;
        if (!JS_IsString(item)) {
            JS_ThrowTypeError(ctx, "Intl.ListFormat elements must be strings");
            goto close;
        }
        if (list->count == INT32_MAX) {
            JS_ThrowRangeError(ctx, "Intl.ListFormat list is too large");
            goto close;
        }
        if (list->count == list->capacity) {
            int32_t capacity = list->capacity > INT32_MAX / 2 ? INT32_MAX
                : list->capacity ? list->capacity * 2 : 8;
            void *new_strings, *new_lengths;
            if ((size_t)capacity > SIZE_MAX / sizeof(*list->strings) ||
                (size_t)capacity > SIZE_MAX / sizeof(*list->lengths)) {
                JS_ThrowOutOfMemory(ctx);
                goto close;
            }
            new_strings = js_realloc(ctx, list->strings,
                (size_t)capacity * sizeof(*list->strings));
            if (!new_strings) goto close;
            list->strings = new_strings;
            new_lengths = js_realloc(ctx, list->lengths,
                (size_t)capacity * sizeof(*list->lengths));
            if (!new_lengths) goto close;
            list->lengths = new_lengths;
            list->capacity = capacity;
        }
        if (js_intl_to_utf16(ctx, item, &text, &length) < 0) goto close;
        list->strings[list->count] = text;
        list->lengths[list->count++] = length;
        text = NULL;
        JS_FreeValue(ctx, item);
        item = JS_UNDEFINED;
    }
    JS_FreeValue(ctx, item);
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iterator);
    return 0;
 close:
    JS_IteratorClose(ctx, iterator, TRUE);
 fail:
    js_free(ctx, text);
    JS_FreeValue(ctx, item);
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iterator);
    return -1;
}
static JSValue js_intl_list_format(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int parts)
{
    JSIntlListFormat *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LIST_FORMAT);
    JSIntlStringList list = { 0 };
    QJSIntlUTF16 *items = NULL;
    QJSIntlFormatted formatted = { 0 };
    QJSIntlStatus status;
    JSValue value = JS_EXCEPTION;
    size_t i, cursor = 0;
    if (!s) return JS_EXCEPTION;
    if (js_intl_string_list(ctx, argc ? argv[0] : JS_UNDEFINED, &list) < 0)
        goto done;
    if ((size_t)list.count > SIZE_MAX / sizeof(*items)) {
        JS_ThrowOutOfMemory(ctx);
        goto done;
    }
    if (list.count) {
        items = js_malloc(ctx, (size_t)list.count * sizeof(*items));
        if (!items) goto done;
    }
    for (i = 0; i < (size_t)list.count; i++)
        items[i] = (QJSIntlUTF16){ list.strings[i], list.lengths[i] };
    status = qjs_intl_native_list_format(s->formatter, items, list.count, &formatted);
    if (js_intl_native_error(ctx, status, "ListFormat.format")) goto done;
    if (formatted.length > JS_STRING_LEN_MAX || formatted.length > INT32_MAX ||
        (formatted.length && !formatted.text) ||
        (formatted.part_count && !formatted.parts) ||
        formatted.part_count > UINT32_MAX) {
        JS_ThrowOutOfMemory(ctx);
        goto done;
    }
    /* Validate offsets/types before copying provider results into JS values.
     * Zero-width element parts are retained, with no synthetic ICU marker. */
    for (i = 0; i < formatted.part_count; i++) {
        const QJSIntlPart *part = &formatted.parts[i];
        if (part->start != cursor || part->end < part->start ||
            part->end > formatted.length ||
            (part->type != QJS_INTL_PART_LITERAL && part->type != QJS_INTL_PART_ELEMENT) ||
            part->source != QJS_INTL_SOURCE_SINGLE) {
            JS_ThrowInternalError(ctx, "invalid native ListFormat parts");
            goto done;
        }
        cursor = part->end;
    }
    if (cursor != formatted.length) {
        JS_ThrowInternalError(ctx, "incomplete native ListFormat parts");
        goto done;
    }
    if (!parts) {
        static const uint16_t empty[] = { 0 };
        value = js_intl_from_utf16(ctx, formatted.text ? formatted.text : empty,
                                   (int32_t)formatted.length);
    } else {
        value = JS_NewArray(ctx);
        if (JS_IsException(value)) goto done;
        for (i = 0; i < formatted.part_count; i++) {
            const QJSIntlPart *part = &formatted.parts[i];
            static const uint16_t empty[] = { 0 };
            JSValue text = js_intl_from_utf16(ctx, formatted.text ?
                formatted.text + part->start : empty, (int32_t)(part->end - part->start));
            int failure;
            if (JS_IsException(text)) goto fail;
            failure = js_intl_add_part(ctx, value, (uint32_t)i,
                part->type == QJS_INTL_PART_ELEMENT ? "element" : "literal",
                text, NULL, JS_UNDEFINED);
            JS_FreeValue(ctx, text);
            if (failure < 0) goto fail;
        }
    }
    goto done;
fail:
    JS_FreeValue(ctx, value);
    value = JS_EXCEPTION;
done:
    qjs_intl_native_list_result_clear(&s->allocator, &formatted);
    js_free(ctx, items);
    js_intl_string_list_free(ctx, &list);
    return value;
}
static JSValue js_intl_list_resolved(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSIntlListFormat *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_LIST_FORMAT);
    JSValue result;
    if (!s) return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale) < 0 ||
        js_intl_define_string(ctx, result, "type", list_types[s->type]) < 0 ||
        js_intl_define_string(ctx, result, "style", js_intl_styles[s->style]) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}
static JSValue js_intl_list_supported(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_LIST_FORMAT,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_list_class = {
    "Intl.ListFormat", .finalizer = js_intl_list_finalizer,
};
static const JSCFunctionListEntry js_intl_list_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_list_supported),
};
static const JSCFunctionListEntry js_intl_list_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_list_resolved),
    JS_CFUNC_MAGIC_DEF("format", 1, js_intl_list_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 1, js_intl_list_format, 1),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.ListFormat", JS_PROP_CONFIGURABLE),
};
int js_intl_init_list_format(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_LIST_FORMAT,
        &js_intl_list_class, "ListFormat", js_intl_list_constructor, 0,
        JS_CFUNC_constructor, js_intl_list_static, countof(js_intl_list_static),
        js_intl_list_prototype, countof(js_intl_list_prototype));
}
#endif
