/* Bytecode diagnostic allocation failure and exception preservation.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include "quickjs.h"

static void *dump_mallocz_rt(JSRuntime *rt, size_t size);

/* Exercise the diagnostic owner without changing the engine archive's
   opcode metadata layout when its normal build has dumps disabled. */
#undef DUMP_BYTECODE
#define DUMP_BYTECODE 7
#define opcode_info test_dump_opcode_info
#define dump_byte_code test_dump_byte_code
#define js_dump_function_bytecode test_dump_function_bytecode
#define js_mallocz_rt dump_mallocz_rt
#include "../src/quickjs/compiler/bytecode-dump.c"
#undef js_mallocz_rt
#undef js_dump_function_bytecode
#undef dump_byte_code
#undef opcode_info

/* The fixture uses only a branch to an ordinary return instruction. */
const JSOpCode test_dump_opcode_info[OP_COUNT + (OP_TEMP_END - OP_TEMP_START)] = {
    [OP_goto] = { "goto", 5, 0, 0, OP_FMT_label },
    [OP_return_undef] = { "return_undef", 1, 0, 0, OP_FMT_none },
};

static int fail_marker_alloc, failures;

static void *dump_mallocz_rt(JSRuntime *rt, size_t size)
{
    if (fail_marker_alloc) {
        failures++;
        return NULL;
    }
    return js_mallocz_rt(rt, size);
}

static void dump_fixture(JSContext *ctx)
{
    uint8_t bytecode[] = {
        OP_goto, 4, 0, 0, 0, OP_return_undef,
    };

    put_u32(bytecode + 1, 4);
    test_dump_byte_code(ctx, 3, bytecode, sizeof(bytecode), NULL,
                        NULL, 0, NULL, 0, NULL, 0, NULL, 0,
                        NULL, NULL, NULL);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue error;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(!JS_HasException(ctx));

    fail_marker_alloc = 1;
    dump_fixture(ctx);
    assert(failures == 1);
    assert(!JS_HasException(ctx));

    JS_Throw(ctx, JS_NewInt32(ctx, 123));
    dump_fixture(ctx);
    assert(failures == 2);
    error = JS_GetException(ctx);
    assert(JS_VALUE_GET_TAG(error) == JS_TAG_INT);
    assert(JS_VALUE_GET_INT(error) == 123);
    JS_FreeValue(ctx, error);

    fail_marker_alloc = 0;
    dump_fixture(ctx);
    assert(failures == 2);
    assert(!JS_HasException(ctx));

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
