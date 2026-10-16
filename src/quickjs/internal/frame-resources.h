/*
 * QuickJS lexical frame resource interface
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#ifndef QUICKJS_FRAME_RESOURCES_H
#define QUICKJS_FRAME_RESOURCES_H

#include "disposal.h"

/* Contiguous hidden locals, rooted by the ordinary function frame. Persistent
   errors are separate from the compiler's completion/Await transport locals. */
enum {
    JS_DISPOSAL_FRAME_ERROR,
    JS_DISPOSAL_FRAME_FLAGS,
    JS_DISPOSAL_FRAME_SIZE,
};

/* Own references independently of the user binding. METHOD is undefined when
   inactive, null for a registered nullish async resource, or callable. */
enum {
    JS_DISPOSAL_RECORD_VALUE,
    JS_DISPOSAL_RECORD_METHOD,
    JS_DISPOSAL_RECORD_SIZE,
};

int js_init_resource_loc(JSContext *ctx, JSValue *record,
                         JSValueConst value, JSDisposalKind kind);
/* First input seeds the completion; later input resumes Await. Return 0 to
   advance, 1 with an owned Await operand to retry this instruction, or -1
   with the final throw completion in the context exception slot. */
int js_dispose_resource_loc(JSContext *ctx, JSValue *state, JSValue *record,
                            JSDisposalKind kind, JSValueConst input,
                            BOOL is_throw, JSValue *await_value);
int js_finish_resource_scope(JSContext *ctx, JSValue *state,
                             JSValueConst input, BOOL is_throw,
                             JSValue *await_value);

#endif
