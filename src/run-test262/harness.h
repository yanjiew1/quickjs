/*
 * ECMA Test 262 Runner harness private interfaces
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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
#ifndef RUN_TEST262_HARNESS_H
#define RUN_TEST262_HARNESS_H

#include <stdint.h>
#include <stdio.h>

#include "../../quickjs.h"

#if defined(__GNUC__) || defined(__clang__)
#define RUN_TEST262_HARNESS_INTERNAL __attribute__((visibility("hidden")))
#else
#define RUN_TEST262_HARNESS_INTERNAL
#endif

typedef struct Test262Harness Test262Harness;

RUN_TEST262_HARNESS_INTERNAL Test262Harness *test262_harness_new(void);
RUN_TEST262_HARNESS_INTERNAL void test262_harness_free(Test262Harness *harness);
RUN_TEST262_HARNESS_INTERNAL void test262_harness_set_output(
    Test262Harness *harness, FILE *output);
RUN_TEST262_HARNESS_INTERNAL void test262_harness_reset_async(
    Test262Harness *harness);
RUN_TEST262_HARNESS_INTERNAL int test262_harness_async_state(
    const Test262Harness *harness);
RUN_TEST262_HARNESS_INTERNAL void test262_harness_add_helpers(JSContext *ctx);
RUN_TEST262_HARNESS_INTERNAL void test262_harness_print_value(
    JSContext *ctx, JSValueConst value);
RUN_TEST262_HARNESS_INTERNAL void test262_harness_free_agents(JSContext *ctx);

#endif /* RUN_TEST262_HARNESS_H */
