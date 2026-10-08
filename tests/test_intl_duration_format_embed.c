/* Standalone CONFIG_INTL embedding/OOM test. Compile with enabled libquickjs.
 * Exercises failure positions in native constructors/ICU-result adaptation,
 * destruction with cached bound functions, and their creation realm. */
#include "quickjs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef union AllocationHeader {
    max_align_t alignment;
    struct { size_t size; } data;
} AllocationHeader;
typedef struct FaultAllocator { size_t calls, fail_at, live_blocks; } FaultAllocator;
static int should_fail(FaultAllocator *f) { return f->fail_at && ++f->calls == f->fail_at; }
static void *fault_malloc(JSMallocState *state, size_t size) {
    FaultAllocator *f = state->opaque;
    AllocationHeader *p;
    if (should_fail(f) || size > SIZE_MAX-sizeof(*p)) return NULL;
    p = malloc(sizeof(*p)+size);
    if (!p) return NULL;
    p->data.size=size; f->live_blocks++; return p+1;
}
static void fault_free(JSMallocState *state, void *ptr) {
    FaultAllocator *f=state->opaque;
    if (ptr) { free((AllocationHeader*)ptr-1); f->live_blocks--; }
}
static void *fault_realloc(JSMallocState *state, void *ptr, size_t size) {
    FaultAllocator *f=state->opaque;
    AllocationHeader *p;
    if (!ptr) return fault_malloc(state,size);
    if (!size) {fault_free(state,ptr);return NULL;}
    if (should_fail(f) || size > SIZE_MAX-sizeof(*p)) return NULL;
    p=realloc((AllocationHeader*)ptr-1,sizeof(*p)+size);
    if (!p) return NULL;
    p->data.size=size;return p+1;
}
static size_t fault_usable(const void *ptr) { return ptr ? ((const AllocationHeader*)ptr-1)->data.size : 0; }
static const JSMallocFunctions allocator={fault_malloc,fault_free,fault_realloc,fault_usable};
static JSValue eval(JSContext *ctx,const char *source) {
    return JS_Eval(ctx,source,strlen(source),"intl-native-embed",JS_EVAL_TYPE_GLOBAL);
}
static int discard(JSContext *ctx,JSValue result) {
    int failure=JS_IsException(result);
    if (failure) {JSValue error=JS_GetException(ctx);JS_FreeValue(ctx,error);}
    JS_FreeValue(ctx,result);return failure;
}
int main(void) {
    const char *sources[]={

        "(()=>{const f=new Intl.DurationFormat('da',{style:'digital',fractionalDigits:9}); f.format({hours:1,minutes:2,seconds:3,nanoseconds:1}); f.formatToParts({hours:1,minutes:2,seconds:3}); return f;})"
    };
    FaultAllocator fault={0};
    JSRuntime *rt=JS_NewRuntime2(&allocator,&fault);
    JSContext *a,*b;
    JSValue result;
    int i,position,failed=0;
    if (!rt) return 1;
    a=JS_NewContext(rt); b=JS_NewContext(rt);
    if (!a || !b) return 2;
    if (JS_AddIntrinsicIntl(a) < 0 || JS_AddIntrinsicIntl(b) < 0) return 3;
    for (i=0;i<1;i++) {
        JSValue factory=eval(a,sources[i]);
        if (JS_IsException(factory)) return 3;
        for (position=1;position<=4096;position++) {
            size_t calls;
            fault.calls=0;fault.fail_at=position;
            result=JS_Call(a,factory,JS_UNDEFINED,0,NULL);
            calls=fault.calls;
            fault.fail_at=0;
            discard(a,result);JS_RunGC(rt);
            if (calls<(size_t)position) break;
        }
        if (position>4096) failed=1;
        JS_FreeValue(a,factory);
    }
    JS_FreeContext(a);JS_FreeContext(b);JS_FreeRuntime(rt);
    if (fault.live_blocks) failed=1;
    if (failed) fprintf(stderr,"Intl number/duration embedding test failed\n");
    return failed;
}
