/*
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
#include "quickjs-libc.h"

static int fail_context;

static JSRuntime *test_new_runtime(void)
{
    return fail_context ? JS_NewRuntime() : NULL;
}

static JSContext *test_new_context(JSRuntime *rt)
{
    (void)rt;
    return NULL;
}

#define JS_NewRuntime test_new_runtime
#define JS_NewContext test_new_context
#define main qjsc_test_main
#include "../tools/qjsc.c"
#undef main

int main(void)
{
    char program[] = "qjsc", compile[] = "-c", output[] = "-o";
    char filename[] = "qjsc-startup-oom.tmp", input[] = "input.js";
    char *args[] = { program, compile, output, filename, input };
    char contents[16];
    FILE *file;
    int i;

    for (i = 0; i < 2; i++) {
        file = fopen(args[3], "w");
        assert(file && fputs("preserve", file) >= 0 && !fclose(file));
        fail_context = i;
        assert(qjsc_test_main(5, args) == 1);
        file = fopen(args[3], "r");
        assert(file && fgets(contents, sizeof(contents), file));
        assert(!strcmp(contents, "preserve") && !fclose(file));
    }
    assert(!remove(args[3]));
    return 0;
}
