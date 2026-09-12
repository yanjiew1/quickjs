/*
 * QuickJS Memory Allocation Subsystem
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 */
#ifndef QUICKJS_ALLOC_H
#define QUICKJS_ALLOC_H

#include "quickjs/def.h"

void js_malloc_init(JSMallocContext *s);
void *__js_malloc(JSMallocContext *s, size_t size);
void __js_free(JSMallocContext *s, void *ptr);
void *__js_realloc(JSMallocContext *s, void *ptr, size_t size);
size_t __js_malloc_usable_size(JSMallocContext *s, const char *ptr);

extern const JSMallocFunctions def_malloc_funcs;

void *js_malloc_rt(JSRuntime *rt, size_t size);
void js_free_rt(JSRuntime *rt, void *ptr);
void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size);
size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr);
void *js_mallocz_rt(JSRuntime *rt, size_t size);

void *js_malloc(JSContext *ctx, size_t size);
void *js_mallocz(JSContext *ctx, size_t size);
void js_free(JSContext *ctx, void *ptr);
void *js_realloc(JSContext *ctx, void *ptr, size_t size);
void *js_realloc2(JSContext *ctx, void *ptr, size_t size, size_t *pslack);
size_t js_malloc_usable_size(JSContext *ctx, const void *ptr);
char *js_strndup(JSContext *ctx, const char *s, size_t n);
char *js_strdup(JSContext *ctx, const char *str);

#endif /* QUICKJS_ALLOC_H */
