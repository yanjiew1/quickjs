/*
 * Map BigInt hash distribution regression
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
#include "../src/quickjs/builtins/collections.h"

int main(void)
{
    static const char source[] =
        "(() => {"
        "  const map = new Map();"
        "  for (let i = 1n; i <= 128n; i++) map.set(i << 32n, i);"
        "  return map;"
        "})()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue map;
    JSMapState *state;
    uint32_t occupied = 0, longest = 0, i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    map = JS_Eval(ctx, source, strlen(source), "map-hash.js",
                  JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(map));
    assert(JS_VALUE_GET_OBJ(map)->class_id == JS_CLASS_MAP);
    state = JS_VALUE_GET_OBJ(map)->u.map_state;
    assert(state->record_count == 128 && state->hash_size >= 128);
    for (i = 0; i < state->hash_size; i++) {
        JSMapRecord *record;
        uint32_t chain = 0;

        for (record = state->hash_table[i]; record; record = record->hash_next)
            chain++;
        if (chain)
            occupied++;
        if (chain > longest)
            longest = chain;
    }
    assert(occupied >= 32 && longest <= 16);

    JS_FreeValue(ctx, map);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
