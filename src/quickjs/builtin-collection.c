/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include "internal-allocator.h"
#include "internal-collection.h"
#include "internal-number.h"
#include "internal-iterator.h"
#include "internal-array.h"

#define js_malloc_rt qjs_malloc_rt_internal
#define js_free_rt qjs_free_rt_internal
#define js_realloc_rt qjs_realloc_rt_internal
#define js_malloc qjs_malloc_internal
#define js_free qjs_free_internal
#define js_realloc qjs_realloc_internal

/* Set/Map/WeakSet/WeakMap */

static BOOL js_weakref_is_target(JSValueConst val)
{
    switch (JS_VALUE_GET_TAG(val)) {
    case JS_TAG_OBJECT:
        return TRUE;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            if (p->atom_type == JS_ATOM_TYPE_SYMBOL &&
                p->hash != JS_ATOM_HASH_PRIVATE)
                return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

/* JS_UNDEFINED is considered as a live weakref */
/* XXX: add a specific JSWeakRef value type ? */
static BOOL js_weakref_is_live(JSValueConst val)
{
    void *p;
    if (JS_IsUndefined(val))
        return TRUE;
    p = JS_VALUE_GET_PTR(val);
    return (qjs_get_ref_header(p)->ref_count != 0);
}

/* 'val' can be JS_UNDEFINED */
static void js_weakref_free(JSRuntime *rt, JSValue val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        assert(p->weakref_count >= 1);
        p->weakref_count--;
        /* 'mark' is tested to avoid freeing the object structure when
           it is about to be freed in a cycle or in
           qjs_free_zero_refcount() */
        if (p->weakref_count == 0 && qjs_get_ref_header(p)->ref_count == 0 &&
            qjs_get_ref_header(p)->mark == 0) {
            js_free_rt(rt, p);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL) {
        JSString *p = JS_VALUE_GET_STRING(val);
        assert(p->hash >= 1);
        p->hash--;
        if (p->hash == 0 && qjs_get_ref_header(p)->ref_count == 0) {
            /* can remove the dummy structure */
            js_free_rt(rt, p);
        }
    }
}

/* val must be an object, a symbol or undefined (see
   js_weakref_is_target). */
static JSValue js_weakref_new(JSContext *ctx, JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        p->weakref_count++;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL) {
        JSString *p = JS_VALUE_GET_STRING(val);
        /* XXX: could return an exception if too many references */
        assert(p->hash < JS_ATOM_HASH_MASK - 2);
        p->hash++;
    } else {
        assert(JS_IsUndefined(val));
    }
    return (JSValue)val;
}

#define MAGIC_SET (1 << 0)
#define MAGIC_WEAK (1 << 1)

static JSValue js_map_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv, int magic)
{
    JSMapState *s;
    JSValue obj, adder = JS_UNDEFINED, iter = JS_UNDEFINED, next_method = JS_UNDEFINED;
    JSValueConst arr;
    BOOL is_set, is_weak;

    is_set = magic & MAGIC_SET;
    is_weak = ((magic & MAGIC_WEAK) != 0);
    obj = qjs_collection_create_from_ctor(ctx, new_target, JS_CLASS_MAP + magic);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    init_list_head(&s->records);
    s->is_weak = is_weak;
    if (is_weak) {
        s->weakref_header.weakref_type = JS_WEAKREF_TYPE_MAP;
        list_add_tail(&s->weakref_header.link, &ctx->rt->weakref_list);
    }
    JS_SetOpaque(obj, s);
    s->hash_bits = 1;
    s->hash_size = 1U << s->hash_bits;
    s->hash_table = js_mallocz(ctx, sizeof(s->hash_table[0]) * s->hash_size);
    if (!s->hash_table)
        goto fail;
    s->record_count_threshold = 4;

    arr = JS_UNDEFINED;
    if (argc > 0)
        arr = argv[0];
    if (!JS_IsUndefined(arr) && !JS_IsNull(arr)) {
        JSValue item, ret;
        BOOL done;

        adder = JS_GetProperty(ctx, obj, is_set ? JS_ATOM_add : JS_ATOM_set);
        if (JS_IsException(adder))
            goto fail;
        if (!JS_IsFunction(ctx, adder)) {
            JS_ThrowTypeError(ctx, "set/add is not a function");
            goto fail;
        }

        iter = qjs_get_iterator(ctx, arr, FALSE);
        if (JS_IsException(iter))
            goto fail;
        next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto fail;

        for(;;) {
            item = qjs_iterator_next(ctx, iter, next_method, 0, NULL, &done);
            if (JS_IsException(item))
                goto fail;
            if (done)
                break;
            if (is_set) {
                ret = JS_Call(ctx, adder, obj, 1, (JSValueConst *)&item);
                if (JS_IsException(ret)) {
                    JS_FreeValue(ctx, item);
                    goto fail_close;
                }
            } else {
                JSValue key, value;
                JSValueConst args[2];
                key = JS_UNDEFINED;
                value = JS_UNDEFINED;
                if (!JS_IsObject(item)) {
                    qjs_collection_throw_not_object(ctx);
                    goto fail1;
                }
                key = JS_GetPropertyUint32(ctx, item, 0);
                if (JS_IsException(key))
                    goto fail1;
                value = JS_GetPropertyUint32(ctx, item, 1);
                if (JS_IsException(value))
                    goto fail1;
                args[0] = key;
                args[1] = value;
                ret = JS_Call(ctx, adder, obj, 2, args);
                if (JS_IsException(ret)) {
                fail1:
                    JS_FreeValue(ctx, item);
                    JS_FreeValue(ctx, key);
                    JS_FreeValue(ctx, value);
                    goto fail_close;
                }
                JS_FreeValue(ctx, key);
                JS_FreeValue(ctx, value);
            }
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, item);
        }
        JS_FreeValue(ctx, next_method);
        JS_FreeValue(ctx, iter);
        JS_FreeValue(ctx, adder);
    }
    return obj;
 fail_close:
    /* close the iterator object, preserving pending exception */
    qjs_iterator_close(ctx, iter, TRUE);
 fail:
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, adder);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

/* XXX: could normalize strings to speed up comparison */
static JSValue map_normalize_key(JSContext *ctx, JSValue key)
{
    uint32_t tag = JS_VALUE_GET_TAG(key);
    /* convert -0.0 to +0.0 */
    if (JS_TAG_IS_FLOAT64(tag) && JS_VALUE_GET_FLOAT64(key) == 0.0) {
        key = JS_NewInt32(ctx, 0);
    }
    return key;
}

static JSValueConst map_normalize_key_const(JSContext *ctx, JSValueConst key)
{
    return (JSValueConst)map_normalize_key(ctx, (JSValue)key);
}

/* hash multipliers, same as the Linux kernel (see Knuth vol 3,
   section 6.4, exercise 9) */
#define HASH_MUL32 0x61C88647
#define HASH_MUL64 UINT64_C(0x61C8864680B583EB)

static uint32_t map_hash32(uint32_t a, int hash_bits)
{
    return (a * HASH_MUL32) >> (32 - hash_bits);
}

static uint32_t map_hash64(uint64_t a, int hash_bits)
{
    return (a * HASH_MUL64) >> (64 - hash_bits);
}

static uint32_t map_hash_pointer(uintptr_t a, int hash_bits)
{
#ifdef JS_PTR64
    return map_hash64(a, hash_bits);
#else
    return map_hash32(a, hash_bits);
#endif
}

/* XXX: better hash ? */
/* precondition: 1 <= hash_bits <= 32 */
static uint32_t map_hash_key(JSValueConst key, int hash_bits)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(key);
    uint32_t h;
    double d;
    JSBigInt *p;
    JSBigIntBuf buf;

    switch(tag) {
    case JS_TAG_BOOL:
        h = map_hash32(JS_VALUE_GET_INT(key) ^ JS_TAG_BOOL, hash_bits);
        break;
    case JS_TAG_STRING:
        h = map_hash32(qjs_hash_string(JS_VALUE_GET_STRING(key), 0) ^ JS_TAG_STRING, hash_bits);
        break;
    case JS_TAG_STRING_ROPE:
        h = map_hash32(hash_string_rope(key, 0) ^ JS_TAG_STRING, hash_bits);
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_SYMBOL:
        h = map_hash_pointer((uintptr_t)JS_VALUE_GET_PTR(key) ^ tag, hash_bits);
        break;
    case JS_TAG_INT:
        d = JS_VALUE_GET_INT(key);
        goto hash_float64;
    case JS_TAG_FLOAT64:
        d = JS_VALUE_GET_FLOAT64(key);
        /* normalize the NaN */
        if (isnan(d))
            d = JS_FLOAT64_NAN;
    hash_float64:
        h = map_hash64(float64_as_uint64(d) ^ JS_TAG_FLOAT64, hash_bits);
        break;
    case JS_TAG_SHORT_BIG_INT:
        p = qjs_bigint_set_short(&buf, key);
        goto hash_bigint;
    case JS_TAG_BIG_INT:
        p = JS_VALUE_GET_PTR(key);
    hash_bigint:
        {
            int i;
            h = 1;
            for(i = p->len - 1; i >= 0; i--) {
                h = h * 263 + p->tab[i];
            }
            /* the final step is necessary otherwise h mod n only
               depends of p->tab[i] mod n */
            h = map_hash32(h ^ JS_TAG_BIG_INT, hash_bits);
        }
        break;
    default:
        h = 0;
        break;
    }
    return h;
}

static JSMapRecord *map_find_record(JSContext *ctx, JSMapState *s,
                                    JSValueConst key)
{
    JSMapRecord *mr;
    uint32_t h;
    h = map_hash_key(key, s->hash_bits);
    for(mr = s->hash_table[h]; mr != NULL; mr = mr->hash_next) {
        if (mr->empty || (s->is_weak && !js_weakref_is_live(mr->key))) {
            /* cannot match */
        } else {
            if (qjs_same_value_zero(ctx, mr->key, key))
                return mr;
        }
    }
    return NULL;
}

static void map_hash_resize(JSContext *ctx, JSMapState *s)
{
    uint32_t new_hash_size, h;
    int new_hash_bits;
    struct list_head *el;
    JSMapRecord *mr, **new_hash_table;

    /* XXX: no reporting of memory allocation failure */
    new_hash_bits = min_int(s->hash_bits + 1, 31);
    new_hash_size = 1U << new_hash_bits;
    new_hash_table = js_realloc(ctx, s->hash_table,
                                sizeof(new_hash_table[0]) * new_hash_size);
    if (!new_hash_table)
        return;

    memset(new_hash_table, 0, sizeof(new_hash_table[0]) * new_hash_size);

    list_for_each(el, &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        if (mr->empty || (s->is_weak && !js_weakref_is_live(mr->key))) {
        } else {
            h = map_hash_key(mr->key, new_hash_bits);
            mr->hash_next = new_hash_table[h];
            new_hash_table[h] = mr;
        }
    }
    s->hash_table = new_hash_table;
    s->hash_bits = new_hash_bits;
    s->hash_size = new_hash_size;
    s->record_count_threshold = new_hash_size * 2;
}

static JSMapRecord *map_add_record(JSContext *ctx, JSMapState *s,
                                   JSValueConst key)
{
    uint32_t h;
    JSMapRecord *mr;

    mr = js_malloc(ctx, sizeof(*mr));
    if (!mr)
        return NULL;
    mr->ref_count = 1;
    mr->empty = FALSE;
    if (s->is_weak) {
        mr->key = js_weakref_new(ctx, key);
    } else {
        mr->key = JS_DupValue(ctx, key);
    }
    h = map_hash_key(key, s->hash_bits);
    mr->hash_next = s->hash_table[h];
    s->hash_table[h] = mr;
    list_add_tail(&mr->link, &s->records);
    s->record_count++;
    if (s->record_count >= s->record_count_threshold) {
        map_hash_resize(ctx, s);
    }
    return mr;
}

static JSMapRecord *set_add_record(JSContext *ctx, JSMapState *s,
                                   JSValueConst key)
{
    JSMapRecord *mr;
    mr = map_add_record(ctx, s, key);
    if (!mr)
        return NULL;
    mr->value = JS_UNDEFINED;
    return mr;
}

/* warning: the record must be removed from the hash table before */
static void map_delete_record_internal(JSRuntime *rt, JSMapState *s, JSMapRecord *mr)
{
    if (mr->empty)
        return;

    if (s->is_weak) {
        js_weakref_free(rt, mr->key);
    } else {
        JS_FreeValueRT(rt, mr->key);
    }
    JS_FreeValueRT(rt, mr->value);
    if (--mr->ref_count == 0) {
        list_del(&mr->link);
        js_free_rt(rt, mr);
    } else {
        /* keep a zombie record for iterators */
        mr->empty = TRUE;
        mr->key = JS_UNDEFINED;
        mr->value = JS_UNDEFINED;
    }
    s->record_count--;
}

static void map_decref_record(JSRuntime *rt, JSMapRecord *mr)
{
    if (--mr->ref_count == 0) {
        /* the record can be safely removed */
        assert(mr->empty);
        list_del(&mr->link);
        js_free_rt(rt, mr);
    }
}

static void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSMapState *s = container_of(wh, JSMapState, weakref_header);
    struct list_head *el, *el1;
    JSMapRecord *mr1, **pmr;
    uint32_t h;

    list_for_each_safe(el, el1, &s->records) {
        JSMapRecord *mr = list_entry(el, JSMapRecord, link);
        if (!js_weakref_is_live(mr->key)) {

            /* even if key is not live it can be hashed as a pointer */
            h = map_hash_key(mr->key, s->hash_bits);
            pmr = &s->hash_table[h];
            for(;;) {
                mr1 = *pmr;
                /* the entry may already be removed from the hash
                   table if the map was resized */
                if (mr1 == NULL)
                    goto done;
                if (mr1 == mr)
                    break;
                pmr = &mr1->hash_next;
            }
            /* remove from the hash table */
            *pmr = mr1->hash_next;
        done:
            map_delete_record_internal(rt, s, mr);
        }
    }
}

static JSValue js_map_set(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSMapRecord *mr;
    JSValueConst key, value;

    if (!s)
        return JS_EXCEPTION;
    key = map_normalize_key_const(ctx, argv[0]);
    if (s->is_weak && !js_weakref_is_target(key))
        return JS_ThrowTypeError(ctx, "invalid value used as %s key", (magic & MAGIC_SET) ? "WeakSet" : "WeakMap");
    if (magic & MAGIC_SET)
        value = JS_UNDEFINED;
    else
        value = argv[1];
    mr = map_find_record(ctx, s, key);
    if (mr) {
        JS_FreeValue(ctx, mr->value);
    } else {
        mr = map_add_record(ctx, s, key);
        if (!mr)
            return JS_EXCEPTION;
    }
    mr->value = JS_DupValue(ctx, value);
    return JS_DupValue(ctx, this_val);
}

static JSValue js_map_get(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSMapRecord *mr;
    JSValueConst key;

    if (!s)
        return JS_EXCEPTION;
    key = map_normalize_key_const(ctx, argv[0]);
    mr = map_find_record(ctx, s, key);
    if (!mr)
        return JS_UNDEFINED;
    else
        return JS_DupValue(ctx, mr->value);
}

/* return JS_TRUE or JS_FALSE */
static JSValue map_delete_record(JSContext *ctx, JSMapState *s, JSValueConst key)
{
    JSMapRecord *mr, **pmr;
    uint32_t h;

    key = map_normalize_key_const(ctx, key);

    h = map_hash_key(key, s->hash_bits);
    pmr = &s->hash_table[h];
    for(;;) {
        mr = *pmr;
        if (mr == NULL)
            return JS_FALSE;
        if (mr->empty || (s->is_weak && !js_weakref_is_live(mr->key))) {
            /* not valid */
        } else {
            if (qjs_same_value_zero(ctx, mr->key, key))
                break;
        }
        pmr = &mr->hash_next;
    }

    /* remove from the hash table */
    *pmr = mr->hash_next;

    map_delete_record_internal(ctx->rt, s, mr);
    return JS_TRUE;
}

static JSValue js_map_getOrInsert(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    BOOL computed = magic & 1;
    JSClassID class_id = magic >> 1;
    JSMapState *s = JS_GetOpaque2(ctx, this_val, class_id);
    JSMapRecord *mr;
    JSValueConst key;
    JSValue value;

    if (!s)
        return JS_EXCEPTION;
    if (computed && !JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "not a function");
    key = map_normalize_key_const(ctx, argv[0]);
    if (s->is_weak && !js_weakref_is_target(key))
        return JS_ThrowTypeError(ctx, "invalid value used as WeakMap key");
    mr = map_find_record(ctx, s, key);
    if (!mr) {
        if (computed) {
            value = JS_Call(ctx, argv[1], JS_UNDEFINED, 1, &key);
            if (JS_IsException(value))
                return JS_EXCEPTION;
            map_delete_record(ctx, s, key);
        } else {
            value = JS_DupValue(ctx, argv[1]);
        }
        mr = map_add_record(ctx, s, key);
        if (!mr) {
            JS_FreeValue(ctx, value);
            return JS_EXCEPTION;
        }
        mr->value = value;
    }
    return JS_DupValue(ctx, mr->value);
}

static JSValue js_map_has(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSMapRecord *mr;
    JSValueConst key;

    if (!s)
        return JS_EXCEPTION;
    key = map_normalize_key_const(ctx, argv[0]);
    mr = map_find_record(ctx, s, key);
    return JS_NewBool(ctx, mr != NULL);
}

static JSValue js_map_delete(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    if (!s)
        return JS_EXCEPTION;
    return map_delete_record(ctx, s, argv[0]);
}

static JSValue js_map_clear(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    struct list_head *el, *el1;
    JSMapRecord *mr;

    if (!s)
        return JS_EXCEPTION;

    /* remove from the hash table */
    memset(s->hash_table, 0, sizeof(s->hash_table[0]) * s->hash_size);

    list_for_each_safe(el, el1, &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        map_delete_record_internal(ctx->rt, s, mr);
    }
    return JS_UNDEFINED;
}

static JSValue js_map_get_size(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    if (!s)
        return JS_EXCEPTION;
    return JS_NewUint32(ctx, s->record_count);
}

static JSValue js_map_forEach(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    JSMapState *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    JSValueConst func, this_arg;
    JSValue ret, args[3];
    struct list_head *el;
    JSMapRecord *mr;

    if (!s)
        return JS_EXCEPTION;
    func = argv[0];
    if (argc > 1)
        this_arg = argv[1];
    else
        this_arg = JS_UNDEFINED;
    if (qjs_check_function(ctx, func))
        return JS_EXCEPTION;
    /* Note: the list can be modified while traversing it, but the
       current element is locked */
    el = s->records.next;
    while (el != &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        if (!mr->empty) {
            mr->ref_count++;
            /* must duplicate in case the record is deleted */
            args[1] = JS_DupValue(ctx, mr->key);
            if (magic)
                args[0] = args[1];
            else
                args[0] = JS_DupValue(ctx, mr->value);
            args[2] = (JSValue)this_val;
            ret = JS_Call(ctx, func, this_arg, 3, (JSValueConst *)args);
            JS_FreeValue(ctx, args[0]);
            if (!magic)
                JS_FreeValue(ctx, args[1]);
            el = el->next;
            map_decref_record(ctx->rt, mr);
            if (JS_IsException(ret))
                return ret;
            JS_FreeValue(ctx, ret);
        } else {
            el = el->next;
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int is_map)
{
    JSValueConst cb, args[2];
    JSValue res, iter, next, groups, key, v, prop;
    JSAtom key_atom = JS_ATOM_NULL;
    int64_t idx;
    BOOL done;

    // "is function?" check must be observed before argv[0] is accessed
    cb = argv[1];
    if (qjs_check_function(ctx, cb))
        return JS_EXCEPTION;

    iter = qjs_get_iterator(ctx, argv[0], /*is_async*/FALSE);
    if (JS_IsException(iter))
        return JS_EXCEPTION;

    key = JS_UNDEFINED;
    key_atom = JS_ATOM_NULL;
    v = JS_UNDEFINED;
    prop = JS_UNDEFINED;
    groups = JS_UNDEFINED;

    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;

    if (is_map) {
        groups = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, 0);
    } else {
        groups = JS_NewObjectProto(ctx, JS_NULL);
    }
    if (JS_IsException(groups))
        goto exception;

    for (idx = 0; ; idx++) {
        if (idx >= QJS_MAX_SAFE_INTEGER) {
            JS_ThrowTypeError(ctx, "too many elements");
            goto iterator_close_exception;
        }
        v = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(v))
            goto exception;
        if (done)
            break; // v is JS_UNDEFINED

        args[0] = v;
        args[1] = JS_NewInt64(ctx, idx);
        key = JS_Call(ctx, cb, ctx->global_obj, 2, args);
        if (JS_IsException(key))
            goto iterator_close_exception;

        if (is_map) {
            prop = js_map_get(ctx, groups, 1, (JSValueConst *)&key, 0);
        } else {
            key_atom = JS_ValueToAtom(ctx, key);
            JS_FreeValue(ctx, key);
            key = JS_UNDEFINED;
            if (key_atom == JS_ATOM_NULL)
                goto iterator_close_exception;
            prop = JS_GetProperty(ctx, groups, key_atom);
        }
        if (JS_IsException(prop))
            goto exception;

        if (JS_IsUndefined(prop)) {
            prop = JS_NewArray(ctx);
            if (JS_IsException(prop))
                goto exception;
            if (is_map) {
                args[0] = key;
                args[1] = prop;
                res = js_map_set(ctx, groups, 2, args, 0);
                if (JS_IsException(res))
                    goto exception;
                JS_FreeValue(ctx, res);
            } else {
                prop = JS_DupValue(ctx, prop);
                if (JS_DefinePropertyValue(ctx, groups, key_atom, prop,
                                           JS_PROP_C_W_E) < 0) {
                    goto exception;
                }
            }
        }
        res = qjs_array_push(ctx, prop, 1, (JSValueConst *)&v, /*unshift*/0);
        if (JS_IsException(res))
            goto exception;
        // res is an int64

        JS_FreeValue(ctx, prop);
        JS_FreeValue(ctx, key);
        JS_FreeAtom(ctx, key_atom);
        JS_FreeValue(ctx, v);
        prop = JS_UNDEFINED;
        key = JS_UNDEFINED;
        key_atom = JS_ATOM_NULL;
        v = JS_UNDEFINED;
    }

    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return groups;

 iterator_close_exception:
    qjs_iterator_close(ctx, iter, TRUE);
 exception:
    JS_FreeAtom(ctx, key_atom);
    JS_FreeValue(ctx, prop);
    JS_FreeValue(ctx, key);
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, groups);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return JS_EXCEPTION;
}

QJS_INTERNAL void js_map_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p;
    JSMapState *s;
    struct list_head *el, *el1;
    JSMapRecord *mr;

    p = JS_VALUE_GET_OBJ(val);
    s = p->u.map_state;
    if (s) {
        /* if the object is deleted we are sure that no iterator is
           using it */
        list_for_each_safe(el, el1, &s->records) {
            mr = list_entry(el, JSMapRecord, link);
            if (!mr->empty) {
                if (s->is_weak)
                    js_weakref_free(rt, mr->key);
                else
                    JS_FreeValueRT(rt, mr->key);
                JS_FreeValueRT(rt, mr->value);
            }
            js_free_rt(rt, mr);
        }
        js_free_rt(rt, s->hash_table);
        if (s->is_weak) {
            list_del(&s->weakref_header.link);
        }
        js_free_rt(rt, s);
    }
}

QJS_INTERNAL void js_map_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSMapState *s;
    struct list_head *el;
    JSMapRecord *mr;

    s = p->u.map_state;
    if (s) {
        list_for_each(el, &s->records) {
            mr = list_entry(el, JSMapRecord, link);
            if (!s->is_weak)
                JS_MarkValue(rt, mr->key, mark_func);
            JS_MarkValue(rt, mr->value, mark_func);
        }
    }
}

/* Map Iterator */

typedef struct JSMapIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    JSMapRecord *cur_record;
} JSMapIteratorData;

QJS_INTERNAL void js_map_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p;
    JSMapIteratorData *it;

    p = JS_VALUE_GET_OBJ(val);
    it = p->u.map_iterator_data;
    if (it) {
        /* During the GC sweep phase the Map finalizer may be
           called before the Map iterator finalizer */
        if (JS_IsLiveObject(rt, it->obj) && it->cur_record) {
            map_decref_record(rt, it->cur_record);
        }
        JS_FreeValueRT(rt, it->obj);
        js_free_rt(rt, it);
    }
}

QJS_INTERNAL void js_map_iterator_mark(JSRuntime *rt, JSValueConst val,
                                        JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSMapIteratorData *it;
    it = p->u.map_iterator_data;
    if (it) {
        /* the record is already marked by the object */
        JS_MarkValue(rt, it->obj, mark_func);
    }
}

static JSValue js_create_map_iterator(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic)
{
    JSIteratorKindEnum kind;
    JSMapState *s;
    JSMapIteratorData *it;
    JSValue enum_obj;

    kind = magic >> 2;
    magic &= 3;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP + magic);
    if (!s)
        return JS_EXCEPTION;
    enum_obj = JS_NewObjectClass(ctx, JS_CLASS_MAP_ITERATOR + magic);
    if (JS_IsException(enum_obj))
        goto fail;
    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, enum_obj);
        goto fail;
    }
    it->obj = JS_DupValue(ctx, this_val);
    it->kind = kind;
    it->cur_record = NULL;
    JS_SetOpaque(enum_obj, it);
    return enum_obj;
 fail:
    return JS_EXCEPTION;
}

static JSValue js_map_iterator_next(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv,
                                    BOOL *pdone, int magic)
{
    JSMapIteratorData *it;
    JSMapState *s;
    JSMapRecord *mr;
    struct list_head *el;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_MAP_ITERATOR + magic);
    if (!it) {
        *pdone = FALSE;
        return JS_EXCEPTION;
    }
    if (JS_IsUndefined(it->obj))
        goto done;
    s = JS_GetOpaque(it->obj, JS_CLASS_MAP + magic);
    assert(s != NULL);
    if (!it->cur_record) {
        el = s->records.next;
    } else {
        mr = it->cur_record;
        el = mr->link.next;
        map_decref_record(ctx->rt, mr); /* the record can be freed here */
    }
    for(;;) {
        if (el == &s->records) {
            /* no more record  */
            it->cur_record = NULL;
            JS_FreeValue(ctx, it->obj);
            it->obj = JS_UNDEFINED;
        done:
            /* end of enumeration */
            *pdone = TRUE;
            return JS_UNDEFINED;
        }
        mr = list_entry(el, JSMapRecord, link);
        if (!mr->empty)
            break;
        /* get the next record */
        el = mr->link.next;
    }

    /* lock the record so that it won't be freed */
    mr->ref_count++;
    it->cur_record = mr;
    *pdone = FALSE;

    if (it->kind == JS_ITERATOR_KIND_KEY) {
        return JS_DupValue(ctx, mr->key);
    } else {
        JSValueConst args[2];
        args[0] = mr->key;
        if (magic)
            args[1] = mr->key;
        else
            args[1] = mr->value;
        if (it->kind == JS_ITERATOR_KIND_VALUE) {
            return JS_DupValue(ctx, args[1]);
        } else {
            return qjs_collection_create_array(ctx, 2, args);
        }
    }
}

static int get_set_record(JSContext *ctx, JSValueConst obj,
                          int64_t *psize, JSValue *phas, JSValue *pkeys)
{
    JSMapState *s;
    int64_t size;
    JSValue has = JS_UNDEFINED, keys = JS_UNDEFINED;

    s = JS_GetOpaque(obj, JS_CLASS_SET);
    if (s) {
        size = s->record_count;
    } else {
        JSValue v;
        double d;

        v = JS_GetProperty(ctx, obj, JS_ATOM_size);
        if (JS_IsException(v))
            goto exception;
        if (qjs_to_float64_free(ctx, &d, v) < 0)
            goto exception;
        if (isnan(d)) {
            JS_ThrowTypeError(ctx, ".size is not a number");
            goto exception;
        }
        if (d < INT64_MIN)
            size = INT64_MIN;
        else if (d >= 0x1p63) /* must use INT64_MAX + 1 because INT64_MAX cannot be exactly represented as a double */
            size = INT64_MAX;
        else
            size = (int64_t)d;
        if (size < 0) {
            JS_ThrowRangeError(ctx, ".size must be positive");
            goto exception;
        }
    }

    has = JS_GetProperty(ctx, obj, JS_ATOM_has);
    if (JS_IsException(has))
        goto exception;
    if (JS_IsUndefined(has)) {
        JS_ThrowTypeError(ctx, ".has is undefined");
        goto exception;
    }
    if (!JS_IsFunction(ctx, has)) {
        JS_ThrowTypeError(ctx, ".has is not a function");
        goto exception;
    }

    keys = JS_GetProperty(ctx, obj, JS_ATOM_keys);
    if (JS_IsException(keys))
        goto exception;
    if (JS_IsUndefined(keys)) {
        JS_ThrowTypeError(ctx, ".keys is undefined");
        goto exception;
    }
    if (!JS_IsFunction(ctx, keys)) {
        JS_ThrowTypeError(ctx, ".keys is not a function");
        goto exception;
    }
    *psize = size;
    *phas = has;
    *pkeys = keys;
    return 0;

 exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    *psize = 0;
    *phas = JS_UNDEFINED;
    *pkeys = JS_UNDEFINED;
    return -1;
}

/* copy 'this_val' in a new set without side effects */
static JSValue js_copy_set(JSContext *ctx, JSValueConst this_val)
{
    JSValue newset;
    JSMapState *s, *t;
    struct list_head *el;
    JSMapRecord *mr;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;

    newset = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, MAGIC_SET);
    if (JS_IsException(newset))
        return JS_EXCEPTION;
    t = JS_GetOpaque(newset, JS_CLASS_SET);

    // can't clone this_val using js_map_constructor(),
    // test262 mandates we don't call the .add method
    list_for_each(el, &s->records) {
        mr = list_entry(el, JSMapRecord, link);
        if (mr->empty)
            continue;
        if (!set_add_record(ctx, t, mr->key))
            goto exception;
    }
    return newset;
 exception:
    JS_FreeValue(ctx, newset);
    return JS_EXCEPTION;
}

static JSValue js_set_isDisjointFrom(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue item, iter, keys, has, next, rv, rval;
    int done;
    BOOL found;
    JSMapState *s;
    int64_t size;
    int ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    rval = JS_EXCEPTION;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    if (s->record_count <= size) {
        iter = js_create_map_iterator(ctx, this_val, 0, NULL, MAGIC_SET);
        if (JS_IsException(iter))
            goto exception;
        found = FALSE;
        do {
            item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
            JS_FreeValue(ctx, item);
            ok = qjs_to_bool_free(ctx, rv); // returns -1 if rv is JS_EXCEPTION
            if (ok < 0)
                goto exception;
            found = (ok > 0);
        } while (!found);
    } else {
        iter = JS_Call(ctx, keys, argv[0], 0, NULL);
        if (JS_IsException(iter))
            goto exception;
        next = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next))
            goto exception;
        found = FALSE;
        for(;;) {
            item = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            item = map_normalize_key(ctx, item);
            found = (NULL != map_find_record(ctx, s, item));
            JS_FreeValue(ctx, item);
            if (found) {
                qjs_iterator_close(ctx, iter, FALSE);
                break;
            }
        }
    }
    rval = !found ? JS_TRUE : JS_FALSE;
exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return rval;
}

static JSValue js_set_isSubsetOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue item, iter, keys, has, next, rv, rval;
    BOOL found;
    JSMapState *s;
    int64_t size;
    int done, ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    rval = JS_EXCEPTION;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    found = FALSE;
    if (s->record_count > size)
        goto fini;
    iter = js_create_map_iterator(ctx, this_val, 0, NULL, MAGIC_SET);
    if (JS_IsException(iter))
        goto exception;
    found = TRUE;
    do {
        item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
        JS_FreeValue(ctx, item);
        ok = qjs_to_bool_free(ctx, rv); // returns -1 if rv is JS_EXCEPTION
        if (ok < 0)
            goto exception;
        found = (ok > 0);
    } while (found);
fini:
    rval = found ? JS_TRUE : JS_FALSE;
exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return rval;
}

static JSValue js_set_isSupersetOf(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue item, iter, keys, has, next, rval;
    int done;
    BOOL found;
    JSMapState *s;
    int64_t size;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    rval = JS_EXCEPTION;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    found = FALSE;
    if (s->record_count < size)
        goto fini;
    iter = JS_Call(ctx, keys, argv[0], 0, NULL);
    if (JS_IsException(iter))
        goto exception;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;
    found = TRUE;
    for(;;) {
        item = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        item = map_normalize_key(ctx, item);
        found = (NULL != map_find_record(ctx, s, item));
        JS_FreeValue(ctx, item);
        if (!found) {
            qjs_iterator_close(ctx, iter, FALSE);
            break;
        }
    }
fini:
    rval = found ? JS_TRUE : JS_FALSE;
exception:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return rval;
}

static JSValue js_set_intersection(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, keys, has, next, rv;
    JSMapState *s, *t;
    JSMapRecord *mr;
    int64_t size;
    int done, ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;
    if (s->record_count > size) {
        iter = JS_Call(ctx, keys, argv[0], 0, NULL);
        if (JS_IsException(iter))
            goto exception;
        next = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next))
            goto exception;
        newset = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, MAGIC_SET);
        if (JS_IsException(newset))
            goto exception;
        t = JS_GetOpaque(newset, JS_CLASS_SET);
        for (;;) {
            item = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            item = map_normalize_key(ctx, item);
            if (!map_find_record(ctx, s, item)) {
                JS_FreeValue(ctx, item);
            } else if (map_find_record(ctx, t, item)) {
                JS_FreeValue(ctx, item); // no duplicates
            } else {
                mr = set_add_record(ctx, t, item);
                JS_FreeValue(ctx, item);
                if (!mr)
                    goto exception;
            }
        }
    } else {
        iter = js_create_map_iterator(ctx, this_val, 0, NULL, MAGIC_SET);
        if (JS_IsException(iter))
            goto exception;
        newset = js_map_constructor(ctx, JS_UNDEFINED, 0, NULL, MAGIC_SET);
        if (JS_IsException(newset))
            goto exception;
        t = JS_GetOpaque(newset, JS_CLASS_SET);
        for (;;) {
            item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
            ok = qjs_to_bool_free(ctx, rv); // returns -1 if rv is JS_EXCEPTION
            if (ok > 0) {
                item = map_normalize_key(ctx, item);
                if (map_find_record(ctx, t, item)) {
                    JS_FreeValue(ctx, item); // no duplicates
                } else {
                    mr = set_add_record(ctx, t, item);
                    JS_FreeValue(ctx, item);
                    if (!mr)
                        goto exception;
                }
            } else {
                JS_FreeValue(ctx, item);
                if (ok < 0)
                    goto exception;
            }
        }
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return newset;
}

static JSValue js_set_difference(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, keys, has, next, rv;
    JSMapState *s, *t;
    int64_t size;
    int done;
    int ok;

    iter = JS_UNDEFINED;
    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        goto exception;

    newset = js_copy_set(ctx, this_val);
    if (JS_IsException(newset))
        goto exception;
    t = JS_GetOpaque(newset, JS_CLASS_SET);

    if (s->record_count <= size) {
        iter = js_create_map_iterator(ctx, newset, 0, NULL, MAGIC_SET);
        if (JS_IsException(iter))
            goto exception;
        for (;;) {
            item = js_map_iterator_next(ctx, iter, 0, NULL, &done, MAGIC_SET);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            rv = JS_Call(ctx, has, argv[0], 1, (JSValueConst *)&item);
            ok = qjs_to_bool_free(ctx, rv); // returns -1 if rv is JS_EXCEPTION
            if (ok < 0) {
                JS_FreeValue(ctx, item);
                goto exception;
            }
            if (ok) {
                map_delete_record(ctx, t, item);
            }
            JS_FreeValue(ctx, item);
        }
    } else {
        iter = JS_Call(ctx, keys, argv[0], 0, NULL);
        if (JS_IsException(iter))
            goto exception;
        next = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next))
            goto exception;
        for (;;) {
            item = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
            if (JS_IsException(item))
                goto exception;
            if (done) // item is JS_UNDEFINED
                break;
            map_delete_record(ctx, t, item);
            JS_FreeValue(ctx, item);
        }
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, has);
    JS_FreeValue(ctx, keys);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return newset;
}

static JSValue js_set_symmetricDifference(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, next, has, keys;
    JSMapState *s, *t;
    JSMapRecord *mr;
    int64_t size;
    int done;
    BOOL present;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        return JS_EXCEPTION;
    JS_FreeValue(ctx, has);

    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    iter = JS_Call(ctx, keys, argv[0], 0, NULL);
    if (JS_IsException(iter))
        goto exception;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;
    newset = js_copy_set(ctx, this_val);
    if (JS_IsException(newset))
        goto exception;
    t = JS_GetOpaque(newset, JS_CLASS_SET);
    for (;;) {
        item = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        // note the subtlety here: due to mutating iterators, it's
        // possible for keys to disappear during iteration; test262
        // still expects us to maintain insertion order though, so
        // we first check |this|, then |new|; |new| is a copy of |this|
        // - if item exists in |this|, delete (if it exists) from |new|
        // - if item misses in |this| and |new|, add to |new|
        // - if item exists in |new| but misses in |this|, *don't* add it,
        //   mutating iterator erased it
        item = map_normalize_key(ctx, item);
        present = (NULL != map_find_record(ctx, s, item));
        mr = map_find_record(ctx, t, item);
        if (present) {
            map_delete_record(ctx, t, item);
            JS_FreeValue(ctx, item);
        } else if (mr) {
            JS_FreeValue(ctx, item);
        } else {
            mr = set_add_record(ctx, t, item);
            JS_FreeValue(ctx, item);
            if (!mr)
                goto exception;
        }
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, keys);
    return newset;
}

static JSValue js_set_union(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    JSValue newset, item, iter, next, has, keys, rv;
    JSMapState *s;
    int64_t size;
    int done;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_SET);
    if (!s)
        return JS_EXCEPTION;
    if (get_set_record(ctx, argv[0], &size, &has, &keys) < 0)
        return JS_EXCEPTION;
    JS_FreeValue(ctx, has);

    next = JS_UNDEFINED;
    newset = JS_UNDEFINED;
    iter = JS_Call(ctx, keys, argv[0], 0, NULL);
    if (JS_IsException(iter))
        goto exception;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto exception;

    newset = js_copy_set(ctx, this_val);
    if (JS_IsException(newset))
        goto exception;

    for (;;) {
        item = qjs_iterator_next(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done) // item is JS_UNDEFINED
            break;
        rv = js_map_set(ctx, newset, 1, (JSValueConst *)&item, MAGIC_SET);
        JS_FreeValue(ctx, item);
        if (JS_IsException(rv))
            goto exception;
        JS_FreeValue(ctx, rv);
    }
    goto fini;
exception:
    JS_FreeValue(ctx, newset);
    newset = JS_EXCEPTION;
fini:
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, keys);
    return newset;
}

static const JSCFunctionListEntry js_map_funcs[] = {
    JS_CFUNC_MAGIC_DEF("groupBy", 2, js_object_groupBy, 1 ),
    JS_CGETSET_DEF("[Symbol.species]", qjs_array_get_this, NULL ),
};

static const JSCFunctionListEntry js_map_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("set", 2, js_map_set, 0 ),
    JS_CFUNC_MAGIC_DEF("get", 1, js_map_get, 0 ),
    JS_CFUNC_MAGIC_DEF("getOrInsert", 2, js_map_getOrInsert,
                       (JS_CLASS_MAP << 1) | /*computed*/FALSE ),
    JS_CFUNC_MAGIC_DEF("getOrInsertComputed", 2, js_map_getOrInsert,
                       (JS_CLASS_MAP << 1) | /*computed*/TRUE ),
    JS_CFUNC_MAGIC_DEF("has", 1, js_map_has, 0 ),
    JS_CFUNC_MAGIC_DEF("delete", 1, js_map_delete, 0 ),
    JS_CFUNC_MAGIC_DEF("clear", 0, js_map_clear, 0 ),
    JS_CGETSET_MAGIC_DEF("size", js_map_get_size, NULL, 0),
    JS_CFUNC_MAGIC_DEF("forEach", 1, js_map_forEach, 0 ),
    JS_CFUNC_MAGIC_DEF("values", 0, js_create_map_iterator, (JS_ITERATOR_KIND_VALUE << 2) | 0 ),
    JS_CFUNC_MAGIC_DEF("keys", 0, js_create_map_iterator, (JS_ITERATOR_KIND_KEY << 2) | 0 ),
    JS_CFUNC_MAGIC_DEF("entries", 0, js_create_map_iterator, (JS_ITERATOR_KIND_KEY_AND_VALUE << 2) | 0 ),
    JS_ALIAS_DEF("[Symbol.iterator]", "entries" ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Map", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_map_iterator_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_map_iterator_next, 0 ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Map Iterator", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_set_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("add", 1, js_map_set, MAGIC_SET ),
    JS_CFUNC_MAGIC_DEF("has", 1, js_map_has, MAGIC_SET ),
    JS_CFUNC_MAGIC_DEF("delete", 1, js_map_delete, MAGIC_SET ),
    JS_CFUNC_MAGIC_DEF("clear", 0, js_map_clear, MAGIC_SET ),
    JS_CGETSET_MAGIC_DEF("size", js_map_get_size, NULL, MAGIC_SET ),
    JS_CFUNC_MAGIC_DEF("forEach", 1, js_map_forEach, MAGIC_SET ),
    JS_CFUNC_DEF("isDisjointFrom", 1, js_set_isDisjointFrom ),
    JS_CFUNC_DEF("isSubsetOf", 1, js_set_isSubsetOf ),
    JS_CFUNC_DEF("isSupersetOf", 1, js_set_isSupersetOf ),
    JS_CFUNC_DEF("intersection", 1, js_set_intersection ),
    JS_CFUNC_DEF("difference", 1, js_set_difference ),
    JS_CFUNC_DEF("symmetricDifference", 1, js_set_symmetricDifference ),
    JS_CFUNC_DEF("union", 1, js_set_union ),
    JS_CFUNC_MAGIC_DEF("values", 0, js_create_map_iterator, (JS_ITERATOR_KIND_KEY << 2) | MAGIC_SET ),
    JS_ALIAS_DEF("keys", "values" ),
    JS_ALIAS_DEF("[Symbol.iterator]", "values" ),
    JS_CFUNC_MAGIC_DEF("entries", 0, js_create_map_iterator, (JS_ITERATOR_KIND_KEY_AND_VALUE << 2) | MAGIC_SET ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Set", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_set_iterator_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_map_iterator_next, MAGIC_SET ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Set Iterator", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_weak_map_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("set", 2, js_map_set, MAGIC_WEAK ),
    JS_CFUNC_MAGIC_DEF("get", 1, js_map_get, MAGIC_WEAK ),
    JS_CFUNC_MAGIC_DEF("getOrInsert", 2, js_map_getOrInsert,
                       (JS_CLASS_WEAKMAP << 1) | /*computed*/FALSE ),
    JS_CFUNC_MAGIC_DEF("getOrInsertComputed", 2, js_map_getOrInsert,
                       (JS_CLASS_WEAKMAP << 1) | /*computed*/TRUE ),
    JS_CFUNC_MAGIC_DEF("has", 1, js_map_has, MAGIC_WEAK ),
    JS_CFUNC_MAGIC_DEF("delete", 1, js_map_delete, MAGIC_WEAK ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "WeakMap", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_weak_set_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("add", 1, js_map_set, MAGIC_SET | MAGIC_WEAK ),
    JS_CFUNC_MAGIC_DEF("has", 1, js_map_has, MAGIC_SET | MAGIC_WEAK ),
    JS_CFUNC_MAGIC_DEF("delete", 1, js_map_delete, MAGIC_SET | MAGIC_WEAK ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "WeakSet", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry * const js_map_proto_funcs_ptr[6] = {
    js_map_proto_funcs,
    js_set_proto_funcs,
    js_weak_map_proto_funcs,
    js_weak_set_proto_funcs,
    js_map_iterator_proto_funcs,
    js_set_iterator_proto_funcs,
};

static const uint8_t js_map_proto_funcs_count[6] = {
    countof(js_map_proto_funcs),
    countof(js_set_proto_funcs),
    countof(js_weak_map_proto_funcs),
    countof(js_weak_set_proto_funcs),
    countof(js_map_iterator_proto_funcs),
    countof(js_set_iterator_proto_funcs),
};

int JS_AddIntrinsicMapSet(JSContext *ctx)
{
    int i;
    JSValue obj1;
    char buf[ATOM_GET_STR_BUF_SIZE];

    for(i = 0; i < 4; i++) {
        JSCFunctionType ft;
        const char *name = JS_AtomGetStr(ctx, buf, sizeof(buf),
                                         JS_ATOM_Map + i);
        ft.constructor_magic = js_map_constructor;
        obj1 = qjs_new_c_constructor(ctx, JS_CLASS_MAP + i, name,
                                  ft.generic, 0, JS_CFUNC_constructor_magic, i,
                                  JS_UNDEFINED,
                                  js_map_funcs, i < 2 ? countof(js_map_funcs) : 0,
                                  js_map_proto_funcs_ptr[i], js_map_proto_funcs_count[i],
                                  0);
        if (JS_IsException(obj1))
            return -1;
        JS_FreeValue(ctx, obj1);
    }

    for(i = 0; i < 2; i++) {
        ctx->class_proto[JS_CLASS_MAP_ITERATOR + i] =
            qjs_new_object_proto_list(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                                  js_map_proto_funcs_ptr[i + 4],
                                  js_map_proto_funcs_count[i + 4]);
        if (JS_IsException(ctx->class_proto[JS_CLASS_MAP_ITERATOR + i]))
            return -1;
    }
    return 0;
}


/* WeakRef */

typedef struct JSWeakRefData {
    JSWeakRefHeader weakref_header;
    JSValue target;
} JSWeakRefData;

static void js_weakref_finalizer(JSRuntime *rt, JSValue val)
{
    JSWeakRefData *wrd = JS_GetOpaque(val, JS_CLASS_WEAK_REF);
    if (!wrd)
        return;
    js_weakref_free(rt, wrd->target);
    list_del(&wrd->weakref_header.link);
    js_free_rt(rt, wrd);
}

static void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSWeakRefData *wrd = container_of(wh, JSWeakRefData, weakref_header);

    if (!js_weakref_is_live(wrd->target)) {
        js_weakref_free(rt, wrd->target);
        wrd->target = JS_UNDEFINED;
    }
}

static JSValue js_weakref_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSValueConst arg;
    JSValue obj;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    arg = argv[0];
    if (!js_weakref_is_target(arg))
        return JS_ThrowTypeError(ctx, "invalid target");
    obj = qjs_collection_create_from_ctor(ctx, new_target, JS_CLASS_WEAK_REF);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    JSWeakRefData *wrd = js_mallocz(ctx, sizeof(*wrd));
    if (!wrd) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    wrd->target = js_weakref_new(ctx, arg);
    wrd->weakref_header.weakref_type = JS_WEAKREF_TYPE_WEAKREF;
    list_add_tail(&wrd->weakref_header.link, &ctx->rt->weakref_list);
    JS_SetOpaque(obj, wrd);
    return obj;
}

static JSValue js_weakref_deref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSWeakRefData *wrd = JS_GetOpaque2(ctx, this_val, JS_CLASS_WEAK_REF);
    if (!wrd)
        return JS_EXCEPTION;
    if (js_weakref_is_live(wrd->target))
        return JS_DupValue(ctx, wrd->target);
    else
        return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_weakref_proto_funcs[] = {
    JS_CFUNC_DEF("deref", 0, js_weakref_deref ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "WeakRef", JS_PROP_CONFIGURABLE ),
};

static const JSClassShortDef js_weakref_class_def[] = {
    { JS_ATOM_WeakRef, js_weakref_finalizer, NULL }, /* JS_CLASS_WEAK_REF */
};

typedef struct JSFinRecEntry {
    struct list_head link;
    JSValue target;
    JSValue held_val;
    JSValue token;
} JSFinRecEntry;

typedef struct JSFinalizationRegistryData {
    JSWeakRefHeader weakref_header;
    struct list_head entries; /* list of JSFinRecEntry.link */
    JSContext *realm;
    JSValue cb;
} JSFinalizationRegistryData;

static void js_finrec_finalizer(JSRuntime *rt, JSValue val)
{
    JSFinalizationRegistryData *frd = JS_GetOpaque(val, JS_CLASS_FINALIZATION_REGISTRY);
    if (frd) {
        struct list_head *el, *el1;
        list_for_each_safe(el, el1, &frd->entries) {
            JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);
            js_weakref_free(rt, fre->target);
            js_weakref_free(rt, fre->token);
            JS_FreeValueRT(rt, fre->held_val);
            js_free_rt(rt, fre);
        }
        JS_FreeValueRT(rt, frd->cb);
        JS_FreeContext(frd->realm);
        list_del(&frd->weakref_header.link);
        js_free_rt(rt, frd);
    }
}

static void js_finrec_mark(JSRuntime *rt, JSValueConst val,
                           JS_MarkFunc *mark_func)
{
    JSFinalizationRegistryData *frd = JS_GetOpaque(val, JS_CLASS_FINALIZATION_REGISTRY);
    struct list_head *el;
    if (frd) {
        list_for_each(el, &frd->entries) {
            JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);
            JS_MarkValue(rt, fre->held_val, mark_func);
        }
        JS_MarkValue(rt, frd->cb, mark_func);
        mark_func(rt, &frd->realm->header);
    }
}

static JSValue js_finrec_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    return JS_Call(ctx, argv[0], JS_UNDEFINED, 1, &argv[1]);
}

static void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSFinalizationRegistryData *frd = container_of(wh, JSFinalizationRegistryData, weakref_header);
    struct list_head *el, *el1;

    list_for_each_safe(el, el1, &frd->entries) {
        JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);

        if (!js_weakref_is_live(fre->token)) {
            js_weakref_free(rt, fre->token);
            fre->token = JS_UNDEFINED;
        }

        if (!js_weakref_is_live(fre->target)) {
            JSValueConst args[2];
            args[0] = frd->cb;
            args[1] = fre->held_val;
            /* no exception is raised to avoid recursing into the GC */
            qjs_enqueue_job2(frd->realm, js_finrec_job, 2, args, TRUE);

            js_weakref_free(rt, fre->target);
            js_weakref_free(rt, fre->token);
            JS_FreeValueRT(rt, fre->held_val);
            list_del(&fre->link);
            js_free_rt(rt, fre);
        }
    }
}

static JSValue js_finrec_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValueConst cb;
    JSValue obj;
    JSFinalizationRegistryData *frd;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    cb = argv[0];
    if (!JS_IsFunction(ctx, cb))
        return JS_ThrowTypeError(ctx, "argument must be a function");

    obj = qjs_collection_create_from_ctor(ctx, new_target, JS_CLASS_FINALIZATION_REGISTRY);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    frd = js_mallocz(ctx, sizeof(*frd));
    if (!frd) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    frd->weakref_header.weakref_type = JS_WEAKREF_TYPE_FINREC;
    list_add_tail(&frd->weakref_header.link, &ctx->rt->weakref_list);
    init_list_head(&frd->entries);
    frd->realm = JS_DupContext(ctx);
    frd->cb = JS_DupValue(ctx, cb);
    JS_SetOpaque(obj, frd);
    return obj;
}

static JSValue js_finrec_register(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValueConst target, held_val, token;
    JSFinalizationRegistryData *frd;
    JSFinRecEntry *fre;

    frd = JS_GetOpaque2(ctx, this_val, JS_CLASS_FINALIZATION_REGISTRY);
    if (!frd)
        return JS_EXCEPTION;
    target = argv[0];
    held_val = argv[1];
    token = argc > 2 ? argv[2] : JS_UNDEFINED;

    if (!js_weakref_is_target(target))
        return JS_ThrowTypeError(ctx, "invalid target");
    if (qjs_same_value(ctx, target, held_val))
        return JS_ThrowTypeError(ctx, "held value cannot be the target");
    if (!JS_IsUndefined(token) && !js_weakref_is_target(token))
        return JS_ThrowTypeError(ctx, "invalid unregister token");
    fre = js_malloc(ctx, sizeof(*fre));
    if (!fre)
        return JS_EXCEPTION;
    fre->target = js_weakref_new(ctx, target);
    fre->held_val = JS_DupValue(ctx, held_val);
    fre->token = js_weakref_new(ctx, token);
    list_add_tail(&fre->link, &frd->entries);
    return JS_UNDEFINED;
}

static JSValue js_finrec_unregister(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSFinalizationRegistryData *frd = JS_GetOpaque2(ctx, this_val, JS_CLASS_FINALIZATION_REGISTRY);
    JSValueConst token;
    BOOL removed;
    struct list_head *el, *el1;

    if (!frd)
        return JS_EXCEPTION;
    token = argv[0];
    if (!js_weakref_is_target(token))
        return JS_ThrowTypeError(ctx, "invalid unregister token");

    removed = FALSE;
    list_for_each_safe(el, el1, &frd->entries) {
        JSFinRecEntry *fre = list_entry(el, JSFinRecEntry, link);
        if (js_weakref_is_live(fre->token) && qjs_same_value(ctx, fre->token, token)) {
            js_weakref_free(ctx->rt, fre->target);
            js_weakref_free(ctx->rt, fre->token);
            JS_FreeValue(ctx, fre->held_val);
            list_del(&fre->link);
            js_free(ctx, fre);
            removed = TRUE;
        }
    }
    return JS_NewBool(ctx, removed);
}

static const JSCFunctionListEntry js_finrec_proto_funcs[] = {
    JS_CFUNC_DEF("register", 2, js_finrec_register ),
    JS_CFUNC_DEF("unregister", 1, js_finrec_unregister ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "FinalizationRegistry", JS_PROP_CONFIGURABLE ),
};

static const JSClassShortDef js_finrec_class_def[] = {
    { JS_ATOM_FinalizationRegistry, js_finrec_finalizer, js_finrec_mark }, /* JS_CLASS_FINALIZATION_REGISTRY */
};

int JS_AddIntrinsicWeakRef(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj;

    /* WeakRef */
    if (!JS_IsRegisteredClass(rt, JS_CLASS_WEAK_REF)) {
        if (qjs_init_class_range(rt, js_weakref_class_def, JS_CLASS_WEAK_REF,
                             countof(js_weakref_class_def)))
            return -1;
    }
    obj = qjs_new_c_constructor(ctx, JS_CLASS_WEAK_REF, "WeakRef",
                             js_weakref_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                             JS_UNDEFINED,
                             NULL, 0,
                             js_weakref_proto_funcs, countof(js_weakref_proto_funcs),
                             0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);

    /* FinalizationRegistry */
    if (!JS_IsRegisteredClass(rt, JS_CLASS_FINALIZATION_REGISTRY)) {
        if (qjs_init_class_range(rt, js_finrec_class_def, JS_CLASS_FINALIZATION_REGISTRY,
                             countof(js_finrec_class_def)))
            return -1;
    }

    obj = qjs_new_c_constructor(ctx, JS_CLASS_FINALIZATION_REGISTRY, "FinalizationRegistry",
                             js_finrec_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                             JS_UNDEFINED,
                             NULL, 0,
                             js_finrec_proto_funcs, countof(js_finrec_proto_funcs),
                             0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);
    return 0;
}



QJS_INTERNAL void qjs_map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *ref)
{ map_delete_weakrefs(rt, ref); }
QJS_INTERNAL void qjs_weakref_delete(JSRuntime *rt, JSWeakRefHeader *ref)
{ weakref_delete_weakref(rt, ref); }
QJS_INTERNAL void qjs_finrec_delete(JSRuntime *rt, JSWeakRefHeader *ref)
{ finrec_delete_weakref(rt, ref); }
QJS_INTERNAL JSValue qjs_object_group_by(JSContext *ctx,
                                         JSValueConst this_val,
                                         int argc, JSValueConst *argv,
                                         int is_map)
{ return js_object_groupBy(ctx, this_val, argc, argv, is_map); }
