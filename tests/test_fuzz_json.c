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
#include <stdint.h>
#include <string.h>

#include "quickjs.h"

static const uint8_t *expected_input;
static size_t expected_length;
static int parse_calls;

static JSValue inspect_json_parse(JSContext *ctx, const char *input,
                                  size_t length, const char *filename)
{
    assert(length == expected_length);
    assert(memcmp(input, expected_input, length) == 0);
    assert(input[length] == '\0');
    parse_calls++;
    return JS_ParseJSON(ctx, input, length, filename);
}

#define JS_ParseJSON inspect_json_parse
#define LLVMFuzzerTestOneInput fuzz_json_test_one_input
#include "../fuzz/fuzz_json.c"
#undef LLVMFuzzerTestOneInput
#undef JS_ParseJSON

static void check_json_input(const uint8_t *input, size_t length)
{
    int previous = parse_calls;
    expected_input = input;
    expected_length = length;
    assert(fuzz_json_test_one_input(input, length) == 0);
    assert(parse_calls == previous + 1);
}

int main(void)
{
    static const uint8_t embedded_nul[] = {'1', 0, '2'};
    static const uint8_t quoted[] = "\"a'b\"";

    check_json_input((const uint8_t *)"", 0);
    check_json_input(quoted, sizeof(quoted) - 1);
    check_json_input(embedded_nul, sizeof(embedded_nul));
    return 0;
}
