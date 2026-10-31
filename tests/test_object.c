/*
 * QuickJS object and property hash tests
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
#include <assert.h>
#include <string.h>

#include "../src/quickjs/internal/object.h"
#include "../src/quickjs/internal/function.h"
#include "oom-allocator.h"
#include "pool-padding.h"

/* Inspect real chains rather than timing the lookup implementation. */
static void check_property_chains(JSValueConst obj, unsigned expected)
{
    JSShape *shape = JS_VALUE_GET_OBJ(obj)->shape;
    JSShapeProperty *props = get_shape_prop(shape);
    unsigned occupied = 0, total = 0, longest = 0;
    uint32_t bucket, slot;
    uint8_t seen[1024] = { 0 };

    assert(shape->prop_count <= (int)sizeof(seen));
    assert(shape->prop_hash_shift < 32);
    assert((UINT32_MAX >> shape->prop_hash_shift) == shape->prop_hash_mask);
    for (bucket = 0; bucket <= shape->prop_hash_mask; bucket++) {
        unsigned chain = 0;
        for (slot = shape->hash_table[bucket]; slot; slot = props[slot - 1].hash_next) {
            assert(slot <= (unsigned)shape->prop_count);
            assert(!seen[slot - 1]);
            seen[slot - 1] = 1;
            assert(props[slot - 1].atom != JS_ATOM_NULL);
            chain++;
        }
        if (chain)
            occupied++;
        if (chain > longest)
            longest = chain;
        total += chain;
    }
    assert(total == expected);
    assert(occupied >= expected / 2);
    assert(longest <= 8);
}

static void test_property_hash(JSContext *ctx, unsigned count, unsigned shift)
{
    JSValue objects[2], value;
    uint32_t i, key;
    int copy;

    for (copy = 0; copy < 2; copy++) {
        objects[copy] = JS_NewObject(ctx);
        assert(!JS_IsException(objects[copy]));
        for (i = 0; i < count; i++) {
            key = i << shift;
            assert(JS_DefinePropertyValueUint32(ctx, objects[copy], key,
                                                JS_NewInt32(ctx, i), JS_PROP_C_W_E) >= 0);
        }
        check_property_chains(objects[copy], count);
    }
    /* Mutate one shared shape, triggering copy-on-write and compaction. */
    for (i = 0; i < count; i += 2) {
        JSAtom atom = JS_NewAtomUInt32(ctx, i << shift);
        assert(atom != JS_ATOM_NULL);
        assert(JS_DeleteProperty(ctx, objects[0], atom, 0) == 1);
        JS_FreeAtom(ctx, atom);
    }
    check_property_chains(objects[0], count / 2);
    check_property_chains(objects[1], count);
    for (copy = 0; copy < 2; copy++) {
        for (i = 0; i < count; i++) {
            value = JS_GetPropertyUint32(ctx, objects[copy], i << shift);
            assert(!JS_IsException(value));
            if (!copy && !(i & 1)) {
                assert(JS_IsUndefined(value));
            } else {
                assert(JS_VALUE_GET_TAG(value) == JS_TAG_INT);
                assert(JS_VALUE_GET_INT(value) == (int)i);
            }
            JS_FreeValue(ctx, value);
        }
        JS_FreeValue(ctx, objects[copy]);
    }
}

static void test_compact_minimum(JSContext *ctx)
{
    JSValue obj = JS_NewObject(ctx);
    uint32_t i;

    assert(!JS_IsException(obj));
    for (i = 0; i < 18; i++)
        assert(JS_DefinePropertyValueUint32(ctx, obj, i,
                                            JS_NewInt32(ctx, i), JS_PROP_C_W_E) >= 0);
    for (i = 0; i < 17; i++) {
        JSAtom atom = JS_NewAtomUInt32(ctx, i);
        assert(atom != JS_ATOM_NULL);
        assert(JS_DeleteProperty(ctx, obj, atom, 0) == 1);
        JS_FreeAtom(ctx, atom);
    }
    check_property_chains(obj, 1);
    assert(JS_VALUE_GET_OBJ(obj)->shape->prop_hash_mask == 1);
    JS_FreeValue(ctx, obj);
}

static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "object-test.js",
                             JS_EVAL_TYPE_GLOBAL);

    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
}

static void test_integrity_proxy_cleanup(JSContext *ctx)
{
    static const char setup[] =
        "const target = {};"
        "Object.defineProperty(target, 'a', {value: 1});"
        "const integrityProxy = new Proxy(target, {"
        "  isExtensible() { throw 1; }"
        "});";
    static const char exercise[] =
        "(() => {"
        "  let caught = 0;"
        "  for (let i = 0; i < 100; i++) {"
        "    try { Object.isSealed(integrityProxy); }"
        "    catch (e) { if (e !== 1) throw e; caught++; }"
        "    try { Object.isFrozen(integrityProxy); }"
        "    catch (e) { if (e !== 1) throw e; caught++; }"
        "  }"
        "  if (caught !== 200) throw 0;"
        "})()";
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSMemoryUsage before, after;

    eval_ok(ctx, setup);
    eval_ok(ctx, exercise);
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &before);

    /* The throwing trap must not retain each call's property enumeration. */
    eval_ok(ctx, exercise);
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_count <= before.malloc_count + 4);
}

static void test_set_prototype_oom(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue obj = JS_NewObject(ctx), peer = JS_NewObject(ctx);
    JSValue proto = JS_NewObject(ctx), error;
    JSMemoryUsage usage;
    void *padding;
    int ref_count;

    assert(!JS_IsException(obj) && !JS_IsException(peer) &&
           !JS_IsException(proto));
    assert(JS_VALUE_GET_OBJ(obj)->shape == JS_VALUE_GET_OBJ(peer)->shape);
    ref_count = js_rc(JS_VALUE_GET_OBJ(proto))->ref_count;
    padding = test_pool_padding(rt);
    JS_ComputeMemoryUsage(rt, &usage);
    JS_SetMemoryLimit(rt, usage.malloc_size);
    assert(JS_SetPrototype(ctx, obj, proto) == -1);
    JS_SetMemoryLimit(rt, (size_t)-1);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    assert(js_rc(JS_VALUE_GET_OBJ(proto))->ref_count == ref_count);
    test_pool_padding_free(rt, padding);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, peer);
    JS_FreeValue(ctx, obj);
}

static void test_delete_global_varref_oom(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue global = JS_GetGlobalObject(ctx), error;
    JSObject *p = JS_VALUE_GET_OBJ(global);
    JSObject *p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
    JSAtom atom = JS_NewAtom(ctx, "oomGlobal");
    JSShapeProperty *prs;
    JSProperty *pr;
    JSVarRef *var_ref;
    JSMemoryUsage usage;
    void *padding;
    uint32_t i = 0;

    assert(atom != JS_ATOM_NULL);
    assert(JS_DefinePropertyValue(ctx, global, atom, JS_NewInt32(ctx, 1),
                                  JS_PROP_C_W_E) == 1);
    prs = find_own_property(&pr, p, atom);
    assert(prs && (prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF);
    var_ref = pr->u.var_ref;
    js_rc(var_ref)->ref_count++; /* Model a reference retained by a closure. */
    assert(js_rc(p->shape)->ref_count == 1);
    while (p1->shape->prop_count < p1->shape->prop_size) {
        assert(JS_SetPropertyUint32(ctx, JS_MKPTR(JS_TAG_OBJECT, p1), i,
                                    JS_NewInt32(ctx, i)) == 1);
        i++;
    }
    padding = test_pool_padding(rt);
    JS_ComputeMemoryUsage(rt, &usage);
    JS_SetMemoryLimit(rt, usage.malloc_size);
    assert(JS_DeleteProperty(ctx, global, atom, 0) == -1);
    JS_SetMemoryLimit(rt, (size_t)-1);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    assert(JS_HasProperty(ctx, global, atom) == 1);
    test_pool_padding_free(rt, padding);
    free_var_ref(rt, var_ref);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, global);
}

static JSValue bound_function_name_after_oom(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    if (JS_HasException(ctx)) {
        JSValue error = JS_GetException(ctx);
        JS_FreeValue(ctx, error);
    }
    return JS_NewString(ctx, "f");
}

static void test_bound_function_properties_oom(void)
{
    size_t step;
    int failures = 0, success = 0;

    for (step = 1; step < 64; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue target, bind, bound, error, proto, peer;
        JSValueConst this_arg = JS_UNDEFINED;
        JSAtom length_atom, name_atom;
        void *padding;

        assert(ctx);
        target = JS_Eval(ctx, "(function f(a, b) {})", 21,
                         "<bound-oom>", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(target));
        proto = JS_GetPrototype(ctx, target);
        peer = JS_NewObjectProto(ctx, proto);
        assert(!JS_IsException(proto) && !JS_IsException(peer));
        bind = JS_GetPropertyStr(ctx, target, "bind");
        assert(!JS_IsException(bind));
        length_atom = JS_NewAtom(ctx, "length");
        name_atom = JS_NewAtom(ctx, "name");
        assert(length_atom != JS_ATOM_NULL && name_atom != JS_ATOM_NULL);
        assert(JS_DefinePropertyGetSet(ctx, target, name_atom,
                   JS_NewCFunction(ctx, bound_function_name_after_oom,
                                   "nameAfterOOM", 0), JS_UNDEFINED,
                   JS_PROP_CONFIGURABLE) == 1);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        bound = JS_Call(ctx, bind, target, 1, &this_arg);
        oom_test_end(&allocator);
        failures += allocator.failures;
        if (JS_IsException(bound)) {
            assert(JS_HasException(ctx));
            error = JS_GetException(ctx);
            JS_FreeValue(ctx, error);
        } else {
            assert(!JS_HasException(ctx));
            assert(JS_GetOwnProperty(ctx, NULL, bound, length_atom) == 1);
            assert(JS_GetOwnProperty(ctx, NULL, bound, name_atom) == 1);
            success = !allocator.failures;
            JS_FreeValue(ctx, bound);
        }
        JS_FreeValue(ctx, bind);
        JS_FreeValue(ctx, peer);
        JS_FreeValue(ctx, proto);
        JS_FreeValue(ctx, target);
        JS_FreeAtom(ctx, length_atom);
        JS_FreeAtom(ctx, name_atom);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
        if (success)
            break;
    }
    assert(success && failures >= 2);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_compact_minimum(ctx);
    test_property_hash(ctx, 128, 24);
    test_property_hash(ctx, 1024, 16);
    test_integrity_proxy_cleanup(ctx);
    test_set_prototype_oom(ctx);
    test_delete_global_varref_oom(ctx);
    test_bound_function_properties_oom();
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
