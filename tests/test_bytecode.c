/*
 * QuickJS bytecode serialization tests
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
#include <string.h>

#include "quickjs.h"
#include "../src/quickjs/internal/bytecode.h"
#include "../src/quickjs/internal/function.h"

static void test_bytecode_roundtrip(const char *source)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled, loaded, result;
    uint8_t *buf;
    size_t len;
    int32_t value;
    assert(rt);
    JS_SetStripInfo(rt, 0);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, strlen(source), "roundtrip.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf && len > 0);
    JS_FreeValue(ctx, compiled);
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(ctx, buf);
    assert(!JS_IsException(loaded));
    result = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(result));
    assert(JS_ToInt32(ctx, &value, result) == 0 && value == 42);
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_import_bytecode_roundtrip(void)
{
    static const char source[] =
        "import { value as imported } from './roundtrip-module.js';"
        "function check(error, code) { try { eval(code); } catch (e) {"
        " if (e instanceof error) return; throw e; }"
        " throw new Error('exception expected'); }"
        "check(ReferenceError, 'value = 1');"
        "check(TypeError, 'imported = 1');"
        "export const value = 1;"
        "globalThis.roundtrip_result = 42;";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *compile_ctx, *ctx, *job_ctx;
    JSValue compiled, loaded, result, global, value;
    uint8_t *buf;
    size_t len;
    int ret;
    int32_t number;

    assert(rt);
    JS_SetStripInfo(rt, 0);
    compile_ctx = JS_NewContext(rt);
    assert(compile_ctx);
    compiled = JS_Eval(compile_ctx, source, sizeof(source) - 1,
                       "roundtrip-module.js",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(compile_ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf && len > 0);
    JS_FreeValue(compile_ctx, compiled);
    ctx = JS_NewContext(rt);
    assert(ctx);
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(compile_ctx, buf);
    assert(!JS_IsException(loaded));
    assert(JS_ResolveModule(ctx, loaded) == 0);
    result = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(result));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        continue;
    assert(ret == 0);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    global = JS_GetGlobalObject(ctx);
    value = JS_GetPropertyStr(ctx, global, "roundtrip_result");
    assert(JS_ToInt32(ctx, &number, value) == 0 && number == 42);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeContext(compile_ctx);
    JS_FreeRuntime(rt);
}

static void test_class_strict_region_bytecode(const char *source, int expected)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled;
    JSFunctionBytecode *b;
    int pos, count = 0, catches = 0;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, strlen(source), "strict-region.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    b = JS_VALUE_GET_PTR(compiled);
    for (pos = 0; pos < b->byte_code_len;) {
        int op = b->byte_code_buf[pos];
        assert(op < OP_COUNT && short_opcode_info(op).size > 0);
        if (op == OP_set_strict)
            count++;
        if (op == OP_catch)
            catches++;
        if (op >= OP_with_get_var && op <= OP_with_get_ref)
            assert(b->byte_code_buf[pos + 9] <= 1);
        pos += short_opcode_info(op).size;
        assert(pos <= b->byte_code_len);
    }
    if (count != expected)
        fprintf(stderr, "strict toggles: expected %d, got %d for %s\n",
                expected, count, source);
    assert(count == expected);
    assert(catches == (expected != 0));
    JS_FreeValue(ctx, compiled);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue class_strict_exotic_get(JSContext *ctx, JSValueConst obj,
                                      JSAtom atom, JSValueConst receiver)
{
    JSAtom locked = JS_NewAtom(ctx, "locked");
    int ret;

    if (locked == JS_ATOM_NULL)
        return JS_EXCEPTION;
    ret = JS_DeleteProperty(ctx, obj, locked, JS_PROP_THROW_STRICT);
    JS_FreeAtom(ctx, locked);
    if (ret < 0)
        return JS_EXCEPTION;
    assert(ret == 0);
    return JS_NewString(ctx, "method");
}

static JSValue class_strict_exotic_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val, int argc,
                                       JSValueConst *argv, int flags)
{
    return class_strict_exotic_get(ctx, func_obj, JS_ATOM_NULL, this_val);
}

static void test_class_strict_exotics(void)
{
    JSClassExoticMethods exotic = { .get_property = class_strict_exotic_get };
    JSClassDef def = { .class_name = "StrictProbe",
                      .call = class_strict_exotic_call, .exotic = &exotic };
    static const char *const expressions[] = {
        "strictProbe.value", "strictProbe()",
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSClassID class_id = 0;
    JSValue global, obj, result;
    char source[1024];
    size_t i;
    int len;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&class_id);
    assert(JS_NewClass(rt, class_id, &def) == 0);
    obj = JS_NewObjectClass(ctx, class_id);
    assert(!JS_IsException(obj));
    assert(JS_DefinePropertyValueStr(ctx, obj, "locked", JS_TRUE, 0) == 1);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "strictProbe", obj) == 1);
    JS_FreeValue(ctx, global);

    for (i = 0; i < sizeof(expressions) / sizeof(expressions[0]); i++) {
        len = snprintf(source, sizeof(source),
            "(function() {"
            " if (%s !== 'method') throw Error('sloppy entry');"
            " class Plain {}"
            " if (%s !== 'method') throw Error('normal class exit');"
            " let caught = false;"
            " try { (class { [%s]() {} }); }"
            " catch (e) { if (!(e instanceof TypeError)) throw e; caught = true; }"
            " if (!caught) throw Error('class must be strict');"
            " if (%s !== 'method') throw Error('abrupt class exit');"
            " return true; })()",
            expressions[i], expressions[i], expressions[i], expressions[i]);
        assert(len > 0 && (size_t)len < sizeof(source));
        result = JS_Eval(ctx, source, len, "class-exotic.js", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(result));
        assert(JS_ToBool(ctx, result) == 1);
        JS_FreeValue(ctx, result);

        len = snprintf(source, sizeof(source),
            "(function() { 'use strict';"
            " try { (class { [%s]() {} }); }"
            " catch (e) { return e instanceof TypeError; }"
            " return false; })()", expressions[i]);
        assert(len > 0 && (size_t)len < sizeof(source));
        result = JS_Eval(ctx, source, len, "class-exotic.js", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(result));
        assert(JS_ToBool(ctx, result) == 1);
        JS_FreeValue(ctx, result);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_class_strict_region_bytecode("class Plain {}", 3);
    test_class_strict_region_bytecode(
        "class Plain { method(obj) { obj.x = 1; } }", 3);
    test_class_strict_region_bytecode(
        "class Plain { [(Object.freeze({}).x = 1)]() {} }", 3);
    test_class_strict_region_bytecode("class Plain { [eval('0')]() {} }", 3);
    test_class_strict_region_bytecode(
        "with ({key: 'method'}) { class C { [key]() {} } }", 3);
    test_class_strict_region_bytecode(
        "(class Outer { [(class Inner {})]() {} });", 3);
    test_class_strict_region_bytecode(
        "'use strict'; class Plain { [eval('0')]() {} }", 0);
    test_class_strict_exotics();
    test_bytecode_roundtrip("function saved() { return 42; } saved();");
    test_bytecode_roundtrip(
        "(function() { let effects = 0; const binding = 1;"
        " function saved() { with ({}) { binding ||= ++effects;"
        " try { binding = ++effects; } catch (e) {"
        " if (!(e instanceof TypeError)) return 0; } }"
        " return effects === 1 && binding === 1 ? 42 : 0; }"
        " return saved(); })();");
    test_import_bytecode_roundtrip();
    return 0;
}
