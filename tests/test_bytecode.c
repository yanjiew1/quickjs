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
#include <string.h>

#include "quickjs.h"

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

int main(void)
{
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
