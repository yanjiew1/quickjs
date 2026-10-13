/*
 * QuickJS serialized JSON preload attribute identity tests
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

#include "quickjs-libc.h"

static int missing_calls;

/* No JSON source file is created. Any preload miss is a test failure. */
static JSModuleDef *reject_loader(JSContext *ctx, const char *name,
                                 void *opaque, JSValueConst attributes)
{
    (void)opaque;
    (void)attributes;
    missing_calls++;
    JS_ThrowReferenceError(ctx, "unexpected source reload: %s", name);
    return NULL;
}

static JSValue make_attributes(JSContext *ctx, const char *type,
                               const char *flavor, size_t flavor_len)
{
    JSValue attributes = JS_NewObjectProto(ctx, JS_NULL);

    assert(!JS_IsException(attributes));
    assert(JS_DefinePropertyValueStr(ctx, attributes, "type",
                                     JS_NewString(ctx, type),
                                     JS_PROP_C_W_E) >= 0);
    assert(JS_DefinePropertyValueStr(ctx, attributes, "flavor",
                                     JS_NewStringLen(ctx, flavor, flavor_len),
                                     JS_PROP_C_W_E) >= 0);
    return attributes;
}

static void preload(JSContext *ctx, const char *name, int value,
                    JSValueConst attributes, int flags, int legacy)
{
    JSValue object = JS_NewObject(ctx);
    uint8_t *value_buf, *attributes_buf;
    size_t value_len, attributes_len;

    assert(!JS_IsException(object));
    assert(JS_SetPropertyStr(ctx, object, "value", JS_NewInt32(ctx, value)) >= 0);
    value_buf = JS_WriteObject(ctx, &value_len, object, flags);
    assert(value_buf && value_len);
    JS_FreeValue(ctx, object);
    if (legacy) {
        /* Existing helper remains a valid no-attribute preload ABI. */
        js_std_eval_binary_json_module(ctx, value_buf, value_len, name);
    } else {
        attributes_buf = JS_WriteObject(ctx, &attributes_len, attributes, flags);
        assert(attributes_buf && attributes_len);
        js_std_eval_binary_json_module2(ctx, value_buf, value_len, name,
                                        attributes_buf, attributes_len);
        js_free(ctx, attributes_buf);
    }
    js_free(ctx, value_buf);
    assert(!JS_HasException(ctx));
}

static int direct_init(JSContext *ctx, JSModuleDef *module)
{
    return JS_SetModuleExport(ctx, module, "default",
                              JS_GetModulePrivateValue(ctx, module));
}

static void test_preload_identity(int flags)
{
    static const char source[] =
        "import a from 'preload-identity.so' with {"
        " type: 'json', flavor: 'one\\0suffix' };"
        "import reordered from 'preload-identity.so' with {"
        " flavor: 'one\\0suffix', type: 'json' };"
        "import b from 'preload-identity.so' with {"
        " type: 'json', flavor: 'two' };"
        "import extended from 'preload-identity.so' with {"
        " type: 'json5', flavor: 'two' };"
        "import plain from 'preload-plain.json';"
        "import empty from 'preload-plain.json' with {};"
        "import legacy from 'preload-legacy.json';"
        "import direct from 'preload-direct' with {"
        " flavor: 'direct', type: 'json' };"
        "if (a !== reordered || a === b || b === extended ||"
        " a.value !== 11 || b.value !== 22 || extended.value !== 33 ||"
        " plain.value !== 44 || plain !== empty || legacy.value !== 55 ||"
        " direct !== 77) throw Error('preload request identity lost');"
        "globalThis.preload_identity_complete = true;";
    static const char missing[] =
        "import a from 'preload-identity.so' with {"
        " type: 'json', flavor: 'one' };";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *job_ctx;
    JSModuleDef *module;
    JSValue attributes, compiled, result, global, complete, exception;
    int jobs, ret;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    missing_calls = 0;
    JS_SetModuleLoaderFunc2(rt, NULL, reject_loader, NULL, NULL);

    attributes = make_attributes(ctx, "json", "one\0suffix", 10);
    preload(ctx, "preload-identity.so", 11, attributes, flags, 0);
    JS_FreeValue(ctx, attributes);
    attributes = make_attributes(ctx, "json", "two", 3);
    preload(ctx, "preload-identity.so", 22, attributes, flags, 0);
    JS_FreeValue(ctx, attributes);
    attributes = make_attributes(ctx, "json5", "two", 3);
    preload(ctx, "preload-identity.so", 33, attributes, flags, 0);
    JS_FreeValue(ctx, attributes);
    preload(ctx, "preload-plain.json", 44, JS_UNDEFINED, flags, 0);
    preload(ctx, "preload-legacy.json", 55, JS_UNDEFINED, flags, 1);

    /* Explicit borrowed attributes are retained independently of a loader. */
    attributes = make_attributes(ctx, "json", "direct", 6);
    module = JS_NewCModule2(ctx, "preload-direct", direct_init, attributes);
    assert(module);
    assert(JS_AddModuleExport(ctx, module, "default") >= 0);
    assert(JS_SetModulePrivateValue(ctx, module, JS_NewInt32(ctx, 77)) >= 0);
    JS_FreeValue(ctx, attributes);
    JS_RunGC(rt);

    compiled = JS_Eval(ctx, source, sizeof(source) - 1, "preload-main.js",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    assert(JS_ResolveModule(ctx, compiled) == 0);
    result = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(result));
    jobs = 0;
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        assert(++jobs < 100);
    assert(ret == 0);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    assert(missing_calls == 0);
    global = JS_GetGlobalObject(ctx);
    complete = JS_GetPropertyStr(ctx, global, "preload_identity_complete");
    assert(JS_ToBool(ctx, complete) == 1);
    JS_FreeValue(ctx, complete);
    JS_FreeValue(ctx, global);

    /* Embedded NUL is significant; a shorter string must not match. */
    compiled = JS_Eval(ctx, missing, sizeof(missing) - 1, "preload-missing.js",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(JS_IsException(compiled));
    assert(missing_calls == 1 && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsError(ctx, exception));
    JS_FreeValue(ctx, exception);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

typedef struct {
    int calls;
    int in_loader;
} ActiveLoader;

static JSModuleDef *active_loader(JSContext *ctx, const char *name,
                                 void *opaque, JSValueConst attributes)
{
    ActiveLoader *state = opaque;
    JSModuleDef *module;
    JSValue flavor_value, explicit_attributes, object, probe;
    const char *flavor, *probe_flavor;
    char probe_source[256], probe_name[64];

    /* A matching-attribute lookup inside this callback must find the
       new native module, without depending on post-return adoption. */
    assert(!state->in_loader);
    state->in_loader = 1;
    assert(++state->calls <= 3);
    flavor_value = JS_GetPropertyStr(ctx, attributes, "flavor");
    assert(JS_IsString(flavor_value));
    flavor = JS_ToCString(ctx, flavor_value);
    assert(flavor);
    probe_flavor = flavor;
    if (!strcmp(name, "active-inherited")) {
        assert(!strcmp(flavor, "first") || !strcmp(flavor, "second"));
        module = JS_NewCModule2(ctx, name, direct_init, JS_UNDEFINED);
    } else {
        assert(!strcmp(name, "active-explicit"));
        assert(!strcmp(flavor, "requested"));
        explicit_attributes = make_attributes(ctx, "json", "explicit", 8);
        module = JS_NewCModule2(ctx, name, direct_init, explicit_attributes);
        JS_FreeValue(ctx, explicit_attributes);
        probe_flavor = "explicit";
    }
    assert(module);
    assert(JS_AddModuleExport(ctx, module, "default") >= 0);
    object = JS_NewObject(ctx);
    assert(!JS_IsException(object));
    assert(JS_SetPropertyStr(ctx, object, "value",
                              JS_NewInt32(ctx, state->calls)) >= 0);
    assert(JS_SetModulePrivateValue(ctx, module, object) >= 0);
    snprintf(probe_source, sizeof(probe_source),
             "import value from '%s' with { type: 'json', flavor: '%s' };",
             name, probe_flavor);
    snprintf(probe_name, sizeof(probe_name), "active-probe-%d.js",
             state->calls);
    JS_FreeCString(ctx, flavor);
    JS_FreeValue(ctx, flavor_value);
    probe = JS_Eval(ctx, probe_source, strlen(probe_source), probe_name,
                    JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(probe));
    JS_FreeValue(ctx, probe);
    state->in_loader = 0;
    return module;
}

static void eval_contract_module(JSContext *ctx, const char *source,
                                  const char *filename)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *job_ctx;
    JSValue compiled, result;
    int jobs = 0, ret;

    compiled = JS_Eval(ctx, source, strlen(source), filename,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    assert(JS_ResolveModule(ctx, compiled) == 0);
    JS_RunGC(rt);
    result = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(result));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        assert(++jobs < 100);
    assert(ret == 0 && JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
}

static void test_active_constructor_contract(void)
{
    static const char source[] =
        "import first from 'active-inherited' with {"
        " type: 'json', flavor: 'first' };"
        "import reordered from 'active-inherited' with {"
        " flavor: 'first', type: 'json' };"
        "import second from 'active-inherited' with {"
        " type: 'json', flavor: 'second' };"
        "import requested from 'active-explicit' with {"
        " type: 'json', flavor: 'requested' };"
        "import explicit from 'active-explicit' with {"
        " flavor: 'explicit', type: 'json' };"
        "if (first !== reordered || first === second || first.value !== 1 ||"
        " second.value !== 2 || requested !== explicit || explicit.value !== 3)"
        " throw Error('active native attribute contract failed');"
        "globalThis.active_first = first;"
        "globalThis.active_explicit = explicit;";
    static const char repeated[] =
        "import first from 'active-inherited' with {"
        " flavor: 'first', type: 'json' };"
        "import explicit from 'active-explicit' with {"
        " type: 'json', flavor: 'explicit' };"
        "if (first !== globalThis.active_first ||"
        " explicit !== globalThis.active_explicit)"
        " throw Error('native module attributes changed after loader return');";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    ActiveLoader state = { 0, 0 };

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, NULL, active_loader, NULL, &state);
    eval_contract_module(ctx, source, "active-contract-main.js");
    assert(state.calls == 3 && !state.in_loader);
    JS_RunGC(rt);
    eval_contract_module(ctx, repeated, "active-contract-repeat.js");
    assert(state.calls == 3 && !state.in_loader);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_preload_identity(0);
    test_active_constructor_contract();
    puts("qjsc JSON preload identity: OK");
    return 0;
}
