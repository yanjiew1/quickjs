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

#include "libregexp.h"
#include "quickjs-libc.h"

static int exec_calls, exhausted_inputs, matches;

static int inspect_regexp_exec(uint8_t **capture, const uint8_t *bytecode,
                               const uint8_t *input, int index, int length,
                               int input_type, void *opaque);

#define lre_exec inspect_regexp_exec
#define LLVMFuzzerTestOneInput fuzz_regexp_timeout_input
#include "../fuzz/fuzz_regexp.c"
#undef LLVMFuzzerTestOneInput
#undef lre_exec

static void exhaust_actual_timeout(void *opaque)
{
    unsigned int polls = 0;

    /* Exercise the actual harness callback, not a copy of its budget logic. */
    while (!lre_check_timeout(opaque))
        assert(++polls < 1000);
}

static int inspect_regexp_exec(uint8_t **capture, const uint8_t *bytecode,
                               const uint8_t *input, int index, int length,
                               int input_type, void *opaque)
{
    int result;

    /* A second entry must not inherit the first entry's exhausted counter. */
    assert(lre_check_timeout(opaque) == 0);
    if (exec_calls++ == 0) {
        exhaust_actual_timeout(opaque);
        exhausted_inputs++;
        return LRE_RET_TIMEOUT;
    }
    result = lre_exec(capture, bytecode, input, index, length,
                      input_type, opaque);
    assert(result == 1);
    matches++;
    return result;
}

int main(void)
{
    static const uint8_t input[] = { 'a', 0, 'a' };
    static const uint8_t missing_delimiter[] = { 'a' };

    assert(fuzz_regexp_timeout_input(input, sizeof(input)) == 0);
    assert(exec_calls == 1 && exhausted_inputs == 1 && matches == 0);
    assert(fuzz_regexp_timeout_input(input, sizeof(input)) == 0);
    assert(exec_calls == 2 && exhausted_inputs == 1 && matches == 1);

    /* Early-returning entries also start a new input's budget. */
    exhaust_actual_timeout(NULL);
    assert(fuzz_regexp_timeout_input(missing_delimiter,
                                     sizeof(missing_delimiter)) == 0);
    assert(exec_calls == 2 && lre_check_timeout(NULL) == 0);
    exhaust_actual_timeout(NULL);
    assert(fuzz_regexp_timeout_input(input, 0) == 0);
    assert(exec_calls == 2 && lre_check_timeout(NULL) == 0);
    return 0;
}
