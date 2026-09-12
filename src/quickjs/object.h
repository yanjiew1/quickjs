/*
 * QuickJS Object and Property definitions
 */
#ifndef QUICKJS_OBJECT_H
#define QUICKJS_OBJECT_H

#include "quickjs/def.h"
#include "quickjs/shape.h"
#include "quickjs/string.h"
#include "quickjs/function.h"

typedef struct JSProperty {
    union {
        JSValue value;      /* JS_PROP_NORMAL */
        struct {            /* JS_PROP_GETSET */
            JSObject *getter; /* NULL if undefined */
            JSObject *setter; /* NULL if undefined */
        } getset;
        JSVarRef *var_ref;  /* JS_PROP_VARREF */
        struct {            /* JS_PROP_AUTOINIT */
            uintptr_t realm_and_id;
            void *opaque;
        } init;
    } u;
} JSProperty;

#define JS_PROP_INITIAL_SIZE 2
#define JS_PROP_INITIAL_HASH_SIZE 4 /* must be a power of two */

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

struct JSForInIterator {
    JSValue obj;
    uint32_t idx;
    uint32_t atom_count;
    uint8_t in_prototype_chain;
    uint8_t is_array;
    JSPropertyEnum *tab_atom; /* is_array = FALSE */
};

struct JSRegExp {
    JSString *pattern;
    JSString *bytecode; /* also contains the flags */
};

struct JSProxyData {
    JSValue target;
    JSValue handler;
    uint8_t is_func;
    uint8_t is_revoked;
};

struct JSArrayBuffer {
    int byte_length; /* 0 if detached */
    int max_byte_length; /* -1 if not resizable; >= byte_length otherwise */
    uint8_t detached;
    uint8_t shared; /* if shared, the array buffer cannot be detached */
    uint8_t *data; /* NULL if detached */
    struct list_head array_list;
    void *opaque;
    JSFreeArrayBufferDataFunc *free_func;
};

struct JSTypedArray {
    struct list_head link; /* link to arraybuffer */
    JSObject *obj; /* back pointer to the TypedArray/DataView object */
    JSObject *buffer; /* based array buffer */
    uint32_t offset; /* byte offset in the array buffer */
    uint32_t length; /* byte length in the array buffer */
    BOOL track_rab; /* auto-track length of backing array buffer */
};

struct JSGlobalObject {
    JSValue uninitialized_vars;
};

struct JSMapRecord {
    int ref_count;
    BOOL empty : 8;
    struct list_head link;
    struct JSMapRecord *hash_next;
    JSValue key;
    JSValue value;
};

struct JSMapState {
    BOOL is_weak;
    struct list_head records;
    uint32_t record_count;
    JSMapRecord **hash_table;
    int hash_bits;
    uint32_t hash_size;
    uint32_t record_count_threshold;
    JSWeakRefHeader weakref_header;
};

struct JSObject {
    JSGCObjectHeader header;
    uint8_t is_std_array_prototype : 1;
    uint8_t extensible : 1;
    uint8_t free_mark : 1;
    uint8_t is_exotic : 1;
    uint8_t fast_array : 1;
    uint8_t is_constructor : 1;
    uint8_t has_immutable_prototype : 1;
    uint8_t tmp_mark : 1;
    uint8_t is_HTMLDDA : 1;
    uint16_t class_id;
    uint32_t weakref_count; 
    JSShape *shape;
    JSProperty *prop;
    union {
        void *opaque;
        struct JSBoundFunction *bound_function;
        struct JSCFunctionDataRecord *c_function_data_record;
        struct JSForInIterator *for_in_iterator;
        struct JSArrayBuffer *array_buffer;
        struct JSTypedArray *typed_array;
        struct JSMapState *map_state;
        struct JSMapIteratorData *map_iterator_data;
        struct JSArrayIteratorData *array_iterator_data;
        struct JSRegExpStringIteratorData *regexp_string_iterator_data;
        struct JSGeneratorData *generator_data;
        struct JSIteratorConcatData *iterator_concat_data;
        struct JSIteratorHelperData *iterator_helper_data;
        struct JSIteratorWrapData *iterator_wrap_data;
        struct JSProxyData *proxy_data;
        struct JSPromiseData *promise_data;
        struct JSPromiseFunctionData *promise_function_data;
        struct JSAsyncFunctionState *async_function_data;
        struct JSAsyncFromSyncIteratorData *async_from_sync_iterator_data;
        struct JSAsyncGeneratorData *async_generator_data;
        struct {
            struct JSFunctionBytecode *function_bytecode;
            JSVarRef **var_refs;
            JSObject *home_object;
        } func;
        struct {
            JSContext *realm;
            JSCFunctionType c_function;
            uint8_t length;
            uint8_t cproto;
            int16_t magic;
        } cfunc;
        struct {
            union {
                uint32_t size;
                struct JSTypedArray *typed_array;
            } u1;
            union {
                JSValue *values;
                JSVarRef **var_refs;
                void *ptr;
                int8_t *int8_ptr;
                uint8_t *uint8_ptr;
                int16_t *int16_ptr;
                uint16_t *uint16_ptr;
                int32_t *int32_ptr;
                uint32_t *uint32_ptr;
                int64_t *int64_ptr;
                uint64_t *uint64_ptr;
                uint16_t *fp16_ptr;
                float *float_ptr;
                double *double_ptr;
            } u;
            uint32_t count;
        } array;
        struct JSRegExp regexp;
        JSValue object_data;
        struct JSGlobalObject global_object;
    } u;
};

#define JS_VALUE_GET_OBJ(v) ((JSObject *)JS_VALUE_GET_PTR(v))

static force_inline JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            return pr;
        }
        h = pr->hash_next;
    }
    return NULL;
}

static force_inline JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            *ppr = &p->prop[h - 1];
            return pr;
        }
        h = pr->hash_next;
    }
    *ppr = NULL;
    return NULL;
}

static inline BOOL JS_IsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return p->is_HTMLDDA;
}

static inline void set_value(JSContext *ctx, JSValue *pval, JSValue new_val)
{
    JSValue old_val;
    old_val = *pval;
    *pval = new_val;
    JS_FreeValue(ctx, old_val);
}

int js_get_length32(JSContext *ctx, uint32_t *pres, JSValueConst obj);
JSValue js_array_buffer_constructor3(JSContext *ctx, JSValueConst new_target,
                                     uint64_t len, uint64_t *max_len,
                                     JSClassID class_id, uint8_t *buf,
                                     JSFreeArrayBufferDataFunc *free_func,
                                     void *opaque, BOOL alloc_flag);
JSValue js_typed_array_constructor(JSContext *ctx,
                                   JSValueConst new_target, int argc,
                                   JSValueConst *argv, int classid);
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);
JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
int js_update_property_flags(JSContext *ctx, JSObject *p, JSShapeProperty **pprs, int flags);
JSProperty *add_property(JSContext *ctx, JSObject *p, JSAtom prop, int prop_flags);

int JS_CopyDataProperties(JSContext *ctx, JSValueConst target, JSValueConst source, JSValueConst excluded, BOOL setprop);
int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj, int64_t idx, JSValue val, int flags);
int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj, JSAtom prop, JSAutoInitIDEnum id, void *opaque, int flags);
int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc, JSObject *p, JSAtom prop);
int JS_GetOwnPropertyNamesInternal(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen, JSObject *p, int flags);
JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);
int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);
JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj, JSValue prop);
JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);
int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val, JSValueConst obj);
void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj);
int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj, JSValue prop, JSValue val, int flags);
int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj, JSValueConst proto_val, BOOL throw_flag);
int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);
int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj, int64_t idx, JSValue val, int flags);
int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj, JSValue prop, JSValue val, int flags);
BOOL check_define_prop_flags(int prop_flags, int flags);
JSObject *get_proto_obj(JSValueConst proto_val);
void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d, JSValueConst desc);
JSValue JS_GetOwnPropertyNames2(JSContext *ctx, JSValueConst obj1, int flags, int kind);
__exception int js_get_length32(JSContext *ctx, uint32_t *pres, JSValueConst obj);
__exception int js_get_length64(JSContext *ctx, int64_t *pres, JSValueConst obj);
int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func);


/* Iterators */
JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);
JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj, JSValueConst method);
JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj, JSValueConst method, int argc, JSValueConst *argv, BOOL *pdone);
JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj, JSValueConst method, int argc, JSValueConst *argv, int *pdone);
JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj, BOOL *pdone);
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj, BOOL is_exception_pending);
JSValue js_create_iterator_result(JSContext *ctx, JSValue val, BOOL done);

#endif /* QUICKJS_OBJECT_H */
