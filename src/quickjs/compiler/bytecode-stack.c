/*
 * QuickJS bytecode stack analysis
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
#include "../internal/base.h"
#include "../internal/allocator.h"
#include "../internal/error.h"
#include "../internal/bytecode.h"
#include "bytecode-stack.h"

#define JS_STACK_SIZE_MAX 65534

/* compute the maximum stack size needed by the function */

typedef struct StackSizeState {
    int bc_len;
    int stack_len_max;
    uint16_t *stack_level_tab;
    int32_t *catch_pos_tab;
    int *pc_stack;
    int pc_stack_len;
    int pc_stack_size;
} StackSizeState;

/* 'op' is only used for error indication */
static __exception int ss_check(JSContext *ctx, StackSizeState *s,
                                int pos, int op, int stack_len, int catch_pos)
{
    if ((unsigned)pos >= s->bc_len) {
        JS_ThrowInternalError(ctx, "bytecode buffer overflow (op=%d, pc=%d)", op, pos);
        return -1;
    }
    if (stack_len > s->stack_len_max) {
        s->stack_len_max = stack_len;
        if (s->stack_len_max > JS_STACK_SIZE_MAX) {
            JS_ThrowInternalError(ctx, "stack overflow (op=%d, pc=%d)", op, pos);
            return -1;
        }
    }
    if (s->stack_level_tab[pos] != 0xffff) {
        /* already explored: check that the stack size is consistent */
        if (s->stack_level_tab[pos] != stack_len) {
            JS_ThrowInternalError(ctx, "inconsistent stack size: %d %d (pc=%d)",
                                  s->stack_level_tab[pos], stack_len, pos);
            return -1;
        } else if (s->catch_pos_tab[pos] != catch_pos) {
            JS_ThrowInternalError(ctx, "inconsistent catch position: %d %d (pc=%d)",
                                  s->catch_pos_tab[pos], catch_pos, pos);
            return -1;
        } else {
            return 0;
        }
    }

    /* mark as explored and store the stack size */
    s->stack_level_tab[pos] = stack_len;
    s->catch_pos_tab[pos] = catch_pos;

    /* queue the new PC to explore */
    if (js_resize_array(ctx, (void **)&s->pc_stack, sizeof(s->pc_stack[0]),
                        &s->pc_stack_size, s->pc_stack_len + 1))
        return -1;
    s->pc_stack[s->pc_stack_len++] = pos;
    return 0;
}

__exception int compute_stack_size(JSContext *ctx,
                                   JSFunctionDef *fd,
                                   int *pstack_size)
{
    StackSizeState s_s, *s = &s_s;
    int i, diff, n_pop, pos_next, stack_len, pos, op, catch_pos, catch_level;
    const JSOpCode *oi;
    const uint8_t *bc_buf;

    bc_buf = fd->byte_code.buf;
    s->bc_len = fd->byte_code.size;
    /* bc_len > 0 */
    s->stack_level_tab = js_malloc(ctx, sizeof(s->stack_level_tab[0]) *
                                   s->bc_len);
    if (!s->stack_level_tab)
        return -1;
    for(i = 0; i < s->bc_len; i++)
        s->stack_level_tab[i] = 0xffff;
    s->pc_stack = NULL;
    s->catch_pos_tab = js_malloc(ctx, sizeof(s->catch_pos_tab[0]) *
                                   s->bc_len);
    if (!s->catch_pos_tab)
        goto fail;

    s->stack_len_max = 0;
    s->pc_stack_len = 0;
    s->pc_stack_size = 0;

    /* breadth-first graph exploration */
    if (ss_check(ctx, s, 0, OP_invalid, 0, -1))
        goto fail;

    while (s->pc_stack_len > 0) {
        pos = s->pc_stack[--s->pc_stack_len];
        stack_len = s->stack_level_tab[pos];
        catch_pos = s->catch_pos_tab[pos];
        op = bc_buf[pos];
        if (op == 0 || op >= OP_COUNT) {
            JS_ThrowInternalError(ctx, "invalid opcode (op=%d, pc=%d)", op, pos);
            goto fail;
        }
        oi = &short_opcode_info(op);
#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 64)
        printf("%5d: %10s %5d %5d\n", pos, oi->name, stack_len, catch_pos);
#endif
        pos_next = pos + oi->size;
        if (pos_next > s->bc_len) {
            JS_ThrowInternalError(ctx, "bytecode buffer overflow (op=%d, pc=%d)", op, pos);
            goto fail;
        }
        n_pop = oi->n_pop;
        /* call pops a variable number of arguments */
        if (oi->fmt == OP_FMT_npop || oi->fmt == OP_FMT_npop_u16) {
            n_pop += get_u16(bc_buf + pos + 1);
        } else {
#if SHORT_OPCODES
            if (oi->fmt == OP_FMT_npopx) {
                n_pop += op - OP_call0;
            }
#endif
        }

        if (stack_len < n_pop) {
            JS_ThrowInternalError(ctx, "stack underflow (op=%d, pc=%d)", op, pos);
            goto fail;
        }
        stack_len += oi->n_push - n_pop;
        if (stack_len > s->stack_len_max) {
            s->stack_len_max = stack_len;
            if (s->stack_len_max > JS_STACK_SIZE_MAX) {
                JS_ThrowInternalError(ctx, "stack overflow (op=%d, pc=%d)", op, pos);
                goto fail;
            }
        }
        switch(op) {
        case OP_tail_call:
        case OP_tail_call_method:
        case OP_return:
        case OP_return_undef:
        case OP_return_async:
        case OP_throw:
        case OP_throw_error:
        case OP_ret:
            goto done_insn;
        case OP_goto:
            diff = get_u32(bc_buf + pos + 1);
            pos_next = pos + 1 + diff;
            break;
#if SHORT_OPCODES
        case OP_goto16:
            diff = (int16_t)get_u16(bc_buf + pos + 1);
            pos_next = pos + 1 + diff;
            break;
        case OP_goto8:
            diff = (int8_t)bc_buf[pos + 1];
            pos_next = pos + 1 + diff;
            break;
        case OP_if_true8:
        case OP_if_false8:
            diff = (int8_t)bc_buf[pos + 1];
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len, catch_pos))
                goto fail;
            break;
#endif
        case OP_if_true:
        case OP_if_false:
            diff = get_u32(bc_buf + pos + 1);
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len, catch_pos))
                goto fail;
            break;
        case OP_gosub:
            diff = get_u32(bc_buf + pos + 1);
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len + 1, catch_pos))
                goto fail;
            break;
        case OP_with_get_var:
        case OP_with_delete_var:
            diff = get_u32(bc_buf + pos + 5);
            if (ss_check(ctx, s, pos + 5 + diff, op, stack_len + 1, catch_pos))
                goto fail;
            break;
        case OP_with_make_ref:
        case OP_with_get_ref:
            diff = get_u32(bc_buf + pos + 5);
            if (ss_check(ctx, s, pos + 5 + diff, op, stack_len + 2, catch_pos))
                goto fail;
            break;
        case OP_with_put_var:
            diff = get_u32(bc_buf + pos + 5);
            if (ss_check(ctx, s, pos + 5 + diff, op, stack_len - 1, catch_pos))
                goto fail;
            break;
        case OP_catch:
            diff = get_u32(bc_buf + pos + 1);
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len, catch_pos))
                goto fail;
            catch_pos = pos;
            break;
        case OP_for_of_start:
        case OP_for_await_of_start:
            catch_pos = pos;
            break;
            /* we assume the catch offset entry is only removed with
               some op codes */
        case OP_drop:
            catch_level = stack_len;
            goto check_catch;
        case OP_nip:
            catch_level = stack_len - 1;
            goto check_catch;
        case OP_nip1:
            catch_level = stack_len - 1;
            goto check_catch;
        case OP_iterator_close:
            catch_level = stack_len + 2;
        check_catch:
            /* Note: for for_of_start/for_await_of_start we consider
               the catch offset is on the first stack entry instead of
               the thirst */
            if (catch_pos >= 0) {
                int level;
                level = s->stack_level_tab[catch_pos];
                if (bc_buf[catch_pos] != OP_catch)
                    level++; /* for_of_start, for_wait_of_start */
                /* catch_level = stack_level before op_catch is executed ? */
                if (catch_level == level) {
                    catch_pos = s->catch_pos_tab[catch_pos];
                }
            }
            break;
        case OP_nip_catch:
            if (catch_pos < 0) {
                JS_ThrowInternalError(ctx, "nip_catch: no catch op (pc=%d)", pos);
                goto fail;
            }
            stack_len = s->stack_level_tab[catch_pos];
            if (bc_buf[catch_pos] != OP_catch)
                stack_len++; /* for_of_start, for_wait_of_start */
            stack_len++; /* no stack overflow is possible by construction */
            catch_pos = s->catch_pos_tab[catch_pos];
            break;
        default:
            break;
        }
        if (ss_check(ctx, s, pos_next, op, stack_len, catch_pos))
            goto fail;
    done_insn: ;
    }
    js_free(ctx, s->pc_stack);
    js_free(ctx, s->catch_pos_tab);
    js_free(ctx, s->stack_level_tab);
    *pstack_size = s->stack_len_max;
    return 0;
 fail:
    js_free(ctx, s->pc_stack);
    js_free(ctx, s->catch_pos_tab);
    js_free(ctx, s->stack_level_tab);
    *pstack_size = 0;
    return -1;
}
