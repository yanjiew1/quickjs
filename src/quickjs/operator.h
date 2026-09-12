/*
 * QuickJS Operator declarations
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 */
#ifndef QUICKJS_OPERATOR_H
#define QUICKJS_OPERATOR_H

#include "quickjs/def.h"
#include "quickjs/value.h"
#include "quickjs/opcode.h"

int js_unary_arith_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_post_inc_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_not_slow(JSContext *ctx, JSValue *sp);
int js_binary_arith_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_add_slow(JSContext *ctx, JSValue *sp);
int js_binary_logic_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_shr_slow(JSContext *ctx, JSValue *sp);
int js_relational_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_eq_slow(JSContext *ctx, JSValue *sp, BOOL is_neq);

int js_operator_in(JSContext *ctx, JSValue *sp);
int js_operator_private_in(JSContext *ctx, JSValue *sp);
int js_has_unscopable(JSContext *ctx, JSValueConst obj, JSAtom atom);
int js_operator_instanceof(JSContext *ctx, JSValue *sp);
JSAtom js_operator_typeof(JSContext *ctx, JSValueConst op1);
int js_operator_delete(JSContext *ctx, JSValue *sp);

#endif /* QUICKJS_OPERATOR_H */
