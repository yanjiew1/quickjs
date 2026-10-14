/* Native general-operation ownership and allocation-failure recovery. MIT. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "allocation-probe.h"
static const char *const operations[] = {
    "()=>Intl.supportedValuesOf('calendar')",
    "()=>Intl.supportedValuesOf('collation')",
    "()=>Intl.supportedValuesOf('currency')",
    "()=>Intl.supportedValuesOf('numberingSystem')",
    "()=>Intl.supportedValuesOf('timeZone')",
    "()=>Intl.supportedValuesOf('unit')",
    "()=>('I\\u0323\\u0307\\ud800'.repeat(40)).toLocaleLowerCase('tr')",
    "()=>('i\\u0323\\u0307\\ud835\\udc22\\u0307'.repeat(40)).toLocaleUpperCase('lt')",
    "()=>('I\\u0301ÌΟΣ\\udc00'.repeat(40)).toLocaleLowerCase('lt')",
    "()=>{let s={toString(){Intl.supportedValuesOf('unit');return 'I'}};return String.prototype.toLocaleLowerCase.call(s,{length:2,get 0(){'i'.toLocaleUpperCase('lt');return 'tr'},1:'und'})}",
    "()=>new Intl.Locale('en').getCollations()",
    "()=>new Intl.Locale('zz').getCollations()",
    "()=>new Intl.Locale('en').getNumberingSystems()",
    "()=>new Intl.Locale('zz').getNumberingSystems()",
    "()=>new Intl.Locale('en').getCalendars()",
    "()=>new Intl.Locale('en-US').getTimeZones()",
};
/* The default sweep samples allocation boundaries; --exhaustive retains
 * every allocation failure and the final successful sentinel. */
static int exhaustive;

static int selected_failure(size_t count, size_t failure)
{
    return exhaustive || failure == 1 || failure == count / 2 ||
           failure == count / 2 + 1 || failure == count || failure == count + 1;
}

static void print_sweep(const char *kind, size_t operation, size_t count)
{
    size_t failure;
    printf("Intl OOM %s operation %zu allocations=%zu mode=%s positions=",
           kind, operation, count, exhaustive ? "exhaustive" : "smoke");
    if (exhaustive) {
        printf("1..%zu", count + 1);
    } else {
        const char *separator = "";
        for (failure = 1; failure <= count + 1; failure++) {
            if (selected_failure(count, failure)) {
                printf("%s%zu", separator, failure);
                separator = ",";
            }
        }
    }
    putchar('\n');
    fflush(stdout);
}

typedef struct Fixture {
    AllocationProbe probe;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue function;
} Fixture;
static void open_fixture(Fixture *f, const char *source)
{
    memset(f, 0, sizeof(*f));
    f->rt = JS_NewRuntime2(&probe_functions, &f->probe); assert(f->rt);
    f->ctx = JS_NewContext(f->rt); assert(f->ctx);
    f->function = JS_Eval(f->ctx, source, strlen(source), "general-oom", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(f->function) && !JS_HasException(f->ctx));
}
static void close_fixture(Fixture *f)
{
    assert(!JS_HasException(f->ctx));
    JS_FreeValue(f->ctx, f->function); JS_RunGC(f->rt);
    JS_FreeContext(f->ctx); JS_FreeRuntime(f->rt);
    assert(!f->probe.live);
}
static void check_result(Fixture *f, JSValueConst value, char **baseline)
{
    JSValue json;
    const char *snapshot;
    size_t length;
    json = JS_JSONStringify(f->ctx, value, JS_UNDEFINED, JS_UNDEFINED);
    assert(!JS_IsException(json) && JS_IsString(json) && !JS_HasException(f->ctx));
    snapshot = JS_ToCStringLen(f->ctx, &length, json);
    assert(snapshot && !JS_HasException(f->ctx));
    if (*baseline) {
        assert(strlen(*baseline) == length && !memcmp(*baseline, snapshot, length));
    } else {
        *baseline = malloc(length + 1); assert(*baseline);
        memcpy(*baseline, snapshot, length + 1);
    }
    JS_FreeCString(f->ctx, snapshot); JS_FreeValue(f->ctx, json);
}
static size_t call(Fixture *f, size_t failure, char **baseline)
{
    JSValue value;
    size_t attempts;
    assert(!JS_HasException(f->ctx));
    f->probe.attempts = f->probe.failed = 0;
    f->probe.failure_at = failure;
    value = JS_Call(f->ctx, f->function, JS_UNDEFINED, 0, NULL);
    f->probe.failure_at = 0; attempts = f->probe.attempts;
    if (JS_IsException(value)) {
        JSValue exception;
        assert(f->probe.failed && JS_HasException(f->ctx));
        exception = JS_GetException(f->ctx); JS_FreeValue(f->ctx, exception);
    } else {
        /* Optional string-buffer shrinking may fail with a complete result. */
        assert(!JS_HasException(f->ctx));
        check_result(f, value, baseline); JS_FreeValue(f->ctx, value);
    }
    assert(!JS_HasException(f->ctx));
    return attempts;
}
int main(int argc, char **argv)
{
    size_t operation, allocations, failure;
    Fixture f;
    if (argc == 2 && !strcmp(argv[1], "--exhaustive")) {
        exhaustive = 1;
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--exhaustive]\n", argv[0]);
        return 2;
    }
    for (operation = 0; operation < sizeof(operations)/sizeof(operations[0]); operation++) {
        char *baseline = NULL;
        /* Stabilize immutable process proofs in a discarded realm; the
           measured realm below still has cold context caches and methods. */
        open_fixture(&f, operations[operation]); call(&f, 0, &baseline); close_fixture(&f);
        open_fixture(&f, operations[operation]); allocations = call(&f, SIZE_MAX, &baseline);
        assert(allocations < SIZE_MAX - 1 && !f.probe.failed); close_fixture(&f);
        print_sweep("cold", operation + 1, allocations);
        for (failure = 1; failure <= allocations + 1; failure++) {
            if (!selected_failure(allocations, failure))
                continue;
            open_fixture(&f, operations[operation]); call(&f, failure, &baseline);
            assert(f.probe.failed == (failure <= allocations));
            call(&f, 0, &baseline); assert(!f.probe.failed); close_fixture(&f);
        }
        free(baseline);
    }
    puts("native Intl general OOM ownership and recovery passed");
    return 0;
}
#else
int main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
#endif
