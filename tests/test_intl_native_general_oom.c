/* Native general-operation ownership and allocation-failure recovery. MIT. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "quickjs.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "intl-allocation-probe.h"
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
static size_t call(Fixture *f, size_t failure)
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
        assert(!f->probe.failed && !JS_HasException(f->ctx)); JS_FreeValue(f->ctx, value);
    }
    return attempts;
}
int main(void)
{
    size_t operation, allocations, failure;
    Fixture f;
    for (operation = 0; operation < sizeof(operations)/sizeof(operations[0]); operation++) {
        open_fixture(&f, operations[operation]); allocations = call(&f, SIZE_MAX);
        assert(allocations < SIZE_MAX - 1 && !f.probe.failed); close_fixture(&f);
        for (failure = 1; failure <= allocations + 1; failure++) {
            open_fixture(&f, operations[operation]); call(&f, failure);
            assert(f.probe.failed == (failure <= allocations));
            call(&f, 0); assert(!f.probe.failed); close_fixture(&f);
        }
    }
    puts("native Intl general OOM ownership and recovery passed");
    return 0;
}
#else
int main(void) { return 0; }
#endif
