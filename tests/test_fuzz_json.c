/*
 * QuickJS native fuzz harness regression tests
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickjs.h"
#include "quickjs-libc.h"

static const char *expected_direct_script;
static int direct_calls;

static JSValue inspect_json_eval(JSContext *ctx, const char *input,
                                 size_t length, const char *filename,
                                 int flags)
{
    if (strcmp(filename, "<json-direct>") == 0) {
        assert(length == strlen(expected_direct_script));
        assert(memcmp(input, expected_direct_script, length + 1) == 0);
        direct_calls++;
    }
    return JS_Eval(ctx, input, length, filename, flags);
}

#define JS_Eval inspect_json_eval
#define LLVMFuzzerTestOneInput fuzz_json_test_one_input
#include "../fuzz/fuzz_json.c"
#undef LLVMFuzzerTestOneInput
#undef JS_Eval

static void check_json_input(const char *input, const char *expected)
{
    int previous = direct_calls;

    expected_direct_script = expected;
    assert(fuzz_json_test_one_input((const uint8_t *)input,
                                    strlen(input)) == 0);
    assert(direct_calls == previous + 1);
}

int main(void)
{
    /* Reuse the actual harness stack with long then short inputs. */
    check_json_input("1111111111111111111111111111111111111111111111111111111111111111",
                     "JSON.parse('1111111111111111111111111111111111111111111111111111111111111111');");
    check_json_input("1", "JSON.parse('1');");
    check_json_input("\"a'b\"", "JSON.parse('\"a\\'b\"');");
    return 0;
}
