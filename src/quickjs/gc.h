/*
 * QuickJS Garbage Collection & Memory Management Subsystem Header
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 */
#ifndef QUICKJS_GC_H
#define QUICKJS_GC_H

#include "quickjs/def.h"

void js_trigger_gc(JSRuntime *rt, size_t size);
void set_cycle_flag(JSContext *ctx, JSValueConst obj);
void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
void free_object(JSRuntime *rt, JSObject *p);
void free_gc_object(JSRuntime *rt, JSGCObjectHeader *gp);
void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
void js_autoinit_free(JSRuntime *rt, JSProperty *pr);

void js_object_data_finalizer(JSRuntime *rt, JSValue val);
void js_object_data_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_c_function_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
void js_bound_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

#endif /* QUICKJS_GC_H */
