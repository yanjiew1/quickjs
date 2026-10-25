/* Small public embedding helpers.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
#include <math.h>
#include <string.h>
#include "quickjs.h"

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "<embedding-helpers>",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void test_class_names(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSClassID hole = 0, registered = 0;
    JSClassDef def = { .class_name = "EmbeddingSample" };
    JSAtom atom, retained;
    const char *name;

    assert(JS_GetClassName(rt, JS_INVALID_CLASS_ID) == JS_ATOM_NULL);
    assert(JS_GetClassName(rt, (JSClassID)-1) == JS_ATOM_NULL);
    JS_NewClassID(&hole);
    JS_NewClassID(&registered);
    assert(JS_GetClassName(rt, registered) == JS_ATOM_NULL);
    assert(JS_NewClass(rt, registered, &def) == 0);
    assert(JS_GetClassName(rt, hole) == JS_ATOM_NULL);
    atom = JS_GetClassName(rt, registered);
    assert(atom != JS_ATOM_NULL);
    retained = JS_DupAtomRT(rt, atom);
    assert(retained == atom);
    JS_FreeAtomRT(rt, atom);
    name = JS_AtomToCString(ctx, retained);
    assert(name && !strcmp(name, "EmbeddingSample"));
    JS_FreeCString(ctx, name);
    JS_FreeAtomRT(rt, retained);
    atom = JS_NewAtom(ctx, "retained without a context");
    assert(atom != JS_ATOM_NULL);
    retained = JS_DupAtomRT(rt, atom);
    JS_FreeAtom(ctx, atom);
    name = JS_AtomToCString(ctx, retained);
    assert(name && !strcmp(name, "retained without a context"));
    JS_FreeCString(ctx, name);
    JS_FreeAtomRT(rt, retained);
}

static void test_to_number(JSContext *ctx)
{
    const char *input[] = { "null", "true", "' 12.5 '", "'-0'", "undefined" };
    const double expected[] = { 0, 1, 12.5, -0.0, NAN };
    const char *errors[] = { "Symbol('x')", "1n", "({ valueOf() { throw 71; } })" };
    size_t i;
    JSValue value, number, check;
    double d;

    for (i = 0; i < sizeof(input) / sizeof(input[0]); i++) {
        value = eval(ctx, input[i]);
        number = JS_ToNumber(ctx, value);
        assert(JS_IsNumber(number));
        assert(JS_ToFloat64(ctx, &d, number) == 0);
        if (isnan(expected[i]))
            assert(isnan(d));
        else
            assert(d == expected[i] && !!signbit(d) == !!signbit(expected[i]));
        JS_FreeValue(ctx, number);
        JS_FreeValue(ctx, value);
    }
    value = eval(ctx,
        "globalThis.coercions = 0; ({"
        " [Symbol.toPrimitive](hint) { coercions++;"
        "   if (hint !== 'number') throw 1; return '42'; },"
        " valueOf() { throw 2; } })");
    number = JS_ToNumber(ctx, value);
    assert(JS_IsNumber(number));
    assert(JS_ToFloat64(ctx, &d, number) == 0 && d == 42);
    check = eval(ctx, "coercions === 1");
    assert(JS_ToBool(ctx, check) == 1);
    JS_FreeValue(ctx, check);
    JS_FreeValue(ctx, number);
    JS_FreeValue(ctx, value);
    for (i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        value = eval(ctx, errors[i]);
        number = JS_ToNumber(ctx, value);
        assert(JS_IsException(number) && JS_HasException(ctx));
        check = JS_GetException(ctx);
        if (i == 2)
            assert(JS_IsNumber(check) && JS_VALUE_GET_INT(check) == 71);
        else {
            JSValue type;
            const char *name;

            assert(JS_IsError(ctx, check));
            type = JS_GetPropertyStr(ctx, check, "name");
            name = JS_ToCString(ctx, type);
            assert(name && !strcmp(name, "TypeError"));
            JS_FreeCString(ctx, name);
            JS_FreeValue(ctx, type);
        }
        JS_FreeValue(ctx, check);
        JS_FreeValue(ctx, value);
    }
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    const char *version = JS_GetVersion();

    assert(rt && version && version[0] && !strcmp(version, JS_GetVersion()));
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetGCThreshold(rt, 123456789);
    assert(JS_GetGCThreshold(rt) == 123456789);
    JS_SetGCThreshold(rt, (size_t)-1);
    assert(JS_GetGCThreshold(rt) == (size_t)-1);
    test_class_names(ctx);
    test_to_number(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
