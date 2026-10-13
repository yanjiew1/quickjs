/* Intl.Segmenter native C frontend.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd; review 2026-10-09.
 * en/en-US explicitly use Unicode18 UAX29 default segmentation. */
#include "intl-internal.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../../intl/provider-native-segmenter.h"
#include <math.h>
typedef struct JSIntlSegmenter {
    JSIntlResolvedLocale locale;
    QJSIntlProviderSegmenter *handle;
    int granularity;
} JSIntlSegmenter;
typedef struct JSIntlSegments {
    JSValue segmenter, string;
    QJSIntlNativeSegments *snapshot;
    size_t length;
} JSIntlSegments;
typedef struct JSIntlSegmentIterator {
    JSValue segments;
    size_t cursor;
    BOOL exhausted;
} JSIntlSegmentIterator;
static const char *const matchers[] = { "lookup", "best fit" };
static const char *const granularities[] = { "grapheme", "word", "sentence" };
static void segmenter_free(JSRuntime *rt, JSIntlSegmenter *s)
{
    int i;
    if (!s) return;
    qjs_intl_native_provider_segmenter_close(s->handle);
    js_free_rt(rt, s->locale.locale);
    js_free_rt(rt, s->locale.data_locale);
    for (i = 0; i < s->locale.key_count; i++) js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s);
}
static void segmenter_finalizer(JSRuntime *rt, JSValue v)
{
    segmenter_free(rt, JS_GetOpaque(v, JS_CLASS_INTL_SEGMENTER));
}
static void segments_finalizer(JSRuntime *rt, JSValue v)
{
    JSIntlSegments *s = JS_GetOpaque(v, JS_CLASS_INTL_SEGMENTS);
    if (!s) return;
    qjs_intl_native_segments_free(s->snapshot);
    JS_FreeValueRT(rt, s->segmenter);
    JS_FreeValueRT(rt, s->string);
    js_free_rt(rt, s);
}
static void segments_mark(JSRuntime *rt, JSValueConst v, JS_MarkFunc *mark)
{
    JSIntlSegments *s = JS_GetOpaque(v, JS_CLASS_INTL_SEGMENTS);
    if (!s) return;
    JS_MarkValue(rt, s->segmenter, mark);
    JS_MarkValue(rt, s->string, mark);
}
static void iterator_finalizer(JSRuntime *rt, JSValue v)
{
    JSIntlSegmentIterator *s = JS_GetOpaque(v, JS_CLASS_INTL_SEGMENT_ITERATOR);
    if (!s) return;
    JS_FreeValueRT(rt, s->segments);
    js_free_rt(rt, s);
}
static void iterator_mark(JSRuntime *rt, JSValueConst v, JS_MarkFunc *mark)
{
    JSIntlSegmentIterator *s = JS_GetOpaque(v, JS_CLASS_INTL_SEGMENT_ITERATOR);
    if (s) JS_MarkValue(rt, s->segments, mark);
}
static JSValue segmenter_constructor(JSContext *ctx, JSValueConst target,
                                     int argc, JSValueConst *argv)
{
    JSValue object, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlSegmenter *s = NULL;
    QJSIntlProvider *provider;
    QJSIntlStatus status;
    int matcher;
    static const QJSIntlSegmentGranularity native[] = {
        QJS_INTL_SEGMENT_GRAPHEME, QJS_INTL_SEGMENT_WORD, QJS_INTL_SEGMENT_SENTENCE };
    if (JS_IsUndefined(target)) return JS_ThrowTypeError(ctx, "Intl.Segmenter requires new");
    object = js_intl_new_object(ctx, target, JS_CLASS_INTL_SEGMENTER);
    if (JS_IsException(object)) return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    if (js_intl_canonicalize_locale_list(ctx, argc > 0 ? argv[0] : JS_UNDEFINED,
                                       &requested) < 0) goto fail;
    options = js_intl_get_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(options)) goto fail;
    if (js_intl_get_string_option(ctx, options, "localeMatcher", matchers, 2, 1,
                                  &matcher) < 0 ||
        js_intl_resolve_locale(ctx, JS_INTL_SEGMENTER, &requested,
            matchers[matcher], NULL, 0, &s->locale) < 0 ||
        js_intl_get_string_option(ctx, options, "granularity", granularities,
                                  3, 0, &s->granularity) < 0) goto fail;
    provider = js_intl_native_provider(ctx);
    if (!provider) goto fail;
    status = qjs_intl_native_provider_segmenter_open(provider,
        (QJSIntlBytes){ s->locale.data_locale, strlen(s->locale.data_locale) },
        native[s->granularity], &s->handle);
    if (js_intl_native_error(ctx, status, "Segmenter open")) goto fail;
    JS_SetOpaque(object, s);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    return object;
fail:
    segmenter_free(ctx->rt, s);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}
static JSValue segmenter_resolved(JSContext *ctx, JSValueConst receiver,
                                  int argc, JSValueConst *argv)
{
    JSIntlSegmenter *s = JS_GetOpaque2(ctx, receiver, JS_CLASS_INTL_SEGMENTER);
    JSValue object;
    if (!s) return JS_EXCEPTION;
    object = JS_NewObject(ctx);
    if (JS_IsException(object)) return object;
    if (js_intl_define_string(ctx, object, "locale", s->locale.locale) < 0 ||
        js_intl_define_string(ctx, object, "granularity", granularities[s->granularity]) < 0) {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    return object;
}
static JSValue segmenter_segment(JSContext *ctx, JSValueConst receiver,
                                 int argc, JSValueConst *argv)
{
    JSIntlSegmenter *segmenter = JS_GetOpaque2(ctx, receiver, JS_CLASS_INTL_SEGMENTER);
    JSIntlSegments *s;
    JSValue object = JS_UNDEFINED, string;
    uint16_t *text = NULL;
    int32_t length;
    QJSIntlStatus status;
    if (!segmenter) return JS_EXCEPTION;
    /* Brand precedes coercion; coercion may reenter without a shared cursor. */
    string = JS_ToString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(string)) return string;
    object = JS_NewObjectClass(ctx, JS_CLASS_INTL_SEGMENTS);
    if (JS_IsException(object)) goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    s->segmenter = JS_DupValue(ctx, receiver);
    s->string = JS_DupValue(ctx, string);
    JS_SetOpaque(object, s);
    if (js_intl_to_utf16(ctx, string, &text, &length) < 0) goto fail;
    status = qjs_intl_native_provider_segments_new(segmenter->handle,
        (QJSIntlUTF16){ text, (size_t)length }, &s->snapshot);
    js_free(ctx, text);
    text = NULL;
    if (js_intl_native_error(ctx, status, "Segmenter.segment")) goto fail;
    s->length = (size_t)length;
    JS_FreeValue(ctx, string);
    return object;
fail:
    js_free(ctx, text);
    JS_FreeValue(ctx, string);
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}
static JSValue segment_data(JSContext *ctx, JSIntlSegments *s,
                            const QJSIntlSegmentResult *result)
{
    JSValue object, part;
    if (result->start >= result->end || result->end > s->length ||
        result->end > INT32_MAX || result->start > INT32_MAX) {
        return JS_ThrowInternalError(ctx, "invalid native segment boundary");
    }
    object = JS_NewObject(ctx);
    if (JS_IsException(object)) return object;
    part = js_sub_string(ctx, JS_VALUE_GET_STRING(s->string),
                          (int32_t)result->start, (int32_t)result->end);
    if (JS_IsException(part)) goto fail;
    if (JS_DefinePropertyValueStr(ctx, object, "segment", part, JS_PROP_C_W_E) < 0 ||
        js_intl_define_int(ctx, object, "index", (int)result->start) < 0 ||
        JS_DefinePropertyValueStr(ctx, object, "input", JS_DupValue(ctx, s->string),
                                  JS_PROP_C_W_E) < 0 ||
        (result->has_word_like && js_intl_define_bool(ctx, object, "isWordLike",
                                                     result->is_word_like) < 0)) goto fail;
    return object;
fail:
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}
static JSValue segments_containing(JSContext *ctx, JSValueConst receiver,
                                   int argc, JSValueConst *argv)
{
    JSIntlSegments *s = JS_GetOpaque2(ctx, receiver, JS_CLASS_INTL_SEGMENTS);
    QJSIntlSegmentResult result;
    QJSIntlStatus status;
    double index;
    int found;
    if (!s) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &index, argc > 0 ? argv[0] : JS_UNDEFINED) < 0)
        return JS_EXCEPTION;
    index = isnan(index) ? 0 : trunc(index);
    if (index < 0 || index >= (double)s->length) return JS_UNDEFINED;
    status = qjs_intl_native_segments_containing(s->snapshot, (size_t)index,
                                                &result, &found);
    if (js_intl_native_error(ctx, status, "Segments.containing")) return JS_EXCEPTION;
    if (!found) return JS_UNDEFINED;
    return segment_data(ctx, s, &result);
}
static JSValue segments_iterator(JSContext *ctx, JSValueConst receiver,
                                 int argc, JSValueConst *argv)
{
    JSIntlSegments *segments = JS_GetOpaque2(ctx, receiver, JS_CLASS_INTL_SEGMENTS);
    JSIntlSegmentIterator *s;
    JSValue object;
    if (!segments) return JS_EXCEPTION;
    object = JS_NewObjectClass(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR);
    if (JS_IsException(object)) return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) { JS_FreeValue(ctx, object); return JS_EXCEPTION; }
    s->segments = JS_DupValue(ctx, receiver);
    JS_SetOpaque(object, s);
    return object;
}
static JSValue iterator_next(JSContext *ctx, JSValueConst receiver,
                             int argc, JSValueConst *argv, BOOL *done, int magic)
{
    JSIntlSegmentIterator *s = JS_GetOpaque2(ctx, receiver, JS_CLASS_INTL_SEGMENT_ITERATOR);
    JSIntlSegments *segments;
    QJSIntlSegmentResult result;
    QJSIntlStatus status;
    int finished;
    if (!s) return JS_EXCEPTION;
    if (s->exhausted) { *done = TRUE; return JS_UNDEFINED; }
    segments = JS_GetOpaque(s->segments, JS_CLASS_INTL_SEGMENTS);
    /* C next commits ordinal before allocating the observable segment data.
     * On a JS allocation failure the next call proceeds to the next segment. */
    status = qjs_intl_native_segments_next(segments->snapshot, &s->cursor,
                                          &result, &finished);
    if (js_intl_native_error(ctx, status, "Segment Iterator.next")) return JS_EXCEPTION;
    if (finished) {
        s->exhausted = TRUE;
        JS_FreeValue(ctx, s->segments);
        s->segments = JS_UNDEFINED;
        *done = TRUE;
        return JS_UNDEFINED;
    }
    *done = FALSE;
    return segment_data(ctx, segments, &result);
}
static JSValue segmenter_supported(JSContext *ctx, JSValueConst receiver,
                                   int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_SEGMENTER,
        argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef segmenter_class = { "Intl.Segmenter", .finalizer = segmenter_finalizer };
static const JSClassDef segments_class = {
    "Intl.Segments", .finalizer = segments_finalizer, .gc_mark = segments_mark };
static const JSClassDef iterator_class = {
    "Segmenter String Iterator", .finalizer = iterator_finalizer, .gc_mark = iterator_mark };
static const JSCFunctionListEntry constructor_functions[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, segmenter_supported) };
static const JSCFunctionListEntry prototype_functions[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, segmenter_resolved),
    JS_CFUNC_DEF("segment", 1, segmenter_segment),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Segmenter", JS_PROP_CONFIGURABLE) };
static const JSCFunctionListEntry segments_functions[] = {
    JS_CFUNC_DEF("containing", 1, segments_containing),
    JS_CFUNC_DEF("[Symbol.iterator]", 0, segments_iterator) };
static const JSCFunctionListEntry iterator_functions[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, iterator_next, 0),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Segmenter String Iterator", JS_PROP_CONFIGURABLE) };
static int install_constructor(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_SEGMENTER,
        &segmenter_class, "Segmenter", segmenter_constructor, 0, JS_CFUNC_constructor,
        constructor_functions, countof(constructor_functions), prototype_functions,
        countof(prototype_functions));
}
int js_intl_init_segmenter(JSContext *ctx, JSValueConst intl)
{
    JSValue segments, iterator;
    if (!JS_IsUndefined(js_intl_constructor(ctx, JS_CLASS_INTL_SEGMENTER)))
        return install_constructor(ctx, intl);
    if (js_intl_register_class(ctx, JS_CLASS_INTL_SEGMENTS, &segments_class) < 0 ||
        js_intl_register_class(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR, &iterator_class) < 0) return -1;
    segments = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_OBJECT],
                                     segments_functions, countof(segments_functions));
    if (JS_IsException(segments)) return -1;
    iterator = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                                     iterator_functions, countof(iterator_functions));
    if (JS_IsException(iterator)) { JS_FreeValue(ctx, segments); return -1; }
    JS_SetClassProto(ctx, JS_CLASS_INTL_SEGMENTS, segments);
    JS_SetClassProto(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR, iterator);
    return install_constructor(ctx, intl);
}
#endif
