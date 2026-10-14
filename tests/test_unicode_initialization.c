/* Engine constructor must stop before calling its allocator on ICU init error.
   GNU linker wrapping changes this test process only; no ICU memory hooks. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "quickjs.h"
#include "cutils.h"
#include "libunicode.h"
#ifdef CONFIG_UNICODE_ICU
#include <unicode/uclean.h>
static int fail_init;

#define QJS_JOIN_INNER(a, b) a ## b
#define QJS_JOIN(a, b) QJS_JOIN_INNER(a, b)
void QJS_JOIN(__real_, u_init)(UErrorCode *status);
void QJS_JOIN(__wrap_, u_init)(UErrorCode *status)
{
    if (fail_init)
        *status = U_MEMORY_ALLOCATION_ERROR;
    else
        QJS_JOIN(__real_, u_init)(status);
}
static int allocator_calls;
static void *count_malloc(JSMallocState *s, size_t size)
{
    (void)s; (void)size; allocator_calls++; return NULL;
}
static void count_free(JSMallocState *s, void *ptr)
{
    (void)s; free(ptr);
}
static void *count_realloc(JSMallocState *s, void *ptr, size_t size)
{
    (void)s; (void)ptr; (void)size; allocator_calls++; return NULL;
}
static size_t count_usable_size(const void *ptr)
{
    (void)ptr; return 0;
}
#endif
int main(void)
{
    const char *error = NULL;
    JSRuntime *rt;
#ifdef CONFIG_UNICODE_ICU
    JSMallocFunctions mf = {count_malloc, count_free, count_realloc, count_usable_size};
    fail_init = 1;
    assert(lre_unicode_initialize(&error) == -1 && error != NULL);
    assert(JS_NewRuntime2(&mf, NULL) == NULL && allocator_calls == 0);
    fail_init = 0;
#endif
    assert(lre_unicode_initialize(&error) == 0 && error == NULL);
    rt = JS_NewRuntime();
    assert(rt != NULL);
    JS_FreeRuntime(rt);
    puts("Unicode provider initialization failure/recovery contract passed");
    return 0;
}
