/* Public API and cross-build bytecode compatibility checks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

typedef union AllocationHeader {
    size_t size;
    long double alignment;
} AllocationHeader;

typedef struct AllocationState {
    size_t remaining;
    size_t live;
} AllocationState;

static void *test_malloc(JSMallocState *s, size_t size)
{
    AllocationState *state = (AllocationState *)s->opaque;
    AllocationHeader *p;
    if (!state->remaining || size > SIZE_MAX - sizeof(*p))
        return NULL;
    state->remaining--;
    p = (AllocationHeader *)malloc(sizeof(*p) + size);
    if (!p)
        return NULL;
    p->size = size;
    state->live++;
    s->malloc_count++;
    s->malloc_size += size;
    return p + 1;
}

static void test_free(JSMallocState *s, void *ptr)
{
    AllocationState *state = (AllocationState *)s->opaque;
    AllocationHeader *p;
    if (!ptr)
        return;
    p = (AllocationHeader *)ptr - 1;
    state->live--;
    s->malloc_count--;
    s->malloc_size -= p->size;
    free(p);
}

static void *test_realloc(JSMallocState *s, void *ptr, size_t size)
{
    void *result;
    size_t old_size;
    if (!ptr)
        return test_malloc(s, size);
    if (!size) {
        test_free(s, ptr);
        return NULL;
    }
    result = test_malloc(s, size);
    if (!result)
        return NULL;
    old_size = ((AllocationHeader *)ptr - 1)->size;
    memcpy(result, ptr, old_size < size ? old_size : size);
    test_free(s, ptr);
    return result;
}

static size_t test_usable_size(const void *ptr)
{
    return ptr ? ((const AllocationHeader *)ptr - 1)->size : 0;
}

static const JSMallocFunctions allocator = {
    test_malloc, test_free, test_realloc, test_usable_size
};

static void check_value(JSContext *ctx, JSValue value)
{
    if (JS_IsException(value)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "unexpected exception: %s\n", message ? message : "<OOM>");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        abort();
    }
}

static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "embedding.js", 0);
    check_value(ctx, value);
    JS_FreeValue(ctx, value);
}

static void drain_jobs(JSRuntime *rt)
{
    JSContext *ctx;
    int result;
    while ((result = JS_ExecutePendingJob(rt, &ctx)) > 0)
        ;
    if (result < 0)
        check_value(ctx, JS_EXCEPTION);
}

static int test_allocation_budget(size_t budget)
{
    AllocationState state = { budget, 0 };
    JSRuntime *rt = JS_NewRuntime2(&allocator, &state);
    int completed = 0;
    if (rt) {
        JSContext *ctx = JS_NewContext(rt);
        if (ctx) {
            const char source[] = "({ value: new Array(1000) })";
            JSValue value;
            completed = 1;
            state.remaining = 0;
            value = JS_Eval(ctx, source, sizeof(source) - 1, "oom.js", 0);
            assert(JS_IsException(value));
            JS_FreeValue(ctx, JS_GetException(ctx));
            state.remaining = 4096;
            JS_FreeContext(ctx);
        }
        JS_FreeRuntime(rt);
    }
    assert(state.live == 0);
    return completed;
}

static void test_allocation_failures(void)
{
    size_t budget;
    /* Budget 11 exposes a pre-existing partial-context teardown crash.
       Individual budgets can be compared across builds with `oom N`. */
    for (budget = 0; budget <= 10; budget++)
        assert(!test_allocation_budget(budget));
    assert(test_allocation_budget(4096));
}

static int finalized;

static void finalizer(JSRuntime *rt, JSValue value)
{
    (void)rt;
    (void)value;
    finalized++;
}

static void test_gc(JSContext *ctx)
{
    JSClassID id = 0;
    const JSClassDef def = { "EmbeddingCycle", finalizer, NULL, NULL, NULL };
    JSValue a, b;
    JS_NewClassID(&id);
    assert(JS_NewClass(JS_GetRuntime(ctx), id, &def) == 0);
    a = JS_NewObjectClass(ctx, id);
    b = JS_NewObjectClass(ctx, id);
    check_value(ctx, a);
    check_value(ctx, b);
    assert(JS_SetPropertyStr(ctx, a, "peer", JS_DupValue(ctx, b)) == 1);
    assert(JS_SetPropertyStr(ctx, b, "peer", JS_DupValue(ctx, a)) == 1);
    JS_FreeValue(ctx, a);
    JS_FreeValue(ctx, b);
    JS_RunGC(JS_GetRuntime(ctx));
    assert(finalized == 2);
}

static JSModuleDef *module_loader(JSContext *ctx, const char *name, void *opaque)
{
    const char *source;
    JSValue value;
    JSModuleDef *module;
    (void)opaque;
    if (!strcmp(name, "a"))
        source = "import { b } from 'b'; export function a() { return 20; }"
                 "await Promise.resolve(); globalThis.moduleAnswer = a() + b();";
    else if (!strcmp(name, "b"))
        source = "import { a } from 'a'; export function b() { return a() + 2; }";
    else {
        JS_ThrowReferenceError(ctx, "unknown module %s", name);
        return NULL;
    }
    value = JS_Eval(ctx, source, strlen(source), name,
                    JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(value))
        return NULL;
    module = (JSModuleDef *)JS_VALUE_GET_PTR(value);
    JS_FreeValue(ctx, value);
    return module;
}

static void test_runtime(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value;
    assert(rt);
    ctx = JS_NewContextRaw(rt);
    assert(ctx);
    assert(JS_AddIntrinsicBaseObjects(ctx) == 0);
    assert(JS_AddIntrinsicEval(ctx) == 0);
    eval_ok(ctx, "if (typeof Date !== 'undefined') throw Error('optional Date');");
    assert(JS_AddIntrinsicDate(ctx) == 0);
    eval_ok(ctx, "if (new Date(0).getTime() !== 0) throw Error('Date');");
    eval_ok(ctx, "if (typeof Proxy !== 'undefined') throw Error('optional Proxy');"
                 "if (Reflect.get({ answer: 42 }, 'answer') !== 42) throw Error('Reflect');");
    assert(JS_AddIntrinsicProxy(ctx) == 0);
    eval_ok(ctx, "let revoked = Proxy.revocable({ answer: 42 }, {});"
                 "if (Reflect.get(revoked.proxy, 'answer') !== 42) throw Error('Proxy');"
                 "revoked.revoke(); let threw = false;"
                 "try { Reflect.get(revoked.proxy, 'answer'); }"
                 "catch (error) { threw = error instanceof TypeError; }"
                 "if (!threw) throw Error('revoked Proxy');");
    JS_FreeContext(ctx);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_gc(ctx);
    eval_ok(ctx, "globalThis.answer = 0; Promise.resolve(40).then(x => answer = x + 2);");
    drain_jobs(rt);
    eval_ok(ctx, "if (answer !== 42) throw Error('jobs');"
                 "let key = {}; let map = new WeakMap([[key, 1]]);"
                 "let weak = new WeakRef(key);"
                 "if (weak.deref() !== key || map.get(key) !== 1) throw Error('weak');"
                 "globalThis.finalization = new FinalizationRegistry(() => {});");
    JS_SetModuleLoaderFunc(rt, NULL, module_loader, NULL);
    value = JS_Eval(ctx, "import 'a';", 11, "main", JS_EVAL_TYPE_MODULE);
    check_value(ctx, value);
    drain_jobs(rt);
    assert(JS_PromiseState(ctx, value) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, value);
    eval_ok(ctx, "if (moduleAnswer !== 42) throw Error('modules');");
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static const char *const fixtures[] = {
    "globalThis.fixtureAnswer = (() => { const x = 40n;"
    "return Number(x) + /a/u.test('a') + new Uint8Array([1])[0]; })();",
    "export const answer = 42; globalThis.fixtureModuleAnswer = answer;"
};

static void bytecode_fixture(const char *path, int writing)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    FILE *file = fopen(path, writing ? "wb" : "rb");
    unsigned int i;
    assert(rt && file);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        JSValue value;
        uint8_t *data;
        size_t size;
        uint32_t length;
        if (writing) {
            value = JS_Eval(ctx, fixtures[i], strlen(fixtures[i]),
                            i ? "fixture-module.js" : "fixture-script.js",
                            JS_EVAL_FLAG_COMPILE_ONLY |
                            (i ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL));
            check_value(ctx, value);
            data = JS_WriteObject(ctx, &size, value, JS_WRITE_OBJ_BYTECODE);
            assert(data && size <= UINT32_MAX);
            length = (uint32_t)size;
            assert(fwrite(&length, sizeof(length), 1, file) == 1);
            assert(fwrite(data, 1, size, file) == size);
            js_free(ctx, data);
            JS_FreeValue(ctx, value);
        } else {
            assert(fread(&length, sizeof(length), 1, file) == 1);
            data = (uint8_t *)malloc(length);
            assert(data);
            assert(fread(data, 1, length, file) == length);
            value = JS_ReadObject(ctx, data, length, JS_READ_OBJ_BYTECODE);
            free(data);
            check_value(ctx, value);
            assert(JS_ResolveModule(ctx, value) == 0);
            value = JS_EvalFunction(ctx, value);
            check_value(ctx, value);
            drain_jobs(rt);
            if (i)
                assert(JS_PromiseState(ctx, value) == JS_PROMISE_FULFILLED);
            JS_FreeValue(ctx, value);
        }
    }
    if (!writing) {
        assert(fgetc(file) == EOF);
        eval_ok(ctx, "if (fixtureAnswer !== 42 || fixtureModuleAnswer !== 42)"
                     "throw Error('bytecode compatibility');");
    }
    assert(fclose(file) == 0);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "oom")) {
        test_allocation_budget((size_t)strtoul(argv[2], NULL, 10));
    } else if (argc == 3 && (!strcmp(argv[1], "write") || !strcmp(argv[1], "read"))) {
        bytecode_fixture(argv[2], !strcmp(argv[1], "write"));
    } else {
        assert(argc == 1);
        test_allocation_failures();
        test_runtime();
    }
    puts("embedding checks passed");
    return 0;
}
