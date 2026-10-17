/* Bounded literal folding and local constant propagation.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include "compiler-internal.h"
#include "../internal/allocator.h"
#include "../internal/atom.h"
#include "../internal/bigint.h"
#include "../internal/bytecode.h"
#include "../internal/bytecode-format.h"
#include "../internal/string.h"
#include "../internal/number.h"

#define FOLD_STRING_LIMIT 4096
#define FOLD_STEP_LIMIT 32
#define FOLD_BIGINT_BITS 4096

typedef struct LocalConstant {
    JSValue value;
    uint32_t epoch;
    BOOL eligible;
} LocalConstant;

static BOOL eligible_local(const JSFunctionDef *s, int index)
{
    const JSVarDef *vd = &s->vars[index];
    return vd->is_lexical && !vd->is_captured &&
        vd->var_kind == JS_VAR_NORMAL;
}

static BOOL known_local(const LocalConstant *locals, uint32_t epoch,
                        int op, const uint8_t *pc)
{
    return locals && (op == OP_get_loc || op == OP_get_loc_check) &&
        locals[get_u16(pc + 1)].epoch == epoch;
}

static void free_local_constants(JSContext *ctx, LocalConstant *locals, int count)
{
    int i;
    if (!locals)
        return;
    for (i = 0; i < count; i++)
        JS_FreeValue(ctx, locals[i].value);
    js_free(ctx, locals);
}

/* No facts flow through joins, exceptional entries or suspended execution.
   Ordinary calls cannot change an uncaptured local in a function without eval. */
static BOOL ends_constant_region(int op)
{
    switch (opcode_info[op].fmt) {
    case OP_FMT_label:
    case OP_FMT_label_u16:
    case OP_FMT_atom_label_u8:
    case OP_FMT_atom_label_u16:
        return TRUE;
    default:
        break;
    }
    switch (op) {
    case OP_return: case OP_return_undef: case OP_return_async:
    case OP_ret: case OP_tail_call: case OP_tail_call_method:
    case OP_throw: case OP_throw_error:
    case OP_initial_yield: case OP_yield: case OP_yield_star: case OP_await:
        return TRUE;
    default:
        return FALSE;
    }
}

/* Keep every store and initialization check. Only remember a value after the
   instruction that installs it; unknown writes and scope cleanup kill it. */
static void update_local_constant(JSContext *ctx, LocalConstant *locals,
                                  uint32_t epoch, int op, const uint8_t *pc,
                                  BOOL known, JSValueConst value)
{
    int index;
    switch (op) {
    case OP_put_loc: case OP_set_loc:
    case OP_put_loc_check: case OP_set_loc_check: case OP_put_loc_check_init:
        index = get_u16(pc + 1);
        break;
    case OP_set_loc_uninitialized: case OP_close_loc:
        index = get_u16(pc + 1);
        known = FALSE;
        break;
    case OP_inc_loc: case OP_dec_loc: case OP_add_loc:
        index = pc[1];
        known = FALSE;
        break;
    default:
        return;
    }
    if (locals[index].eligible) {
        JS_FreeValue(ctx, locals[index].value);
        locals[index].value = known ? JS_DupValue(ctx, value) : JS_UNDEFINED;
        locals[index].epoch = known ? epoch : 0;
    }
}

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

static BOOL is_comparison(int op)
{
    switch (op) {
    case OP_lt: case OP_lte: case OP_gt: case OP_gte:
    case OP_eq: case OP_neq: case OP_strict_eq: case OP_strict_neq:
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOL is_binary_operation(int op)
{
    switch (op) {
    case OP_add: case OP_sub: case OP_mul: case OP_div: case OP_mod:
    case OP_pow: case OP_and: case OP_or: case OP_xor:
    case OP_shl: case OP_sar: case OP_shr:
        return TRUE;
    default:
        return is_comparison(op);
    }
}

static BOOL bigint_is_zero(JSValueConst value)
{
    if (JS_VALUE_GET_TAG(value) == JS_TAG_SHORT_BIG_INT)
        return JS_VALUE_GET_SHORT_BIG_INT(value) == 0;
    return ((JSBigInt *)JS_VALUE_GET_PTR(value))->len == 1 &&
        ((JSBigInt *)JS_VALUE_GET_PTR(value))->tab[0] == 0;
}

static BOOL bigint_is_negative(JSValueConst value)
{
    if (JS_VALUE_GET_TAG(value) == JS_TAG_SHORT_BIG_INT)
        return JS_VALUE_GET_SHORT_BIG_INT(value) < 0;
    return js_bigint_sign(JS_VALUE_GET_PTR(value));
}

/* Borrow a normalized BigInt, using temporary storage for immediate values. */
static JSBigInt *literal_bigint(JSValueConst value, JSBigIntBuf *buf)
{
    if (JS_VALUE_GET_TAG(value) == JS_TAG_SHORT_BIG_INT)
        return js_bigint_set_short(buf, value);
    return JS_VALUE_GET_PTR(value);
}

/* Conservative magnitude bound, including the carry from negating a negative
   two's-complement value. This budget is independent of the target limb size. */
static unsigned bigint_bits(const JSBigInt *value)
{
    BOOL negative = js_bigint_sign(value);
    js_limb_t high = value->tab[value->len - 1] ^ -(js_limb_t)negative;
    unsigned bits = (value->len - 1) * JS_LIMB_BITS + negative;

    if (high) {
#if JS_LIMB_BITS == 32
        bits += 32 - clz32(high);
#else
        bits += 64 - clz64(high);
#endif
    }
    return bits;
}

/* 1: folded, 0: leave the operation at runtime, -1: allocation failure.
   Operands are borrowed. Bound expensive operations before allocating results. */
static int fold_bigint(JSContext *ctx, int op, JSValueConst left,
                       JSValueConst right, JSValue *pres)
{
    JSBigIntBuf abuf, bbuf;
    JSBigInt *a = literal_bigint(left, &abuf), *b = NULL, *result;
    unsigned abits = bigint_bits(a), bbits = 0;
    js_slimb_t shift;

    if (abits > FOLD_BIGINT_BITS)
        return 0;
    if (op != OP_neg && op != OP_not) {
        b = literal_bigint(right, &bbuf);
        bbits = bigint_bits(b);
        if (bbits > FOLD_BIGINT_BITS)
            return 0;
    }
    switch (op) {
    case OP_neg: result = js_bigint_neg(ctx, a); break;
    case OP_not: result = js_bigint_not(ctx, a); break;
    case OP_add: case OP_sub:
        result = js_bigint_add(ctx, a, b, op == OP_sub);
        break;
    case OP_mul:
        if (abits + bbits > FOLD_BIGINT_BITS)
            return 0;
        result = js_bigint_mul(ctx, a, b);
        break;
    case OP_div: case OP_mod:
        result = js_bigint_divrem(ctx, a, b, op == OP_mod);
        break;
    case OP_and: case OP_or: case OP_xor:
        result = js_bigint_logic(ctx, a, b, op);
        break;
    case OP_pow:
        /* The helper handles 0, 1 and -1 without expanding the exponent. */
        if (!(a->len == 1 && (a->tab[0] <= 1 || a->tab[0] == (js_limb_t)-1)) &&
            (b->len != 1 || b->tab[0] > FOLD_BIGINT_BITS ||
             (uint64_t)abits * b->tab[0] > FOLD_BIGINT_BITS))
            return 0;
        result = js_bigint_pow(ctx, a, b);
        break;
    case OP_shl: case OP_sar:
        shift = js_bigint_get_si_sat(b);
        if (shift < -FOLD_BIGINT_BITS || shift > FOLD_BIGINT_BITS)
            return 0;
        if (op == OP_sar)
            shift = -shift;
        if (shift > 0 && abits + shift > FOLD_BIGINT_BITS)
            return 0;
        result = shift < 0 ? js_bigint_shr(ctx, a, -shift) :
            js_bigint_shl(ctx, a, shift);
        break;
    default:
        return 0;
    }
    if (!result)
        return -1;
    if (bigint_bits(result) > FOLD_BIGINT_BITS) {
        js_free(ctx, result);
        return 0;
    }
    *pres = JS_CompactBigIntRT(ctx->rt, result);
    return 1;
}

/* Compare primitive numeric values without rounding a BigInt to Number.
   Two denotes unordered (NaN), and must not be negated when swapping operands. */
static int compare_numeric_literals(JSContext *ctx, JSValueConst left,
                                    JSValueConst right, int *pres)
{
    JSBigIntBuf abuf, bbuf;
    BOOL a_big = JS_IsBigInt(ctx, left), b_big = JS_IsBigInt(ctx, right);
    double a, b;
    int cmp;

    if (a_big && b_big) {
        cmp = js_bigint_cmp(literal_bigint(left, &abuf),
                            literal_bigint(right, &bbuf));
    } else if (a_big || b_big) {
        if (JS_ToFloat64Free(ctx, &a, JS_DupValue(ctx, a_big ? right : left)))
            return -1;
        cmp = js_bigint_float64_cmp(literal_bigint(a_big ? left : right, &abuf), a);
        if (!a_big && cmp != 2)
            cmp = -cmp;
    } else {
        if (JS_ToFloat64Free(ctx, &a, JS_DupValue(ctx, left)) ||
            JS_ToFloat64Free(ctx, &b, JS_DupValue(ctx, right)))
            return -1;
        cmp = isnan(a) || isnan(b) ? 2 : (a > b) - (a < b);
    }
    *pres = cmp;
    return 0;
}

static int fold_comparison(JSContext *ctx, int op, JSValueConst left,
                           JSValueConst right, JSValue *pres)
{
    JSValue a = JS_DupValue(ctx, left), b = JS_DupValue(ctx, right);
    BOOL equality = op == OP_eq || op == OP_neq;
    int cmp, result;

    if (op == OP_strict_eq || op == OP_strict_neq) {
        result = JS_StrictEq(ctx, a, b) ^ (op == OP_strict_neq);
        goto done;
    }
    if (equality) {
        BOOL a_null = JS_IsNull(a) || JS_IsUndefined(a);
        BOOL b_null = JS_IsNull(b) || JS_IsUndefined(b);
        if (a_null || b_null) {
            result = (a_null && b_null) ^ (op == OP_neq);
            goto done;
        }
        if (JS_IsBool(a))
            a = JS_NewInt32(ctx, JS_VALUE_GET_BOOL(a));
        if (JS_IsBool(b))
            b = JS_NewInt32(ctx, JS_VALUE_GET_BOOL(b));
    }
    if (JS_IsString(a) && JS_IsString(b)) {
        if (equality) {
            result = JS_StrictEq(ctx, a, b) ^ (op == OP_neq);
            goto done;
        }
        cmp = js_string_rope_compare(a, b, FALSE);
        cmp = (cmp > 0) - (cmp < 0);
    } else {
        if (JS_IsBigInt(ctx, a) && JS_IsString(b)) {
            b = JS_StringToBigInt(ctx, b);
            if (JS_IsException(b))
                goto fail;
            if (!JS_IsBigInt(ctx, b)) {
                cmp = 2;
                goto compare;
            }
        } else if (JS_IsString(a) && JS_IsBigInt(ctx, b)) {
            a = JS_StringToBigInt(ctx, a);
            if (JS_IsException(a))
                goto fail;
            if (!JS_IsBigInt(ctx, a)) {
                cmp = 2;
                goto compare;
            }
        }
        if (compare_numeric_literals(ctx, a, b, &cmp))
            goto fail;
    }
 compare:
    switch (op) {
    case OP_lt: result = cmp != 2 && cmp < 0; break;
    case OP_lte: result = cmp != 2 && cmp <= 0; break;
    case OP_gt: result = cmp != 2 && cmp > 0; break;
    case OP_gte: result = cmp != 2 && cmp >= 0; break;
    case OP_eq: result = cmp == 0; break;
    default:
        assert(op == OP_neq);
        result = cmp != 0;
        break;
    }
 done:
    JS_FreeValue(ctx, a);
    JS_FreeValue(ctx, b);
    *pres = JS_NewBool(ctx, result);
    return 0;
 fail:
    JS_FreeValue(ctx, a);
    JS_FreeValue(ctx, b);
    return -1;
}

/* Speculative values own their references; input bytecode remains untouched.
   Record only complete prefixes and never consume a label or user operation. */
static int fold_literal_expression(JSContext *ctx, JSFunctionDef *s, int start,
                                    JSValue *pres, int *pline,
                                    int *perror, const char **pmessage,
                                    const LocalConstant *locals, uint32_t epoch,
                                    BOOL folding)
{
    const uint8_t *buf = s->byte_code.buf;
    int pos = start, best = start, line = *pline, count = 0, steps;
    JSValue stack[FOLD_STEP_LIMIT], result, left, right;
    double a, b, number;
    int32_t i1, i2;
    uint32_t bits;
    int error_type = JS_THROW_ERROR_TYPE;
    const char *message = "cannot convert bigint to number";

    *pres = JS_UNDEFINED;
    *perror = -1;
    *pmessage = NULL;
    for (steps = 0; steps < FOLD_STEP_LIMIT && pos < s->byte_code.size; steps++) {
        int op = buf[pos];
        int len = opcode_info[op].size;
        if (op == OP_line_num) {
            line = get_u32(buf + pos + 1);
            pos += len;
            continue;
        }
        if (is_literal(s, op, buf + pos) ||
            known_local(locals, epoch, op, buf + pos)) {
            BOOL local = known_local(locals, epoch, op, buf + pos);
            result = local ? JS_DupValue(ctx, locals[get_u16(buf + pos + 1)].value) :
                read_literal(ctx, s, buf + pos);
            if (JS_IsException(result))
                goto fail;
            if ((JS_IsString(result) &&
                 literal_string_length(result) > FOLD_STRING_LIMIT) ||
                (JS_VALUE_GET_TAG(result) == JS_TAG_BIG_INT &&
                 ((JSBigInt *)JS_VALUE_GET_PTR(result))->len > FOLD_BIGINT_BITS / JS_LIMB_BITS + 1)) {
                JS_FreeValue(ctx, result);
                break;
            }
            stack[count++] = result;
            pos += len;
            if (local && count == 1) {
                JS_FreeValue(ctx, *pres);
                *pres = JS_DupValue(ctx, result);
                *pline = line;
                best = pos;
            }
            continue;
        }
        if (!folding)
            break;
        /* A pure void operand leaves undefined. Do not cross arbitrary
           drops, since they can separate independently evaluated expressions. */
        if (op == OP_drop && count >= 1 && pos + len < s->byte_code.size &&
            buf[pos + len] == OP_undefined) {
            JS_FreeValue(ctx, stack[--count]);
            result = JS_UNDEFINED;
            len += opcode_info[OP_undefined].size;
            goto have_result;
        }
        if ((op == OP_plus || op == OP_neg || op == OP_not || op == OP_lnot) && count >= 1) {
            left = stack[count - 1];
            if (op == OP_lnot) {
                result = JS_NewBool(ctx, !JS_ToBool(ctx, left));
            } else if (JS_IsBigInt(ctx, left)) {
                int folded;
                if (op == OP_plus)
                    goto known_throw;
                folded = fold_bigint(ctx, op, left, JS_UNDEFINED, &result);
                if (folded < 0)
                    goto fail;
                if (!folded)
                    break;
            } else if (op == OP_not) {
                if (JS_ToInt32Free(ctx, &i1, JS_DupValue(ctx, left)))
                    goto fail;
                result = JS_NewInt32(ctx, ~i1);
            } else {
                if (JS_ToFloat64Free(ctx, &a, JS_DupValue(ctx, left)))
                    goto fail;
                result = JS_NewFloat64(ctx, op == OP_neg ? -a : a);
            }
            JS_FreeValue(ctx, stack[--count]);
            goto have_result;
        }
        if (!is_binary_operation(op) || count < 2)
            break;
        left = stack[count - 2];
        right = stack[count - 1];
        if (is_comparison(op)) {
            if (fold_comparison(ctx, op, left, right, &result))
                goto fail;
        } else if (op == OP_add && (JS_IsString(left) || JS_IsString(right))) {
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
            if (op == OP_shr && (JS_IsBigInt(ctx, left) || JS_IsBigInt(ctx, right))) {
                message = "bigint operands are forbidden for >>>";
                goto known_throw;
            }
            if (JS_IsBigInt(ctx, left) != JS_IsBigInt(ctx, right))
                goto known_throw;
            if (JS_IsBigInt(ctx, left)) {
                if ((op == OP_div || op == OP_mod) && bigint_is_zero(right)) {
                    error_type = JS_THROW_ERROR_RANGE;
                    message = "BigInt division by zero";
                    goto known_throw;
                }
                if (op == OP_pow && bigint_is_negative(right)) {
                    error_type = JS_THROW_ERROR_RANGE;
                    message = "BigInt negative exponent";
                    goto known_throw;
                }
                {
                    int folded = fold_bigint(ctx, op, left, right, &result);
                    if (folded < 0)
                        goto fail;
                    if (!folded)
                        break;
                }
                goto binary_result;
            }
            if (op == OP_and || op == OP_or || op == OP_xor ||
                op == OP_shl || op == OP_sar || op == OP_shr) {
                int shift;
                if (JS_ToInt32Free(ctx, &i1, JS_DupValue(ctx, left)) ||
                    JS_ToInt32Free(ctx, &i2, JS_DupValue(ctx, right)))
                    goto fail;
                shift = (uint32_t)i2 & 31;
                switch (op) {
                case OP_and: bits = (uint32_t)i1 & (uint32_t)i2; break;
                case OP_or: bits = (uint32_t)i1 | (uint32_t)i2; break;
                case OP_xor: bits = (uint32_t)i1 ^ (uint32_t)i2; break;
                case OP_shl: bits = (uint32_t)i1 << shift; break;
                default:
                    bits = (uint32_t)i1 >> shift;
                    if (op == OP_sar && i1 < 0 && shift != 0)
                        bits |= UINT32_MAX << (32 - shift);
                    break;
                }
                number = bits;
                if (op != OP_shr && bits > INT32_MAX)
                    number -= 4294967296.0;
            } else {
                if (JS_ToFloat64Free(ctx, &a, JS_DupValue(ctx, left)) ||
                    JS_ToFloat64Free(ctx, &b, JS_DupValue(ctx, right)))
                    goto fail;
                switch (op) {
                case OP_add: number = a + b; break;
                case OP_sub: number = a - b; break;
                case OP_mul: number = a * b; break;
                case OP_div: number = a / b; break;
                case OP_mod: number = fmod(a, b); break;
                default: number = js_pow(a, b); break;
                }
            }
            result = JS_NewFloat64(ctx, number);
        }
    binary_result:
        JS_FreeValue(ctx, stack[--count]);
        JS_FreeValue(ctx, stack[--count]);
    have_result:
        stack[count++] = result;
        pos += len;
        if (count == 1) {
            JS_FreeValue(ctx, *pres);
            *pres = JS_DupValue(ctx, result);
            *pline = line;
            best = pos;
        }
    }
    goto done;
 known_throw:
    JS_FreeValue(ctx, *pres);
    *pres = JS_UNDEFINED;
    *perror = error_type;
    *pmessage = message;
    *pline = line;
    best = pos + opcode_info[buf[pos]].size;
 done:
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

static int put_throw(JSContext *ctx, DynBuf *out, int error, const char *message)
{
    uint8_t code[6];
    JSAtom atom = JS_NewAtom(ctx, message);

    if (atom == JS_ATOM_NULL)
        return -1;
    code[0] = OP_throw_error;
    put_u32(code + 1, atom);
    code[5] = error;
    if (dbuf_put(out, code, sizeof(code))) {
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
    int index, len = sizeof(code);

    if (JS_VALUE_GET_TAG(value) == JS_TAG_INT) {
        code[0] = OP_push_i32;
        put_u32(code + 1, JS_VALUE_GET_INT(value));
    } else if (JS_IsBool(value) || JS_IsNull(value) || JS_IsUndefined(value)) {
        code[0] = JS_IsBool(value) ? (JS_VALUE_GET_BOOL(value) ? OP_push_true : OP_push_false) :
            (JS_IsNull(value) ? OP_null : OP_undefined);
        len = 1;
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
    if (dbuf_put(out, code, len)) {
        JS_FreeAtom(ctx, atom);
        return -1;
    }
    return 0;
}

/* Recognize only the adjacent short-circuit scaffold emitted by the parser.
   Labels and source markers are never crossed by this match. */
static int literal_branch(const uint8_t *buf, int size, int pos,
                           BOOL *pnullish)
{
    int branch = pos + 1, end;

    if (buf[pos] != OP_dup || branch >= size)
        return -1;
    *pnullish = buf[branch] == OP_is_undefined_or_null;
    if (*pnullish)
        branch++;
    if (branch >= size || (buf[branch] != OP_if_true && buf[branch] != OP_if_false))
        return -1;
    end = branch + opcode_info[buf[branch]].size;
    return end < size && buf[end] == OP_drop ? branch : -1;
}

int js_optimize_constants(JSContext *ctx, JSFunctionDef *s)
{
    DynBuf out;
    const uint8_t *buf = s->byte_code.buf;
    int pos, next, op, i, line = s->source_pos, depth = 0;
    BOOL candidate = FALSE, propagate = FALSE, pending_known = FALSE;
    BOOL folding = s->compiler_optimization_flags & JS_COMPILER_OPT_CONSTANT_FOLDING;
    LocalConstant *locals = NULL;
    uint32_t epoch = 1;
    JSValue value, pending = JS_UNDEFINED;
    uint8_t marker[5];

    if ((s->compiler_optimization_flags & JS_COMPILER_OPT_CONSTANT_PROPAGATION) &&
        !s->has_eval_call) {
        for (i = 0; i < s->var_count; i++) {
            if (eligible_local(s, i)) {
                propagate = TRUE;
                break;
            }
        }
    }
    if (!folding && !propagate)
        return 0;
    /* Avoid constructing another bytecode buffer for functions without a
       foldable expression or an eligible local read. */
    if (propagate) {
        BOOL seed = FALSE, literal_top = FALSE;
        propagate = FALSE;
        for (pos = 0; pos < s->byte_code.size; pos += opcode_info[op].size) {
            op = buf[pos];
            if ((op == OP_get_loc || op == OP_get_loc_check) && seed &&
                eligible_local(s, get_u16(buf + pos + 1))) {
                propagate = candidate = TRUE;
                break;
            }
            /* A local fact must originate in a possible primitive store.
               This inexpensive filter tolerates false positives; it does
               not establish facts or remove instructions. */
            if (is_literal(s, op, buf + pos)) {
                literal_top = TRUE;
            } else if (op == OP_put_loc || op == OP_set_loc ||
                       op == OP_put_loc_check || op == OP_set_loc_check ||
                       op == OP_put_loc_check_init) {
                if (literal_top && eligible_local(s, get_u16(buf + pos + 1)))
                    seed = TRUE;
                if (op != OP_set_loc && op != OP_set_loc_check)
                    literal_top = FALSE;
            } else if (op != OP_line_num && op != OP_dup &&
                       !(folding && (is_binary_operation(op) ||
                                      op == OP_plus || op == OP_neg ||
                                      op == OP_not || op == OP_lnot))) {
                literal_top = FALSE;
            }
            if (ends_constant_region(op))
                seed = literal_top = FALSE;
        }
    }
    if (!candidate && folding) {
        for (pos = 0; pos < s->byte_code.size; pos += opcode_info[op].size) {
            op = buf[pos];
            if (is_literal(s, op, buf + pos)) {
                depth++;
            } else if (op == OP_dup && depth >= 1) {
                BOOL nullish;
                if (literal_branch(buf, s->byte_code.size, pos, &nullish) >= 0) {
                    candidate = TRUE;
                    break;
                }
                depth = 0;
            } else if ((is_binary_operation(op) && depth >= 2) ||
                       ((op == OP_plus || op == OP_neg || op == OP_not || op == OP_lnot ||
                         (op == OP_drop && pos + 1 < s->byte_code.size &&
                          buf[pos + 1] == OP_undefined)) && depth >= 1)) {
                candidate = TRUE;
                break;
            } else if (op != OP_line_num) {
                depth = 0;
            }
        }
    }
    if (!candidate)
        return 0;
    dbuf_init2(&out, ctx->rt, js_realloc_bytecode_rt);
    if (propagate) {
        locals = js_mallocz(ctx, sizeof(*locals) * s->var_count);
        if (!locals)
            goto fail;
        for (i = 0; i < s->var_count; i++) {
            locals[i].value = JS_UNDEFINED;
            locals[i].eligible = eligible_local(s, i);
        }
    }
    for (pos = 0; pos < s->byte_code.size; pos = next) {
        op = buf[pos];
        next = pos + opcode_info[op].size;
        if (folding && pending_known && op == OP_dup) {
            BOOL nullish;
            int branch = literal_branch(buf, s->byte_code.size, pos, &nullish);
            if (branch >= 0) {
                int label = get_u32(buf + branch + 1);
                BOOL test = nullish ? (JS_IsNull(pending) || JS_IsUndefined(pending)) :
                    JS_ToBool(ctx, pending);
                BOOL taken = test == (buf[branch] == OP_if_true);
                if (taken) {
                    uint8_t jump[5];
                    jump[0] = OP_goto;
                    put_u32(jump + 1, label);
                    if (dbuf_put(&out, jump, sizeof(jump)))
                        goto fail;
                    epoch++;
                    JS_FreeValue(ctx, pending);
                    pending = JS_UNDEFINED;
                    pending_known = FALSE;
                } else {
                    assert(s->label_slots[label].ref_count > 0);
                    s->label_slots[label].ref_count--;
                }
                /* Keep the drop: it consumes the original operand when the
                   RHS runs, or becomes unreachable after the new goto. */
                next = branch + opcode_info[buf[branch]].size;
                continue;
            }
        }
        if (is_literal(s, op, buf + pos) || known_local(locals, epoch, op, buf + pos)) {
            int folded_line = line, error;
            const char *message;
            int end = fold_literal_expression(ctx, s, pos, &value, &folded_line,
                                              &error, &message, locals, epoch, folding);
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
                if (error >= 0 ? put_throw(ctx, &out, error, message) :
                    put_literal(ctx, s, &out, value)) {
                    JS_FreeValue(ctx, value);
                    goto fail;
                }
                JS_FreeValue(ctx, pending);
                pending = value;
                pending_known = error < 0;
                if (error >= 0)
                    epoch++;
                next = end;
                continue;
            }
            JS_FreeValue(ctx, value);
            {
                JS_FreeValue(ctx, pending);
                pending = known_local(locals, epoch, op, buf + pos) ?
                    JS_DupValue(ctx, locals[get_u16(buf + pos + 1)].value) :
                    read_literal(ctx, s, buf + pos);
                if (JS_IsException(pending))
                    goto fail;
                pending_known = TRUE;
            }
        } else if (op != OP_line_num) {
            if (locals)
                update_local_constant(ctx, locals, epoch, op, buf + pos,
                                      pending_known, pending);
            if (op != OP_set_loc && op != OP_set_loc_check && op != OP_dup) {
                JS_FreeValue(ctx, pending);
                pending = JS_UNDEFINED;
                pending_known = FALSE;
            }
        }
        if (ends_constant_region(op)) {
            epoch++;
            JS_FreeValue(ctx, pending);
            pending = JS_UNDEFINED;
            pending_known = FALSE;
        }
        if (op == OP_line_num)
            line = get_u32(buf + pos + 1);
        if (op == OP_label)
            s->label_slots[get_u32(buf + pos + 1)].pos2 = out.size + 5;
        if (put_instruction(ctx, &out, buf + pos))
            goto fail;
    }
    JS_FreeValue(ctx, pending);
    free_local_constants(ctx, locals, s->var_count);
    free_bytecode_atoms(ctx->rt, buf, s->byte_code.size, FALSE);
    dbuf_free(&s->byte_code);
    s->byte_code = out;
    return 0;
 fail:
    JS_FreeValue(ctx, pending);
    free_local_constants(ctx, locals, s->var_count);
    free_bytecode_atoms(ctx->rt, out.buf, out.size, FALSE);
    dbuf_free(&out);
    if (!JS_HasException(ctx))
        JS_ThrowOutOfMemory(ctx);
    return -1;
}
