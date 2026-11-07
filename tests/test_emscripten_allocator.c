/*
 * Emscripten default allocator accounting test
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
#include <stdint.h>
#include <stdlib.h>

/* Compile the allocator's Emscripten size-header path on the host. */
#define __EMSCRIPTEN__ 1
#include "../src/quickjs/allocator.c"
#undef __EMSCRIPTEN__

int main(void)
{
    JSMallocState state = {0};
    void *ptr[3];
    int i;

    state.malloc_limit = 3 * (300 + JS_DEF_MALLOC_OVERHEAD);
    for (i = 0; i < 3; i++) {
        ptr[i] = js_def_malloc(&state, 300);
        assert(ptr[i] && js_def_malloc_usable_size(ptr[i]) == 300);
        assert((uintptr_t)ptr[i] % _Alignof(max_align_t) == 0);
    }
    assert(js_def_malloc(&state, 300) == NULL);
    for (i = 0; i < 3; i++)
        js_def_free(&state, ptr[i]);
    assert(state.malloc_count == 0 && state.malloc_size == 0);

    state.malloc_limit = 200;
    ptr[0] = js_def_malloc(&state, 50);
    assert(ptr[0]);
    ptr[0] = js_def_realloc(&state, ptr[0], 100);
    assert(ptr[0] && js_def_malloc_usable_size(ptr[0]) == 100);
    assert(js_def_realloc(&state, ptr[0], 190) == NULL);
    assert(js_def_malloc_usable_size(ptr[0]) == 100);
    ptr[0] = js_def_realloc(&state, ptr[0], 30);
    assert(ptr[0] && js_def_malloc_usable_size(ptr[0]) == 30);
    js_def_free(&state, ptr[0]);
    assert(state.malloc_count == 0 && state.malloc_size == 0);

    ptr[0] = js_def_malloc(&state, 20);
    assert(ptr[0]);
    assert(js_def_realloc(&state, ptr[0], 0) == NULL);
    assert(state.malloc_count == 0 && state.malloc_size == 0);
    return 0;
}
