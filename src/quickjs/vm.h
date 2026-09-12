/*
 * QuickJS VM & Interpreter Subsystem Header
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 */
#ifndef QUICKJS_VM_H
#define QUICKJS_VM_H

#include "quickjs/def.h"

typedef enum JSGeneratorStateEnum {
    JS_GENERATOR_STATE_SUSPENDED_START,
    JS_GENERATOR_STATE_SUSPENDED_YIELD,
    JS_GENERATOR_STATE_SUSPENDED_YIELD_STAR,
    JS_GENERATOR_STATE_EXECUTING,
    JS_GENERATOR_STATE_COMPLETED,
} JSGeneratorStateEnum;

typedef struct JSGeneratorData {
    JSGeneratorStateEnum state;
    JSAsyncFunctionState *func_state;
} JSGeneratorData;

void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
JSValue async_func_resume(JSContext *ctx, JSAsyncFunctionState *s);
JSAsyncFunctionState *async_func_init(JSContext *ctx, JSValueConst func_obj,
                                     JSValueConst this_obj, int argc,
                                     JSValueConst *argv);

#define JS_CALL_FLAG_COPY_ARGV   (1 << 1)
#define JS_CALL_FLAG_GENERATOR   (1 << 2)

const char *get_prop_string(JSContext *ctx, JSValueConst val, JSAtom atom);

JSValue JS_CallInternal(JSContext *ctx, JSValueConst func_obj,
                        JSValueConst this_obj, JSValueConst new_target,
                        int argc, JSValue *argv, int flags);
JSValue JS_CallConstructorInternal(JSContext *ctx,
                                   JSValueConst func_obj,
                                   JSValueConst new_target,
                                   int argc, JSValue *argv, int flags);
JSValue JS_CallConstructor2(JSContext *ctx, JSValueConst func_obj,
                            JSValueConst new_target,
                            int argc, JSValueConst *argv);
JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                      int argc, JSValueConst *argv);

JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                   JSValueConst this_obj,
                                   int argc, JSValueConst *argv,
                                   int flags);
void js_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_generator_mark(JSRuntime *rt, JSValueConst val,
                       JS_MarkFunc *mark_func);
void free_generator_stack_rt(JSRuntime *rt, JSGeneratorData *s);
void free_generator_stack(JSContext *ctx, JSGeneratorData *s);
void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                             JS_MarkFunc *mark_func);

JSValue JS_Throw(JSContext *ctx, JSValue obj);
JSValue JS_GetException(JSContext *ctx);
JS_BOOL JS_HasException(JSContext *ctx);
void JS_IgnoreError(JSContext *ctx, BOOL reset_uncatchable_error);
void JS_ResetUncatchableException(JSContext *ctx);
JSValue JS_NewError(JSContext *ctx);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowSyntaxError(JSContext *ctx, const char *fmt, ...);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowTypeError(JSContext *ctx, const char *fmt, ...);
int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowReferenceError(JSContext *ctx, const char *fmt, ...);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowRangeError(JSContext *ctx, const char *fmt, ...);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
JSValue JS_ThrowStackOverflow(JSContext *ctx);
JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx, JSValueConst func_obj);
JSValue JS_ThrowTypeErrorNotASymbol(JSContext *ctx);
JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx, JSFunctionBytecode *b, int idx, BOOL is_arg);
JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
void JS_ThrowInterrupted(JSContext *ctx);
__exception int js_poll_interrupts(JSContext *ctx);
void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                     const char *filename, int line_num, int col_num,
                     int backtrace_flags);

#endif /* QUICKJS_VM_H */
