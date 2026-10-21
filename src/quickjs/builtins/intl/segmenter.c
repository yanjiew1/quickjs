/* Intl.Segmenter binding and realm/GC ownership of its hidden iterables.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/runtime.h"
#include "../../internal/atom.h"
#include "../../internal/object.h"
#include "../../internal/number.h"

static const char *const granularities[] = {"grapheme", "word", "sentence"};

typedef struct JSIntlSegmenter {
    QJSIntlSegmenter *handle;
    char *locale;
    int granularity;
} JSIntlSegmenter;

/* Segments and its iterators independently retain immutable input. Iterators
   do not retain the Segments wrapper or copy the whole text on every creation.
   input is always a primitive string; segmenter is the only GC object edge. */
typedef struct JSIntlSegmentInput {
    size_t references;
    JSValue input;
    QJSIntlUTF16 text;
} JSIntlSegmentInput;

typedef struct JSIntlSegments {
    JSValue segmenter;
    JSIntlSegmentInput *input;
    size_t next_index; /* Used by iterators only, never by containing(). */
} JSIntlSegments;

static QJSIntlSegmentService *segment_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_segment) {
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        QJSIntlSegmentService *service;
        if (js_intl_error(ctx, qjs_intl_segment_open(&allocator, &service), "segmentation initialization"))
            return NULL;
        rt->intl_segment = service;
    }
    return rt->intl_segment;
}

static void segmenter_finalizer(JSRuntime *rt, JSValue value)
{
    JSIntlSegmenter *state = JS_GetOpaque(value, JS_CLASS_INTL_SEGMENTER);
    if (state) {
        qjs_intl_segmenter_close(state->handle);
        js_free_rt(rt, state->locale);
        js_free_rt(rt, state);
    }
}

static void input_release(JSRuntime *rt, JSIntlSegmentInput *input)
{
    if (input && !--input->references) {
        JS_FreeCStringRT_UTF16(rt, input->text.data);
        JS_FreeValueRT(rt, input->input);
        js_free_rt(rt, input);
    }
}

static void segments_finalizer(JSRuntime *rt, JSValue value)
{
    JSClassID class_id = JS_VALUE_GET_OBJ(value)->class_id;
    JSIntlSegments *state = JS_GetOpaque(value, class_id);
    if (state) {
        JS_FreeValueRT(rt, state->segmenter);
        input_release(rt, state->input);
        js_free_rt(rt, state);
    }
}

static void segments_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark)
{
    JSClassID class_id = JS_VALUE_GET_OBJ(value)->class_id;
    JSIntlSegments *state = JS_GetOpaque(value, class_id);
    if (state) {
        JS_MarkValue(rt, state->segmenter, mark);
        if (state->input)
            JS_MarkValue(rt, state->input->input, mark);
    }
}

static JSValue segmenter_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                                     JSValueConst *argv)
{
    JSIntlLocaleList requested = {0};
    JSValue object, options = JS_UNDEFINED;
    QJSIntlSegmentService *service;
    const QJSIntlBytes *available;
    size_t count, locale;
    int granularity;
    JSIntlSegmenter *state;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.Segmenter requires new");
    object = js_intl_create_from_constructor(ctx, new_target, JS_CLASS_INTL_SEGMENTER);
    if (JS_IsException(object))
        return object;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto fail;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, FALSE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0)
        goto fail;
    service = segment_service(ctx);
    if (!service ||
        js_intl_error(ctx, qjs_intl_segment_available(service, &available, &count), "segmenter locales") ||
        js_intl_resolve_locale_index(ctx, available, count, &requested, &locale))
        goto fail;
    granularity = js_intl_string_option(ctx, options, "granularity", granularities, 3, 0);
    if (granularity < 0)
        goto fail;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto fail;
    JS_SetOpaque(object, state);
    state->granularity = granularity;
    state->locale = js_strndup(ctx, available[locale].data, available[locale].length);
    if (!state->locale || js_intl_error(ctx,
            qjs_intl_segmenter_open(service, locale, (QJSIntlGranularity)granularity, &state->handle), "Segmenter"))
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

static JSValue segmenter_supported(JSContext *ctx, JSValueConst this_val, int argc,
                                   JSValueConst *argv)
{
    QJSIntlSegmentService *service = segment_service(ctx);
    const QJSIntlBytes *available;
    size_t count;
    if (!service || js_intl_error(ctx, qjs_intl_segment_available(service, &available, &count),
                                  "segmenter locales"))
        return JS_EXCEPTION;
    return js_intl_filter_locales(ctx, available, count, argc ? argv[0] : JS_UNDEFINED,
                                  argc > 1 ? argv[1] : JS_UNDEFINED);
}

static JSValue segmenter_resolved(JSContext *ctx, JSValueConst this_val, int argc,
                                  JSValueConst *argv)
{
    JSIntlSegmenter *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_SEGMENTER);
    JSValue result, locale, granularity;
    if (!state)
        return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    locale = JS_NewString(ctx, state->locale);
    if (JS_IsException(locale) || JS_DefinePropertyValueStr(ctx, result, "locale", locale,
                                                           JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto fail;
    granularity = JS_NewString(ctx, granularities[state->granularity]);
    if (JS_IsException(granularity) || JS_DefinePropertyValueStr(ctx, result, "granularity", granularity,
                                                                JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto fail;
    return result;
fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue segmenter_segment(JSContext *ctx, JSValueConst this_val, int argc,
                                 JSValueConst *argv)
{
    JSIntlSegmenter *segmenter = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_SEGMENTER);
    JSIntlSegments *state;
    JSIntlSegmentInput *input;
    JSValue object, string;
    if (!segmenter)
        return JS_EXCEPTION;
    string = JS_ToString(ctx, argc ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(string))
        return string;
    object = JS_NewObjectClass(ctx, JS_CLASS_INTL_SEGMENTS);
    if (JS_IsException(object)) {
        JS_FreeValue(ctx, string);
        return object;
    }
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        goto fail;
    state->segmenter = JS_DupValue(ctx, this_val);
    JS_SetOpaque(object, state);
    input = js_mallocz(ctx, sizeof(*input));
    if (!input)
        goto fail;
    input->references = 1;
    input->input = string;
    string = JS_UNDEFINED;
    state->input = input;
    input->text.data = JS_ToCStringLenUTF16(ctx, &input->text.length, input->input);
    if (!input->text.data)
        goto fail;
    return object;
fail:
    JS_FreeValue(ctx, string);
    JS_FreeValue(ctx, object);
    return JS_EXCEPTION;
}

static JSValue segment_data(JSContext *ctx, const JSIntlSegments *state,
                            const QJSIntlSegment *segment)
{
    JSIntlSegmenter *segmenter = JS_GetOpaque(state->segmenter, JS_CLASS_INTL_SEGMENTER);
    JSValue result = JS_NewObject(ctx), value;
    if (JS_IsException(result))
        return result;
    value = JS_NewStringUTF16(ctx, state->input->text.data + segment->start,
                             segment->end - segment->start);
    if (JS_IsException(value) || JS_DefinePropertyValueStr(ctx, result, "segment", value,
                                                          JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto fail;
    if (JS_DefinePropertyValueStr(ctx, result, "index", JS_NewFloat64(ctx, segment->start),
                                  JS_PROP_C_W_E | JS_PROP_THROW) < 0 ||
        JS_DefinePropertyValueStr(ctx, result, "input", JS_DupValue(ctx, state->input->input),
                                  JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto fail;
    if (segmenter->granularity == QJS_INTL_SEGMENT_WORD &&
        JS_DefinePropertyValueStr(ctx, result, "isWordLike", JS_NewBool(ctx, segment->is_word_like),
                                  JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto fail;
    return result;
fail:
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}

static JSValue segments_containing(JSContext *ctx, JSValueConst this_val, int argc,
                                   JSValueConst *argv)
{
    JSIntlSegments *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_SEGMENTS);
    JSIntlSegmenter *segmenter;
    JSValue integer;
    QJSIntlSegment segment;
    double index;
    if (!state)
        return JS_EXCEPTION;
    /* Even an empty input must perform observable ToIntegerOrInfinity. */
    integer = JS_ToIntegerFree(ctx, JS_DupValue(ctx, argc ? argv[0] : JS_UNDEFINED));
    if (JS_IsException(integer))
        return integer;
    JS_ToFloat64(ctx, &index, integer);
    JS_FreeValue(ctx, integer);
    if (index < 0 || index >= state->input->text.length)
        return JS_UNDEFINED;
    segmenter = JS_GetOpaque(state->segmenter, JS_CLASS_INTL_SEGMENTER);
    if (js_intl_error(ctx, qjs_intl_segment_containing(segmenter->handle, &state->input->text,
                                                      (size_t)index, &segment), "segment lookup"))
        return JS_EXCEPTION;
    return segment_data(ctx, state, &segment);
}

static JSValue segments_iterator(JSContext *ctx, JSValueConst this_val, int argc,
                                 JSValueConst *argv)
{
    JSIntlSegments *source = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_SEGMENTS);
    JSIntlSegments *state;
    JSValue object;
    if (!source)
        return JS_EXCEPTION;
    if (source->input->references == SIZE_MAX)
        return JS_ThrowOutOfMemory(ctx);
    object = JS_NewObjectClass(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR);
    if (JS_IsException(object))
        return object;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state) {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    state->segmenter = JS_DupValue(ctx, source->segmenter);
    state->input = source->input;
    state->input->references++;
    JS_SetOpaque(object, state);
    return object;
}

static JSValue segment_iterator_next(JSContext *ctx, JSValueConst this_val, int argc,
                                     JSValueConst *argv, BOOL *done, int magic)
{
    JSIntlSegments *state = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_SEGMENT_ITERATOR);
    JSIntlSegmenter *segmenter;
    QJSIntlSegment segment;
    *done = FALSE;
    if (!state)
        return JS_EXCEPTION;
    if (state->next_index == state->input->text.length) {
        *done = TRUE;
        return JS_UNDEFINED;
    }
    segmenter = JS_GetOpaque(state->segmenter, JS_CLASS_INTL_SEGMENTER);
    if (js_intl_error(ctx, qjs_intl_segment_next(segmenter->handle, &state->input->text,
                                                state->next_index, &segment), "segment iteration"))
        return JS_EXCEPTION;
    /* The specification commits the cursor before creating the result objects;
       an allocation failure must not rewind an otherwise completed step. */
    state->next_index = segment.end;
    return segment_data(ctx, state, &segment);
}

static const JSCFunctionListEntry segmenter_statics[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, segmenter_supported),
};
static const JSCFunctionListEntry segmenter_properties[] = {
    JS_CFUNC_DEF("segment", 1, segmenter_segment),
    JS_CFUNC_DEF("resolvedOptions", 0, segmenter_resolved),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.Segmenter", JS_PROP_CONFIGURABLE),
};
static const JSCFunctionListEntry segments_properties[] = {
    JS_CFUNC_DEF("containing", 1, segments_containing),
    JS_CFUNC_DEF("[Symbol.iterator]", 0, segments_iterator),
};
static const JSCFunctionListEntry iterator_properties[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, segment_iterator_next, 0),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Segmenter String Iterator", JS_PROP_CONFIGURABLE),
};

int js_intl_init_segmenter(JSContext *ctx, JSValueConst namespace_object)
{
    static const JSClassDef definitions[] = {
        {.class_name = "Intl.Segmenter", .finalizer = segmenter_finalizer},
        {.class_name = "Intl.Segments", .finalizer = segments_finalizer, .gc_mark = segments_mark},
        {.class_name = "Segmenter String Iterator", .finalizer = segments_finalizer, .gc_mark = segments_mark},
    };
    static const JSClassID classes[] = {
        JS_CLASS_INTL_SEGMENTER, JS_CLASS_INTL_SEGMENTS, JS_CLASS_INTL_SEGMENT_ITERATOR,
    };
    JSValue constructor, prototype;
    size_t i;
    for (i = 0; i < countof(classes); i++) {
        if (!JS_IsRegisteredClass(ctx->rt, classes[i]) && JS_NewClass(ctx->rt, classes[i], &definitions[i]) < 0) {
            JS_ThrowOutOfMemory(ctx);
            return -1;
        }
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_INTL_SEGMENTER, "Segmenter", segmenter_constructor, 0,
                                     JS_CFUNC_constructor, 0, JS_UNDEFINED,
                                     segmenter_statics, countof(segmenter_statics),
                                     segmenter_properties, countof(segmenter_properties), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    prototype = JS_NewObject(ctx);
    if (JS_IsException(prototype))
        goto fail;
    if (JS_SetPropertyFunctionList(ctx, prototype, segments_properties, countof(segments_properties)) < 0) {
        JS_FreeValue(ctx, prototype);
        goto fail;
    }
    JS_SetClassProto(ctx, JS_CLASS_INTL_SEGMENTS, prototype);
    prototype = JS_NewObjectProto(ctx, ctx->class_proto[JS_CLASS_ITERATOR]);
    if (JS_IsException(prototype))
        goto fail;
    if (JS_SetPropertyFunctionList(ctx, prototype, iterator_properties, countof(iterator_properties)) < 0) {
        JS_FreeValue(ctx, prototype);
        goto fail;
    }
    JS_SetClassProto(ctx, JS_CLASS_INTL_SEGMENT_ITERATOR, prototype);
    return JS_DefinePropertyValueStr(ctx, namespace_object, "Segmenter", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
fail:
    JS_FreeValue(ctx, constructor);
    return -1;
}
