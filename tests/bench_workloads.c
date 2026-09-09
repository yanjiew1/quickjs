/* Supplemental whole-engine workloads, timed using process CPU time. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "quickjs.h"

static const char source[] =
    "class Counter { #n = 0; add(n) { this.#n += n; return this.#n; } }"
    "function outer(x) { return y => x + y; }"
    "const object = { ['answer']: 42, ...{ a: 1, b: 2 } };"
    "for (let i = 0; i < 10; i++) { object.a += outer(i)(1); }"
    "async function work() { await Promise.resolve(1); return object; }";

static void workload(int kind, unsigned int count)
{
    JSRuntime *rt = NULL;
    JSContext *ctx = NULL;
    unsigned int i;
    if (kind != 0) {
        rt = JS_NewRuntime();
        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
    }
    for (i = 0; i < count; i++) {
        JSValue value;
        if (kind == 0) {
            rt = JS_NewRuntime();
            assert(rt);
            ctx = JS_NewContext(rt);
            assert(ctx);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
        } else if (kind == 1) {
            value = JS_Eval(ctx, source, sizeof(source) - 1, "compile.js",
                            JS_EVAL_FLAG_COMPILE_ONLY);
            assert(!JS_IsException(value));
            JS_FreeValue(ctx, value);
        } else {
            const char jobs[] = "(() => { globalThis.result = 0;"
                "let p = Promise.resolve();"
                "for (let i = 0; i < 100; i++) p = p.then(() => ++result); })();";
            JSContext *job_ctx;
            int result;
            value = JS_Eval(ctx, jobs, sizeof(jobs) - 1, "jobs.js",
                            JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(value));
            JS_FreeValue(ctx, value);
            while ((result = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
                ;
            assert(result == 0);
        }
    }
    if (kind != 0) {
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

int main(void)
{
    const char *const names[] = { "startup", "compile", "promise_jobs" };
    int kind;
    puts("{");
    for (kind = 0; kind < 3; kind++) {
        unsigned int count = 1;
        double elapsed;
        do {
            clock_t start = clock();
            workload(kind, count);
            elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            if (elapsed >= 0.2)
                break;
            count *= 2;
        } while (count < (1U << 28));
        printf("  \"%s\": %.2f%s\n", names[kind], elapsed * 1e9 / count,
               kind == 2 ? "" : ",");
    }
    puts("}");
    return 0;
}
