/* Private duration conversion without constructor or namespace initialization.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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
#include "../src/quickjs/builtins/duration-value.h"

#if defined(CONFIG_TEMPORAL) || defined(CONFIG_INTL)
static JSValue evaluate(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "<duration-record>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, exception);
        fprintf(stderr, "%s: %s\n", source, message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, exception);
    }
    assert(!JS_IsException(result));
    return result;
}

static void check(JSContext *ctx, const char *source, const double *expected,
                  const char *error_name)
{
    QJSTemporalDuration result;
    JSValue value = evaluate(ctx, source);
    int i, ret;

    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
        result.fields[i] = 123;
    ret = js_duration_to_record(ctx, value, &result);
    JS_FreeValue(ctx, value);
    if (error_name) {
        JSValue exception, global, constructor;
        assert(ret == -1 && JS_HasException(ctx));
        exception = JS_GetException(ctx);
        global = JS_GetGlobalObject(ctx);
        constructor = JS_GetPropertyStr(ctx, global, error_name);
        assert(JS_IsInstanceOf(ctx, exception, constructor) == 1);
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
            assert(result.fields[i] == 123);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, global);
        JS_FreeValue(ctx, exception);
    } else {
        assert(ret == 0 && !JS_HasException(ctx));
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
            assert(result.fields[i] == expected[i]);
            assert(result.fields[i] != 0 || !signbit(result.fields[i]));
        }
    }
    assert(!JS_HasException(ctx));
}

static void test_record_conversion(void)
{
    static const double zero[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static const double complete[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    static const double large[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 9223372036854775808.0};
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value;

    assert(rt);
    ctx = JS_NewContextRaw(rt);
    assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
    assert(JS_AddIntrinsicEval(ctx) == 0 && JS_AddIntrinsicProxy(ctx) == 0);
    check(ctx, "'-PT0S'", zero, NULL);
    check(ctx, "({seconds: -0})", zero, NULL);
    check(ctx, "'P1Y2M3W4DT5H6M7.008009010S'", complete, NULL);
    check(ctx, "({nanoseconds: 2 ** 63})", large, NULL);
    check(ctx, "(() => { globalThis.order = []; return new Proxy({"
          "years:1, months:2, weeks:3, days:4, hours:5, minutes:6, seconds:7,"
          "milliseconds:8, microseconds:9, nanoseconds:10}, {get(t,k) {"
          "order.push(k); return {valueOf() { order.push('value ' + k); return t[k]; }}; }}); })()",
          complete, NULL);
    value = evaluate(ctx, "order.join(',') === 'days,value days,hours,value hours,"
        "microseconds,value microseconds,milliseconds,value milliseconds,minutes,value minutes,"
        "months,value months,nanoseconds,value nanoseconds,seconds,value seconds,weeks,value weeks,years,value years'");
    assert(JS_ToBool(ctx, value) == 1);
    JS_FreeValue(ctx, value);
    check(ctx, "({days: 1, hours: -1})", NULL, "RangeError");
    check(ctx, "({days: 1, hours: -1, get years() { throw new URIError('late'); }})",
          NULL, "URIError");
    check(ctx, "({years: 2 ** 32})", NULL, "RangeError");
    check(ctx, "({seconds: 2 ** 53})", NULL, "RangeError");
    check(ctx, "({seconds: 1.5})", NULL, "RangeError");
    check(ctx, "({seconds: 1n})", NULL, "TypeError");
    check(ctx, "({seconds: undefined})", NULL, "TypeError");
    check(ctx, "({get days() { throw new URIError('stop'); },"
               "get hours() { throw new Error('unreachable'); }})", NULL, "URIError");
    check(ctx, "'PT1S\\u0000'", NULL, "RangeError");
    check(ctx, "({toString() { throw new Error('not coerced'); }})", NULL, "TypeError");
    check(ctx, "null", NULL, "TypeError");
    value = evaluate(ctx, "typeof Temporal === 'undefined'");
    assert(JS_ToBool(ctx, value) == 1);
    JS_FreeValue(ctx, value);

#ifdef CONFIG_TEMPORAL
    assert(JS_AddIntrinsicTemporal(ctx) == 0);
    check(ctx, "(() => { const d = new Temporal.Duration(1,2,3,4,5,6,7,8,9,10);"
          "Object.defineProperty(d,'years',{get(){throw new Error('must copy slots');}});"
          "return d; })()", complete, NULL);
#endif
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}
#endif

int main(void)
{
#if defined(CONFIG_TEMPORAL) || defined(CONFIG_INTL)
    test_record_conversion();
#endif
    return 0;
}
