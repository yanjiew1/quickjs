/*
 * QuickJS Javascript Engine: Promise, Async & Iterator Builtins
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

#include "quickjs-internal.h"
#include "js_builtins.h"

/* Iterator Wrap */

typedef struct JSIteratorWrapData {
    JSValue wrapped_iter;
    JSValue wrapped_next;
} JSIteratorWrapData;

void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorWrapData *it = p->u.iterator_wrap_data;
    if (it) {
        JS_FreeValueRT(rt, it->wrapped_iter);
        JS_FreeValueRT(rt, it->wrapped_next);
        js_free_rt(rt, it);
    }
}

void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorWrapData *it = p->u.iterator_wrap_data;
    if (it) {
        JS_MarkValue(rt, it->wrapped_iter, mark_func);
        JS_MarkValue(rt, it->wrapped_next, mark_func);
    }
}

static JSValue js_iterator_wrap_next(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv,
                                     int *pdone, int magic)
{
    JSIteratorWrapData *it;
    JSValue method, ret;
    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_WRAP);
    if (!it)
        return JS_EXCEPTION;
    if (magic == GEN_MAGIC_NEXT) {
        return JS_IteratorNext(ctx, it->wrapped_iter, it->wrapped_next, 0, NULL, pdone);
    } else {
        method = JS_GetProperty(ctx, it->wrapped_iter, JS_ATOM_return);
        if (JS_IsException(method))
            return JS_EXCEPTION;
        if (JS_IsNull(method) || JS_IsUndefined(method)) {
            *pdone = TRUE;
            return JS_UNDEFINED;
        }
        ret = JS_IteratorNext2(ctx, it->wrapped_iter, method, 0, NULL, pdone);
        JS_FreeValue(ctx, method);
        return ret;
    }
}

static const JSCFunctionListEntry js_iterator_wrap_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_iterator_wrap_next, GEN_MAGIC_NEXT ),
    JS_ITERATOR_NEXT_DEF("return", 0, js_iterator_wrap_next, GEN_MAGIC_RETURN ),
};

/* Iterator */

static JSValue js_iterator_constructor_getset(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data)
{
    int ret;

    if (argc > 0) { // if setter
        if (!JS_IsObject(argv[0]))
            return JS_ThrowTypeErrorNotAnObject(ctx);
        ret = JS_DefinePropertyValue(ctx, this_val, JS_ATOM_constructor,
                                     JS_DupValue(ctx, argv[0]),
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        if (ret < 0)
            return JS_EXCEPTION;
        return JS_UNDEFINED;
    } else {
        return JS_DupValue(ctx, func_data[0]);
    }
}

static JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv)
{
    JSObject *p;

    if (JS_TAG_OBJECT != JS_VALUE_GET_TAG(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    p = JS_VALUE_GET_OBJ(new_target);
    if (p->class_id == JS_CLASS_C_FUNCTION &&
        p->u.cfunc.c_function.generic == js_iterator_constructor) {
        return JS_ThrowTypeError(ctx, "abstract class not constructable");
    }
    return js_create_from_ctor(ctx, new_target, JS_CLASS_ITERATOR);
}

// note: deliberately doesn't use space-saving bit fields for
// |index|, |count| and |running| because tcc miscompiles them
typedef struct JSIteratorConcatData {
    int index, count;             // elements (not pairs!) in values[] array
    BOOL running;
    JSValue iter, next, values[]; // array of (object, method) pairs
} JSIteratorConcatData;

void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorConcatData *it = p->u.iterator_concat_data;
    if (it) {
        JS_FreeValueRT(rt, it->iter);
        JS_FreeValueRT(rt, it->next);
        for (int i = it->index; i < it->count; i++)
            JS_FreeValueRT(rt, it->values[i]);
        js_free_rt(rt, it);
    }
}

void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorConcatData *it = p->u.iterator_concat_data;
    if (it) {
        JS_MarkValue(rt, it->iter, mark_func);
        JS_MarkValue(rt, it->next, mark_func);
        for (int i = it->index; i < it->count; i++)
            JS_MarkValue(rt, it->values[i], mark_func);
    }
}

static JSValue js_iterator_concat_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int *pdone, int magic)
{
    JSValue iter, item, next, val, *obj, *meth, ret;
    JSIteratorConcatData *it;
    int done;

    *pdone = FALSE;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_CONCAT);
    if (!it)
        return JS_EXCEPTION;
    if (it->running)
        return JS_ThrowTypeError(ctx, "already running");

    it->running = TRUE;
    for(;;) {
        if (it->index >= it->count) {
            *pdone = TRUE;
            ret = JS_UNDEFINED;
            break;
        }
        obj = &it->values[it->index + 0];
        meth = &it->values[it->index + 1];
        iter = it->iter;
        if (JS_IsUndefined(iter)) {
            iter = JS_GetIterator2(ctx, *obj, *meth);
            if (JS_IsException(iter))
                goto fail;
            it->iter = iter;
        }
        next = it->next;
        if (JS_IsUndefined(next)) {
            next = JS_GetProperty(ctx, iter, JS_ATOM_next);
            if (JS_IsException(next))
                goto fail;
            it->next = next;
        }
        item = JS_IteratorNext2(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto fail;
        if (done == 0) {
            ret = item;
            break;
        } else if (done == 2) {
            val = JS_GetProperty(ctx, item, JS_ATOM_done);
            if (JS_IsException(val)) {
                JS_FreeValue(ctx, item);
            fail:
                ret = JS_EXCEPTION;
                break;
            }
            done = JS_ToBoolFree(ctx, val);
            if (done)
                goto done_next;
            ret = JS_GetProperty(ctx, item, JS_ATOM_value);
            JS_FreeValue(ctx, item);
            break;
        } else {
        done_next:
            JS_FreeValue(ctx, item);
            JS_FreeValue(ctx, iter);
            JS_FreeValue(ctx, next);
            it->iter = JS_UNDEFINED;
            it->next = JS_UNDEFINED;
            JS_FreeValue(ctx, *meth);
            JS_FreeValue(ctx, *obj);
            it->index += 2;
        }
    }
    it->running = FALSE;
    return ret;
}

static JSValue js_iterator_concat_return(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSIteratorConcatData *it;
    JSValue ret;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_CONCAT);
    if (!it)
        return JS_EXCEPTION;
    if (it->running)
        return JS_ThrowTypeError(ctx, "already running");
    ret = JS_UNDEFINED;
    if (!JS_IsUndefined(it->iter)) {
        it->running = TRUE;
        ret = JS_GetProperty(ctx, it->iter, JS_ATOM_return);
        if (JS_IsException(ret)) {
            it->running = FALSE;
            return JS_EXCEPTION;
        }
        ret = JS_CallFree(ctx, ret, it->iter, 0, NULL);
        it->running = FALSE;
    }
    while (it->index < it->count)
        JS_FreeValue(ctx, it->values[it->index++]);
    JS_FreeValue(ctx, it->iter);
    JS_FreeValue(ctx, it->next);
    it->iter = JS_UNDEFINED;
    it->next = JS_UNDEFINED;
    return ret;
}

static const JSCFunctionListEntry js_iterator_concat_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_iterator_concat_next, 0 ),
    JS_CFUNC_DEF("return", 0, js_iterator_concat_return ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Iterator Concat", JS_PROP_CONFIGURABLE ),
};

static JSValue js_iterator_concat(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSIteratorConcatData *it;
    JSValue obj, method;

    it = js_malloc(ctx, sizeof(*it) + 2*argc * sizeof(it->values[0]));
    if (!it)
        return JS_EXCEPTION;
    it->running = FALSE;
    it->index = 0;
    it->count = 0;
    it->iter = JS_UNDEFINED;
    it->next = JS_UNDEFINED;
    for (int i = 0; i < argc; i++) {
        JSValueConst obj = argv[i];
        if (!JS_IsObject(obj)) {
            JS_ThrowTypeErrorNotAnObject(ctx);
            goto fail;
        }
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
        if (JS_IsException(method))
            goto fail;
        if (!JS_IsFunction(ctx, method)) {
            JS_ThrowTypeError(ctx, "not a function");
            JS_FreeValue(ctx, method);
            goto fail;
        }
        it->values[it->count++] = JS_DupValue(ctx, obj);
        it->values[it->count++] = method;
    }
    obj = JS_NewObjectClass(ctx, JS_CLASS_ITERATOR_CONCAT);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, it);
    return obj;
fail:
    for (int i = 0; i < it->count; i++)
        JS_FreeValue(ctx, it->values[i]);
    js_free(ctx, it);
    return JS_EXCEPTION;
}

static JSValue js_iterator_from(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValueConst obj = argv[0];
    JSValue method, iter, wrapper;
    JSIteratorWrapData *it;
    int ret;

    if (!JS_IsObject(obj)) {
        if (!JS_IsString(obj))
            return JS_ThrowTypeError(ctx, "Iterator.from called on non-object");
    }
    method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
    if (JS_IsException(method))
        return JS_EXCEPTION;
    if (JS_IsNull(method) || JS_IsUndefined(method)) {
        iter = JS_DupValue(ctx, obj);
    } else {
        iter = JS_GetIterator2(ctx, obj, method);
        JS_FreeValue(ctx, method);
        if (JS_IsException(iter))
            return JS_EXCEPTION;
    }

    wrapper = JS_UNDEFINED;
    method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(method))
        goto fail;

    ret = JS_OrdinaryIsInstanceOf(ctx, iter, ctx->iterator_ctor);
    if (ret < 0)
        goto fail;
    if (ret) {
        JS_FreeValue(ctx, method);
        return iter;
    }
    
    wrapper = JS_NewObjectClass(ctx, JS_CLASS_ITERATOR_WRAP);
    if (JS_IsException(wrapper))
        goto fail;
    it = js_malloc(ctx, sizeof(*it));
    if (!it)
        goto fail;
    it->wrapped_iter = iter;
    it->wrapped_next = method;
    JS_SetOpaque(wrapper, it);
    return wrapper;

 fail:
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, wrapper);
    return JS_EXCEPTION;
}

typedef enum JSIteratorHelperKindEnum {
    JS_ITERATOR_HELPER_KIND_DROP,
    JS_ITERATOR_HELPER_KIND_EVERY,
    JS_ITERATOR_HELPER_KIND_FILTER,
    JS_ITERATOR_HELPER_KIND_FIND,
    JS_ITERATOR_HELPER_KIND_FLAT_MAP,
    JS_ITERATOR_HELPER_KIND_FOR_EACH,
    JS_ITERATOR_HELPER_KIND_MAP,
    JS_ITERATOR_HELPER_KIND_SOME,
    JS_ITERATOR_HELPER_KIND_TAKE,
} JSIteratorHelperKindEnum;

typedef struct JSIteratorHelperData {
    JSValue obj;
    JSValue next;
    JSValue func; // predicate (filter) or mapper (flatMap, map)
    JSValue inner; // innerValue (flatMap)
    int64_t count; // limit (drop, take) or counter (filter, map, flatMap)
    JSIteratorHelperKindEnum kind : 8;
    uint8_t executing : 1;
    uint8_t done : 1;
} JSIteratorHelperData;

static JSValue js_create_iterator_helper(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv, int magic)
{
    JSValueConst func;
    JSValue obj, method;
    int64_t count;
    JSIteratorHelperData *it;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    func = JS_UNDEFINED;
    count = 0;

    switch(magic) {
    case JS_ITERATOR_HELPER_KIND_DROP:
    case JS_ITERATOR_HELPER_KIND_TAKE:
        {
            JSValue v;
            double dlimit;
            v = JS_ToNumber(ctx, argv[0]);
            if (JS_IsException(v))
                goto fail;
            // Check for Infinity.
            if (JS_ToFloat64(ctx, &dlimit, v)) {
                JS_FreeValue(ctx, v);
                goto fail;
            }
            if (isnan(dlimit)) {
                JS_FreeValue(ctx, v);
                goto range_error;
            }
            if (!isfinite(dlimit)) {
                JS_FreeValue(ctx, v);
                if (dlimit < 0)
                    goto range_error;
                else
                    count = MAX_SAFE_INTEGER;
            } else {
                v = JS_ToIntegerFree(ctx, v);
                if (JS_IsException(v))
                    goto fail;
                if (JS_ToInt64Free(ctx, &count, v))
                    goto fail;
            }
            if (count < 0)
                goto range_error;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FILTER:
    case JS_ITERATOR_HELPER_KIND_FLAT_MAP:
    case JS_ITERATOR_HELPER_KIND_MAP:
        {
            func = argv[0];
            if (check_function(ctx, func))
                goto fail;
        }
        break;
    default:
        abort();
        break;
    }

    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        goto fail;
    obj = JS_NewObjectClass(ctx, JS_CLASS_ITERATOR_HELPER);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, method);
        goto fail;
    }
    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, obj);
        JS_FreeValue(ctx, method);
        goto fail;
    }
    it->kind = magic;
    it->obj = JS_DupValue(ctx, this_val);
    it->func = JS_DupValue(ctx, func);
    it->next = method;
    it->inner = JS_UNDEFINED;
    it->count = count;
    it->executing = 0;
    it->done = 0;
    JS_SetOpaque(obj, it);
    return obj;
range_error:
    JS_ThrowRangeError(ctx, "must be positive");
fail:
    JS_IteratorClose(ctx, this_val, TRUE);
    return JS_EXCEPTION;
}

static JSValue js_iterator_proto_func(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic)
{
    JSValue item, method, ret, func, index_val, r;
    JSValueConst args[2];
    int64_t idx;
    int done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    func = JS_UNDEFINED;
    method = JS_UNDEFINED;
    
    if (check_function(ctx, argv[0]))
        goto fail;
    func = JS_DupValue(ctx, argv[0]);
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        goto fail_no_close;

    r = JS_UNDEFINED;

    switch(magic) {
    case JS_ITERATOR_HELPER_KIND_EVERY:
        {
            r = JS_TRUE;
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                if (!JS_ToBoolFree(ctx, ret)) {
                    if (JS_IteratorClose(ctx, this_val, FALSE) < 0)
                        r = JS_EXCEPTION;
                    else
                        r = JS_FALSE;
                    break;
                }
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FIND:
        {
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret)) {
                    JS_FreeValue(ctx, item);
                    goto fail;
                }
                if (JS_ToBoolFree(ctx, ret)) {
                    if (JS_IteratorClose(ctx, this_val, FALSE) < 0) {
                        JS_FreeValue(ctx, item);
                        r = JS_EXCEPTION;
                    } else {
                        r = item;
                    }
                    break;
                }
                JS_FreeValue(ctx, item);
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FOR_EACH:
        {
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                JS_FreeValue(ctx, ret);
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_SOME:
        {
            r = JS_FALSE;
            for (idx = 0; /*empty*/; idx++) {
                item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (done)
                    break;
                index_val = JS_NewInt64(ctx, idx);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                if (JS_ToBoolFree(ctx, ret)) {
                    if (JS_IteratorClose(ctx, this_val, FALSE) < 0)
                        r = JS_EXCEPTION;
                    else
                        r = JS_TRUE;
                    break;
                }
                index_val = JS_UNDEFINED;
                ret = JS_UNDEFINED;
                item = JS_UNDEFINED;
            }
        }
        break;
    default:
        abort();
        break;
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return r;
 fail:
    JS_IteratorClose(ctx, this_val, TRUE);
 fail_no_close:
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return JS_EXCEPTION;
}

static JSValue js_iterator_proto_reduce(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValue item, method, ret, func, index_val, acc;
    JSValueConst args[3];
    int64_t idx;
    int done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    acc = JS_UNDEFINED;
    func = JS_UNDEFINED;
    method = JS_UNDEFINED;
    if (check_function(ctx, argv[0]))
        goto exception;
    func = JS_DupValue(ctx, argv[0]);
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        goto exception;
    if (argc > 1) {
        acc = JS_DupValue(ctx, argv[1]);
        idx = 0;
    } else {
        acc = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(acc))
            goto exception_no_close;
        if (done) {
            JS_ThrowTypeError(ctx, "empty iterator");
            goto exception;
        }
        idx = 1;
    }
    for (/* empty */; /*empty*/; idx++) {
        item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception_no_close;
        if (done)
            break;
        index_val = JS_NewInt64(ctx, idx);
        args[0] = acc;
        args[1] = item;
        args[2] = index_val;
        ret = JS_Call(ctx, func, JS_UNDEFINED, countof(args), args);
        JS_FreeValue(ctx, item);
        JS_FreeValue(ctx, index_val);
        if (JS_IsException(ret))
            goto exception;
        JS_FreeValue(ctx, acc);
        acc = ret;
        index_val = JS_UNDEFINED;
        ret = JS_UNDEFINED;
        item = JS_UNDEFINED;
    }
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return acc;
 exception:
    JS_IteratorClose(ctx, this_val, TRUE);
 exception_no_close:
    JS_FreeValue(ctx, acc);
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, method);
    return JS_EXCEPTION;
}

static JSValue js_iterator_proto_toArray(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSValue item, method, result;
    int64_t idx;
    int done;

    result = JS_UNDEFINED;
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        return JS_EXCEPTION;
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        goto exception;
    for (idx = 0; /*empty*/; idx++) {
        item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(item))
            goto exception;
        if (done)
            break;
        if (JS_DefinePropertyValueInt64(ctx, result, idx, item,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto exception;
    }
    if (JS_SetProperty(ctx, result, JS_ATOM_length, JS_NewUint32(ctx, idx)) < 0)
        goto exception;
    JS_FreeValue(ctx, method);
    return result;
exception:
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, method);
    return JS_EXCEPTION;
}

static JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    return JS_DupValue(ctx, this_val);
}

static JSValue js_iterator_proto_get_toStringTag(JSContext *ctx, JSValueConst this_val)
{
    return JS_AtomToString(ctx, JS_ATOM_Iterator);
}

static JSValue js_iterator_proto_set_toStringTag(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
    int res;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_ITERATOR]))
        return JS_ThrowTypeError(ctx, "Cannot assign to read only property");
    res = JS_GetOwnProperty(ctx, NULL, this_val, JS_ATOM_Symbol_toStringTag);
    if (res < 0)
        return JS_EXCEPTION;
    if (res) {
        if (JS_SetProperty(ctx, this_val, JS_ATOM_Symbol_toStringTag, JS_DupValue(ctx, val)) < 0)
            return JS_EXCEPTION;
    } else {
        if (JS_DefinePropertyValue(ctx, this_val, JS_ATOM_Symbol_toStringTag, JS_DupValue(ctx, val), JS_PROP_C_W_E) < 0)
            return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

/* Iterator Helper */

void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorHelperData *it = p->u.iterator_helper_data;
    if (it) {
        JS_FreeValueRT(rt, it->obj);
        JS_FreeValueRT(rt, it->func);
        JS_FreeValueRT(rt, it->next);
        JS_FreeValueRT(rt, it->inner);
        js_free_rt(rt, it);
    }
}

void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorHelperData *it = p->u.iterator_helper_data;
    if (it) {
        JS_MarkValue(rt, it->obj, mark_func);
        JS_MarkValue(rt, it->func, mark_func);
        JS_MarkValue(rt, it->next, mark_func);
        JS_MarkValue(rt, it->inner, mark_func);
    }
}

static JSValue js_iterator_helper_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      int *pdone, int magic)
{
    JSIteratorHelperData *it;
    JSValue ret;

    *pdone = FALSE;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_HELPER);
    if (!it)
        return JS_EXCEPTION;
    if (it->executing)
        return JS_ThrowTypeError(ctx, "cannot invoke a running iterator");
    if (it->done) {
        *pdone = TRUE;
        return JS_UNDEFINED;
    }

    it->executing = 1;

    switch (it->kind) {
    case JS_ITERATOR_HELPER_KIND_DROP:
        {
            JSValue item, method;
            if (magic == GEN_MAGIC_NEXT) {
                method = JS_DupValue(ctx, it->next);
            } else {
                method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                if (JS_IsException(method))
                    goto fail;
            }
            while (it->count > 0) {
                it->count--;
                item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
                if (JS_IsException(item)) {
                    JS_FreeValue(ctx, method);
                    goto fail_no_close;
                }
                JS_FreeValue(ctx, item);
                if (magic == GEN_MAGIC_RETURN)
                    *pdone = TRUE;
                if (*pdone) {
                    JS_FreeValue(ctx, method);
                    ret = JS_UNDEFINED;
                    goto done;
                }
            }

            item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
            JS_FreeValue(ctx, method);
            if (JS_IsException(item))
                goto fail_no_close;
            ret = item;
            goto done;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FILTER:
        {
            JSValue item, method, selected, index_val;
            JSValueConst args[2];
            if (magic == GEN_MAGIC_NEXT) {
                method = JS_DupValue(ctx, it->next);
            } else {
                method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                if (JS_IsException(method))
                    goto fail;
            }
        filter_again:
            item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
            if (JS_IsException(item)) {
                JS_FreeValue(ctx, method);
                goto fail_no_close;
            }
            if (*pdone || magic == GEN_MAGIC_RETURN) {
                JS_FreeValue(ctx, method);
                ret = item;
                goto done;
            }
            index_val = JS_NewInt64(ctx, it->count++);
            args[0] = item;
            args[1] = index_val;
            selected = JS_Call(ctx, it->func, JS_UNDEFINED, countof(args), args);
            JS_FreeValue(ctx, index_val);
            if (JS_IsException(selected)) {
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, method);
                goto fail;
            }
            if (JS_ToBoolFree(ctx, selected)) {
                JS_FreeValue(ctx, method);
                ret = item;
                goto done;
            }
            JS_FreeValue(ctx, item);
            goto filter_again;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FLAT_MAP:
        {
            JSValue item, method, index_val, iter;
            JSValueConst args[2];
        flat_map_again:
            if (JS_IsUndefined(it->inner)) {
                if (magic == GEN_MAGIC_NEXT) {
                    method = JS_DupValue(ctx, it->next);
                } else {
                    method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                    if (JS_IsException(method))
                        goto fail;
                }
                item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
                JS_FreeValue(ctx, method);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (*pdone || magic == GEN_MAGIC_RETURN) {
                    ret = item;
                    goto done;
                }
                index_val = JS_NewInt64(ctx, it->count++);
                args[0] = item;
                args[1] = index_val;
                ret = JS_Call(ctx, it->func, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, item);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(ret))
                    goto fail;
                if (!JS_IsObject(ret)) {
                    JS_FreeValue(ctx, ret);
                    JS_ThrowTypeError(ctx, "not an object");
                    goto fail;
                }
                method = JS_GetProperty(ctx, ret, JS_ATOM_Symbol_iterator);
                if (JS_IsException(method)) {
                    JS_FreeValue(ctx, ret);
                    goto fail;
                }
                if (JS_IsNull(method) || JS_IsUndefined(method)) {
                    JS_FreeValue(ctx, method);
                    iter = ret;
                } else {
                    iter = JS_GetIterator2(ctx, ret, method);
                    JS_FreeValue(ctx, method);
                    JS_FreeValue(ctx, ret);
                    if (JS_IsException(iter))
                        goto fail;
                }

                it->inner = iter;
            }

            if (magic == GEN_MAGIC_NEXT)
                method = JS_GetProperty(ctx, it->inner, JS_ATOM_next);
            else
                method = JS_GetProperty(ctx, it->inner, JS_ATOM_return);
            if (JS_IsException(method)) {
            inner_fail:
                JS_IteratorClose(ctx, it->inner, FALSE);
                JS_FreeValue(ctx, it->inner);
                it->inner = JS_UNDEFINED;
                goto fail;
            }
            if (magic == GEN_MAGIC_RETURN && (JS_IsUndefined(method) || JS_IsNull(method))) {
                goto inner_end;
            } else {
                item = JS_IteratorNext(ctx, it->inner, method, 0, NULL, pdone);
                JS_FreeValue(ctx, method);
                if (JS_IsException(item))
                    goto inner_fail;
            }
            if (*pdone) {
            inner_end:
                *pdone = FALSE; // The outer iterator must continue.
                JS_IteratorClose(ctx, it->inner, FALSE);
                JS_FreeValue(ctx, it->inner);
                it->inner = JS_UNDEFINED;
                goto flat_map_again;
            }
            ret = item;
            goto done;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_MAP:
        {
            JSValue item, method, index_val;
            JSValueConst args[2];
            if (magic == GEN_MAGIC_NEXT) {
                method = JS_DupValue(ctx, it->next);
            } else {
                method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                if (JS_IsException(method))
                    goto fail;
            }
            item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
            JS_FreeValue(ctx, method);
            if (JS_IsException(item))
                goto fail_no_close;
            if (*pdone || magic == GEN_MAGIC_RETURN) {
                ret = item;
                goto done;
            }
            index_val = JS_NewInt64(ctx, it->count++);
            args[0] = item;
            args[1] = index_val;
            ret = JS_Call(ctx, it->func, JS_UNDEFINED, countof(args), args);
            JS_FreeValue(ctx, index_val);
            JS_FreeValue(ctx, item);
            if (JS_IsException(ret))
                goto fail;
            goto done;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_TAKE:
        {
            JSValue item, method;
            if (it->count > 0) {
                if (magic == GEN_MAGIC_NEXT) {
                    method = JS_DupValue(ctx, it->next);
                } else {
                    method = JS_GetProperty(ctx, it->obj, JS_ATOM_return);
                    if (JS_IsException(method))
                        goto fail;
                }
                it->count--;
                item = JS_IteratorNext(ctx, it->obj, method, 0, NULL, pdone);
                JS_FreeValue(ctx, method);
                if (JS_IsException(item))
                    goto fail_no_close;
                ret = item;
                goto done;
            }

            *pdone = TRUE;
            if (JS_IteratorClose(ctx, it->obj, FALSE))
                ret = JS_EXCEPTION;
            else
                ret = JS_UNDEFINED;
            goto done;
        }
        break;
    default:
        abort();
    }

 done:
    it->done = magic == GEN_MAGIC_NEXT ? *pdone : 1;
    it->executing = 0;
    return ret;
 fail:
    /* close the iterator object, preserving pending exception */
    JS_IteratorClose(ctx, it->obj, TRUE);
 fail_no_close:
    ret = JS_EXCEPTION;
    goto done;
}

static const JSCFunctionListEntry js_iterator_funcs[] = {
    JS_CFUNC_DEF("concat", 0, js_iterator_concat ),
    JS_CFUNC_DEF("from", 1, js_iterator_from ),
};

static const JSCFunctionListEntry js_iterator_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("drop", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_DROP ),
    JS_CFUNC_MAGIC_DEF("filter", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_FILTER ),
    JS_CFUNC_MAGIC_DEF("flatMap", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_FLAT_MAP ),
    JS_CFUNC_MAGIC_DEF("map", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_MAP ),
    JS_CFUNC_MAGIC_DEF("take", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_TAKE ),
    JS_CFUNC_MAGIC_DEF("every", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_EVERY ),
    JS_CFUNC_MAGIC_DEF("find", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_FIND),
    JS_CFUNC_MAGIC_DEF("forEach", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_FOR_EACH ),
    JS_CFUNC_MAGIC_DEF("some", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_SOME ),
    JS_CFUNC_DEF("reduce", 1, js_iterator_proto_reduce ),
    JS_CFUNC_DEF("toArray", 0, js_iterator_proto_toArray ),
    JS_CFUNC_DEF("[Symbol.iterator]", 0, js_iterator_proto_iterator ),
    JS_CGETSET_DEF("[Symbol.toStringTag]", js_iterator_proto_get_toStringTag, js_iterator_proto_set_toStringTag),
};

static const JSCFunctionListEntry js_iterator_helper_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_iterator_helper_next, GEN_MAGIC_NEXT ),
    JS_ITERATOR_NEXT_DEF("return", 0, js_iterator_helper_next, GEN_MAGIC_RETURN ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Iterator Helper", JS_PROP_CONFIGURABLE ),
};


int js_iterator_init(JSContext *ctx)
{
    JSValue obj1, obj2;

    /* Iterator */
    obj2 = JS_NewCConstructor(ctx, JS_CLASS_ITERATOR, "Iterator",
                                     js_iterator_constructor, 0, JS_CFUNC_constructor_or_func, 0,
                                     JS_UNDEFINED,
                                     js_iterator_funcs, countof(js_iterator_funcs),
                                     js_iterator_proto_funcs, countof(js_iterator_proto_funcs),
                                     0);
    if (JS_IsException(obj2))
        return -1;
    // quirk: Iterator.prototype.constructor is an accessor property
    // TODO(bnoordhuis) mildly inefficient because JS_NewGlobalCConstructor
    // first creates a .constructor value property that we then replace with
    // an accessor
    obj1 = JS_NewCFunctionData(ctx, js_iterator_constructor_getset,
                               0, 0, 1, (JSValueConst *)&obj2);
    if (JS_IsException(obj1)) {
        JS_FreeValue(ctx, obj2);
        return -1;
    }
    if (JS_DefineProperty(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                          JS_ATOM_constructor, JS_UNDEFINED,
                          obj1, obj1,
                          JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_CONFIGURABLE) < 0) {
        JS_FreeValue(ctx, obj2);
        JS_FreeValue(ctx, obj1);
        return -1;
    }
    JS_FreeValue(ctx, obj1);
    ctx->iterator_ctor = obj2;
    
    ctx->class_proto[JS_CLASS_ITERATOR_CONCAT] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR], 
                              js_iterator_concat_proto_funcs,
                              countof(js_iterator_concat_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ITERATOR_CONCAT]))
        return -1;
    ctx->class_proto[JS_CLASS_ITERATOR_HELPER] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR], 
                              js_iterator_helper_proto_funcs,
                              countof(js_iterator_helper_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ITERATOR_HELPER]))
        return -1;
                       
    ctx->class_proto[JS_CLASS_ITERATOR_WRAP] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR], 
                              js_iterator_wrap_proto_funcs,
                              countof(js_iterator_wrap_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ITERATOR_WRAP]))
        return -1;

    return 0;
}

/* Promise */

typedef struct JSPromiseData {
    JSPromiseStateEnum promise_state;
    /* 0=fulfill, 1=reject, list of JSPromiseReactionData.link */
    struct list_head promise_reactions[2];
    BOOL is_handled; /* Note: only useful to debug */
    JSValue promise_result;
} JSPromiseData;

typedef struct JSPromiseFunctionDataResolved {
    int ref_count;
    BOOL already_resolved;
} JSPromiseFunctionDataResolved;

typedef struct JSPromiseFunctionData {
    JSValue promise;
    JSPromiseFunctionDataResolved *presolved;
} JSPromiseFunctionData;

typedef struct JSPromiseReactionData {
    struct list_head link; /* not used in promise_reaction_job */
    JSValue resolving_funcs[2];
    JSValue handler;
} JSPromiseReactionData;

JSPromiseStateEnum JS_PromiseState(JSContext *ctx, JSValue promise)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    if (!s)
        return -1;
    return s->promise_state;
}

JSValue JS_PromiseResult(JSContext *ctx, JSValue promise)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    if (!s)
        return JS_UNDEFINED;
    return JS_DupValue(ctx, s->promise_result);
}

static int js_create_resolving_functions(JSContext *ctx, JSValue *args,
                                         JSValueConst promise);

static void promise_reaction_data_free(JSRuntime *rt,
                                       JSPromiseReactionData *rd)
{
    JS_FreeValueRT(rt, rd->resolving_funcs[0]);
    JS_FreeValueRT(rt, rd->resolving_funcs[1]);
    JS_FreeValueRT(rt, rd->handler);
    js_free_rt(rt, rd);
}

static JSValue promise_reaction_job(JSContext *ctx, int argc,
                                    JSValueConst *argv)
{
    JSValueConst handler, arg, func;
    JSValue res, res2;
    BOOL is_reject;

    assert(argc == 5);
    handler = argv[2];
    is_reject = JS_ToBool(ctx, argv[3]);
    arg = argv[4];
#ifdef DUMP_PROMISE
    printf("promise_reaction_job: is_reject=%d\n", is_reject);
#endif

    if (JS_IsUndefined(handler)) {
        if (is_reject) {
            res = JS_Throw(ctx, JS_DupValue(ctx, arg));
        } else {
            res = JS_DupValue(ctx, arg);
        }
    } else {
        res = JS_Call(ctx, handler, JS_UNDEFINED, 1, &arg);
    }
    is_reject = JS_IsException(res);
    if (is_reject)
        res = JS_GetException(ctx);
    func = argv[is_reject];
    /* as an extension, we support undefined as value to avoid
       creating a dummy promise in the 'await' implementation of async
       functions */
    if (!JS_IsUndefined(func)) {
        res2 = JS_Call(ctx, func, JS_UNDEFINED,
                       1, (JSValueConst *)&res);
    } else {
        res2 = JS_UNDEFINED;
    }
    JS_FreeValue(ctx, res);

    return res2;
}

void JS_SetHostPromiseRejectionTracker(JSRuntime *rt,
                                       JSHostPromiseRejectionTracker *cb,
                                       void *opaque)
{
    rt->host_promise_rejection_tracker = cb;
    rt->host_promise_rejection_tracker_opaque = opaque;
}

static void fulfill_or_reject_promise(JSContext *ctx, JSValueConst promise,
                                      JSValueConst value, BOOL is_reject)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    struct list_head *el, *el1;
    JSPromiseReactionData *rd;
    JSValueConst args[5];

    if (!s || s->promise_state != JS_PROMISE_PENDING)
        return; /* should never happen */
    set_value(ctx, &s->promise_result, JS_DupValue(ctx, value));
    s->promise_state = JS_PROMISE_FULFILLED + is_reject;
#ifdef DUMP_PROMISE
    printf("fulfill_or_reject_promise: is_reject=%d\n", is_reject);
#endif
    if (s->promise_state == JS_PROMISE_REJECTED && !s->is_handled) {
        JSRuntime *rt = ctx->rt;
        if (rt->host_promise_rejection_tracker) {
            rt->host_promise_rejection_tracker(ctx, promise, value, FALSE,
                                               rt->host_promise_rejection_tracker_opaque);
        }
    }

    list_for_each_safe(el, el1, &s->promise_reactions[is_reject]) {
        rd = list_entry(el, JSPromiseReactionData, link);
        args[0] = rd->resolving_funcs[0];
        args[1] = rd->resolving_funcs[1];
        args[2] = rd->handler;
        args[3] = JS_NewBool(ctx, is_reject);
        args[4] = value;
        JS_EnqueueJob(ctx, promise_reaction_job, 5, args);
        list_del(&rd->link);
        promise_reaction_data_free(ctx->rt, rd);
    }

    list_for_each_safe(el, el1, &s->promise_reactions[1 - is_reject]) {
        rd = list_entry(el, JSPromiseReactionData, link);
        list_del(&rd->link);
        promise_reaction_data_free(ctx->rt, rd);
    }
}

static void reject_promise(JSContext *ctx, JSValueConst promise,
                           JSValueConst value)
{
    fulfill_or_reject_promise(ctx, promise, value, TRUE);
}

static JSValue js_promise_resolve_thenable_job(JSContext *ctx,
                                               int argc, JSValueConst *argv)
{
    JSValueConst promise, thenable, then;
    JSValue args[2], res;

#ifdef DUMP_PROMISE
    printf("js_promise_resolve_thenable_job\n");
#endif
    assert(argc == 3);
    promise = argv[0];
    thenable = argv[1];
    then = argv[2];
    if (js_create_resolving_functions(ctx, args, promise) < 0)
        return JS_EXCEPTION;
    res = JS_Call(ctx, then, thenable, 2, (JSValueConst *)args);
    if (JS_IsException(res)) {
        JSValue error = JS_GetException(ctx);
        res = JS_Call(ctx, args[1], JS_UNDEFINED, 1, (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
    }
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    return res;
}

static void js_promise_resolve_function_free_resolved(JSRuntime *rt,
                                                      JSPromiseFunctionDataResolved *sr)
{
    if (--sr->ref_count == 0) {
        js_free_rt(rt, sr);
    }
}

static int js_create_resolving_functions(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst promise)

{
    JSValue obj;
    JSPromiseFunctionData *s;
    JSPromiseFunctionDataResolved *sr;
    int i, ret;

    sr = js_malloc(ctx, sizeof(*sr));
    if (!sr)
        return -1;
    sr->ref_count = 1;
    sr->already_resolved = FALSE; /* must be shared between the two functions */
    ret = 0;
    for(i = 0; i < 2; i++) {
        obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                     JS_CLASS_PROMISE_RESOLVE_FUNCTION + i);
        if (JS_IsException(obj))
            goto fail;
        s = js_malloc(ctx, sizeof(*s));
        if (!s) {
            JS_FreeValue(ctx, obj);
        fail:

            if (i != 0)
                JS_FreeValue(ctx, resolving_funcs[0]);
            ret = -1;
            break;
        }
        sr->ref_count++;
        s->presolved = sr;
        s->promise = JS_DupValue(ctx, promise);
        JS_SetOpaque(obj, s);
        js_function_set_properties(ctx, obj, JS_ATOM_empty_string, 1);
        resolving_funcs[i] = obj;
    }
    js_promise_resolve_function_free_resolved(ctx->rt, sr);
    return ret;
}

static void js_promise_resolve_function_finalizer(JSRuntime *rt, JSValue val)
{
    JSPromiseFunctionData *s = JS_VALUE_GET_OBJ(val)->u.promise_function_data;
    if (s) {
        js_promise_resolve_function_free_resolved(rt, s->presolved);
        JS_FreeValueRT(rt, s->promise);
        js_free_rt(rt, s);
    }
}

static void js_promise_resolve_function_mark(JSRuntime *rt, JSValueConst val,
                                             JS_MarkFunc *mark_func)
{
    JSPromiseFunctionData *s = JS_VALUE_GET_OBJ(val)->u.promise_function_data;
    if (s) {
        JS_MarkValue(rt, s->promise, mark_func);
    }
}

static JSValue js_promise_resolve_function_call(JSContext *ctx,
                                                JSValueConst func_obj,
                                                JSValueConst this_val,
                                                int argc, JSValueConst *argv,
                                                int flags)
{
    JSObject *p = JS_VALUE_GET_OBJ(func_obj);
    JSPromiseFunctionData *s;
    JSValueConst resolution, args[3];
    JSValue then;
    BOOL is_reject;

    s = p->u.promise_function_data;
    if (!s || s->presolved->already_resolved)
        return JS_UNDEFINED;
    s->presolved->already_resolved = TRUE;
    is_reject = p->class_id - JS_CLASS_PROMISE_RESOLVE_FUNCTION;
    if (argc > 0)
        resolution = argv[0];
    else
        resolution = JS_UNDEFINED;
#ifdef DUMP_PROMISE
    printf("js_promise_resolving_function_call: is_reject=%d ", is_reject);
    JS_DumpValue(ctx, "resolution", resolution);
    printf("\n");
#endif
    if (is_reject || !JS_IsObject(resolution)) {
        goto done;
    } else if (js_same_value(ctx, resolution, s->promise)) {
        JS_ThrowTypeError(ctx, "promise self resolution");
        goto fail_reject;
    }
    then = JS_GetProperty(ctx, resolution, JS_ATOM_then);
    if (JS_IsException(then)) {
        JSValue error;
    fail_reject:
        error = JS_GetException(ctx);
        reject_promise(ctx, s->promise, error);
        JS_FreeValue(ctx, error);
    } else if (!JS_IsFunction(ctx, then)) {
        JS_FreeValue(ctx, then);
    done:
        fulfill_or_reject_promise(ctx, s->promise, resolution, is_reject);
    } else {
        args[0] = s->promise;
        args[1] = resolution;
        args[2] = then;
        JS_EnqueueJob(ctx, js_promise_resolve_thenable_job, 3, args);
        JS_FreeValue(ctx, then);
    }
    return JS_UNDEFINED;
}

static void js_promise_finalizer(JSRuntime *rt, JSValue val)
{
    JSPromiseData *s = JS_GetOpaque(val, JS_CLASS_PROMISE);
    struct list_head *el, *el1;
    int i;

    if (!s)
        return;
    for(i = 0; i < 2; i++) {
        list_for_each_safe(el, el1, &s->promise_reactions[i]) {
            JSPromiseReactionData *rd =
                list_entry(el, JSPromiseReactionData, link);
            promise_reaction_data_free(rt, rd);
        }
    }
    JS_FreeValueRT(rt, s->promise_result);
    js_free_rt(rt, s);
}

static void js_promise_mark(JSRuntime *rt, JSValueConst val,
                            JS_MarkFunc *mark_func)
{
    JSPromiseData *s = JS_GetOpaque(val, JS_CLASS_PROMISE);
    struct list_head *el;
    int i;

    if (!s)
        return;
    for(i = 0; i < 2; i++) {
        list_for_each(el, &s->promise_reactions[i]) {
            JSPromiseReactionData *rd =
                list_entry(el, JSPromiseReactionData, link);
            JS_MarkValue(rt, rd->resolving_funcs[0], mark_func);
            JS_MarkValue(rt, rd->resolving_funcs[1], mark_func);
            JS_MarkValue(rt, rd->handler, mark_func);
        }
    }
    JS_MarkValue(rt, s->promise_result, mark_func);
}

static JSValue js_promise_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSValueConst executor;
    JSValue obj;
    JSPromiseData *s;
    JSValue args[2], ret;
    int i;

    executor = argv[0];
    if (check_function(ctx, executor))
        return JS_EXCEPTION;
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_PROMISE);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    s->promise_state = JS_PROMISE_PENDING;
    s->is_handled = FALSE;
    for(i = 0; i < 2; i++)
        init_list_head(&s->promise_reactions[i]);
    s->promise_result = JS_UNDEFINED;
    JS_SetOpaque(obj, s);
    if (js_create_resolving_functions(ctx, args, obj))
        goto fail;
    ret = JS_Call(ctx, executor, JS_UNDEFINED, 2, (JSValueConst *)args);
    if (JS_IsException(ret)) {
        JSValue ret2, error;
        error = JS_GetException(ctx);
        ret2 = JS_Call(ctx, args[1], JS_UNDEFINED, 1, (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
        if (JS_IsException(ret2))
            goto fail1;
        JS_FreeValue(ctx, ret2);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    return obj;
 fail1:
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_promise_executor(JSContext *ctx,
                                   JSValueConst this_val,
                                   int argc, JSValueConst *argv,
                                   int magic, JSValue *func_data)
{
    int i;

    for(i = 0; i < 2; i++) {
        if (!JS_IsUndefined(func_data[i]))
            return JS_ThrowTypeError(ctx, "resolving function already set");
        func_data[i] = JS_DupValue(ctx, argv[i]);
    }
    return JS_UNDEFINED;
}

static JSValue js_promise_executor_new(JSContext *ctx)
{
    JSValueConst func_data[2];

    func_data[0] = JS_UNDEFINED;
    func_data[1] = JS_UNDEFINED;
    return JS_NewCFunctionData(ctx, js_promise_executor, 2,
                               0, 2, func_data);
}

static JSValue js_new_promise_capability(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst ctor)
{
    JSValue executor, result_promise;
    JSCFunctionDataRecord *s;
    int i;

    executor = js_promise_executor_new(ctx);
    if (JS_IsException(executor))
        return executor;

    if (JS_IsUndefined(ctor)) {
        result_promise = js_promise_constructor(ctx, ctor, 1,
                                                (JSValueConst *)&executor);
    } else {
        result_promise = JS_CallConstructor(ctx, ctor, 1,
                                            (JSValueConst *)&executor);
    }
    if (JS_IsException(result_promise))
        goto fail;
    s = JS_GetOpaque(executor, JS_CLASS_C_FUNCTION_DATA);
    for(i = 0; i < 2; i++) {
        if (check_function(ctx, s->data[i]))
            goto fail;
    }
    for(i = 0; i < 2; i++)
        resolving_funcs[i] = JS_DupValue(ctx, s->data[i]);
    JS_FreeValue(ctx, executor);
    return result_promise;
 fail:
    JS_FreeValue(ctx, executor);
    JS_FreeValue(ctx, result_promise);
    return JS_EXCEPTION;
}

JSValue JS_NewPromiseCapability(JSContext *ctx, JSValue *resolving_funcs)
{
    return js_new_promise_capability(ctx, resolving_funcs, JS_UNDEFINED);
}

JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSValue result_promise, resolving_funcs[2], ret;
    BOOL is_reject = magic;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (!is_reject && JS_GetOpaque(argv[0], JS_CLASS_PROMISE)) {
        JSValue ctor;
        BOOL is_same;
        ctor = JS_GetProperty(ctx, argv[0], JS_ATOM_constructor);
        if (JS_IsException(ctor))
            return ctor;
        is_same = js_same_value(ctx, ctor, this_val);
        JS_FreeValue(ctx, ctor);
        if (is_same)
            return JS_DupValue(ctx, argv[0]);
    }
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    ret = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED, 1, argv);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, result_promise);
        return ret;
    }
    JS_FreeValue(ctx, ret);
    return result_promise;
}

static JSValue js_promise_withResolvers(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValue result_promise, resolving_funcs[2], obj;
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        goto exception;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_promise, result_promise,
                               JS_PROP_C_W_E) < 0) {
        goto exception;
    }
    result_promise = JS_UNDEFINED;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_resolve, resolving_funcs[0],
                               JS_PROP_C_W_E) < 0) {
        goto exception;
    }
    resolving_funcs[0] = JS_UNDEFINED;
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_reject, resolving_funcs[1],
                               JS_PROP_C_W_E) < 0) {
        goto exception;
    }
    return obj;
exception:
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, result_promise);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_promise_try(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue result_promise, resolving_funcs[2], ret, ret2;
    BOOL is_reject = 0;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    ret = JS_Call(ctx, argv[0], JS_UNDEFINED, argc - 1, argv + 1);
    if (JS_IsException(ret)) {
        is_reject = 1;
        ret = JS_GetException(ctx);
    }
    ret2 = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED, 1, (JSValueConst *)&ret);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, ret);
    if (JS_IsException(ret2)) {
        JS_FreeValue(ctx, result_promise);
        return ret2;
    }
    JS_FreeValue(ctx, ret2);
    return result_promise;
}

static __exception int remainingElementsCount_add(JSContext *ctx,
                                                  JSValueConst resolve_element_env,
                                                  int addend)
{
    JSValue val;
    int remainingElementsCount;

    val = JS_GetPropertyUint32(ctx, resolve_element_env, 0);
    if (JS_IsException(val))
        return -1;
    if (JS_ToInt32Free(ctx, &remainingElementsCount, val))
        return -1;
    remainingElementsCount += addend;
    if (JS_SetPropertyUint32(ctx, resolve_element_env, 0,
                             JS_NewInt32(ctx, remainingElementsCount)) < 0)
        return -1;
    return (remainingElementsCount == 0);
}

#define PROMISE_MAGIC_all        0
#define PROMISE_MAGIC_allSettled 1
#define PROMISE_MAGIC_any        2

static JSValue js_promise_all_resolve_element(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data)
{
    int resolve_type = magic & 3;
    int is_reject = magic & 4;
    BOOL alreadyCalled = JS_ToBool(ctx, func_data[0]);
    JSValueConst values = func_data[2];
    JSValueConst resolve = func_data[3];
    JSValueConst resolve_element_env = func_data[4];
    JSValue ret, obj;
    int is_zero, index;

    if (JS_ToInt32(ctx, &index, func_data[1]))
        return JS_EXCEPTION;
    if (alreadyCalled)
        return JS_UNDEFINED;
    func_data[0] = JS_NewBool(ctx, TRUE);

    if (resolve_type == PROMISE_MAGIC_allSettled) {
        JSValue str;

        obj = JS_NewObject(ctx);
        if (JS_IsException(obj))
            return JS_EXCEPTION;
        str = js_new_string8(ctx, is_reject ? "rejected" : "fulfilled");
        if (JS_IsException(str))
            goto fail1;
        if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_status,
                                   str,
                                   JS_PROP_C_W_E) < 0)
            goto fail1;
        if (JS_DefinePropertyValue(ctx, obj,
                                   is_reject ? JS_ATOM_reason : JS_ATOM_value,
                                   JS_DupValue(ctx, argv[0]),
                                   JS_PROP_C_W_E) < 0) {
        fail1:
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
    } else {
        obj = JS_DupValue(ctx, argv[0]);
    }
    if (JS_DefinePropertyValueUint32(ctx, values, index,
                                     obj, JS_PROP_C_W_E) < 0)
        return JS_EXCEPTION;

    is_zero = remainingElementsCount_add(ctx, resolve_element_env, -1);
    if (is_zero < 0)
        return JS_EXCEPTION;
    if (is_zero) {
        if (resolve_type == PROMISE_MAGIC_any) {
            JSValue error;
            error = js_aggregate_error_constructor(ctx, values);
            if (JS_IsException(error))
                return JS_EXCEPTION;
            ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, (JSValueConst *)&error);
            JS_FreeValue(ctx, error);
        } else {
            ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, (JSValueConst *)&values);
        }
        if (JS_IsException(ret))
            return ret;
        JS_FreeValue(ctx, ret);
    }
    return JS_UNDEFINED;
}

/* magic = 0: Promise.all 1: Promise.allSettled */
static JSValue js_promise_all(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    JSValue result_promise, resolving_funcs[2], item, next_promise, ret;
    JSValue next_method = JS_UNDEFINED, values = JS_UNDEFINED;
    JSValue resolve_element_env = JS_UNDEFINED, resolve_element, reject_element;
    JSValue promise_resolve = JS_UNDEFINED, iter = JS_UNDEFINED;
    JSValueConst then_args[2], resolve_element_data[5];
    BOOL done;
    int index, is_zero, is_promise_any = (magic == PROMISE_MAGIC_any);

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    promise_resolve = JS_GetProperty(ctx, this_val, JS_ATOM_resolve);
    if (JS_IsException(promise_resolve) ||
        check_function(ctx, promise_resolve))
        goto fail_reject;
    iter = JS_GetIterator(ctx, argv[0], FALSE);
    if (JS_IsException(iter)) {
        JSValue error;
    fail_reject:
        error = JS_GetException(ctx);
        ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                       (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
        if (JS_IsException(ret))
            goto fail;
        JS_FreeValue(ctx, ret);
    } else {
        next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto fail_reject;
        values = JS_NewArray(ctx);
        if (JS_IsException(values))
            goto fail_reject;
        resolve_element_env = JS_NewArray(ctx);
        if (JS_IsException(resolve_element_env))
            goto fail_reject;
        /* remainingElementsCount field */
        if (JS_DefinePropertyValueUint32(ctx, resolve_element_env, 0,
                                         JS_NewInt32(ctx, 1),
                                         JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_WRITABLE) < 0)
            goto fail_reject;

        index = 0;
        for(;;) {
            /* XXX: conformance: should close the iterator if error on 'done'
               access, but not on 'value' access */
            item = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
            if (JS_IsException(item))
                goto fail_reject;
            if (done)
                break;
            next_promise = JS_Call(ctx, promise_resolve,
                                   this_val, 1, (JSValueConst *)&item);
            JS_FreeValue(ctx, item);
            if (JS_IsException(next_promise)) {
            fail_reject1:
                JS_IteratorClose(ctx, iter, TRUE);
                goto fail_reject;
            }
            resolve_element_data[0] = JS_NewBool(ctx, FALSE);
            resolve_element_data[1] = (JSValueConst)JS_NewInt32(ctx, index);
            resolve_element_data[2] = values;
            resolve_element_data[3] = resolving_funcs[is_promise_any];
            resolve_element_data[4] = resolve_element_env;
            resolve_element =
                JS_NewCFunctionData(ctx, js_promise_all_resolve_element, 1,
                                    magic, 5, resolve_element_data);
            if (JS_IsException(resolve_element)) {
                JS_FreeValue(ctx, next_promise);
                goto fail_reject1;
            }

            if (magic == PROMISE_MAGIC_allSettled) {
                reject_element =
                    JS_NewCFunctionData(ctx, js_promise_all_resolve_element, 1,
                                        magic | 4, 5, resolve_element_data);
                if (JS_IsException(reject_element)) {
                    JS_FreeValue(ctx, next_promise);
                    goto fail_reject1;
                }
            } else if (magic == PROMISE_MAGIC_any) {
                if (JS_DefinePropertyValueUint32(ctx, values, index,
                                                 JS_UNDEFINED, JS_PROP_C_W_E) < 0)
                    goto fail_reject1;
                reject_element = resolve_element;
                resolve_element = JS_DupValue(ctx, resolving_funcs[0]);
            } else {
                reject_element = JS_DupValue(ctx, resolving_funcs[1]);
            }

            if (remainingElementsCount_add(ctx, resolve_element_env, 1) < 0) {
                JS_FreeValue(ctx, next_promise);
                JS_FreeValue(ctx, resolve_element);
                JS_FreeValue(ctx, reject_element);
                goto fail_reject1;
            }

            then_args[0] = resolve_element;
            then_args[1] = reject_element;
            ret = JS_InvokeFree(ctx, next_promise, JS_ATOM_then, 2, then_args);
            JS_FreeValue(ctx, resolve_element);
            JS_FreeValue(ctx, reject_element);
            if (check_exception_free(ctx, ret))
                goto fail_reject1;
            index++;
        }

        is_zero = remainingElementsCount_add(ctx, resolve_element_env, -1);
        if (is_zero < 0)
            goto fail_reject;
        if (is_zero) {
            if (magic == PROMISE_MAGIC_any) {
                JSValue error;
                error = js_aggregate_error_constructor(ctx, values);
                if (JS_IsException(error))
                    goto fail_reject;
                JS_FreeValue(ctx, values);
                values = error;
            }
            ret = JS_Call(ctx, resolving_funcs[is_promise_any], JS_UNDEFINED,
                          1, (JSValueConst *)&values);
            if (check_exception_free(ctx, ret))
                goto fail_reject;
        }
    }
 done:
    JS_FreeValue(ctx, promise_resolve);
    JS_FreeValue(ctx, resolve_element_env);
    JS_FreeValue(ctx, values);
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return result_promise;
 fail:
    JS_FreeValue(ctx, result_promise);
    result_promise = JS_EXCEPTION;
    goto done;
}

static JSValue js_promise_race(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSValue result_promise, resolving_funcs[2], item, next_promise, ret;
    JSValue next_method = JS_UNDEFINED, iter = JS_UNDEFINED;
    JSValue promise_resolve = JS_UNDEFINED;
    BOOL done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    result_promise = js_new_promise_capability(ctx, resolving_funcs, this_val);
    if (JS_IsException(result_promise))
        return result_promise;
    promise_resolve = JS_GetProperty(ctx, this_val, JS_ATOM_resolve);
    if (JS_IsException(promise_resolve) ||
        check_function(ctx, promise_resolve))
        goto fail_reject;
    iter = JS_GetIterator(ctx, argv[0], FALSE);
    if (JS_IsException(iter)) {
        JSValue error;
    fail_reject:
        error = JS_GetException(ctx);
        ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                       (JSValueConst *)&error);
        JS_FreeValue(ctx, error);
        if (JS_IsException(ret))
            goto fail;
        JS_FreeValue(ctx, ret);
    } else {
        next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
        if (JS_IsException(next_method))
            goto fail_reject;

        for(;;) {
            /* XXX: conformance: should close the iterator if error on 'done'
               access, but not on 'value' access */
            item = JS_IteratorNext(ctx, iter, next_method, 0, NULL, &done);
            if (JS_IsException(item))
                goto fail_reject;
            if (done)
                break;
            next_promise = JS_Call(ctx, promise_resolve,
                                   this_val, 1, (JSValueConst *)&item);
            JS_FreeValue(ctx, item);
            if (JS_IsException(next_promise)) {
            fail_reject1:
                JS_IteratorClose(ctx, iter, TRUE);
                goto fail_reject;
            }
            ret = JS_InvokeFree(ctx, next_promise, JS_ATOM_then, 2,
                                (JSValueConst *)resolving_funcs);
            if (check_exception_free(ctx, ret))
                goto fail_reject1;
        }
    }
 done:
    JS_FreeValue(ctx, promise_resolve);
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return result_promise;
 fail:
    //JS_FreeValue(ctx, next_method); // why not???
    JS_FreeValue(ctx, result_promise);
    result_promise = JS_EXCEPTION;
    goto done;
}

__exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs)
{
    JSPromiseData *s = JS_GetOpaque(promise, JS_CLASS_PROMISE);
    JSPromiseReactionData *rd_array[2], *rd;
    int i, j;

    rd_array[0] = NULL;
    rd_array[1] = NULL;
    for(i = 0; i < 2; i++) {
        JSValueConst handler;
        rd = js_mallocz(ctx, sizeof(*rd));
        if (!rd) {
            if (i == 1)
                promise_reaction_data_free(ctx->rt, rd_array[0]);
            return -1;
        }
        for(j = 0; j < 2; j++)
            rd->resolving_funcs[j] = JS_DupValue(ctx, cap_resolving_funcs[j]);
        handler = resolve_reject[i];
        if (!JS_IsFunction(ctx, handler))
            handler = JS_UNDEFINED;
        rd->handler = JS_DupValue(ctx, handler);
        rd_array[i] = rd;
    }

    if (s->promise_state == JS_PROMISE_PENDING) {
        for(i = 0; i < 2; i++)
            list_add_tail(&rd_array[i]->link, &s->promise_reactions[i]);
    } else {
        JSValueConst args[5];
        if (s->promise_state == JS_PROMISE_REJECTED && !s->is_handled) {
            JSRuntime *rt = ctx->rt;
            if (rt->host_promise_rejection_tracker) {
                rt->host_promise_rejection_tracker(ctx, promise, s->promise_result,
                                                   TRUE, rt->host_promise_rejection_tracker_opaque);
            }
        }
        i = s->promise_state - JS_PROMISE_FULFILLED;
        rd = rd_array[i];
        args[0] = rd->resolving_funcs[0];
        args[1] = rd->resolving_funcs[1];
        args[2] = rd->handler;
        args[3] = JS_NewBool(ctx, i);
        args[4] = s->promise_result;
        JS_EnqueueJob(ctx, promise_reaction_job, 5, args);
        for(i = 0; i < 2; i++)
            promise_reaction_data_free(ctx->rt, rd_array[i]);
    }
    s->is_handled = TRUE;
    return 0;
}

JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSValue ctor, result_promise, resolving_funcs[2];
    JSPromiseData *s;
    int i, ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_PROMISE);
    if (!s)
        return JS_EXCEPTION;

    ctor = JS_SpeciesConstructor(ctx, this_val, JS_UNDEFINED);
    if (JS_IsException(ctor))
        return ctor;
    result_promise = js_new_promise_capability(ctx, resolving_funcs, ctor);
    JS_FreeValue(ctx, ctor);
    if (JS_IsException(result_promise))
        return result_promise;
    ret = perform_promise_then(ctx, this_val, argv,
                               (JSValueConst *)resolving_funcs);
    for(i = 0; i < 2; i++)
        JS_FreeValue(ctx, resolving_funcs[i]);
    if (ret) {
        JS_FreeValue(ctx, result_promise);
        return JS_EXCEPTION;
    }
    return result_promise;
}

static JSValue js_promise_catch(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValueConst args[2];
    args[0] = JS_UNDEFINED;
    args[1] = argv[0];
    return JS_Invoke(ctx, this_val, JS_ATOM_then, 2, args);
}

static JSValue js_promise_finally_value_thunk(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic, JSValue *func_data)
{
    return JS_DupValue(ctx, func_data[0]);
}

static JSValue js_promise_finally_thrower(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int magic, JSValue *func_data)
{
    return JS_Throw(ctx, JS_DupValue(ctx, func_data[0]));
}

static JSValue js_promise_then_finally_func(JSContext *ctx, JSValueConst this_val,
                                            int argc, JSValueConst *argv,
                                            int magic, JSValue *func_data)
{
    JSValueConst ctor = func_data[0];
    JSValueConst onFinally = func_data[1];
    JSValue res, promise, ret, then_func;

    res = JS_Call(ctx, onFinally, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(res))
        return res;
    promise = js_promise_resolve(ctx, ctor, 1, (JSValueConst *)&res, 0);
    JS_FreeValue(ctx, res);
    if (JS_IsException(promise))
        return promise;
    if (magic == 0) {
        then_func = JS_NewCFunctionData(ctx, js_promise_finally_value_thunk, 0,
                                        0, 1, argv);
    } else {
        then_func = JS_NewCFunctionData(ctx, js_promise_finally_thrower, 0,
                                        0, 1, argv);
    }
    if (JS_IsException(then_func)) {
        JS_FreeValue(ctx, promise);
        return then_func;
    }
    ret = JS_InvokeFree(ctx, promise, JS_ATOM_then, 1, (JSValueConst *)&then_func);
    JS_FreeValue(ctx, then_func);
    return ret;
}

static JSValue js_promise_finally(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValueConst onFinally = argv[0];
    JSValue ctor, ret;
    JSValue then_funcs[2];
    JSValueConst func_data[2];
    int i;

    ctor = JS_SpeciesConstructor(ctx, this_val, JS_UNDEFINED);
    if (JS_IsException(ctor))
        return ctor;
    if (!JS_IsFunction(ctx, onFinally)) {
        then_funcs[0] = JS_DupValue(ctx, onFinally);
        then_funcs[1] = JS_DupValue(ctx, onFinally);
    } else {
        func_data[0] = ctor;
        func_data[1] = onFinally;
        for(i = 0; i < 2; i++) {
            then_funcs[i] = JS_NewCFunctionData(ctx, js_promise_then_finally_func, 1, i, 2, func_data);
            if (JS_IsException(then_funcs[i])) {
                if (i == 1)
                    JS_FreeValue(ctx, then_funcs[0]);
                JS_FreeValue(ctx, ctor);
                return JS_EXCEPTION;
            }
        }
    }
    JS_FreeValue(ctx, ctor);
    ret = JS_Invoke(ctx, this_val, JS_ATOM_then, 2, (JSValueConst *)then_funcs);
    JS_FreeValue(ctx, then_funcs[0]);
    JS_FreeValue(ctx, then_funcs[1]);
    return ret;
}

static const JSCFunctionListEntry js_promise_funcs[] = {
    JS_CFUNC_MAGIC_DEF("resolve", 1, js_promise_resolve, 0 ),
    JS_CFUNC_MAGIC_DEF("reject", 1, js_promise_resolve, 1 ),
    JS_CFUNC_MAGIC_DEF("all", 1, js_promise_all, PROMISE_MAGIC_all ),
    JS_CFUNC_MAGIC_DEF("allSettled", 1, js_promise_all, PROMISE_MAGIC_allSettled ),
    JS_CFUNC_MAGIC_DEF("any", 1, js_promise_all, PROMISE_MAGIC_any ),
    JS_CFUNC_DEF("try", 1, js_promise_try ),
    JS_CFUNC_DEF("race", 1, js_promise_race ),
    JS_CFUNC_DEF("withResolvers", 0, js_promise_withResolvers ),
    JS_CGETSET_DEF("[Symbol.species]", js_get_this, NULL),
};

static const JSCFunctionListEntry js_promise_proto_funcs[] = {
    JS_CFUNC_DEF("then", 2, js_promise_then ),
    JS_CFUNC_DEF("catch", 1, js_promise_catch ),
    JS_CFUNC_DEF("finally", 1, js_promise_finally ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Promise", JS_PROP_CONFIGURABLE ),
};

/* AsyncFunction */
static const JSCFunctionListEntry js_async_function_proto_funcs[] = {
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncFunction", JS_PROP_CONFIGURABLE ),
};

/* AsyncIteratorPrototype */

static const JSCFunctionListEntry js_async_iterator_proto_funcs[] = {
    JS_CFUNC_DEF("[Symbol.asyncIterator]", 0, js_iterator_proto_iterator ),
};

/* AsyncFromSyncIteratorPrototype */

typedef struct JSAsyncFromSyncIteratorData {
    JSValue sync_iter;
    JSValue next_method;
} JSAsyncFromSyncIteratorData;

static void js_async_from_sync_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSAsyncFromSyncIteratorData *s =
        JS_GetOpaque(val, JS_CLASS_ASYNC_FROM_SYNC_ITERATOR);
    if (s) {
        JS_FreeValueRT(rt, s->sync_iter);
        JS_FreeValueRT(rt, s->next_method);
        js_free_rt(rt, s);
    }
}

static void js_async_from_sync_iterator_mark(JSRuntime *rt, JSValueConst val,
                                             JS_MarkFunc *mark_func)
{
    JSAsyncFromSyncIteratorData *s =
        JS_GetOpaque(val, JS_CLASS_ASYNC_FROM_SYNC_ITERATOR);
    if (s) {
        JS_MarkValue(rt, s->sync_iter, mark_func);
        JS_MarkValue(rt, s->next_method, mark_func);
    }
}

JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                       JSValueConst sync_iter)
{
    JSValue async_iter, next_method;
    JSAsyncFromSyncIteratorData *s;

    next_method = JS_GetProperty(ctx, sync_iter, JS_ATOM_next);
    if (JS_IsException(next_method))
        return JS_EXCEPTION;
    async_iter = JS_NewObjectClass(ctx, JS_CLASS_ASYNC_FROM_SYNC_ITERATOR);
    if (JS_IsException(async_iter)) {
        JS_FreeValue(ctx, next_method);
        return async_iter;
    }
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, async_iter);
        JS_FreeValue(ctx, next_method);
        return JS_EXCEPTION;
    }
    s->sync_iter = JS_DupValue(ctx, sync_iter);
    s->next_method = next_method;
    JS_SetOpaque(async_iter, s);
    return async_iter;
}

static JSValue js_async_from_sync_iterator_unwrap(JSContext *ctx,
                                                  JSValueConst this_val,
                                                  int argc, JSValueConst *argv,
                                                  int magic, JSValue *func_data)
{
    return js_create_iterator_result(ctx, JS_DupValue(ctx, argv[0]),
                                     JS_ToBool(ctx, func_data[0]));
}

static JSValue js_async_from_sync_iterator_unwrap_func_create(JSContext *ctx,
                                                              BOOL done)
{
    JSValueConst func_data[1];

    func_data[0] = (JSValueConst)JS_NewBool(ctx, done);
    return JS_NewCFunctionData(ctx, js_async_from_sync_iterator_unwrap,
                               1, 0, 1, func_data);
}

static JSValue js_async_from_sync_iterator_close_wrap(JSContext *ctx,
                                                      JSValueConst this_val,
                                                      int argc, JSValueConst *argv,
                                                      int magic, JSValue *func_data)
{
    JS_Throw(ctx, JS_DupValue(ctx, argv[0]));
    JS_IteratorClose(ctx, func_data[0], TRUE);
    return JS_EXCEPTION;
}

static JSValue js_async_from_sync_iterator_close_wrap_func_create(JSContext *ctx, JSValueConst sync_iter)
{
    return JS_NewCFunctionData(ctx, js_async_from_sync_iterator_close_wrap,
                               1, 0, 1, &sync_iter);
}

static JSValue js_async_from_sync_iterator_next(JSContext *ctx, JSValueConst this_val,
                                                int argc, JSValueConst *argv,
                                                int magic)
{
    JSValue promise, resolving_funcs[2], value, err, method;
    JSAsyncFromSyncIteratorData *s;
    int done;
    int is_reject;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;
    s = JS_GetOpaque(this_val, JS_CLASS_ASYNC_FROM_SYNC_ITERATOR);
    if (!s) {
        JS_ThrowTypeError(ctx, "not an Async-from-Sync Iterator");
        goto reject;
    }

    if (magic == GEN_MAGIC_NEXT) {
        method = JS_DupValue(ctx, s->next_method);
    } else {
        method = JS_GetProperty(ctx, s->sync_iter,
                                magic == GEN_MAGIC_RETURN ? JS_ATOM_return :
                                JS_ATOM_throw);
        if (JS_IsException(method))
            goto reject;
        if (JS_IsUndefined(method) || JS_IsNull(method)) {
            if (magic == GEN_MAGIC_RETURN) {
                err = js_create_iterator_result(ctx, JS_DupValue(ctx, argv[0]), TRUE);
                is_reject = 0;
                goto done_resolve;
            } else {
                if (JS_IteratorClose(ctx, s->sync_iter, FALSE))
                    goto reject;
                JS_ThrowTypeError(ctx, "throw is not a method");
                goto reject;
            }
        }
    }
    value = JS_IteratorNext2(ctx, s->sync_iter, method,
                             argc >= 1 ? 1 : 0, argv, &done);
    JS_FreeValue(ctx, method);
    if (JS_IsException(value))
        goto reject;
    if (done == 2) {
        JSValue obj = value;
        value = JS_IteratorGetCompleteValue(ctx, obj, &done);
        JS_FreeValue(ctx, obj);
        if (JS_IsException(value))
            goto reject;
    }
    
    if (JS_IsException(value))
        goto reject;
    {
        JSValue value_wrapper_promise, resolve_reject[2];
        int res;

        value_wrapper_promise = js_promise_resolve(ctx, ctx->promise_ctor,
                                                   1, (JSValueConst *)&value, 0);
        if (JS_IsException(value_wrapper_promise)) {
            JSValue res2;
            JS_FreeValue(ctx, value);
            if (magic != GEN_MAGIC_RETURN && !done) {
                JS_IteratorClose(ctx, s->sync_iter, TRUE);
            }
        reject:
            err = JS_GetException(ctx);
            is_reject = 1;
        done_resolve:
            res2 = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED,
                           1, (JSValueConst *)&err);
            JS_FreeValue(ctx, err);
            JS_FreeValue(ctx, res2);
            JS_FreeValue(ctx, resolving_funcs[0]);
            JS_FreeValue(ctx, resolving_funcs[1]);
            return promise;
        }

        resolve_reject[0] =
            js_async_from_sync_iterator_unwrap_func_create(ctx, done);
        if (JS_IsException(resolve_reject[0])) {
            JS_FreeValue(ctx, value_wrapper_promise);
            goto fail;
        }
        if (done || magic == GEN_MAGIC_RETURN) {
            resolve_reject[1] = JS_UNDEFINED;
        } else {
            resolve_reject[1] =
                js_async_from_sync_iterator_close_wrap_func_create(ctx, s->sync_iter);
            if (JS_IsException(resolve_reject[1])) {
                JS_FreeValue(ctx, value_wrapper_promise);
                JS_FreeValue(ctx, resolve_reject[0]);
                goto fail;
            }
        }
        JS_FreeValue(ctx, value);
        res = perform_promise_then(ctx, value_wrapper_promise,
                                   (JSValueConst *)resolve_reject,
                                   (JSValueConst *)resolving_funcs);
        JS_FreeValue(ctx, resolve_reject[0]);
        JS_FreeValue(ctx, resolve_reject[1]);
        JS_FreeValue(ctx, value_wrapper_promise);
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        if (res) {
            JS_FreeValue(ctx, promise);
            return JS_EXCEPTION;
        }
    }
    return promise;
 fail:
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_EXCEPTION;
}

static const JSCFunctionListEntry js_async_from_sync_iterator_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("next", 1, js_async_from_sync_iterator_next, GEN_MAGIC_NEXT ),
    JS_CFUNC_MAGIC_DEF("return", 1, js_async_from_sync_iterator_next, GEN_MAGIC_RETURN ),
    JS_CFUNC_MAGIC_DEF("throw", 1, js_async_from_sync_iterator_next, GEN_MAGIC_THROW ),
};

/* AsyncGeneratorFunction */

static const JSCFunctionListEntry js_async_generator_function_proto_funcs[] = {
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncGeneratorFunction", JS_PROP_CONFIGURABLE ),
};

/* AsyncGenerator prototype */

static const JSCFunctionListEntry js_async_generator_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("next", 1, js_async_generator_next, GEN_MAGIC_NEXT ),
    JS_CFUNC_MAGIC_DEF("return", 1, js_async_generator_next, GEN_MAGIC_RETURN ),
    JS_CFUNC_MAGIC_DEF("throw", 1, js_async_generator_next, GEN_MAGIC_THROW ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "AsyncGenerator", JS_PROP_CONFIGURABLE ),
};

static JSClassShortDef const js_async_class_def[] = {
    { JS_ATOM_Promise, js_promise_finalizer, js_promise_mark },                      /* JS_CLASS_PROMISE */
    { JS_ATOM_PromiseResolveFunction, js_promise_resolve_function_finalizer, js_promise_resolve_function_mark }, /* JS_CLASS_PROMISE_RESOLVE_FUNCTION */
    { JS_ATOM_PromiseRejectFunction, js_promise_resolve_function_finalizer, js_promise_resolve_function_mark }, /* JS_CLASS_PROMISE_REJECT_FUNCTION */
    { JS_ATOM_AsyncFunction, js_bytecode_function_finalizer, js_bytecode_function_mark },  /* JS_CLASS_ASYNC_FUNCTION */
    { JS_ATOM_AsyncFunctionResolve, js_async_function_resolve_finalizer, js_async_function_resolve_mark }, /* JS_CLASS_ASYNC_FUNCTION_RESOLVE */
    { JS_ATOM_AsyncFunctionReject, js_async_function_resolve_finalizer, js_async_function_resolve_mark }, /* JS_CLASS_ASYNC_FUNCTION_REJECT */
    { JS_ATOM_empty_string, js_async_from_sync_iterator_finalizer, js_async_from_sync_iterator_mark }, /* JS_CLASS_ASYNC_FROM_SYNC_ITERATOR */
    { JS_ATOM_AsyncGeneratorFunction, js_bytecode_function_finalizer, js_bytecode_function_mark },  /* JS_CLASS_ASYNC_GENERATOR_FUNCTION */
    { JS_ATOM_AsyncGenerator, js_async_generator_finalizer, js_async_generator_mark },  /* JS_CLASS_ASYNC_GENERATOR */
};

int JS_AddIntrinsicPromise(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj1;
    JSCFunctionType ft;

    if (!JS_IsRegisteredClass(rt, JS_CLASS_PROMISE)) {
        if (init_class_range(rt, js_async_class_def, JS_CLASS_PROMISE,
                             countof(js_async_class_def)))
            return -1;
        rt->class_array[JS_CLASS_PROMISE_RESOLVE_FUNCTION].call = js_promise_resolve_function_call;
        rt->class_array[JS_CLASS_PROMISE_REJECT_FUNCTION].call = js_promise_resolve_function_call;
        rt->class_array[JS_CLASS_ASYNC_FUNCTION].call = js_async_function_call;
        rt->class_array[JS_CLASS_ASYNC_FUNCTION_RESOLVE].call = js_async_function_resolve_call;
        rt->class_array[JS_CLASS_ASYNC_FUNCTION_REJECT].call = js_async_function_resolve_call;
        rt->class_array[JS_CLASS_ASYNC_GENERATOR_FUNCTION].call = js_async_generator_function_call;
    }

    /* Promise */
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_PROMISE, "Promise",
                                     js_promise_constructor, 1, JS_CFUNC_constructor, 0,
                                     JS_UNDEFINED,
                                     js_promise_funcs, countof(js_promise_funcs),
                                     js_promise_proto_funcs, countof(js_promise_proto_funcs),
                                     0);
    if (JS_IsException(obj1))
        return -1;
    ctx->promise_ctor = obj1;
    
    /* AsyncFunction */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_FUNCTION, "AsyncFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_ASYNC,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_async_function_proto_funcs, countof(js_async_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    
    /* AsyncIteratorPrototype */
    ctx->async_iterator_proto =
        JS_NewObjectProtoList(ctx,  ctx->class_proto[JS_CLASS_OBJECT],
                              js_async_iterator_proto_funcs,
                              countof(js_async_iterator_proto_funcs));
    if (JS_IsException(ctx->async_iterator_proto))
        return -1;

    /* AsyncFromSyncIteratorPrototype */
    ctx->class_proto[JS_CLASS_ASYNC_FROM_SYNC_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->async_iterator_proto,
                              js_async_from_sync_iterator_proto_funcs,
                              countof(js_async_from_sync_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ASYNC_FROM_SYNC_ITERATOR]))
        return -1;
    
    /* AsyncGeneratorPrototype */
    ctx->class_proto[JS_CLASS_ASYNC_GENERATOR] =
        JS_NewObjectProtoList(ctx, ctx->async_iterator_proto, 
                              js_async_generator_proto_funcs,
                              countof(js_async_generator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_ASYNC_GENERATOR]))
        return -1;

    /* AsyncGeneratorFunction */
    ft.generic_magic = js_function_constructor;
    obj1 = JS_NewCConstructor(ctx, JS_CLASS_ASYNC_GENERATOR_FUNCTION, "AsyncGeneratorFunction",
                                     ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_ASYNC_GENERATOR,
                                     ctx->function_ctor,
                                     NULL, 0,
                                     js_async_generator_function_proto_funcs, countof(js_async_generator_function_proto_funcs),
                                     JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_READONLY);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    return JS_SetConstructor2(ctx, ctx->class_proto[JS_CLASS_ASYNC_GENERATOR_FUNCTION],
                              ctx->class_proto[JS_CLASS_ASYNC_GENERATOR],
                              JS_PROP_CONFIGURABLE, JS_PROP_CONFIGURABLE);
}

