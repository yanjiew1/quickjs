/*
 * QuickJS property hash tests
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

#include "../src/quickjs/internal/object.h"

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
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
