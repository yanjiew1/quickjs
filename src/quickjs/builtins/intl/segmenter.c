/* Native Intl.Segmenter, Segments, and Segment Iterator.
 * ECMA-402 7ae78cfdf8255468ffc8ebda33dafaea952808dd (2026-10-06).
 * All boundaries and public indices are UTF-16 code unit offsets.
 */
#include "intl-internal.h"
#ifdef CONFIG_INTL
#include <unicode/ubrk.h>
#include <math.h>

typedef struct JSIntlSegmenter {
    JSIntlResolvedLocale locale;
    int granularity;
} JSIntlSegmenter;

/* Every Segments and every iterator owns a separate ICU cursor and buffer.
   ICU retains the UChar pointer until close; the buffer outlives the cursor.
   No full list of segment objects or boundaries is materialized. */
typedef struct JSIntlSegmentCursor {
    JSValue segmenter;
    JSValue string;
    UChar *text;
    UBreakIterator *iterator;
    int32_t length;
    int32_t next_index;
} JSIntlSegmentCursor;

static const char *const segmenter_matcher[] = { "lookup", "best fit" };
static const char *const segmenter_granularity[] = {
    "grapheme", "word", "sentence"
};
static const UBreakIteratorType segmenter_break_type[] = {
    UBRK_CHARACTER, UBRK_WORD, UBRK_SENTENCE
};

static void js_intl_segmenter_free(JSRuntime *rt, JSIntlSegmenter *s)
{
    int i;
    if (!s)
        return;
    js_free_rt(rt, s->locale.locale);
    js_free_rt(rt, s->locale.data_locale);
    js_free_rt(rt, s->locale.icu_locale);
    for (i = 0; i < s->locale.key_count; i++)
        js_free_rt(rt, s->locale.values[i]);
    js_free_rt(rt, s);
}

static void js_intl_segmenter_finalizer(JSRuntime *rt, JSValue value)
{
    js_intl_segmenter_free(rt, JS_GetOpaque(value, JS_CLASS_INTL_SEGMENTER));
}

static void js_intl_segment_cursor_free(JSRuntime *rt, JSIntlSegmentCursor *s)
{
    if (!s)
        return;
    if (s->iterator)
        ubrk_close(s->iterator);
    js_free_rt(rt, s->text);
    JS_FreeValueRT(rt, s->segmenter);
    JS_FreeValueRT(rt, s->string);
    js_free_rt(rt, s);
}

static void js_intl_segments_finalizer(JSRuntime *rt, JSValue value)
{
    js_intl_segment_cursor_free(rt, JS_GetOpaque(value, JS_CLASS_INTL_SEGMENTS));
}

static void js_intl_segment_iterator_finalizer(JSRuntime *rt, JSValue value)
{
    js_intl_segment_cursor_free(rt,
        JS_GetOpaque(value, JS_CLASS_INTL_SEGMENT_ITERATOR));
}

static void js_intl_segment_cursor_mark(JSRuntime *rt, JSIntlSegmentCursor *s,
                                        JS_MarkFunc *mark_func)
{
    if (!s)
        return;
    JS_MarkValue(rt, s->segmenter, mark_func);
    JS_MarkValue(rt, s->string, mark_func);
}

static void js_intl_segments_mark(JSRuntime *rt, JSValueConst value,
                                   JS_MarkFunc *mark_func)
{
    js_intl_segment_cursor_mark(rt, JS_GetOpaque(value, JS_CLASS_INTL_SEGMENTS),
                                mark_func);
}

static void js_intl_segment_iterator_mark(JSRuntime *rt, JSValueConst value,
                                           JS_MarkFunc *mark_func)
{
    js_intl_segment_cursor_mark(rt,
        JS_GetOpaque(value, JS_CLASS_INTL_SEGMENT_ITERATOR), mark_func);
}

static JSValue js_intl_segment_cursor_new(JSContext *ctx,
                                           JSClassID class_id,
                                           JSValueConst segmenter,
                                           JSValueConst string)
{
    JSValue object;
    JSIntlSegmentCursor *s;
    object = JS_NewObjectClass(ctx, class_id);
    if (JS_IsException(object))
        return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    s->segmenter = JS_DupValue(ctx, segmenter);
    s->string = JS_DupValue(ctx, string);
    s->length = JS_VALUE_GET_STRING(string)->len;
    JS_SetOpaque(object, s);
    return object;
}

static int js_intl_segment_cursor_open(JSContext *ctx, JSIntlSegmentCursor *s)
{
    JSIntlSegmenter *segmenter;
    UChar *text = NULL;
    UBreakIterator *iterator;
    int32_t length;
    UErrorCode status = U_ZERO_ERROR;
    if (s->iterator)
        return 0;
    segmenter = JS_GetOpaque(s->segmenter, JS_CLASS_INTL_SEGMENTER);
    if (js_intl_to_uchar(ctx, s->string, &text, &length) < 0)
        return -1;
    iterator = ubrk_open(segmenter_break_type[segmenter->granularity],
                         segmenter->locale.icu_locale, text, length, &status);
    if (js_intl_icu_error(ctx, status, "Segmenter break iterator") < 0 ||
        !iterator) {
        if (iterator)
            ubrk_close(iterator);
        js_free(ctx, text);
        if (U_SUCCESS(status))
            JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    /* Commit both resource owners together, after success. */
    s->text = text;
    s->iterator = iterator;
    return 0;
}

static JSValue js_intl_segmenter_constructor(JSContext *ctx,
                                             JSValueConst new_target,
                                             int argc, JSValueConst *argv)
{
    JSValue object, options = JS_UNDEFINED;
    JSIntlLocaleList requested = { 0 };
    JSIntlSegmenter *s = NULL;
    int matcher;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.Segmenter requires new");
    object = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_SEGMENTER);
    if (JS_IsException(object))
        return object;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    if (js_intl_canonicalize_locale_list(ctx, argc > 0 ? argv[0] : JS_UNDEFINED,
                                        &requested) < 0)
        goto fail;
    options = js_intl_get_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(options))
        goto fail;
    if (js_intl_get_string_option(ctx, options, "localeMatcher", segmenter_matcher,
                                  2, 1, &matcher) < 0 ||
        js_intl_resolve_locale(ctx, JS_INTL_SEGMENTER, &requested,
                               segmenter_matcher[matcher], NULL, 0,
                               &s->locale) < 0 ||
        js_intl_get_string_option(ctx, options, "granularity",
                                  segmenter_granularity, 3, 0,
                                  &s->granularity) < 0)
        goto fail;
    JS_SetOpaque(object, s);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    return object;
 fail:
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, options);
    js_intl_segmenter_free(ctx->rt, s);
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue js_intl_segmenter_resolved_options(JSContext *ctx,
                                                  JSValueConst this_value,
                                                  int argc, JSValueConst *argv)
{
    JSIntlSegmenter *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_SEGMENTER);
    JSValue result;
    if (!s)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale.locale) < 0 ||
        js_intl_define_string(ctx, result, "granularity",
                              segmenter_granularity[s->granularity]) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}

static JSValue js_intl_segmenter_segment(JSContext *ctx,
                                         JSValueConst this_value,
                                         int argc, JSValueConst *argv)
{
    JSIntlSegmenter *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_SEGMENTER);
    JSValue string, result;
    if (!s)
        return JS_EXCEPTION;
    string = JS_ToString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(string))
        return string;
    result = js_intl_segment_cursor_new(ctx, JS_CLASS_INTL_SEGMENTS, this_value,
                                        string);
    JS_FreeValue(ctx, string);
    return result;
}

static JSValue js_intl_segment_data(JSContext *ctx, JSIntlSegmentCursor *s,
                                    int32_t start, int32_t end)
{
    JSIntlSegmenter *segmenter = JS_GetOpaque(s->segmenter, JS_CLASS_INTL_SEGMENTER);
    JSValue object, part;
    BOOL word_like;
    /* Cursor must still be at the end boundary for ICU's rule status. */
    word_like = ubrk_getRuleStatus(s->iterator) >= UBRK_WORD_NONE_LIMIT;
    object = JS_NewObject(ctx);
    if (JS_IsException(object))
        return object;
    part = js_sub_string(ctx, JS_VALUE_GET_STRING(s->string), start, end);
    if (JS_IsException(part))
        goto fail;
    if (JS_DefinePropertyValueStr(ctx, object, "segment", part, JS_PROP_C_W_E) < 0 ||
        js_intl_define_int(ctx, object, "index", start) < 0 ||
        JS_DefinePropertyValueStr(ctx, object, "input", JS_DupValue(ctx, s->string),
                                  JS_PROP_C_W_E) < 0 ||
        (segmenter->granularity == 1 &&
         js_intl_define_bool(ctx, object, "isWordLike", word_like) < 0))
        goto fail;
    return object;
 fail:
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue js_intl_segments_containing(JSContext *ctx,
                                           JSValueConst this_value,
                                           int argc, JSValueConst *argv)
{
    JSIntlSegmentCursor *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_SEGMENTS);
    double index;
    int32_t position, start, end;
    if (!s)
        return JS_EXCEPTION;
    /* sec-%intlsegmentsprototype%.containing requires ToIntegerOrInfinity,
       including NaN and negative zero, before the range check. */
    if (JS_ToFloat64(ctx, &index, argc > 0 ? argv[0] : JS_UNDEFINED) < 0)
        return JS_EXCEPTION;
    index = isnan(index) ? 0 : trunc(index);
    if (index < 0 || index >= s->length)
        return JS_UNDEFINED;
    position = (int32_t)index;
    if (js_intl_segment_cursor_open(ctx, s) < 0)
        return JS_EXCEPTION;
    /* isBoundary leaves ICU at the first boundary at or after position.
       previous therefore finds the containing start without passing a trail
       surrogate offset to ICU's strictly preceding operation. */
    start = ubrk_isBoundary(s->iterator, position) ? position :
        ubrk_previous(s->iterator);
    end = ubrk_following(s->iterator, position);
    if (start == UBRK_DONE)
        start = 0;
    if (end == UBRK_DONE)
        end = s->length;
    return js_intl_segment_data(ctx, s, start, end);
}

static JSValue js_intl_segments_iterator(JSContext *ctx,
                                         JSValueConst this_value,
                                         int argc, JSValueConst *argv)
{
    JSIntlSegmentCursor *s = JS_GetOpaque2(ctx, this_value, JS_CLASS_INTL_SEGMENTS);
    if (!s)
        return JS_EXCEPTION;
    return js_intl_segment_cursor_new(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR,
                                       s->segmenter, s->string);
}

static JSValue js_intl_segment_iterator_next(JSContext *ctx,
                                             JSValueConst this_value,
                                             int argc, JSValueConst *argv,
                                             BOOL *done, int magic)
{
    JSIntlSegmentCursor *s = JS_GetOpaque2(ctx, this_value,
                                          JS_CLASS_INTL_SEGMENT_ITERATOR);
    int32_t start, end;
    if (!s)
        return JS_EXCEPTION;
    start = s->next_index;
    if (start >= s->length) {
        /* Keep the slots for branding; release native text/ICU at exhaustion. */
        if (s->iterator) {
            ubrk_close(s->iterator);
            s->iterator = NULL;
        }
        js_free(ctx, s->text);
        s->text = NULL;
        *done = TRUE;
        return JS_UNDEFINED;
    }
    if (js_intl_segment_cursor_open(ctx, s) < 0)
        return JS_EXCEPTION;
    end = ubrk_following(s->iterator, start);
    if (end == UBRK_DONE)
        end = s->length;
    /* The spec advances before creating the Segment Data object. */
    s->next_index = end;
    *done = FALSE;
    return js_intl_segment_data(ctx, s, start, end);
}

static JSValue js_intl_segmenter_supported_locales(JSContext *ctx,
                                                   JSValueConst this_value,
                                                   int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_SEGMENTER,
        argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
}

static const JSClassDef segmenter_class = {
    "Intl.Segmenter", .finalizer = js_intl_segmenter_finalizer,
};
static const JSClassDef segments_class = {
    "Intl.Segments", .finalizer = js_intl_segments_finalizer,
    .gc_mark = js_intl_segments_mark,
};
static const JSClassDef segment_iterator_class = {
    "Segmenter String Iterator", .finalizer = js_intl_segment_iterator_finalizer,
    .gc_mark = js_intl_segment_iterator_mark,
};
static const JSCFunctionListEntry segmenter_constructor_functions[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_segmenter_supported_locales),
};
static const JSCFunctionListEntry segmenter_prototype_functions[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_segmenter_resolved_options),
    JS_CFUNC_DEF("segment", 1, js_intl_segmenter_segment),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Segmenter", JS_PROP_CONFIGURABLE),
};
static const JSCFunctionListEntry segments_prototype_functions[] = {
    JS_CFUNC_DEF("containing", 1, js_intl_segments_containing),
    JS_CFUNC_DEF("[Symbol.iterator]", 0, js_intl_segments_iterator),
};
static const JSCFunctionListEntry segment_iterator_prototype_functions[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_intl_segment_iterator_next, 0),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Segmenter String Iterator",
                        JS_PROP_CONFIGURABLE),
};

static int js_intl_segmenter_install_constructor(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_SEGMENTER,
        &segmenter_class, "Segmenter", js_intl_segmenter_constructor, 0,
        JS_CFUNC_constructor, segmenter_constructor_functions,
        countof(segmenter_constructor_functions), segmenter_prototype_functions,
        countof(segmenter_prototype_functions));
}

int js_intl_init_segmenter(JSContext *ctx, JSValueConst intl)
{
    JSValue segments_prototype, iterator_prototype;
    /* Lazy initialization followed by global publication must preserve the
       realm's private Segments and Segment Iterator prototype identities. */
    if (!JS_IsUndefined(js_intl_constructor(ctx, JS_CLASS_INTL_SEGMENTER)))
        return js_intl_segmenter_install_constructor(ctx, intl);
    if (js_intl_register_class(ctx, JS_CLASS_INTL_SEGMENTS, &segments_class) < 0 ||
        js_intl_register_class(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR,
                                &segment_iterator_class) < 0)
        return -1;
    segments_prototype = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_OBJECT],
        segments_prototype_functions, countof(segments_prototype_functions));
    if (JS_IsException(segments_prototype))
        return -1;
    iterator_prototype = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
        segment_iterator_prototype_functions,
        countof(segment_iterator_prototype_functions));
    if (JS_IsException(iterator_prototype)) {
        JS_FreeValue(ctx, segments_prototype);
        return -1;
    }
    JS_SetClassProto(ctx, JS_CLASS_INTL_SEGMENTS, segments_prototype);
    JS_SetClassProto(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR, iterator_prototype);
    return js_intl_segmenter_install_constructor(ctx, intl);
}
#endif /* CONFIG_INTL */
