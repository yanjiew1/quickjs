/*
 * QuickJS native Intl services and tests
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
/* Native Intl allocation failure and cross-context ownership tests.
 * Run only in an enabled build after all four service stages are integrated.
 * Original C test; ICU allocation paths additionally need ASan/LSan CI. */
#include "quickjs.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef CONFIG_INTL
typedef struct FailureState {
    size_t live, calls, fail_at;
    int armed, failed;
} FailureState;
typedef union AllocationHeader {
    max_align_t alignment;
    struct { size_t size; } h;
} AllocationHeader;
static int should_fail(JSMallocState *s)
{
    FailureState *f = s->opaque;
    if (!f->armed) return 0;
    if (f->calls++ == f->fail_at) { f->failed++; return 1; }
    return 0;
}
static void *failure_malloc(JSMallocState *s, size_t size)
{
    FailureState *f = s->opaque;
    AllocationHeader *h;
    if (size > SIZE_MAX - sizeof(*h) || should_fail(s)) return NULL;
    h = malloc(sizeof(*h) + (size ? size : 1));
    if (!h) return NULL;
    h->h.size = size;
    f->live++;
    s->malloc_count++;
    s->malloc_size += size + sizeof(*h);
    return h + 1;
}
static void failure_free(JSMallocState *s, void *ptr)
{
    FailureState *f = s->opaque;
    AllocationHeader *h;
    if (!ptr) return;
    h = (AllocationHeader *)ptr - 1;
    assert(f->live);
    f->live--;
    s->malloc_count--;
    s->malloc_size -= h->h.size + sizeof(*h);
    free(h);
}
static void *failure_realloc(JSMallocState *s, void *ptr, size_t size)
{
    AllocationHeader *h;
    size_t old;
    if (!ptr) return failure_malloc(s, size);
    if (!size) { failure_free(s, ptr); return NULL; }
    if (size > SIZE_MAX - sizeof(*h) || should_fail(s)) return NULL;
    h = (AllocationHeader *)ptr - 1;
    old = h->h.size;
    h = realloc(h, sizeof(*h) + size);
    if (!h) return NULL;
    h->h.size = size;
    s->malloc_size = s->malloc_size - old + size;
    return h + 1;
}
static size_t failure_usable_size(const void *ptr)
{
    return ptr ? ((const AllocationHeader *)ptr - 1)->h.size : 0;
}
static const JSMallocFunctions failure_functions = {
    failure_malloc, failure_free, failure_realloc, failure_usable_size
};
static JSValue evaluate(JSContext *ctx, const char *source, int flags)
{
    return JS_Eval(ctx, source, strlen(source), "intl-api", flags);
}
static void clear_exception(JSContext *ctx)
{
    JS_FreeValue(ctx, JS_GetException(ctx));
}
/* Compile before arming so failures exercise constructor/format/parts paths.
   Each runtime is destroyed, checking that every engine allocation is freed.
   A success run counts allocations; then each observed host allocation fails
   once. Pooled engine allocations may not reach the host allocator. */
static void failure_sweep(const char *source)
{
    size_t limit = 0;
    for (size_t index = 0;; index++) {
        FailureState f = {0};
        JSRuntime *rt = JS_NewRuntime2(&failure_functions, &f);
        JSContext *ctx;
        JSValue compiled, result;
        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
        compiled = evaluate(ctx, source, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(compiled));
        f.armed = 1;
        f.fail_at = index == 0 ? SIZE_MAX : index - 1;
        result = JS_EvalFunction(ctx, compiled);
        f.armed = 0;
        if (index == 0) {
            assert(!JS_IsException(result));
            limit = f.calls;
        } else {
            assert(f.failed == 1);
            assert(JS_IsException(result));
            clear_exception(ctx);
        }
        JS_FreeValue(ctx, result);
        JS_RunGC(rt);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(f.live == 0);
        if (index == limit) break;
    }
}
/* Repeat ordinary lifetime work separately from the exhaustive fault sweep.
   No failure is injected here; the original twelve iterations must succeed,
   and destroying each runtime must release all counted engine allocations. */
static void repeated_lifetime(const char *source)
{
    FailureState f = {0};
    JSRuntime *rt = JS_NewRuntime2(&failure_functions, &f);
    JSContext *ctx;
    JSValue compiled, result;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = evaluate(ctx, source, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    result = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(f.live == 0);
}
static void cross_context(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *first = JS_NewContext(rt), *second = JS_NewContext(rt);
    JSValue foreign, global, result;
    assert(rt && first && second);
    foreign = evaluate(first,
        "[new Intl.PluralRules('en'),new Intl.ListFormat('en'),"
        "new Intl.RelativeTimeFormat('en'),new Intl.DisplayNames('en',{type:'region'}),"
        "(()=>{function F(){};F.prototype=null;return F})(),"
        "Intl.PluralRules.prototype,Intl.ListFormat.prototype,"
        "Intl.RelativeTimeFormat.prototype,Intl.DisplayNames.prototype]",
        JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(foreign));
    global = JS_GetGlobalObject(second);
    assert(JS_DefinePropertyValueStr(second, global, "foreign", foreign, JS_PROP_C_W_E) >= 0);
    JS_FreeValue(second, global);
    result = evaluate(second,
        "if(Intl.PluralRules.prototype.select.call(foreign[0],1)!=='one')throw Error('plural');"
        "if(Intl.ListFormat.prototype.format.call(foreign[1],['A','B'])!=='A and B')throw Error('list');"
        "if(Intl.RelativeTimeFormat.prototype.format.call(foreign[2],1,'day')!=='in 1 day')throw Error('relative');"
        "if(Intl.DisplayNames.prototype.of.call(foreign[3],'US')!=='United States')throw Error('display');"
        "for(const [i,C] of [Intl.PluralRules,Intl.ListFormat,Intl.RelativeTimeFormat,Intl.DisplayNames].entries()){"
        "let x=Reflect.construct(C,['en',{type:i===3?'region':undefined}],foreign[4]);"
        "if(Object.getPrototypeOf(x)!==foreign[5+i])throw Error('foreign prototype');}",
        JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    JS_FreeValue(second, result);
    /* Their native ICU state survives destruction of the creation context. */
    JS_FreeContext(first);
    JS_RunGC(rt);
    result = evaluate(second, "foreign.slice(0,4).map(x=>x.resolvedOptions().locale).join(',')", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    JS_FreeValue(second, result);
    JS_FreeContext(second);
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
}
/* Each distinct operation is fault-swept once. Its original twelve repeated
   lifetime executions use the identical body below, without fault injection. */
#define INTL_PLURAL_OPERATIONS \
    "let p=new Intl.PluralRules('ar',{minimumFractionDigits:2});" \
    "p.select('90071992547409921.005');" \
    "p.selectRange(1,2);" \
    "p.resolvedOptions();"
#define INTL_LIST_OPERATIONS \
    "let p=new Intl.ListFormat('he');" \
    "p.format(['A','','B']);" \
    "p.formatToParts(['','','B']);" \
    "p.resolvedOptions();"
#define INTL_RELATIVE_OPERATIONS \
    "let p=new Intl.RelativeTimeFormat('ar-u-nu-arab');" \
    "p.format(-0,'day');" \
    "p.formatToParts(-12345.67,'day');" \
    "p.resolvedOptions();"
#define INTL_DISPLAY_OPERATIONS \
    "let p=new Intl.DisplayNames('fr',{type:'language'});" \
    "p.of('en-GB');" \
    "p.resolvedOptions();" \
    "new Intl.DisplayNames('fr',{type:'dateTimeField'}).of('month');"
static const struct {
    const char *once, *repeated;
} service_sources[] = {
    { "{" INTL_PLURAL_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_PLURAL_OPERATIONS "}" },
    { "{" INTL_LIST_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_LIST_OPERATIONS "}" },
    { "{" INTL_RELATIVE_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_RELATIVE_OPERATIONS "}" },
    { "{" INTL_DISPLAY_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_DISPLAY_OPERATIONS "}" },
};
#undef INTL_PLURAL_OPERATIONS
#undef INTL_LIST_OPERATIONS
#undef INTL_RELATIVE_OPERATIONS
#undef INTL_DISPLAY_OPERATIONS
int main(void)
{
    for (size_t i = 0; i < sizeof(service_sources) / sizeof(service_sources[0]); i++) {
        failure_sweep(service_sources[i].once);
        repeated_lifetime(service_sources[i].repeated);
    }
    cross_context();
    puts("Intl service API tests passed");
    return 0;
}
#else
int main(void) { return 0; }
#endif
