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
#include "oom-allocator.h"
#include "../src/quickjs/internal/bytecode.h"
#include "../src/quickjs/internal/function.h"
#include "../src/quickjs/compiler/compiler-internal.h"
#include "../src/quickjs/internal/bytecode-format.h"

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

typedef struct ModuleNamesLoader {
    JSModuleDef *source;
} ModuleNamesLoader;

static JSModuleDef *module_names_loader(JSContext *ctx, const char *module_name,
                                        void *opaque)
{
    static const char source[] =
        "export let value = 1; export { value as '*'};"
        "export function update(next) { value = next; }";
    ModuleNamesLoader *loader = opaque;
    JSValue compiled;

    assert(strcmp(module_name, "module-names-source") == 0);
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, module_name,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled))
        return NULL;
    loader->source = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    return loader->source;
}

static void test_module_names_bytecode_roundtrip(void)
{
    static const char source[] =
        "import * as namespace from 'module-names-source';"
        "import { '*' as imported } from 'module-names-source';"
        "export { namespace as 'imported namespace',"
        " imported as 'imported star' };"
        "export { '*' } from 'module-names-source';"
        "export * as 'direct namespace' from 'module-names-source';";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *compile_ctx, *ctx, *job_ctx;
    JSModuleDef *module;
    ModuleNamesLoader compile_loader = { NULL }, loader = { NULL };
    JSValue compiled, loaded, result, ns, source_ns, value, update, arg;
    uint8_t *buf;
    size_t len;
    int32_t number;
    int ret, i;
    uint8_t version;
    const char *namespace_names[] = { "imported namespace", "direct namespace" };
    const char *binding_names[] = { "*", "imported star" };

    assert(rt);
    JS_SetStripInfo(rt, 0);
    compile_ctx = JS_NewContext(rt);
    assert(compile_ctx);
    JS_SetModuleLoaderFunc(rt, NULL, module_names_loader, &compile_loader);
    compiled = JS_Eval(compile_ctx, source, sizeof(source) - 1,
                       "module-names-roundtrip",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    assert(compile_loader.source);
    buf = JS_WriteObject(compile_ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf && len > 0);
    JS_FreeValue(compile_ctx, compiled);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc(rt, NULL, module_names_loader, &loader);
    version = buf[0];
    buf[0] = version - 1;
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    assert(JS_IsException(loaded));
    JS_FreeValue(ctx, JS_GetException(ctx));
    buf[0] = version;
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(compile_ctx, buf);
    assert(!JS_IsException(loaded));
    module = JS_VALUE_GET_PTR(loaded);
    assert(JS_ResolveModule(ctx, loaded) == 0);
    result = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(result));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        continue;
    assert(ret == 0);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    assert(loader.source && loader.source != compile_loader.source);
    ns = JS_GetModuleNamespace(ctx, module);
    source_ns = JS_GetModuleNamespace(ctx, loader.source);
    assert(!JS_IsException(ns) && !JS_IsException(source_ns));
    for (i = 0; i < 2; i++) {
        value = JS_GetPropertyStr(ctx, ns, namespace_names[i]);
        assert(JS_VALUE_GET_TAG(value) == JS_TAG_OBJECT);
        assert(JS_VALUE_GET_PTR(value) == JS_VALUE_GET_PTR(source_ns));
        JS_FreeValue(ctx, value);
    }
    for (number = 1; number <= 3; number += 2) {
        for (i = 0; i < 2; i++) {
            int32_t actual;
            value = JS_GetPropertyStr(ctx, ns, binding_names[i]);
            assert(JS_ToInt32(ctx, &actual, value) == 0 && actual == number);
            JS_FreeValue(ctx, value);
        }
        if (number == 1) {
            update = JS_GetPropertyStr(ctx, source_ns, "update");
            arg = JS_NewInt32(ctx, 3);
            value = JS_Call(ctx, update, JS_UNDEFINED, 1, &arg);
            assert(!JS_IsException(value));
            JS_FreeValue(ctx, value);
            JS_FreeValue(ctx, arg);
            JS_FreeValue(ctx, update);
        }
    }
    JS_FreeValue(ctx, source_ns);
    JS_FreeValue(ctx, ns);
    JS_FreeContext(ctx);
    JS_FreeContext(compile_ctx);
    JS_FreeRuntime(rt);
}

static uint32_t module_bytecode_get_leb128(const uint8_t **ptr,
                                           const uint8_t *end)
{
    uint32_t value = 0;
    unsigned shift = 0;
    uint8_t byte;

    do {
        assert(*ptr < end && shift < 32);
        byte = *(*ptr)++;
        if (shift == 28)
            assert((byte & 0xf0) == 0);
        value |= (uint32_t)(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return value;
}

static void test_module_names_invalid_export_kind(void)
{
    static const char source[] = "export const value = 42;";
    static const uint8_t invalid_kinds[] = { 3, 255 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *job_ctx;
    JSModuleDef *module;
    JSValue compiled, loaded, result, ns, value;
    uint8_t *buf, saved_kind;
    const uint8_t *ptr, *end;
    size_t len, kind_offset, string_bytes;
    uint32_t atom_count, encoded_length, i;
    int ret;
    int32_t number;

    assert(rt);
    JS_SetStripInfo(rt, 0);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, sizeof(source) - 1,
                       "module-invalid-export-kind",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf && len > 0);
    JS_FreeValue(ctx, compiled);

    /* Locate the first kind in a serialized module with no dependencies. */
    ptr = buf + 1;
    end = buf + len;
    atom_count = module_bytecode_get_leb128(&ptr, end);
    for (i = 0; i < atom_count; i++) {
        encoded_length = module_bytecode_get_leb128(&ptr, end);
        string_bytes = (size_t)(encoded_length >> 1)
            * (1 + (encoded_length & 1));
        assert(string_bytes <= (size_t)(end - ptr));
        ptr += string_bytes;
    }
    assert(ptr < end);
    ptr++; /* module tag */
    module_bytecode_get_leb128(&ptr, end); /* module name */
    assert(module_bytecode_get_leb128(&ptr, end) == 0); /* dependencies */
    assert(module_bytecode_get_leb128(&ptr, end) == 1); /* exports */
    assert(ptr < end);
    kind_offset = (size_t)(ptr - buf);
    saved_kind = buf[kind_offset];
    assert(saved_kind == 0);
    for (i = 0; i < sizeof(invalid_kinds) / sizeof(invalid_kinds[0]); i++) {
        buf[kind_offset] = invalid_kinds[i];
        loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
        assert(JS_IsException(loaded));
        JS_FreeValue(ctx, JS_GetException(ctx));
        JS_RunGC(rt);
    }
    buf[kind_offset] = saved_kind;
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(ctx, buf);
    assert(!JS_IsException(loaded));
    module = JS_VALUE_GET_PTR(loaded);
    assert(JS_ResolveModule(ctx, loaded) == 0);
    result = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(result));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        continue;
    assert(ret == 0);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    ns = JS_GetModuleNamespace(ctx, module);
    assert(!JS_IsException(ns));
    value = JS_GetPropertyStr(ctx, ns, "value");
    assert(JS_ToInt32(ctx, &number, value) == 0 && number == 42);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, ns);
    JS_FreeContext(ctx);
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

static void test_writer_allocation_failure(int kind)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue input, decoded, result;
        uint8_t *buf, **sab = (uint8_t **)(uintptr_t)1;
        size_t len = 17, sab_count = 17;
        char text[2048];
        int flags = kind == 2 ? JS_WRITE_OBJ_BYTECODE : 0;

        assert(ctx);
        memset(text, 'x', sizeof(text));
        if (kind == 0) {
            input = JS_NewStringLen(ctx, text, sizeof(text));
        } else if (kind == 1) {
            input = JS_NewObject(ctx);
            text[sizeof(text) - 1] = 0;
            assert(JS_DefinePropertyValueStr(ctx, input, text,
                        JS_NewInt32(ctx, 42), JS_PROP_C_W_E) == 1);
        } else {
            const char *source = "(() => { let value = 40; return value + 2; })()";
            input = JS_Eval(ctx, source, strlen(source), "writer-oom.js",
                            JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        }
        assert(!JS_IsException(input));
        oom_test_begin(&allocator, fail_at);
        buf = JS_WriteObject2(ctx, &len, input, flags, &sab, &sab_count);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 64);
        } else {
            assert(allocator.failures == 1);
        }
        if (!buf) {
            assert(fail_at && len == 0 && sab == NULL && sab_count == 0);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx) && len > 0);
            decoded = JS_ReadObject(ctx, buf, len,
                                    kind == 2 ? JS_READ_OBJ_BYTECODE : 0);
            assert(!JS_IsException(decoded));
            if (kind == 0) {
                assert(JS_StrictEq(ctx, input, decoded));
            } else if (kind == 1) {
                result = JS_GetPropertyStr(ctx, decoded, text);
                assert(JS_StrictEq(ctx, result, JS_NewInt32(ctx, 42)));
                JS_FreeValue(ctx, result);
            } else {
                result = JS_EvalFunction(ctx, decoded);
                decoded = JS_UNDEFINED;
                assert(JS_StrictEq(ctx, result, JS_NewInt32(ctx, 42)));
                JS_FreeValue(ctx, result);
            }
            JS_FreeValue(ctx, decoded);
            js_free(ctx, buf);
            js_free(ctx, sab);
        }
        JS_FreeValue(ctx, input);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_integer_folding_bytecode_flags(const char *source, int opcode,
                                                int expected, uint32_t flags)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled;
    JSFunctionBytecode *b;
    int pos, count = 0;

    assert(rt);
    assert(JS_GetCompilerOptimizationFlags(rt) ==
           (JS_COMPILER_OPT_CONSTANT_FOLDING | JS_COMPILER_OPT_CONSTANT_PROPAGATION));
    JS_SetCompilerOptimizationFlags(rt, flags);
    assert(JS_GetCompilerOptimizationFlags(rt) == flags);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, strlen(source), "constant-folding.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    b = JS_VALUE_GET_PTR(compiled);
    for (pos = 0; pos < b->byte_code_len;) {
        int op = b->byte_code_buf[pos];
        assert(op < OP_COUNT && short_opcode_info(op).size > 0);
        count += op == opcode;
#if SHORT_OPCODES
        count += (opcode == OP_if_false && op == OP_if_false8) ||
            (opcode == OP_if_true && op == OP_if_true8);
#endif
        pos += short_opcode_info(op).size;
        assert(pos <= b->byte_code_len);
    }
    if (count != expected)
        fprintf(stderr, "opcode %d: expected %d, got %d for %s\n",
                opcode, expected, count, source);
    assert(count == expected);
    JS_FreeValue(ctx, compiled);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_local_constant_bytecode(const char *source, uint32_t flags,
                                         int reads, int adds)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue function;
    JSFunctionBytecode *b;
    int pos, actual_reads = 0, actual_adds = 0;

    assert(rt);
    JS_SetCompilerOptimizationFlags(rt, flags);
    ctx = JS_NewContext(rt);
    assert(ctx);
    function = JS_Eval(ctx, source, strlen(source), "local-constant.js", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(function));
    b = JS_GetFunctionBytecode(function);
    assert(b);
    for (pos = 0; pos < b->byte_code_len;) {
        int op = b->byte_code_buf[pos];
        actual_reads += op == OP_get_loc || op == OP_get_loc_check ||
            op == OP_get_loc8 || (op >= OP_get_loc0 && op <= OP_get_loc3);
        actual_adds += op == OP_add;
        assert(short_opcode_info(op).size > 0);
        pos += short_opcode_info(op).size;
    }
    if (actual_reads != reads || actual_adds != adds)
        fprintf(stderr, "reads=%d/%d adds=%d/%d flags=%u for %s\n",
                actual_reads, reads, actual_adds, adds, flags, source);
    assert(actual_reads == reads && actual_adds == adds);
    JS_FreeValue(ctx, function);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_captured_constant_bytecode(uint32_t flags, int expected_adds)
{
    const char source[] = "(function() { const x = 3; return () => () => x + 4; })()()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled, loaded, function, result;
    JSFunctionBytecode *b;
    uint8_t *buf;
    size_t len;
    int pos, reads = 0, adds = 0;

    assert(rt);
    JS_SetCompilerOptimizationFlags(rt, flags);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, "captured-constant.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf);
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    assert(!JS_IsException(loaded));
    js_free(ctx, buf);
    JS_FreeValue(ctx, compiled);
    function = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(function));
    b = JS_GetFunctionBytecode(function);
    assert(b);
    for (pos = 0; pos < b->byte_code_len; pos += short_opcode_info(b->byte_code_buf[pos]).size) {
        reads += b->byte_code_buf[pos] == OP_get_var_ref_check;
        adds += b->byte_code_buf[pos] == OP_add;
    }
    assert(reads == 1 && adds == expected_adds);
    result = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
    assert(JS_IsNumber(result) && JS_VALUE_GET_INT(result) == 7);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, function);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* Existing free arena blocks must not bypass the requested host failure. */
static void *exhaust_optimizer_pools(JSRuntime *rt)
{
    void *padding = NULL, *ptr;
    size_t size;

    JS_SetMemoryLimit(rt, 0);
    for (size = sizeof(void *); size <= 512; size += sizeof(void *)) {
        while ((ptr = js_malloc_rt(rt, size)) != NULL) {
            memcpy(ptr, &padding, sizeof(padding));
            padding = ptr;
        }
    }
    JS_SetMemoryLimit(rt, (size_t)-1);
    return padding;
}

static void free_optimizer_padding(JSRuntime *rt, void *padding)
{
    while (padding) {
        void *next;
        memcpy(&next, padding, sizeof(next));
        js_free_rt(rt, padding);
        padding = next;
    }
}

/* Exercise summary lifetime while preparation and child finalization fail. */
static void test_captured_constant_allocation_failure(void)
{
    const char source[] =
        "(function() { const x = 'outer'; return () => { const y = 'inner';"
        " return () => x + y; }; })";
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue compiled;
        void *padding;

        assert(ctx);
        padding = exhaust_optimizer_pools(rt);
        oom_test_begin(&allocator, fail_at);
        compiled = JS_Eval(ctx, source, sizeof(source) - 1, "captured-oom.js",
                           JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(!JS_IsException(compiled));
            assert(allocations > 0 && allocations < 256);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(compiled))
            oom_test_check_exception(ctx);
        else
            assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, compiled);
        free_optimizer_padding(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_literal_folding_allocation_failure(int mode)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSFunctionDef fd = { 0 };
        JSVarDef local = { 0 };
        LabelSlot labels[2] = { { 0 } };
        int ret;
        void *padding;

        assert(ctx);
        fd.ctx = ctx;
        fd.compiler_optimization_flags = JS_COMPILER_OPT_CONSTANT_FOLDING;
        if (mode == 4 || mode == 7) {
            local.is_const = TRUE;
            local.is_lexical = TRUE;
            fd.vars = &local;
            fd.var_count = 1;
            fd.compiler_optimization_flags |= JS_COMPILER_OPT_CONSTANT_PROPAGATION;
        }
        fd.label_slots = labels;
        fd.label_count = mode == 7 ? 2 : 1;
        dbuf_init2(&fd.byte_code, rt, js_realloc_bytecode_rt);
        if (mode == 7) {
            dbuf_putc(&fd.byte_code, OP_label);
            dbuf_put_u32(&fd.byte_code, 1);
        }
        if (mode < 2 || mode == 4 || mode == 7) {
            dbuf_putc(&fd.byte_code, OP_push_atom_value);
            dbuf_put_u32(&fd.byte_code, JS_NewAtom(ctx, mode == 1 ? "1" : "folded-left"));
            dbuf_putc(&fd.byte_code, OP_push_atom_value);
            dbuf_put_u32(&fd.byte_code, JS_NewAtom(ctx, mode == 1 ? "2" : "folded-right"));
        } else {
            dbuf_putc(&fd.byte_code, OP_push_bigint_i32);
            dbuf_put_u32(&fd.byte_code, mode == 5 ? 3 : 1);
            dbuf_putc(&fd.byte_code, mode == 6 ? OP_push_atom_value :
                      mode == 2 ? OP_push_i32 : OP_push_bigint_i32);
            dbuf_put_u32(&fd.byte_code, mode == 6 ?
                         JS_NewAtom(ctx, "12345678901234567890") :
                         mode == 5 ? 55 : mode == 2 ? 1 : 0);
        }
        dbuf_putc(&fd.byte_code, mode == 6 ? OP_lt :
                  mode == 5 ? OP_pow : mode == 3 ? OP_div : OP_add);
        if (mode == 4 || mode == 7) {
            dbuf_putc(&fd.byte_code, OP_put_loc);
            dbuf_put_u16(&fd.byte_code, 0);
            if (mode == 7) {
                dbuf_putc(&fd.byte_code, OP_label);
                dbuf_put_u32(&fd.byte_code, 0);
            }
            dbuf_putc(&fd.byte_code, OP_get_loc_check);
            dbuf_put_u16(&fd.byte_code, 0);
            if (mode == 7) {
                dbuf_putc(&fd.byte_code, OP_push_atom_value);
                dbuf_put_u32(&fd.byte_code, JS_NewAtom(ctx, "guarded-tail"));
                dbuf_putc(&fd.byte_code, OP_add);
            }
        }
        dbuf_putc(&fd.byte_code, OP_push_atom_value);
        dbuf_put_u32(&fd.byte_code, JS_NewAtom(ctx, "borrowed-tail"));
        dbuf_putc(&fd.byte_code, OP_drop);
        if (mode != 7) {
            dbuf_putc(&fd.byte_code, OP_label);
            dbuf_put_u32(&fd.byte_code, 0);
        }
        dbuf_putc(&fd.byte_code, OP_return);
        assert(!dbuf_error(&fd.byte_code));
        padding = exhaust_optimizer_pools(rt);
        oom_test_begin(&allocator, fail_at);
        ret = js_optimize_constants(ctx, &fd);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 64);
        } else {
            assert(allocator.failures == 1);
        }
        if (ret < 0) {
            oom_test_check_exception(ctx);
        } else {
            int expected_size = mode == 7 ? 34 : mode == 6 ? 13 : mode == 4 ? 25 :
                ((mode < 2 || mode == 5) ? 17 : 18);
            assert(!JS_HasException(ctx));
            assert(fd.byte_code.size == expected_size);
            assert(labels[0].pos2 == (mode == 7 ? 18 : expected_size - 1));
            if (mode == 7) {
                assert(labels[1].pos2 == 5);
                assert(fd.byte_code.buf[18] == OP_get_loc_check);
                assert(fd.byte_code.buf[21] == OP_drop);
            }
        }
        free_bytecode_atoms(rt, fd.byte_code.buf, fd.byte_code.size, FALSE);
        dbuf_free(&fd.byte_code);
        while (fd.cpool_count)
            JS_FreeValue(ctx, fd.cpool[--fd.cpool_count]);
        js_free(ctx, fd.cpool);
        free_optimizer_padding(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_compiler_optimization_flags(void)
{
    const char source[] = "(1 + 2) * 6;";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled, loaded, result;
    uint8_t *buf;
    size_t len;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetCompilerOptimizationFlags(rt, UINT32_MAX);
    assert(JS_GetCompilerOptimizationFlags(rt) ==
           (JS_COMPILER_OPT_CONSTANT_FOLDING | JS_COMPILER_OPT_CONSTANT_PROPAGATION));
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, "flags.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    JS_SetCompilerOptimizationFlags(rt, 0);
    buf = JS_WriteObject(ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf);
    loaded = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    assert(!JS_IsException(loaded));
    js_free(ctx, buf);
    result = JS_EvalFunction(ctx, compiled);
    assert(JS_StrictEq(ctx, result, JS_NewInt32(ctx, 18)));
    JS_FreeValue(ctx, result);
    result = JS_EvalFunction(ctx, loaded);
    assert(JS_StrictEq(ctx, result, JS_NewInt32(ctx, 18)));
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_integer_folding_bytecode(const char *source, int opcode,
                                          int expected)
{
    test_integer_folding_bytecode_flags(source, opcode, expected,
                                        JS_COMPILER_OPT_CONSTANT_FOLDING);
}

int main(void)
{
    test_compiler_optimization_flags();
    test_captured_constant_allocation_failure();
    test_captured_constant_bytecode(JS_COMPILER_OPT_CONSTANT_FOLDING |
                                    JS_COMPILER_OPT_CONSTANT_PROPAGATION, 0);
    test_captured_constant_bytecode(JS_COMPILER_OPT_CONSTANT_FOLDING, 1);
    test_captured_constant_bytecode(JS_COMPILER_OPT_CONSTANT_PROPAGATION, 1);
    test_captured_constant_bytecode(0, 1);
    {
        uint32_t both = JS_COMPILER_OPT_CONSTANT_FOLDING | JS_COMPILER_OPT_CONSTANT_PROPAGATION;
        const char *branch = "(function(flag) { var a = 3; if(flag) return a + 1; return a + 2; })";
        test_local_constant_bytecode(branch, both, 0, 0);
        test_local_constant_bytecode(branch, JS_COMPILER_OPT_CONSTANT_FOLDING, 2, 2);
        test_local_constant_bytecode(branch, JS_COMPILER_OPT_CONSTANT_PROPAGATION, 0, 2);
        test_local_constant_bytecode(
            "(function(flag) { if(flag) { var a = 3; } return a + 1; })", both, 1, 1);
        test_local_constant_bytecode(
            "(function() { var a = 3; a = 4; return a + 1; })", both, 0, 0);
    }
    test_literal_folding_allocation_failure(7);
    {
        uint32_t both = JS_COMPILER_OPT_CONSTANT_FOLDING | JS_COMPILER_OPT_CONSTANT_PROPAGATION;
        const char *branch = "(function(flag) { let a = 3; if (flag) return a + 1; return a + 2; })";
        test_local_constant_bytecode(branch, both, 0, 0);
        test_local_constant_bytecode(branch, JS_COMPILER_OPT_CONSTANT_FOLDING, 2, 2);
        test_local_constant_bytecode(branch, JS_COMPILER_OPT_CONSTANT_PROPAGATION, 0, 2);
        test_local_constant_bytecode(
            "(function(flag) { if(flag) {} let a = 3; if(flag) return a + 1; return a + 2; })",
            both, 2, 0);
        test_local_constant_bytecode(
            "(function(flag) { let a = flag ? 3 : 9; return a + 1; })", both, 1, 1);
        test_local_constant_bytecode(
            "(function(flag) { let a = 3; if (flag) a = 9; return a + 1; })", both, 1, 1);
    }
    test_integer_folding_bytecode("false && unknown();", OP_if_false, 0);
    test_integer_folding_bytecode("true || unknown();", OP_if_true, 0);
    test_integer_folding_bytecode("null ?? unknown();", OP_if_false, 0);
    test_integer_folding_bytecode("42 ?? unknown();", OP_if_false, 0);
    test_integer_folding_bytecode("!12345678901234567890n;", OP_lnot, 0);
    test_integer_folding_bytecode_flags("false && unknown();", OP_if_false, 1, 0);
    test_integer_folding_bytecode_flags("false && unknown();", OP_if_false, 1,
                                      JS_COMPILER_OPT_CONSTANT_PROPAGATION);
    test_bytecode_roundtrip("Number(false || 42);");
    test_literal_folding_allocation_failure(6);
    test_integer_folding_bytecode("9007199254740993n > 9007199254740992;", OP_gt, 0);
    test_integer_folding_bytecode("'c' > 'a';", OP_gt, 0);
    test_integer_folding_bytecode("'1.25' < 2n;", OP_lt, 0);
    test_integer_folding_bytecode("null == (void 0);", OP_eq, 0);
    test_integer_folding_bytecode_flags("9007199254740993n > 9007199254740992;", OP_gt, 1, 0);
    test_bytecode_roundtrip("(9007199254740993n > 9007199254740992) ? 42 : 0;");
    test_literal_folding_allocation_failure(5);
    test_integer_folding_bytecode("3n ** 55n;", OP_pow, 0);
    test_integer_folding_bytecode_flags("3n ** 55n;", OP_pow, 1, 0);
    test_integer_folding_bytecode("12345678901234567890n * -17n;", OP_mul, 0);
    test_integer_folding_bytecode("~12345678901234567890n;", OP_not, 0);
    test_integer_folding_bytecode("1n << 4095n;", OP_shl, 0);
    test_integer_folding_bytecode("(-1n) << 4095n;", OP_shl, 0);
    test_integer_folding_bytecode("1n << 4096n;", OP_shl, 1);
    test_integer_folding_bytecode("1n << 5000n;", OP_shl, 1);
    test_integer_folding_bytecode("3n ** 5000n;", OP_pow, 1);
    test_bytecode_roundtrip("Number((3n ** 50n) % 100n) - 7;");
    test_literal_folding_allocation_failure(4);
    {
        const char *chain = "(function() { const a = 3; const b = a + 4; return b + 35; })";
        uint32_t both = JS_COMPILER_OPT_CONSTANT_FOLDING | JS_COMPILER_OPT_CONSTANT_PROPAGATION;
        test_local_constant_bytecode(chain, both, 0, 0);
        test_local_constant_bytecode(chain, JS_COMPILER_OPT_CONSTANT_FOLDING, 2, 2);
        test_local_constant_bytecode(chain, JS_COMPILER_OPT_CONSTANT_PROPAGATION, 1, 2);
        test_local_constant_bytecode(chain, 0, 2, 2);
        test_local_constant_bytecode(
            "(function() { const a = 3; eval('a'); return a + 1; })", both, 2, 1);
        test_local_constant_bytecode(
            "(function() { const a = 3; const f = () => a; return a + 1; })", both, 1, 1);
        test_bytecode_roundtrip(
            "(function() { const a = 3; const b = a + 4; return b * 6; })();");
    }
    {
        uint32_t both = JS_COMPILER_OPT_CONSTANT_FOLDING | JS_COMPILER_OPT_CONSTANT_PROPAGATION;
        test_local_constant_bytecode(
            "(function() { let a = 3; let b = a + 4; return b + 35; })", both, 0, 0);
        test_local_constant_bytecode(
            "(function() { let a = 3; a = 8; return a + 34; })", both, 0, 0);
        test_local_constant_bytecode(
            "(function() { let a = 3; a += 4; return a + 35; })", both, 0, 0);
        test_local_constant_bytecode(
            "(function() { let a = 3; a = 8; return a + 34; })",
            JS_COMPILER_OPT_CONSTANT_FOLDING, 0, 1);
        test_bytecode_roundtrip(
            "(function() { let a = 3; a = 8; return a + 34; })();");
    }
    test_literal_folding_allocation_failure(0);
    test_literal_folding_allocation_failure(1);
    test_literal_folding_allocation_failure(2);
    test_literal_folding_allocation_failure(3);
    test_integer_folding_bytecode("1n + 1;", OP_add, 0);
    test_integer_folding_bytecode("1n + 1;", OP_throw_error, 1);
    test_integer_folding_bytecode("1n / 0n;", OP_div, 0);
    test_integer_folding_bytecode("1n / 0n;", OP_throw_error, 1);
    test_integer_folding_bytecode("+1n;", OP_throw_error, 1);
    test_integer_folding_bytecode_flags("1n + 1;", OP_add, 1, 0);
    test_integer_folding_bytecode("'foo' + 'bar';", OP_add, 0);
    test_integer_folding_bytecode("'1' + '2';", OP_add, 0);
    test_integer_folding_bytecode("'x' + true + null;", OP_add, 0);
    test_integer_folding_bytecode("1 + 2 + 'x';", OP_add, 0);
    test_integer_folding_bytecode_flags("'foo' + 'bar';", OP_add, 1, 0);
    test_integer_folding_bytecode_flags("1 + 2 * 3;", OP_add, 1, 0);
    test_integer_folding_bytecode_flags("1 + 2 * 3;", OP_mul, 1, 0);
    test_integer_folding_bytecode("unknown[5 + 3];", OP_add, 0);
    test_integer_folding_bytecode("unknown[5 + 3];", OP_get_array_el, 1);
    test_integer_folding_bytecode("1 + 2 * 3;", OP_add, 0);
    test_integer_folding_bytecode("1 + 2 * 3;", OP_mul, 0);
    test_integer_folding_bytecode("(-7 + 3) * (9 - 2);", OP_mul, 0);
    test_integer_folding_bytecode("9 / 3 + 7 % 3;", OP_div, 0);
    test_integer_folding_bytecode("((3 << 5) | 7) ^ 2;", OP_shl, 0);
    test_integer_folding_bytecode("~(1 + 2);", OP_not, 0);
    test_integer_folding_bytecode("-(1 + 2);", OP_neg, 0);
    test_integer_folding_bytecode("-8 >> 2;", OP_sar, 0);
    test_integer_folding_bytecode("-1 >>> 1;", OP_shr, 0);
    test_integer_folding_bytecode("1 +\n2 * 3;", OP_add, 0);
    test_integer_folding_bytecode("2147483647 + 1;", OP_add, 0);
    test_integer_folding_bytecode("(2147483647 + 1) - 1;", OP_add, 0);
    test_integer_folding_bytecode("-1 * 0;", OP_mul, 0);
    test_integer_folding_bytecode("-4 % 2;", OP_mod, 0);
    test_integer_folding_bytecode("1 / 0;", OP_div, 0);
    test_integer_folding_bytecode("(1 + 2) + unknown;", OP_add, 1);
    test_bytecode_roundtrip("(2 + 5) * (9 - 3);");
    test_bytecode_roundtrip("try { 1n + 1; } catch(e) { e instanceof TypeError ? 42 : 0; }");
    test_bytecode_roundtrip("try { 1n / 0n; } catch(e) { e instanceof RangeError ? 42 : 0; }");
    test_bytecode_roundtrip(
        "var fn = function self() {"
        " [self] = [0]; (() => { [self] = [1]; })(); return self; };"
        "fn() === fn ? 42 : 0;");

    test_writer_allocation_failure(0);
    test_writer_allocation_failure(1);
    test_writer_allocation_failure(2);
    test_class_strict_region_bytecode("class Plain {}", 3);
    test_class_strict_region_bytecode(
        "(class { [(checked_class_mode_target = 1)]() {} });", 3);
    test_class_strict_region_bytecode(
        "'use strict'; (class { [(checked_class_mode_target = 1)]() {} });", 0);
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
    test_module_names_bytecode_roundtrip();
    test_module_names_invalid_export_kind();
    return 0;
}
