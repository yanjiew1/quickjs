/* JSValue codec and inline/heap BigInt boundary tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "quickjs.h"

static void test_immediates(JSContext *ctx)
{
    static const int32_t integers[] = { INT32_MIN, -1, 0, 1, INT32_MAX };
    static const int specials[] = {
        JS_TAG_BOOL, JS_TAG_NULL, JS_TAG_UNDEFINED, JS_TAG_UNINITIALIZED,
        JS_TAG_CATCH_OFFSET, JS_TAG_EXCEPTION,
    };
    size_t i;
    for (i = 0; i < sizeof(integers) / sizeof(*integers); i++) {
        JSValue v = JS_NewInt32(ctx, integers[i]);
        assert(JS_VALUE_GET_TAG(v) == JS_TAG_INT);
        assert(JS_VALUE_GET_INT(v) == integers[i]);
        assert(JS_VALUE_IS_BOTH_INT(v, v));
        assert(!JS_VALUE_IS_BOTH_FLOAT(v, v));
        assert(!JS_VALUE_IS_BOTH_INT(v, JS_NAN));
        assert(!JS_VALUE_IS_BOTH_FLOAT(v, JS_NAN));
        assert(!JS_VALUE_HAS_REF_COUNT(v));
        JS_FreeValue(ctx, v);
    }
    for (i = 0; i < sizeof(specials) / sizeof(*specials); i++) {
        JSValue v = JS_MKVAL(specials[i], UINT32_C(0xfedcba98));
        assert(JS_VALUE_GET_TAG(v) == specials[i]);
        assert((uint32_t)JS_VALUE_GET_INT(v) == UINT32_C(0xfedcba98));
        assert(!JS_VALUE_HAS_REF_COUNT(v));
    }
    assert(JS_IsBool(JS_TRUE) && JS_IsBool(JS_FALSE));
    assert(JS_IsNull(JS_NULL) && !JS_IsUndefined(JS_NULL));
    assert(JS_IsUndefined(JS_UNDEFINED) && !JS_IsNull(JS_UNDEFINED));
    assert(JS_IsException(JS_EXCEPTION));
    assert(JS_IsUninitialized(JS_UNINITIALIZED));
#if defined(JS_COMPACT_NAN_BOXING)
    {
        static const JSValue negative = JS_MKVAL(JS_TAG_SHORT_BIG_INT, -1);
        assert(JS_VALUE_GET_TAG(negative) == JS_TAG_SHORT_BIG_INT);
        assert(JS_VALUE_GET_SHORT_BIG_INT(negative) == -1);
        assert(JS_IsBigInt(ctx, negative));
        assert(!JS_VALUE_HAS_REF_COUNT(negative));
    }
#endif
}

static void test_doubles(JSContext *ctx)
{
    static const uint64_t bits[] = {
        0, UINT64_C(0x8000000000000000), /* signed zero */
        1, UINT64_C(0x8000000000000001), /* subnormals */
        UINT64_C(0x3ff0000000000000), UINT64_C(0xbff0000000000000),
        UINT64_C(0x7fefffffffffffff), UINT64_C(0xffefffffffffffff),
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff0000000000001), UINT64_C(0xfff0000000000001),
        UINT64_C(0x7ff8000000000000), UINT64_C(0xfff8000000000000),
        UINT64_C(0x7fffffffffffffff), UINT64_C(0xffffffffffffffff),
    };
    size_t i;
    for (i = 0; i < sizeof(bits) / sizeof(*bits); i++) {
        double d, decoded;
        uint64_t actual;
        JSValue v;
        memcpy(&d, &bits[i], sizeof(d));
        v = __JS_NewFloat64(ctx, d);
        assert(JS_VALUE_GET_NORM_TAG(v) == JS_TAG_FLOAT64);
        assert(JS_VALUE_IS_BOTH_FLOAT(v, v));
        assert(!JS_VALUE_IS_BOTH_INT(v, v));
        assert(!JS_VALUE_HAS_REF_COUNT(v));
        assert(JS_IsNumber(v));
        decoded = JS_VALUE_GET_FLOAT64(v);
        if ((bits[i] & UINT64_C(0x7fffffffffffffff)) >
            UINT64_C(0x7ff0000000000000)) {
            assert(JS_VALUE_IS_NAN(v));
            assert(isnan(decoded));
        } else {
            assert(!JS_VALUE_IS_NAN(v));
            memcpy(&actual, &decoded, sizeof(actual));
            assert(actual == bits[i]);
        }
    }
}

static void test_pointer_tags(JSContext *ctx)
{
    static const int tags[] = {
        JS_TAG_OBJECT, JS_TAG_STRING, JS_TAG_STRING_ROPE, JS_TAG_SYMBOL,
        JS_TAG_BIG_INT, JS_TAG_VAR_REF, JS_TAG_VAR_REF_RO, JS_TAG_MODULE,
        JS_TAG_FUNCTION_BYTECODE,
    };
    _Alignas(8) uint64_t aligned;
    size_t i;
    for (i = 0; i < sizeof(tags) / sizeof(*tags); i++) {
        JSValue v = JS_MKPTR(tags[i], &aligned);
        assert(JS_VALUE_GET_TAG(v) == tags[i]);
        assert(JS_VALUE_GET_PTR(v) == &aligned);
        assert(JS_VALUE_HAS_REF_COUNT(v));
        assert(!JS_IsNumber(v));
        assert(!JS_VALUE_IS_BOTH_INT(v, v));
        assert(!JS_VALUE_IS_BOTH_FLOAT(v, v));
        assert(JS_IsObject(v) == (tags[i] == JS_TAG_OBJECT));
        assert(JS_IsString(v) == (tags[i] == JS_TAG_STRING ||
                                  tags[i] == JS_TAG_STRING_ROPE));
        assert(JS_IsSymbol(v) == (tags[i] == JS_TAG_SYMBOL));
        assert(JS_IsBigInt(ctx, v) == (tags[i] == JS_TAG_BIG_INT));
        assert(!JS_VALUE_IS_BOTH_INT(JS_NewInt32(ctx, 0), v));
        assert(!JS_VALUE_IS_BOTH_FLOAT(JS_NAN, v));
    }
#if defined(JS_COMPACT_NAN_BOXING) && defined(JS_PTR64)
    /* Synthetic addresses are decoded only, never dereferenced. Bit 48 must
       not be mistaken for a tag bit, and low subtype bits must be stripped. */
    for (i = 0; i < sizeof(tags) / sizeof(*tags); i++) {
        uintptr_t address = UINT64_C(0x0001fffffffffff8);
        JSValue v = JS_MKPTR(tags[i], (void *)address);
        assert((uintptr_t)JS_VALUE_GET_PTR(v) == address);
        assert(JS_VALUE_GET_TAG(v) == tags[i]);
    }
    assert(sizeof(JSValue) == 8);
#if defined(__SIZEOF_INT128__)
    assert(JS_LIMB_BITS == 64);
#endif
#endif
}

static void test_bigint_boundaries(JSContext *ctx)
{
    static const int64_t integers[] = {
        INT64_MIN, INT64_MAX, INT32_MIN, INT32_MAX,
        -INT64_C(140737488355329), -INT64_C(140737488355328),
        INT64_C(140737488355327), INT64_C(140737488355328),
        -1, 0, 1,
    };
    size_t i;
    for (i = 0; i < sizeof(integers) / sizeof(*integers); i++) {
        JSValue v = JS_NewBigInt64(ctx, integers[i]);
        int64_t decoded;
        assert(JS_IsBigInt(ctx, v));
        assert(JS_ToBigInt64(ctx, &decoded, v) == 0);
        assert(decoded == integers[i]);
#if defined(JS_COMPACT_NAN_BOXING) && defined(JS_PTR64)
        assert((JS_VALUE_GET_TAG(v) == JS_TAG_SHORT_BIG_INT) ==
               (integers[i] >= -INT64_C(140737488355328) &&
                integers[i] <= INT64_C(140737488355327)));
#endif
        JS_FreeValue(ctx, v);
    }
    {
        JSValue object = JS_NewObject(ctx);
        JSValue copy = JS_DupValue(ctx, object);
        JS_FreeValue(ctx, object);
        assert(JS_IsObject(copy));
        JS_FreeValue(ctx, copy);
    }
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_immediates(ctx);
    test_doubles(ctx);
    test_pointer_tags(ctx);
    test_bigint_boundaries(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
