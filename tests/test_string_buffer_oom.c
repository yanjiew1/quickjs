/*
 * String builder allocation-failure ordering tests
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

#include "../src/quickjs/builtins/string.h"
#include "../src/quickjs/internal/string-buffer.h"
#include "oom-allocator.h"

typedef struct StringOOMState {
    OOMTestAllocator *allocator;
    JSValue long_string;
    int first_calls;
    int later_calls;
} StringOOMState;

static JSValue first_value(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    StringOOMState *state = JS_GetContextOpaque(ctx);

    if (++state->first_calls == 1)
        oom_test_begin(state->allocator, 1);
    return JS_DupValue(ctx, state->long_string);
}

static JSValue later_value(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    StringOOMState *state = JS_GetContextOpaque(ctx);

    state->later_calls++;
    return JS_UNDEFINED;
}

static void define_getter(JSContext *ctx, JSValueConst object,
                          const char *name, JSCFunction *getter)
{
    JSAtom atom = JS_NewAtom(ctx, name);
    JSValue function = JS_NewCFunction(ctx, getter, name, 0);

    assert(atom != JS_ATOM_NULL && !JS_IsException(function));
    assert(JS_DefinePropertyGetSet(ctx, object, atom, function, JS_UNDEFINED,
                                   JS_PROP_CONFIGURABLE) >= 0);
    JS_FreeAtom(ctx, atom);
}

static void test_public_string_builder_oom(int replace_all)
{
    OOMTestAllocator allocator;
    StringOOMState state = { 0 };
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, ctor, method, holder, raw, args[2], result;
    char text[10001];

    assert(ctx);
    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    state.allocator = &allocator;
    state.long_string = JS_NewString(ctx, text);
    assert(!JS_IsException(state.long_string));
    JS_SetContextOpaque(ctx, &state);

    global = JS_GetGlobalObject(ctx);
    ctor = JS_GetPropertyStr(ctx, global, "String");
    raw = JS_UNDEFINED;
    args[0] = args[1] = JS_UNDEFINED;
    if (replace_all) {
        JSValue proto = JS_GetPropertyStr(ctx, ctor, "prototype");
        method = JS_GetPropertyStr(ctx, proto, "replaceAll");
        JS_FreeValue(ctx, proto);
        holder = JS_NewString(ctx, "aaa");
        args[0] = JS_NewString(ctx, "a");
        args[1] = JS_NewCFunction(ctx, first_value, "replacement", 0);
    } else {
        method = JS_GetPropertyStr(ctx, ctor, "raw");
        raw = JS_NewObject(ctx);
        assert(JS_SetPropertyStr(ctx, raw, "length", JS_NewInt32(ctx, 2)) >= 0);
        define_getter(ctx, raw, "0", first_value);
        define_getter(ctx, raw, "1", later_value);
        holder = JS_NewObject(ctx);
        assert(JS_SetPropertyStr(ctx, holder, "raw", JS_DupValue(ctx, raw)) >= 0);
        args[0] = JS_DupValue(ctx, holder);
    }
    assert(!JS_IsException(global) && !JS_IsException(ctor) &&
           !JS_IsException(method) && !JS_IsException(holder) &&
           !JS_IsException(args[0]) && !JS_IsException(args[1]));

    result = JS_Call(ctx, method, replace_all ? holder : JS_UNDEFINED,
                     replace_all ? 2 : 1, (JSValueConst *)args);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && JS_IsException(result));
    assert(state.first_calls == 1 && state.later_calls == 0);
    oom_test_check_exception(ctx);

    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, holder);
    JS_FreeValue(ctx, raw);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, state.long_string);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}

static void test_substitution_oom(void)
{
    OOMTestAllocator allocator;
    StringOOMState state = { 0 };
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue source, matched, replacement, groups;
    StringBuffer buffer;
    char text[10005];
    int ret;

    assert(ctx);
    memset(text, 'x', 10000);
    memcpy(text + 10000, "$<x>", 4);
    source = JS_NewString(ctx, "a");
    matched = JS_NewString(ctx, "a");
    replacement = JS_NewStringLen(ctx, text, 10004);
    groups = JS_NewObject(ctx);
    assert(!JS_IsException(source) && !JS_IsException(matched) &&
           !JS_IsException(replacement) && !JS_IsException(groups));
    state.allocator = &allocator;
    state.long_string = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &state);
    define_getter(ctx, groups, "x", later_value);
    assert(string_buffer_init(ctx, &buffer, 0) == 0);

    oom_test_begin(&allocator, 1);
    ret = js_string_GetSubstitution(ctx, &buffer, matched,
                                     JS_VALUE_GET_STRING(source), 0,
                                     JS_UNDEFINED, groups, replacement,
                                     NULL, 0);
    oom_test_end(&allocator);
    assert(ret < 0 && allocator.failures == 1 && state.later_calls == 0);
    oom_test_check_exception(ctx);

    string_buffer_free(&buffer);
    JS_FreeValue(ctx, source);
    JS_FreeValue(ctx, matched);
    JS_FreeValue(ctx, replacement);
    JS_FreeValue(ctx, groups);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}

static void test_regexp_replace_oom(void)
{
    OOMTestAllocator allocator;
    StringOOMState state = { 0 };
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue regexp, method, input, callback, result;
    JSValueConst args[2];
    char text[10001];

    assert(ctx);
    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    state.allocator = &allocator;
    state.long_string = JS_NewString(ctx, text);
    JS_SetContextOpaque(ctx, &state);
    regexp = JS_Eval(ctx, "/a/g", 4, "regexp-oom.js", JS_EVAL_TYPE_GLOBAL);
    method = JS_GetProperty(ctx, regexp, JS_ATOM_Symbol_replace);
    input = JS_NewString(ctx, "aaa");
    callback = JS_NewCFunction(ctx, first_value, "replacement", 0);
    assert(!JS_IsException(state.long_string) && !JS_IsException(regexp) &&
           !JS_IsException(method) && !JS_IsException(input) &&
           !JS_IsException(callback));
    args[0] = input;
    args[1] = callback;

    result = JS_Call(ctx, method, regexp, 2, args);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && JS_IsException(result));
    assert(state.first_calls == 1);
    oom_test_check_exception(ctx);

    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, callback);
    JS_FreeValue(ctx, input);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, regexp);
    JS_FreeValue(ctx, state.long_string);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}

int main(void)
{
    test_public_string_builder_oom(0);
    test_public_string_builder_oom(1);
    test_substitution_oom();
    test_regexp_replace_oom();
    return 0;
}
