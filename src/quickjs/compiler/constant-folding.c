/* Literal expression folding.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include "compiler-internal.h"
#include "../internal/allocator.h"
#include "../internal/atom.h"
#include "../internal/bigint.h"
#include "../internal/bytecode.h"
#include "../internal/bytecode-format.h"
#include "../internal/string.h"

#define FOLD_STRING_LIMIT 4096
#define FOLD_STEP_LIMIT 32

static BOOL is_literal(JSFunctionDef *s, int op, const uint8_t *pc)
{
    switch (op) {
    case OP_push_i32:
    case OP_push_bigint_i32:
    case OP_push_atom_value:
    case OP_push_true:
    case OP_push_false:
    case OP_null:
    case OP_undefined:
        return TRUE;
    case OP_push_const:
        {
            JSValueConst value = s->cpool[get_u32(pc + 1)];
            return JS_IsNumber(value) || JS_IsString(value) ||
                JS_IsBigInt(s->ctx, value);
        }
    default:
        return FALSE;
    }
}

static uint32_t literal_string_length(JSValueConst value)
{
    if (JS_VALUE_GET_TAG(value) == JS_TAG_STRING_ROPE)
        return ((JSStringRope *)JS_VALUE_GET_PTR(value))->len;
    return JS_VALUE_GET_STRING(value)->len;
}

/* Return an owned primitive; objects and function constants are never read. */
static JSValue read_literal(JSContext *ctx, JSFunctionDef *s,
                            const uint8_t *pc)
{
    switch (pc[0]) {
    case OP_push_i32:
        return JS_NewInt32(ctx, get_i32(pc + 1));
    case OP_push_bigint_i32:
        return __JS_NewShortBigInt(get_i32(pc + 1));
    case OP_push_atom_value:
        return JS_AtomToString(ctx, get_u32(pc + 1));
    case OP_push_const:
        return JS_DupValue(ctx, s->cpool[get_u32(pc + 1)]);
    case OP_push_true:
        return JS_TRUE;
    case OP_push_false:
        return JS_FALSE;
    case OP_null:
        return JS_NULL;
    default:
        assert(pc[0] == OP_undefined);
        return JS_UNDEFINED;
    }
}

/* Speculative values own their references; input bytecode remains untouched.
   Record only complete prefixes and never consume a label or user operation. */
static int fold_addition(JSContext *ctx, JSFunctionDef *s, int start,
                         JSValue *pres, int *pline)
{
    const uint8_t *buf = s->byte_code.buf;
    int pos = start, best = start, line = *pline, count = 0, steps;
    JSValue stack[FOLD_STEP_LIMIT], result, left, right;
    double a, b;

    *pres = JS_UNDEFINED;
    for (steps = 0; steps < FOLD_STEP_LIMIT && pos < s->byte_code.size; steps++) {
        int op = buf[pos];
        int len = opcode_info[op].size;
        if (op == OP_line_num) {
            line = get_u32(buf + pos + 1);
            pos += len;
            continue;
        }
        if (is_literal(s, op, buf + pos)) {
            result = read_literal(ctx, s, buf + pos);
            if (JS_IsException(result))
                goto fail;
            if ((JS_IsString(result) &&
                 literal_string_length(result) > FOLD_STRING_LIMIT) ||
                (JS_VALUE_GET_TAG(result) == JS_TAG_BIG_INT &&
                 ((JSBigInt *)JS_VALUE_GET_PTR(result))->len > 64)) {
                JS_FreeValue(ctx, result);
                break;
            }
            stack[count++] = result;
            pos += len;
            continue;
        }
        if (op != OP_add || count < 2)
            break;
        left = stack[count - 2];
        right = stack[count - 1];
        if (JS_IsString(left) || JS_IsString(right)) {
            left = JS_ToString(ctx, left);
            if (JS_IsException(left))
                goto fail;
            right = JS_ToString(ctx, right);
            if (JS_IsException(right)) {
                JS_FreeValue(ctx, left);
                goto fail;
            }
            if (literal_string_length(left) + literal_string_length(right) >
                FOLD_STRING_LIMIT) {
                JS_FreeValue(ctx, left);
                JS_FreeValue(ctx, right);
                break;
            }
            result = JS_ConcatString(ctx, left, right);
            if (JS_IsException(result))
                goto fail;
        } else {
            /* BigInt arithmetic and incompatible numeric types remain runtime
               operations here. Primitive Number conversions cannot call JS. */
            if (JS_IsBigInt(ctx, left) || JS_IsBigInt(ctx, right))
                break;
            if (JS_ToFloat64(ctx, &a, left) || JS_ToFloat64(ctx, &b, right))
                goto fail;
            result = JS_NewFloat64(ctx, a + b);
        }
        JS_FreeValue(ctx, stack[--count]);
        JS_FreeValue(ctx, stack[--count]);
        stack[count++] = result;
        pos += len;
        if (count == 1) {
            JS_FreeValue(ctx, *pres);
            *pres = JS_DupValue(ctx, result);
            *pline = line;
            best = pos;
        }
    }
    while (count)
        JS_FreeValue(ctx, stack[--count]);
    return best;
 fail:
    while (count)
        JS_FreeValue(ctx, stack[--count]);
    JS_FreeValue(ctx, *pres);
    *pres = JS_UNDEFINED;
    return -1;
}

static int put_instruction(JSContext *ctx, DynBuf *out, const uint8_t *pc)
{
    const JSOpCode *oi = &opcode_info[pc[0]];
    JSAtom atom = JS_ATOM_NULL;

    switch (oi->fmt) {
    case OP_FMT_atom:
    case OP_FMT_atom_u8:
    case OP_FMT_atom_u16:
    case OP_FMT_atom_label_u8:
    case OP_FMT_atom_label_u16:
        atom = JS_DupAtom(ctx, get_u32(pc + 1));
        break;
    default:
        break;
    }
    if (dbuf_put(out, pc, oi->size)) {
        JS_FreeAtom(ctx, atom);
        return -1;
    }
    return 0;
}

static int put_literal(JSContext *ctx, JSFunctionDef *s, DynBuf *out,
                        JSValueConst value)
{
    uint8_t code[5];
    JSAtom atom = JS_ATOM_NULL;
    int index;

    if (JS_VALUE_GET_TAG(value) == JS_TAG_INT) {
        code[0] = OP_push_i32;
        put_u32(code + 1, JS_VALUE_GET_INT(value));
    } else {
        if (JS_IsString(value)) {
            atom = JS_ValueToAtom(ctx, value);
            if (atom == JS_ATOM_NULL)
                return -1;
        }
        if (atom != JS_ATOM_NULL && !__JS_AtomIsTaggedInt(atom)) {
            code[0] = OP_push_atom_value;
            put_u32(code + 1, atom);
        } else {
            JS_FreeAtom(ctx, atom);
            atom = JS_ATOM_NULL;
            if (js_resize_array(ctx, (void *)&s->cpool, sizeof(*s->cpool),
                                &s->cpool_size, s->cpool_count + 1))
                return -1;
            index = s->cpool_count++;
            s->cpool[index] = JS_DupValue(ctx, value);
            code[0] = OP_push_const;
            put_u32(code + 1, index);
        }
    }
    if (dbuf_put(out, code, sizeof(code))) {
        JS_FreeAtom(ctx, atom);
        return -1;
    }
    return 0;
}

int js_fold_literal_expressions(JSContext *ctx, JSFunctionDef *s)
{
    DynBuf out;
    const uint8_t *buf = s->byte_code.buf;
    int pos, next, op, line = s->source_pos, depth = 0;
    BOOL candidate = FALSE;
    JSValue value;
    uint8_t marker[5];

    if (!(s->compiler_optimization_flags & JS_COMPILER_OPT_CONSTANT_FOLDING))
        return 0;
    /* Most functions have no adjacent literal operands. Avoid constructing
       another bytecode buffer or retaining duplicate atoms for those. */
    for (pos = 0; pos < s->byte_code.size; pos += opcode_info[op].size) {
        op = buf[pos];
        if (is_literal(s, op, buf + pos)) {
            depth++;
        } else if (op == OP_add && depth >= 2) {
            candidate = TRUE;
            break;
        } else if (op != OP_line_num) {
            depth = 0;
        }
    }
    if (!candidate)
        return 0;
    dbuf_init2(&out, ctx->rt, js_realloc_bytecode_rt);
    for (pos = 0; pos < s->byte_code.size; pos = next) {
        op = buf[pos];
        next = pos + opcode_info[op].size;
        if (is_literal(s, op, buf + pos)) {
            int folded_line = line;
            int end = fold_addition(ctx, s, pos, &value, &folded_line);
            if (end < 0)
                goto fail;
            if (end > pos) {
                if (folded_line != line) {
                    marker[0] = OP_line_num;
                    put_u32(marker + 1, folded_line);
                    if (dbuf_put(&out, marker, sizeof(marker))) {
                        JS_FreeValue(ctx, value);
                        goto fail;
                    }
                    line = folded_line;
                }
                if (put_literal(ctx, s, &out, value)) {
                    JS_FreeValue(ctx, value);
                    goto fail;
                }
                JS_FreeValue(ctx, value);
                next = end;
                continue;
            }
            JS_FreeValue(ctx, value);
        }
        if (op == OP_line_num)
            line = get_u32(buf + pos + 1);
        if (op == OP_label)
            s->label_slots[get_u32(buf + pos + 1)].pos2 = out.size + 5;
        if (put_instruction(ctx, &out, buf + pos))
            goto fail;
    }
    free_bytecode_atoms(ctx->rt, buf, s->byte_code.size, FALSE);
    dbuf_free(&s->byte_code);
    s->byte_code = out;
    return 0;
 fail:
    free_bytecode_atoms(ctx->rt, out.buf, out.size, FALSE);
    dbuf_free(&out);
    if (!JS_HasException(ctx))
        JS_ThrowOutOfMemory(ctx);
    return -1;
}
