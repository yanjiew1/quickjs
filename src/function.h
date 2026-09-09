/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
/* Internal call ownership helpers. */
#ifndef QUICKJS_FUNCTION_H
#define QUICKJS_FUNCTION_H

#include "config.h"
#include "quickjs.h"

/* Call a function, consuming func_obj; this_obj and argv are borrowed. */
JS_INTERNAL JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv);


#include "runtime.h"

#define JS_MODE_STRICT (1 << 0)
#define JS_MODE_ASYNC  (1 << 2) /* async function */
#define JS_MODE_BACKTRACE_BARRIER (1 << 3) /* stop backtrace before this frame */

typedef struct JSStackFrame {
    struct JSStackFrame *prev_frame; /* NULL if first stack frame */
    JSValue cur_func; /* current function, JS_UNDEFINED if the frame is detached */
    JSValue *arg_buf; /* arguments */
    JSValue *var_buf; /* variables */
    struct JSVarRef **var_refs; /* references to arguments or local variables */ 
    const uint8_t *cur_pc; /* only used in bytecode functions : PC of the
                        instruction after the call */
    int arg_count;
    int js_mode; /* not supported for C functions */
    /* only used in generators. Current stack pointer value. NULL if
       the function is running. */
    JSValue *cur_sp;
} JSStackFrame;


static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}
/* Return an owned argument vector and length from a borrowed array-like value. */
JS_INTERNAL JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);

/* Release an argument vector and all its owned values. */
JS_INTERNAL void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);

/* Shared Function.apply/Reflect.apply implementation; all inputs are borrowed. */
JS_INTERNAL JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);


#endif /* QUICKJS_FUNCTION_H */
