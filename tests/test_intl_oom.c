/*
 * QuickJS native Intl embedding and allocator ownership tests
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
#ifdef CONFIG_INTL
#include "intl-allocation-probe.h"
/* Real constructor/method sequences exercise opaque state and retained ICU
   input, cached functions, exact number values, parts and range cleanup. */
static const char *const operations[] = {
    "() => new Intl.Locale('en-US').maximize().getWeekInfo()",
    "() => new Intl.Collator('de').compare('a\\u0000b', 'a\\u0000c')",
    "() => new Intl.NumberFormat('en').formatToParts(123456789012345678901n)",
    "() => new Intl.NumberFormat('en').formatRangeToParts('1.25', '2.75')",
    "() => new Intl.DateTimeFormat('en', {timeZone:'UTC'}).formatRangeToParts(0,86400000)",
    "() => new Intl.DateTimeFormat('en', {timeZone:'UTC'}).format(0)",
    "() => new Intl.PluralRules('en').selectRange(1,2)",
    "() => new Intl.ListFormat('en').formatToParts(['a','b','c'])",
    "() => new Intl.RelativeTimeFormat('en').formatToParts(-2,'days')",
    "() => new Intl.DisplayNames('en',{type:'language'}).of('fr-CA')",
    "() => {let s=new Intl.Segmenter('en',{granularity:'word'}).segment('a b'); return [s.containing(1),...s]}",
    "() => new Intl.DurationFormat('en',{style:'digital'}).formatToParts({hours:1,minutes:2,seconds:3})",
};

static void check_operation_failures(const char *source)
{
    size_t failure;
    for (failure = 1; failure < 4096; failure++) {
        AllocationProbe probe = { 0 };
        JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
        JSContext *ctx;
        JSValue function, value;
        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
        function = JS_Eval(ctx, source, strlen(source), "intl-oom-probe",
                           JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(function));
        probe.failure_at = failure;
        value = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
        probe.failure_at = 0;
        if (JS_IsException(value)) {
            JSValue error = JS_GetException(ctx);
            assert(probe.failed); /* A baseline correctness failure is visible. */
            JS_FreeValue(ctx, error);
        }
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, function);
        JS_RunGC(rt);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!probe.live);
        if (!probe.failed)
            return;
    }
    assert(!"Intl operation exceeds fault-injection bound");
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    size_t i;
    for (i = 0; i < sizeof(operations) / sizeof(operations[0]); i++)
        check_operation_failures(operations[i]);
#endif
    return 0;
}
