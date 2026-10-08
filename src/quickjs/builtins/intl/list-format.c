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
 * Original native C implementation; ICU supplies locale data. */
#include "intl-service-common.h"
#ifdef CONFIG_INTL
#include "../../internal/iterator.h"
#include <unicode/ulistformatter.h>
#include <unicode/uformattedvalue.h>
typedef struct JSIntlListFormat {
    char *locale;
    UListFormatter *formatter;
    int type, style;
} JSIntlListFormat;
static const char *const list_types[] = { "conjunction", "disjunction", "unit" };
static void js_intl_list_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlListFormat *s = JS_GetOpaque(obj, JS_CLASS_INTL_LIST_FORMAT);
    if (s) {
        if (s->formatter) ulistfmt_close(s->formatter);
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
    UErrorCode status = U_ZERO_ERROR;
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
    s->formatter = ulistfmt_openForType(resolved.icu_locale,
        (UListFormatterType)s->type, (UListFormatterWidth)s->style, &status);
    if (js_intl_icu_error(ctx, status, "ListFormat") < 0) goto fail;
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
    UChar **strings;
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
    UChar *text = NULL;
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
        if (js_intl_to_uchar(ctx, item, &text, &length) < 0) goto close;
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
    JSIntlStringList list = {0};
    UFormattedList *result = NULL;
    UConstrainedFieldPosition *position = NULL;
    const UFormattedValue *formatted;
    const UChar *text;
    UErrorCode status = U_ZERO_ERROR;
    JSValue value = JS_UNDEFINED;
    int32_t length, start, limit, cursor = 0, field, i;
    unsigned char *empty = NULL;
    static const UChar empty_marker[] = { 0x05d0, 0 };
    uint32_t part = 0;
    if (!s) return JS_EXCEPTION;
    if (js_intl_string_list(ctx, argv[0], &list) < 0) goto fail;
    if (!list.count) {
        value = parts ? JS_NewArray(ctx) : JS_NewString(ctx, "");
        goto done;
    }
    if (list.count == 1 && parts) {
        value = JS_NewArray(ctx);
        if (JS_IsException(value) || js_intl_add_part_uchar(ctx, value, 0,
                "element", list.strings[0], list.lengths[0], NULL,
                JS_UNDEFINED) < 0) goto fail;
        goto done;
    }
    if (parts) {
        empty = js_mallocz(ctx, (size_t)list.count);
        if (!empty) goto fail;
        /* ICU78.3 suppresses zero-width spans. Hebrew alef follows exactly
           the same context branches as empty input in its Spanish/Hebrew
           handlers. The marker is excluded from emitted element parts. */
        for (i = 0; i < list.count; i++) {
            if (!list.lengths[i]) {
                empty[i] = 1;
                js_free(ctx, list.strings[i]);
                list.strings[i] = js_intl_alloc_uchar(ctx, 1);
                if (!list.strings[i]) goto fail;
                list.strings[i][0] = empty_marker[0];
                list.strings[i][1] = 0;
                list.lengths[i] = 1;
            }
        }
    }
    result = ulistfmt_openResult(&status);
    ulistfmt_formatStringsToResult(s->formatter,
        (const UChar *const *)list.strings, list.lengths, list.count,
        result, &status);
    formatted = ulistfmt_resultAsValue(result, &status);
    text = ufmtval_getString(formatted, &length, &status);
    if (js_intl_icu_error(ctx, status, "ListFormat.format") < 0) goto fail;
    if (!parts) {
        value = js_intl_from_uchar(ctx, text, length);
        goto done;
    }
    value = JS_NewArray(ctx);
    if (JS_IsException(value)) goto fail;
    position = ucfpos_open(&status);
    ucfpos_constrainCategory(position, UFIELD_CATEGORY_LIST_SPAN, &status);
    while (ufmtval_nextPosition(formatted, position, &status)) {
        field = ucfpos_getField(position, &status);
        ucfpos_getIndexes(position, &start, &limit, &status);
        if (js_intl_icu_error(ctx, status, "ListFormat parts") < 0) goto fail;
        if (field < 0 || field >= list.count || start < cursor ||
                limit < start || limit > length) {
            JS_ThrowInternalError(ctx, "invalid ICU list span");
            goto fail;
        }
        if (start > cursor && js_intl_add_part_uchar(ctx, value, part++, "literal",
                text + cursor, start - cursor, NULL, JS_UNDEFINED) < 0) goto fail;
        if (js_intl_add_part_uchar(ctx, value, part++, "element",
                text + start, empty[field] ? 0 : limit - start,
                NULL, JS_UNDEFINED) < 0) goto fail;
        cursor = limit;
    }
    if (js_intl_icu_error(ctx, status, "ListFormat parts") < 0) goto fail;
    if (cursor < length && js_intl_add_part_uchar(ctx, value, part++, "literal",
            text + cursor, length - cursor, NULL, JS_UNDEFINED) < 0) goto fail;
    goto done;
 fail:
    JS_FreeValue(ctx, value);
    value = JS_EXCEPTION;
 done:
    if (position) ucfpos_close(position);
    if (result) ulistfmt_closeResult(result);
    js_free(ctx, empty);
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
