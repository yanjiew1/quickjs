#ifndef QUICKJS_ARRAY_H
#define QUICKJS_ARRAY_H

#include "quickjs/def.h"
#include "quickjs/object.h"

typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;

static force_inline BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                                              JSValue **arrpp, uint32_t *countp)
{
    /* Try and handle fast arrays explicitly */
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id == JS_CLASS_ARRAY && p->fast_array) {
            *countp = p->u.array.count;
            *arrpp = p->u.array.u.values;
            return TRUE;
        }
    }
    return FALSE;
}

int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);
static force_inline BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}
JSValue js_allocate_fast_array(JSContext *ctx, int64_t len);
JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);
JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);
int convert_fast_array_to_array(JSContext *ctx, JSObject *p);
int set_array_length(JSContext *ctx, JSObject *p, JSValue val, int flags);
int add_fast_array_element(JSContext *ctx, JSObject *p, JSValue val, int flags);
void js_array_finalizer(JSRuntime *rt, JSValue val);
void js_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
BOOL typed_array_is_oob(JSObject *p);
int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
JSValue js_typed_array_constructor_ta(JSContext *ctx, JSValueConst new_target,
                                      JSValueConst src_obj, int classid, uint32_t len);
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

#define special_every    0
#define special_some     1
#define special_forEach  2
#define special_map      3
#define special_filter   4
#define special_TA       8

#define special_reduce       0
#define special_reduceRight  1

enum {
    ArrayFind,
    ArrayFindIndex,
    ArrayFindLast,
    ArrayFindLastIndex,
};

JSValue js_array_every(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int special);
JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int special);

void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_array_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv,
                               BOOL *pdone, int magic);

#endif /* QUICKJS_ARRAY_H */
