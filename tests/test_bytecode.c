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
#include "pool-padding.h"
#include "../src/quickjs/internal/bytecode.h"
#include "../src/quickjs/internal/function.h"
#include "../src/quickjs/internal/module.h"
#include "../src/quickjs/internal/string.h"

static void rope_print_write(void *opaque, const char *buf, size_t len)
{
    DynBuf *output = opaque;
    dbuf_put(output, (const uint8_t *)buf, len);
}

/* The representation and binary API cannot be checked from JavaScript. */
static void test_rope_serialization(void)
{
    static const char source[] = "'a'.repeat(10000) + '\\u0100\\u0000\\ud800'";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue rope, loaded, flat, holder;
    JSMemoryUsage before, live;
    uint8_t *buf;
    size_t len;
    DynBuf output;
    JSPrintValueOptions options;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_ComputeMemoryUsage(rt, &before);
    rope = JS_Eval(ctx, source, sizeof(source) - 1, "rope.js", JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsStringRope(rope));
    assert(JS_VALUE_GET_TAG(rope) == JS_TAG_STRING_ROPE);
    dbuf_init(&output);
    JS_PrintValueSetDefaultOptions(&options);
    options.raw_dump = TRUE;
    JS_PrintValue(ctx, rope_print_write, &output, rope, &options);
    dbuf_putc(&output, 0);
    assert(!dbuf_error(&output));
    assert(strstr((const char *)output.buf, "[rope len=10003 depth=1]"));
    dbuf_free(&output);
    dbuf_init(&output);
    options.raw_dump = FALSE;
    options.max_string_length = 3;
    JS_PrintValue(ctx, rope_print_write, &output, rope, &options);
    dbuf_putc(&output, 0);
    assert(!dbuf_error(&output));
    assert(strstr((const char *)output.buf, "aaa"));
    assert(strstr((const char *)output.buf, "10000 more characters"));
    dbuf_free(&output);
    dbuf_init(&output);
    options.max_string_length = 10003;
    JS_PrintValue(ctx, rope_print_write, &output, rope, &options);
    dbuf_putc(&output, 0);
    assert(!dbuf_error(&output));
    assert(strstr((const char *)output.buf, "\xc4\x80\\u0000\\ud800"));
    dbuf_free(&output);
    /* Normal object properties participate in string memory accounting. */
    holder = JS_NewObject(ctx);
    assert(!JS_IsException(holder));
    assert(JS_SetPropertyStr(ctx, holder, "savedRope", rope) >= 0);
    JS_ComputeMemoryUsage(rt, &live);
    assert(live.str_size >= before.str_size + sizeof(JSStringRope));
    rope = JS_GetPropertyStr(ctx, holder, "savedRope");
    buf = JS_WriteObject(ctx, &len, rope, 0);
    assert(buf && len);
    loaded = JS_ReadObject(ctx, buf, len, 0);
    js_free(ctx, buf);
    assert(JS_IsFlatString(loaded));
    flat = JS_ToString(ctx, rope);
    assert(JS_IsFlatString(flat));
    assert(JS_VALUE_GET_STRING(flat)->len == 10003);
    assert(js_string_eq(JS_VALUE_GET_STRING(loaded), JS_VALUE_GET_STRING(flat)));
    assert(string_rope_get(rope, 10000) == 0x100);
    assert(string_rope_get(rope, 10001) == 0);
    assert(string_rope_get(rope, 10002) == 0xd800);
    JS_FreeValue(ctx, loaded);
    JS_FreeValue(ctx, flat);
    JS_FreeValue(ctx, rope);
    {
        static const char dag_source[] =
            "(function() { let s = 'x'.repeat(9000);"
            "for (let i = 0; i < 16; i++) s = s + s; return s; })()";
        JSValue result = JS_Eval(ctx, dag_source, sizeof(dag_source) - 1,
                                 "shared-rope.js", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(result));
        assert(JS_IsStringRope(result));
        assert(JS_SetPropertyStr(ctx, holder, "sharedRope", result) >= 0);
        JS_ComputeMemoryUsage(rt, &live);
        assert(live.str_size < before.str_size + 1024 * 1024);
    }
    JS_FreeValue(ctx, holder);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

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

static void test_rom_bytecode_endianness(void)
{
    static const char source[] = "258;";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled, loaded, result;
    JSFunctionBytecode *b;
    uint8_t *buf, *saved;
    size_t len;
    int32_t value;

    assert(rt);
    JS_SetStripInfo(rt, JS_STRIP_DEBUG);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, "rom.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(ctx, &len, compiled, JS_WRITE_OBJ_BYTECODE);
    JS_FreeValue(ctx, compiled);
    assert(buf && len > 2 && buf[1] == 0); /* no atom relocation */
    saved = js_malloc(ctx, len);
    assert(saved);
    memcpy(saved, buf, len);

    loaded = JS_ReadObject(ctx, buf, len,
                           JS_READ_OBJ_BYTECODE | JS_READ_OBJ_ROM_DATA);
    assert(!JS_IsException(loaded));
    assert(JS_VALUE_GET_TAG(loaded) == JS_TAG_FUNCTION_BYTECODE);
    assert(memcmp(buf, saved, len) == 0);
    b = JS_VALUE_GET_PTR(loaded);
    assert(b->read_only_bytecode == !is_be());
    result = JS_EvalFunction(ctx, loaded);
    assert(!JS_IsException(result));
    assert(JS_ToInt32(ctx, &value, result) == 0 && value == 258);
    JS_FreeValue(ctx, result);
    js_free(ctx, saved);
    js_free(ctx, buf);
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

static void test_truncated_function_atoms(void)
{
    static const char source[] = "(function truncated_function_name() { return 123; })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue compiled, result;
    JSMemoryUsage before, after;
    uint8_t *buf;
    size_t length, cut;

    assert(rt && ctx);
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, "truncated-function.js",
                       JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    buf = JS_WriteObject(ctx, &length, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(buf && length);
    JS_FreeValue(ctx, compiled);
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &before);
    /* Exercise every header/body boundary without coupling the test to byte
       offsets. Failed reads must release atoms acquired before allocation. */
    for (cut = 0; cut < length; cut++) {
        result = JS_ReadObject(ctx, buf, cut, JS_READ_OBJ_BYTECODE);
        assert(JS_IsException(result));
        JS_FreeValue(ctx, JS_GetException(ctx));
        JS_RunGC(rt);
        JS_ComputeMemoryUsage(rt, &after);
        assert(after.atom_count == before.atom_count);
    }
    js_free(ctx, buf);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_read_oom(void)
{
    static const char source[] =
        "import { local as value } from 'module-read-oom';"
        "export { value }; export * from 'module-read-oom';"
        "export const local = 1;";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue module;
    uint8_t *encoded, *copy;
    size_t size, step, calls = 0;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    module = JS_Eval(ctx, source, sizeof(source) - 1, "module-read-oom",
                     JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(module));
    encoded = JS_WriteObject(ctx, &size, module, JS_WRITE_OBJ_BYTECODE);
    assert(encoded && size);
    copy = malloc(size);
    assert(copy);
    memcpy(copy, encoded, size);
    js_free(ctx, encoded);
    JS_FreeValue(ctx, module);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        void *padding;

        rt = oom_test_runtime(&allocator);
        ctx = JS_NewContext(rt);
        assert(ctx);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        module = JS_ReadObject(ctx, copy, size, JS_READ_OBJ_BYTECODE);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 128);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(module)) {
            JSValue retry;

            assert(step);
            oom_test_check_exception(ctx);
            retry = JS_ReadObject(ctx, copy, size, JS_READ_OBJ_BYTECODE);
            assert(!JS_IsException(retry));
            assert(JS_ResolveModule(ctx, retry) == 0);
            assert(JS_LinkModule(ctx, retry) == 0);
            JS_FreeValue(ctx, retry);
        } else {
            assert(!JS_HasException(ctx));
        }
        JS_FreeValue(ctx, module);
        test_pool_padding_free(rt, padding);
        JS_RunGC(rt);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    free(copy);
}

typedef struct ReaderSABState {
    JSContext *ctx;
    JSValue unrelated_module;
    void *data;
    int refs, armed, called;
} ReaderSABState;

static void *reader_sab_alloc(void *opaque, size_t size)
{
    ReaderSABState *state = opaque;
    state->data = calloc(1, size);
    assert(state->data);
    state->refs = 1;
    return state->data;
}

static void reader_sab_free(void *opaque, void *ptr)
{
    ReaderSABState *state = opaque;
    assert(ptr == state->data && state->refs > 0);
    if (--state->refs == 0) {
        free(state->data);
        state->data = NULL;
    }
}

static void reader_sab_dup(void *opaque, void *ptr)
{
    static const char source[] = "export const independent = 1";
    ReaderSABState *state = opaque;

    assert(ptr == state->data && state->refs > 0);
    state->refs++;
    if (state->armed && !state->called) {
        state->called = 1;
        state->unrelated_module = JS_Eval(state->ctx, source, sizeof(source) - 1,
                                          "independent-read-module",
                                          JS_EVAL_TYPE_MODULE |
                                          JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(state->unrelated_module));
    }
}

static void test_failed_read_preserves_host_module(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    ReaderSABState state = { 0 };
    JSSharedArrayBufferFunctions functions = {
        reader_sab_alloc, reader_sab_free, reader_sab_dup, &state,
    };
    JSValue array, shared, result, error;
    uint8_t *encoded, **pointers;
    size_t len, pointer_count;
    JSModuleDef *module;

    assert(rt);
    JS_SetSharedArrayBufferFunctions(rt, &functions);
    ctx = JS_NewContext(rt);
    assert(ctx);
    state.ctx = ctx;
    state.unrelated_module = JS_UNDEFINED;
    array = JS_NewArray(ctx);
    shared = JS_Eval(ctx, "new SharedArrayBuffer(4)", 24,
                     "<read-callback>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(array) && !JS_IsException(shared));
    assert(JS_SetPropertyUint32(ctx, array, 0, shared) == 1);
    assert(JS_SetPropertyUint32(ctx, array, 1, JS_NewInt32(ctx, 1)) == 1);
    encoded = JS_WriteObject2(ctx, &len, array, JS_WRITE_OBJ_SAB,
                              &pointers, &pointer_count);
    assert(encoded && pointer_count == 1 && len > 1);
    state.armed = 1;
    result = JS_ReadObject(ctx, encoded, len - 1, JS_READ_OBJ_SAB);
    assert(JS_IsException(result) && state.called);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    module = JS_VALUE_GET_PTR(state.unrelated_module);
    assert(module->link.prev && module->link.next);
    JS_FreeValue(ctx, state.unrelated_module);
    JS_FreeValue(ctx, array);
    js_free(ctx, pointers);
    js_free(ctx, encoded);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(state.refs == 0);
}

static void test_reference_read_oom(int is_array)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue input, result;
    uint8_t *encoded, *copy;
    size_t size, step, calls = 0;

    assert(rt && ctx);
    /* A failed constructor must not replace its OOM with a bad self-reference. */
    input = is_array ? JS_NewArray(ctx) : JS_NewObject(ctx);
    assert(!JS_IsException(input));
    if (is_array)
        assert(JS_SetPropertyUint32(ctx, input, 0, JS_DupValue(ctx, input)) == 1);
    else
        assert(JS_SetPropertyStr(ctx, input, "self", JS_DupValue(ctx, input)) == 1);
    encoded = JS_WriteObject(ctx, &size, input, JS_WRITE_OBJ_REFERENCE);
    assert(encoded && size);
    copy = malloc(size);
    assert(copy);
    memcpy(copy, encoded, size);
    js_free(ctx, encoded);
    JS_FreeValue(ctx, input);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        void *padding;

        rt = oom_test_runtime(&allocator);
        ctx = JS_NewContext(rt);
        assert(ctx);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_ReadObject(ctx, copy, size, JS_READ_OBJ_REFERENCE);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            JSValue error, message;
            const char *text;

            assert(step);
            error = JS_GetException(ctx);
            assert(JS_IsError(ctx, error));
            message = JS_GetPropertyStr(ctx, error, "message");
            text = JS_ToCString(ctx, message);
            assert(text && !strcmp(text, "out of memory"));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, message);
            JS_FreeValue(ctx, error);
        } else {
            assert(!JS_HasException(ctx));
        }
        JS_FreeValue(ctx, result);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    free(copy);
}

static void test_invalid_object_tag(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue result, error;
    uint8_t *buf;
    size_t len;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buf = JS_WriteObject(ctx, &len, JS_NULL, 0);
    assert(buf && len > 0);
    /* Keep a valid header and replace the single value's tag. */
    buf[len - 1] = UINT8_MAX;
    result = JS_ReadObject(ctx, buf, len, 0);
    assert(JS_IsException(result));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    js_free(ctx, buf);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_invalid_object_tag();
    test_rom_bytecode_endianness();
    test_module_read_oom();
    test_failed_read_preserves_host_module();
    test_reference_read_oom(0);
    test_reference_read_oom(1);
    test_truncated_function_atoms();
    test_rope_serialization();
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
