/*
 * QuickJS Iterator builtin
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
#include "../internal/vm.h"
#include "../internal/base.h"
#include "../internal/generator.h"
#include "../value/conversion.h"
#include "../value/compare.h"
#include "../internal/iterator.h"
#include "../internal/runtime.h"
#include "../internal/number.h"
#include "../internal/string.h"
#include "../internal/string-buffer.h"
#include "../internal/function.h"
#include "../internal/object.h"
#include "../internal/error.h"
#include "iterator.h"

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
                                     int argc, JSValueConst *argv, int magic)
{
    JSIteratorWrapData *it;
    JSValue method;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_WRAP);
    if (!it)
        return JS_EXCEPTION;
    if (magic == GEN_MAGIC_NEXT)
        return JS_Call(ctx, it->wrapped_next, it->wrapped_iter, 0, NULL);
    method = JS_GetProperty(ctx, it->wrapped_iter, JS_ATOM_return);
    if (JS_IsException(method))
        return JS_EXCEPTION;
    if (JS_IsNull(method) || JS_IsUndefined(method)) {
        JS_FreeValue(ctx, method);
        return js_create_iterator_result(ctx, JS_UNDEFINED, TRUE);
    }
    return JS_CallFree(ctx, method, it->wrapped_iter, 0, NULL);
}

const JSCFunctionListEntry js_iterator_wrap_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("next", 0, js_iterator_wrap_next, GEN_MAGIC_NEXT ),
    JS_CFUNC_MAGIC_DEF("return", 0, js_iterator_wrap_next, GEN_MAGIC_RETURN ),
};

/* Iterator */

static JSValue js_iterator_set_own_property(JSContext *ctx,
                                            JSValueConst this_val,
                                            JSAtom prop, JSValueConst val)
{
    int res;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_ITERATOR]))
        return JS_ThrowTypeError(ctx, "Cannot assign to read only property");
    res = JS_GetOwnProperty(ctx, NULL, this_val, prop);
    if (res < 0)
        return JS_EXCEPTION;
    if (res) {
        if (JS_SetProperty(ctx, this_val, prop, JS_DupValue(ctx, val)) < 0)
            return JS_EXCEPTION;
    } else {
        if (JS_DefinePropertyValue(ctx, this_val, prop, JS_DupValue(ctx, val),
                                    JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

static JSValue js_iterator_constructor_get(JSContext *ctx,
                                           JSValueConst this_val)
{
    return JS_DupValue(ctx, ctx->iterator_ctor);
}

static JSValue js_iterator_constructor_set(JSContext *ctx,
                                           JSValueConst this_val,
                                           JSValueConst val)
{
    return js_iterator_set_own_property(ctx, this_val, JS_ATOM_constructor, val);
}

const JSCFunctionListEntry js_iterator_constructor_proto_funcs[] = {
    JS_CGETSET_DEF("constructor", js_iterator_constructor_get,
                   js_iterator_constructor_set ),
};

JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                int argc, JSValueConst *argv)
{
    if (JS_TAG_OBJECT != JS_VALUE_GET_TAG(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    if (js_same_value(ctx, new_target, ctx->iterator_ctor)) {
        return JS_ThrowTypeError(ctx, "abstract class not constructable");
    }
    return js_create_from_ctor(ctx, new_target, JS_CLASS_ITERATOR);
}

// note: deliberately doesn't use space-saving bit fields for
// |index|, |count| and |running| because tcc miscompiles them
typedef struct JSIteratorConcatData {
    JSContext *realm;
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
        if (it->realm)
            JS_FreeContext(it->realm);
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
        if (it->realm)
            mark_func(rt, &it->realm->header);
    }
}

static void js_iterator_concat_complete(JSContext *ctx,
                                         JSIteratorConcatData *it)
{
    while (it->index < it->count)
        JS_FreeValue(ctx, it->values[it->index++]);
    JS_FreeValue(ctx, it->iter);
    JS_FreeValue(ctx, it->next);
    it->iter = JS_UNDEFINED;
    it->next = JS_UNDEFINED;
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
    if (!it->realm) {
        *pdone = TRUE;
        return JS_UNDEFINED;
    }

    ctx = it->realm;
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
    if (JS_IsException(ret)) {
        js_iterator_concat_complete(ctx, it);
        *pdone = TRUE;
    } else {
        ret = js_create_iterator_result(ctx, ret, *pdone);
        if (JS_IsException(ret) && !*pdone) {
            JS_IteratorClose(ctx, it->iter, TRUE);
            js_iterator_concat_complete(ctx, it);
            *pdone = TRUE;
        }
    }
    it->running = FALSE;
    if (*pdone) {
        JS_FreeContext(it->realm);
        it->realm = NULL;
    }
    *pdone = 2;
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
        ctx = it->realm;
        it->running = TRUE;
        if (JS_IteratorClose(ctx, it->iter, FALSE) < 0)
            ret = JS_EXCEPTION;
    }
    js_iterator_concat_complete(ctx, it);
    if (!JS_IsException(ret))
        ret = js_create_iterator_result(ctx, JS_UNDEFINED, TRUE);
    it->running = FALSE;
    if (it->realm) {
        JS_FreeContext(it->realm);
        it->realm = NULL;
    }
    return ret;
}

static JSValue js_iterator_concat(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSIteratorConcatData *it;
    JSValue obj, method;

    it = js_malloc(ctx, sizeof(*it) + 2*argc * sizeof(it->values[0]));
    if (!it)
        return JS_EXCEPTION;
    it->realm = JS_DupContext(ctx);
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
    JS_FreeContext(it->realm);
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

    if (!JS_IsObject(iter)) {
        JS_FreeValue(ctx, iter);
        return JS_ThrowTypeErrorNotAnObject(ctx);
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

#define JS_ITERATOR_LIMIT_INFINITY (-1)

typedef struct JSIteratorHelperData {
    JSContext *realm;
    JSValue obj;
    JSValue next;
    JSValue argument; // callback
    JSValue inner; // innerValue (flatMap)
    JSValue inner_next; // innerValue next method (flatMap)
    int64_t count; // limit (drop, take; -1 means infinity) or callback counter
    JSIteratorHelperKindEnum kind : 8;
    uint8_t executing : 1;
    uint8_t done : 1;
    uint8_t started : 1;
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
            double d;

            if (JS_ToFloat64(ctx, &d, argv[0]))
                goto fail;
            if (isnan(d) || (isfinite(d) && d > MAX_SAFE_INTEGER))
                goto range_error;
            d = trunc(d);
            if (d < 0)
                goto range_error;
            count = isinf(d) ? JS_ITERATOR_LIMIT_INFINITY : (int64_t)d;
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
        return JS_EXCEPTION;
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
    it->realm = JS_DupContext(ctx);
    it->kind = magic;
    it->obj = JS_DupValue(ctx, this_val);
    it->argument = JS_DupValue(ctx, func);
    it->next = method;
    it->inner = JS_UNDEFINED;
    it->inner_next = JS_UNDEFINED;
    it->count = count;
    it->executing = 0;
    it->done = 0;
    it->started = 0;
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

static JSValue js_iterator_proto_includes(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSValueConst skipped = argc > 1 ? argv[1] : JS_UNDEFINED;
    JSValue method, item, result = JS_FALSE;
    double to_skip = 0;
    uint64_t remaining;
    BOOL skip_all, match;
    int done;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (!JS_IsUndefined(skipped)) {
        if (!JS_IsNumber(skipped))
            goto type_error;
        if (JS_ToFloat64(ctx, &to_skip, skipped))
            goto validation_error;
        if (isnan(to_skip) || trunc(to_skip) != to_skip)
            goto type_error;
        if (to_skip < 0 || (isfinite(to_skip) && to_skip > MAX_SAFE_INTEGER)) {
            JS_ThrowRangeError(ctx, "skippedElements is out of range");
            goto validation_error;
        }
    }
    skip_all = isinf(to_skip);
    remaining = skip_all ? 0 : (uint64_t)to_skip;
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method))
        return JS_EXCEPTION;
    for (;;) {
        item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(item)) {
            result = JS_EXCEPTION;
            break;
        }
        if (done)
            break;
        if (skip_all || remaining > 0) {
            if (remaining > 0)
                remaining--;
            JS_FreeValue(ctx, item);
            continue;
        }
        match = js_same_value_zero(ctx, item, argv[0]);
        JS_FreeValue(ctx, item);
        if (match) {
            result = JS_IteratorClose(ctx, this_val, FALSE) < 0 ?
                JS_EXCEPTION : JS_TRUE;
            break;
        }
    }
    JS_FreeValue(ctx, method);
    return result;
 type_error:
    JS_ThrowTypeError(ctx, "skippedElements must be an integer Number");
 validation_error:
    JS_IteratorClose(ctx, this_val, TRUE);
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
        goto exception_no_close;
    if (argc > 1) {
        acc = JS_DupValue(ctx, argv[1]);
        idx = 0;
    } else {
        acc = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(acc))
            goto exception_no_close;
        if (done) {
            JS_ThrowTypeError(ctx, "empty iterator");
            goto exception_no_close;
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

static JSValue js_iterator_proto_join(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSValue separator = JS_UNDEFINED, method, item;
    StringBuffer b_s, *b = &b_s;
    JSString *separator_string = NULL;
    int separator_char = ',', done;
    BOOL first = TRUE;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    if (!JS_IsUndefined(argv[0])) {
        separator = JS_ToString(ctx, argv[0]);
        if (JS_IsException(separator)) {
            JS_IteratorClose(ctx, this_val, TRUE);
            return JS_EXCEPTION;
        }
        separator_string = JS_VALUE_GET_STRING(separator);
        if (separator_string->len == 1 && !separator_string->is_wide_char)
            separator_char = separator_string->u.str8[0];
        else
            separator_char = -1;
    }
    /* GetIteratorDirect comes after separator conversion. */
    method = JS_GetProperty(ctx, this_val, JS_ATOM_next);
    if (JS_IsException(method)) {
        JS_FreeValue(ctx, separator);
        return JS_EXCEPTION;
    }
    if (string_buffer_init(ctx, b, 0))
        goto fail;
    for (;;) {
        item = JS_IteratorNext(ctx, this_val, method, 0, NULL, &done);
        if (JS_IsException(item))
            goto fail;
        if (done)
            break;
        if (!first) {
            int ret;
            if (separator_char >= 0)
                ret = string_buffer_putc8(b, separator_char);
            else
                ret = string_buffer_concat(b, separator_string, 0,
                                           separator_string->len);
            if (ret) {
                JS_FreeValue(ctx, item);
                goto fail;
            }
        }
        first = FALSE;
        if (!JS_IsNull(item) && !JS_IsUndefined(item)) {
            /* Strings, including ropes, need no observable conversion. */
            if (!JS_IsString(item)) {
                item = JS_ToStringFree(ctx, item);
                if (JS_IsException(item)) {
                    JS_IteratorClose(ctx, this_val, TRUE);
                    goto fail;
                }
            }
            if (string_buffer_concat_value(b, item)) {
                JS_FreeValue(ctx, item);
                goto fail;
            }
        }
        JS_FreeValue(ctx, item);
    }
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, separator);
    return string_buffer_end(b);
fail:
    string_buffer_free(b);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, separator);
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

JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
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
    return js_iterator_set_own_property(ctx, this_val, JS_ATOM_Symbol_toStringTag,
                                        val);
}

/* Iterator Helper */

void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSIteratorHelperData *it = p->u.iterator_helper_data;
    if (it) {
        JS_FreeValueRT(rt, it->obj);
        JS_FreeValueRT(rt, it->argument);
        JS_FreeValueRT(rt, it->next);
        JS_FreeValueRT(rt, it->inner);
        JS_FreeValueRT(rt, it->inner_next);
        if (it->realm)
            JS_FreeContext(it->realm);
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
        JS_MarkValue(rt, it->argument, mark_func);
        JS_MarkValue(rt, it->next, mark_func);
        JS_MarkValue(rt, it->inner, mark_func);
        JS_MarkValue(rt, it->inner_next, mark_func);
        if (it->realm)
            mark_func(rt, &it->realm->header);
    }
}

static JSValue js_iterator_helper_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      int *pdone, int magic)
{
    JSIteratorHelperData *it;
    JSValue ret;

    *pdone = FALSE;

    if (JS_IsObject(this_val) &&
        JS_VALUE_GET_OBJ(this_val)->class_id == JS_CLASS_ITERATOR_CONCAT) {
        if (magic == GEN_MAGIC_RETURN) {
            *pdone = 2;
            return js_iterator_concat_return(ctx, this_val, argc, argv);
        }
        return js_iterator_concat_next(ctx, this_val, argc, argv, pdone, magic);
    }
    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_ITERATOR_HELPER);
    if (!it)
        return JS_EXCEPTION;
    if (it->executing)
        return JS_ThrowTypeError(ctx, "cannot invoke a running iterator");
    if (it->done) {
        *pdone = TRUE;
        return JS_UNDEFINED;
    }

    if (magic == GEN_MAGIC_RETURN && !it->started) {
        it->done = 1;
        *pdone = TRUE;
        ret = JS_IteratorClose(ctx, it->obj, FALSE)
            ? JS_EXCEPTION : JS_UNDEFINED;
        JS_FreeContext(it->realm);
        it->realm = NULL;
        return ret;
    }
    it->executing = 1;
    it->started = 1;
    ctx = it->realm;

    if (magic == GEN_MAGIC_RETURN) {
        *pdone = TRUE;
        ret = JS_UNDEFINED;
        
        if (!JS_IsUndefined(it->inner)) {
            if (JS_IteratorClose(ctx, it->inner, FALSE))
                ret = JS_EXCEPTION;
            JS_FreeValue(ctx, it->inner);
            JS_FreeValue(ctx, it->inner_next);
            it->inner = JS_UNDEFINED;
            it->inner_next = JS_UNDEFINED;
        }
        if (JS_IteratorClose(ctx, it->obj, JS_IsException(ret)))
            ret = JS_EXCEPTION;
        goto done;
    }

    switch (it->kind) {
    case JS_ITERATOR_HELPER_KIND_DROP:
        {
            JSValue item;
            while (it->count != 0) {
                if (it->count > 0)
                    it->count--;
                item = JS_IteratorNext(ctx, it->obj, it->next, 0, NULL, pdone);
                if (JS_IsException(item))
                    goto fail_no_close;
                JS_FreeValue(ctx, item);
                if (*pdone) {
                    ret = JS_UNDEFINED;
                    goto done;
                }
            }

            item = JS_IteratorNext(ctx, it->obj, it->next, 0, NULL, pdone);
            if (JS_IsException(item))
                goto fail_no_close;
            ret = item;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FILTER:
        {
            JSValue item, selected, index_val;
            JSValueConst args[2];
            for(;;) {
                item = JS_IteratorNext(ctx, it->obj, it->next, 0, NULL, pdone);
                if (JS_IsException(item))
                    goto fail_no_close;
                if (*pdone) {
                    ret = item;
                    break;
                }
                index_val = JS_NewInt64(ctx, it->count++);
                args[0] = item;
                args[1] = index_val;
                selected = JS_Call(ctx, it->argument, JS_UNDEFINED, countof(args), args);
                JS_FreeValue(ctx, index_val);
                if (JS_IsException(selected)) {
                    JS_FreeValue(ctx, item);
                    goto fail;
                }
                if (JS_ToBoolFree(ctx, selected)) {
                    ret = item;
                    break;
                }
                JS_FreeValue(ctx, item);
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_FLAT_MAP:
        {
            JSValue item, method, index_val, iter;
            JSValueConst args[2];
            for(;;) {
                if (JS_IsUndefined(it->inner)) {
                    item = JS_IteratorNext(ctx, it->obj, it->next, 0, NULL, pdone);
                    if (JS_IsException(item))
                        goto fail_no_close;
                    if (*pdone) {
                        ret = item;
                        break;
                    }
                    index_val = JS_NewInt64(ctx, it->count++);
                    args[0] = item;
                    args[1] = index_val;
                    ret = JS_Call(ctx, it->argument, JS_UNDEFINED, countof(args), args);
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
                    method = JS_GetProperty(ctx, it->inner, JS_ATOM_next);
                    if (JS_IsException(method))
                        goto inner_fail;
                    it->inner_next = method;
                }

                item = JS_IteratorNext(ctx, it->inner, it->inner_next, 0, NULL, pdone);
                if (JS_IsException(item)) {
                inner_fail:
                    JS_FreeValue(ctx, it->inner);
                    JS_FreeValue(ctx, it->inner_next);
                    it->inner = JS_UNDEFINED;
                    it->inner_next = JS_UNDEFINED;
                    goto fail;
                }
                if (!*pdone) {
                    ret = item;
                    break;
                }
                *pdone = FALSE; // The outer iterator must continue.
                JS_FreeValue(ctx, it->inner);
                JS_FreeValue(ctx, it->inner_next);
                it->inner = JS_UNDEFINED;
                it->inner_next = JS_UNDEFINED;
            }
        }
        break;
    case JS_ITERATOR_HELPER_KIND_MAP:
        {
            JSValue item, index_val;
            JSValueConst args[2];
            item = JS_IteratorNext(ctx, it->obj, it->next, 0, NULL, pdone);
            if (JS_IsException(item))
                goto fail_no_close;
            if (*pdone) {
                ret = item;
                goto done;
            }
            index_val = JS_NewInt64(ctx, it->count++);
            args[0] = item;
            args[1] = index_val;
            ret = JS_Call(ctx, it->argument, JS_UNDEFINED, countof(args), args);
            JS_FreeValue(ctx, index_val);
            JS_FreeValue(ctx, item);
            if (JS_IsException(ret))
                goto fail;
        }
        break;
    case JS_ITERATOR_HELPER_KIND_TAKE:
        {
            JSValue item;
            if (it->count != 0) {
                if (it->count > 0)
                    it->count--;
                item = JS_IteratorNext(ctx, it->obj, it->next, 0, NULL, pdone);
                if (JS_IsException(item))
                    goto fail_no_close;
                ret = item;
            } else {
                *pdone = TRUE;
                if (JS_IteratorClose(ctx, it->obj, FALSE))
                    ret = JS_EXCEPTION;
                else
                    ret = JS_UNDEFINED;
            }
        }
        break;
    default:
        abort();
    }

 done:
    if (!JS_IsException(ret)) {
        ret = js_create_iterator_result(ctx, ret, *pdone);
        if (JS_IsException(ret) && !*pdone) {
            if (!JS_IsUndefined(it->inner)) {
                JS_IteratorClose(ctx, it->inner, TRUE);
                JS_FreeValue(ctx, it->inner);
                JS_FreeValue(ctx, it->inner_next);
                it->inner = JS_UNDEFINED;
                it->inner_next = JS_UNDEFINED;
            }
            JS_IteratorClose(ctx, it->obj, TRUE);
            *pdone = TRUE;
        }
    }
    it->done = *pdone;
    it->executing = 0;
    if (*pdone) {
        JS_FreeContext(it->realm);
        it->realm = NULL;
    }
    *pdone = 2;
    return ret;
 fail:
    /* close the iterator object, preserving pending exception */
    JS_IteratorClose(ctx, it->obj, TRUE);
 fail_no_close:
    *pdone = TRUE;
    ret = JS_EXCEPTION;
    goto done;
}

const JSCFunctionListEntry js_iterator_funcs[] = {
    JS_CFUNC_DEF("concat", 0, js_iterator_concat ),
    JS_CFUNC_DEF("from", 1, js_iterator_from ),
};

const JSCFunctionListEntry js_iterator_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("drop", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_DROP ),
    JS_CFUNC_MAGIC_DEF("filter", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_FILTER ),
    JS_CFUNC_MAGIC_DEF("flatMap", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_FLAT_MAP ),
    JS_CFUNC_MAGIC_DEF("map", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_MAP ),
    JS_CFUNC_MAGIC_DEF("take", 1, js_create_iterator_helper, JS_ITERATOR_HELPER_KIND_TAKE ),
    JS_CFUNC_MAGIC_DEF("every", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_EVERY ),
    JS_CFUNC_MAGIC_DEF("find", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_FIND),
    JS_CFUNC_MAGIC_DEF("forEach", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_FOR_EACH ),
    JS_CFUNC_MAGIC_DEF("some", 1, js_iterator_proto_func, JS_ITERATOR_HELPER_KIND_SOME ),
    JS_CFUNC_DEF("includes", 1, js_iterator_proto_includes ),
    JS_CFUNC_DEF("join", 1, js_iterator_proto_join ),
    JS_CFUNC_DEF("reduce", 1, js_iterator_proto_reduce ),
    JS_CFUNC_DEF("toArray", 0, js_iterator_proto_toArray ),
    JS_CFUNC_DEF("[Symbol.iterator]", 0, js_iterator_proto_iterator ),
    JS_CGETSET_DEF("[Symbol.toStringTag]", js_iterator_proto_get_toStringTag, js_iterator_proto_set_toStringTag),
};

const JSCFunctionListEntry js_iterator_helper_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_iterator_helper_next, GEN_MAGIC_NEXT ),
    JS_ITERATOR_NEXT_DEF("return", 0, js_iterator_helper_next, GEN_MAGIC_RETURN ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Iterator Helper", JS_PROP_CONFIGURABLE ),
};
