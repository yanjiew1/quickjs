/*
 * Regular Expression Executor
 *
 * Copyright (c) 2017-2018 Fabrice Bellard
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "cutils.h"
#include "libregexp.h"
#include "libunicode.h"

#if defined(TEST)
#define DUMP_REOP
#endif
//#define DUMP_EXEC
#include "regexp-internal.h"

/* must be large enough to have a negligible runtime cost and small
   enough to call the interrupt callback often. */
#define INTERRUPT_COUNTER_INIT 10000

/* unicode code points */
#define CP_LS   0x2028
#define CP_PS   0x2029

static BOOL is_line_terminator(uint32_t c)
{
    return (c == '\n' || c == '\r' || c == CP_LS || c == CP_PS);
}

#define GET_CHAR(c, cptr, cbuf_end, cbuf_type)                          \
    do {                                                                \
        if (cbuf_type == 0) {                                           \
            c = *cptr++;                                                \
        } else {                                                        \
            const uint16_t *_p = (const uint16_t *)cptr;                \
            const uint16_t *_end = (const uint16_t *)cbuf_end;          \
            c = *_p++;                                                  \
            if (is_hi_surrogate(c) && cbuf_type == 2) {                 \
                if (_p < _end && is_lo_surrogate(*_p)) {                \
                    c = from_surrogate(c, *_p++);                       \
                }                                                       \
            }                                                           \
            cptr = (const void *)_p;                                    \
        }                                                               \
    } while (0)

#define PEEK_CHAR(c, cptr, cbuf_end, cbuf_type)                         \
    do {                                                                \
        if (cbuf_type == 0) {                                           \
            c = cptr[0];                                                \
        } else {                                                        \
            const uint16_t *_p = (const uint16_t *)cptr;                \
            const uint16_t *_end = (const uint16_t *)cbuf_end;          \
            c = *_p++;                                                  \
            if (is_hi_surrogate(c) && cbuf_type == 2) {                 \
                if (_p < _end && is_lo_surrogate(*_p)) {                \
                    c = from_surrogate(c, *_p);                         \
                }                                                       \
            }                                                           \
        }                                                               \
    } while (0)

#define PEEK_PREV_CHAR(c, cptr, cbuf_start, cbuf_type)                  \
    do {                                                                \
        if (cbuf_type == 0) {                                           \
            c = cptr[-1];                                               \
        } else {                                                        \
            const uint16_t *_p = (const uint16_t *)cptr - 1;            \
            const uint16_t *_start = (const uint16_t *)cbuf_start;      \
            c = *_p;                                                    \
            if (is_lo_surrogate(c) && cbuf_type == 2) {                 \
                if (_p > _start && is_hi_surrogate(_p[-1])) {           \
                    c = from_surrogate(*--_p, c);                       \
                }                                                       \
            }                                                           \
        }                                                               \
    } while (0)

#define GET_PREV_CHAR(c, cptr, cbuf_start, cbuf_type)                   \
    do {                                                                \
        if (cbuf_type == 0) {                                           \
            cptr--;                                                     \
            c = cptr[0];                                                \
        } else {                                                        \
            const uint16_t *_p = (const uint16_t *)cptr - 1;            \
            const uint16_t *_start = (const uint16_t *)cbuf_start;      \
            c = *_p;                                                    \
            if (is_lo_surrogate(c) && cbuf_type == 2) {                 \
                if (_p > _start && is_hi_surrogate(_p[-1])) {           \
                    c = from_surrogate(*--_p, c);                       \
                }                                                       \
            }                                                           \
            cptr = (const void *)_p;                                    \
        }                                                               \
    } while (0)

#define PREV_CHAR(cptr, cbuf_start, cbuf_type)                          \
    do {                                                                \
        if (cbuf_type == 0) {                                           \
            cptr--;                                                     \
        } else {                                                        \
            const uint16_t *_p = (const uint16_t *)cptr - 1;            \
            const uint16_t *_start = (const uint16_t *)cbuf_start;      \
            if (is_lo_surrogate(*_p) && cbuf_type == 2) {               \
                if (_p > _start && is_hi_surrogate(_p[-1])) {           \
                    --_p;                                               \
                }                                                       \
            }                                                           \
            cptr = (const void *)_p;                                    \
        }                                                               \
    } while (0)

typedef enum {
    RE_EXEC_STATE_SPLIT,
    RE_EXEC_STATE_LOOKAHEAD,
    RE_EXEC_STATE_NEGATIVE_LOOKAHEAD,
} REExecStateEnum;

#if INTPTR_MAX >= INT64_MAX
#define BP_TYPE_BITS 3
#else
#define BP_TYPE_BITS 2
#endif

typedef union {
    uint8_t *ptr;
    intptr_t val; /* for bp, the low BP_SHIFT bits store REExecStateEnum */
    struct {
        uintptr_t val : sizeof(uintptr_t) * 8 - BP_TYPE_BITS;
        uintptr_t type : BP_TYPE_BITS;
    } bp;
} StackElem;

typedef struct {
    const uint8_t *cbuf;
    const uint8_t *cbuf_end;
    /* 0 = 8 bit chars, 1 = 16 bit chars, 2 = 16 bit chars, UTF-16 */
    int cbuf_type;
    int capture_count;
    BOOL is_unicode;
    int interrupt_counter;
    void *opaque; /* used for stack overflow check */

    StackElem *stack_buf;
    size_t stack_size;
    StackElem static_stack_buf[32]; /* static stack to avoid allocation in most cases */
} REExecContext;

static int lre_poll_timeout(REExecContext *s)
{
    if (unlikely(--s->interrupt_counter <= 0)) {
        s->interrupt_counter = INTERRUPT_COUNTER_INIT;
        if (lre_check_timeout(s->opaque))
            return LRE_RET_TIMEOUT;
    }
    return 0;
}

static no_inline int stack_realloc(REExecContext *s, size_t n)
{
    StackElem *new_stack;
    size_t new_size;
    new_size = s->stack_size * 3 / 2;
    if (new_size < n)
        new_size = n;
    if (s->stack_buf == s->static_stack_buf) {
        new_stack = lre_realloc(s->opaque, NULL, new_size * sizeof(StackElem));
        if (!new_stack)
            return -1;
        /* XXX: could use correct size */
        memcpy(new_stack, s->stack_buf, s->stack_size * sizeof(StackElem));
    } else {
        new_stack = lre_realloc(s->opaque, s->stack_buf, new_size * sizeof(StackElem));
        if (!new_stack)
            return -1;
    }
    s->stack_size = new_size;
    s->stack_buf = new_stack;
    return 0;
}

/* return 1 if match, 0 if not match or < 0 if error. */
static intptr_t lre_exec_backtrack(REExecContext *s, uint8_t **capture,
                                   const uint8_t *pc, const uint8_t *cptr)
{
    int opcode;
    int cbuf_type;
    uint32_t val, c, idx;
    const uint8_t *cbuf_end;
    StackElem *sp, *bp, *stack_end;
#ifdef DUMP_EXEC
    const uint8_t *pc_start = pc; /* TEST */
#endif
    cbuf_type = s->cbuf_type;
    cbuf_end = s->cbuf_end;

    sp = s->stack_buf;
    bp = s->stack_buf;
    stack_end = s->stack_buf + s->stack_size;
    
#define CHECK_STACK_SPACE(n)                            \
    if (unlikely((stack_end - sp) < (n))) {             \
        size_t saved_sp = sp - s->stack_buf;            \
        size_t saved_bp = bp - s->stack_buf;            \
        if (stack_realloc(s, sp - s->stack_buf + (n)))  \
            return LRE_RET_MEMORY_ERROR;                \
        stack_end = s->stack_buf + s->stack_size;       \
        sp = s->stack_buf + saved_sp;                   \
        bp = s->stack_buf + saved_bp;                   \
    }

    /* XXX: could test if the value was saved to reduce the stack size
       but slower */
#define SAVE_CAPTURE(idx, value)                        \
    {                                                   \
        CHECK_STACK_SPACE(2);                           \
        sp[0].val = idx;                                \
        sp[1].ptr = capture[idx];                       \
        sp += 2;                                        \
        capture[idx] = (value);                         \
    }

    /* avoid saving the previous value if already saved */
#define SAVE_CAPTURE_CHECK(idx, value)          \
    {                                           \
        StackElem *sp1;                         \
        sp1 = sp;                               \
        for(;;) {                               \
            if (sp1 > bp) {                             \
                if (sp1[-2].val == idx)                 \
                    break;                              \
                sp1 -= 2;                               \
            } else {                                    \
                CHECK_STACK_SPACE(2);                   \
                sp[0].val = idx;                        \
                sp[1].ptr = capture[idx];               \
                sp += 2;                                \
                break;                                  \
            }                                           \
        }                                               \
        capture[idx] = (value);                         \
    }


#ifdef DUMP_EXEC
    printf("%5s %5s %5s %5s %s\n", "PC", "CP", "BP", "SP", "OPCODE");
#endif    
    for(;;) {
        opcode = *pc++;
#ifdef DUMP_EXEC
        printf("%5ld %5ld %5ld %5ld %s\n",
               pc - 1 - pc_start,
               cbuf_type == 0 ? cptr - s->cbuf : (cptr - s->cbuf) / 2,
               bp - s->stack_buf,
               sp - s->stack_buf,
               reopcode_info[opcode].name);
#endif        
        switch(opcode) {
        case REOP_match:
            return 1;
        no_match:
            for(;;) {
                REExecStateEnum type;
                if (bp == s->stack_buf)
                    return 0;
                /* undo the modifications to capture[] */
                while (sp > bp) {
                    capture[sp[-2].val] = sp[-1].ptr;
                    sp -= 2;
                }
                
                pc = sp[-3].ptr;
                cptr = sp[-2].ptr;
                type = sp[-1].bp.type;
                bp = s->stack_buf + sp[-1].bp.val;
                sp -= 3;
                if (type != RE_EXEC_STATE_LOOKAHEAD)
                    break;
            }
            if (lre_poll_timeout(s))
                return LRE_RET_TIMEOUT;
            break;
        case REOP_lookahead_match:
            /* pop all the saved states until reaching the start of
               the lookahead and keep the updated captures and
               variables and the corresponding undo info. */
            {
                StackElem *sp1, *sp_top, *next_sp;
                REExecStateEnum type;

                sp_top = sp;
                for(;;) {
                    sp1 = sp;
                    sp = bp;
                    pc = sp[-3].ptr;
                    cptr = sp[-2].ptr;
                    type = sp[-1].bp.type;
                    bp = s->stack_buf + sp[-1].bp.val;
                    sp[-1].ptr = (void *)sp1; /* save the next value for the copy step */
                    sp -= 3;
                    if (type == RE_EXEC_STATE_LOOKAHEAD)
                        break;
                }
                if (sp != s->stack_buf) {
                    /* keep the undo info if there is a saved state */
                    sp1 = sp;
                    while (sp1 < sp_top) {
                        next_sp = (void *)sp1[2].ptr;
                        sp1 += 3;
                        while (sp1 < next_sp)
                            *sp++ = *sp1++;
                    }
                }
            }
            break;
        case REOP_negative_lookahead_match:
            /* pop all the saved states until reaching start of the negative lookahead */
            for(;;) {
                REExecStateEnum type;
                type = bp[-1].bp.type;
                /* undo the modifications to capture[] */
                while (sp > bp) {
                    capture[sp[-2].val] = sp[-1].ptr;
                    sp -= 2;
                }
                pc = sp[-3].ptr;
                cptr = sp[-2].ptr;
                type = sp[-1].bp.type;
                bp = s->stack_buf + sp[-1].bp.val;
                sp -= 3;
                if (type == RE_EXEC_STATE_NEGATIVE_LOOKAHEAD)
                    break;
            }
            goto no_match;
        case REOP_char32:
        case REOP_char32_i:
            val = get_u32(pc);
            pc += 4;
            goto test_char;
        case REOP_char:
        case REOP_char_i:
            val = get_u16(pc);
            pc += 2;
        test_char:
            if (cptr >= cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end, cbuf_type);
            if (opcode == REOP_char_i || opcode == REOP_char32_i) {
                c = lre_canonicalize(c, s->is_unicode);
            }
            if (val != c)
                goto no_match;
            break;
        case REOP_split_goto_first:
        case REOP_split_next_first:
            {
                const uint8_t *pc1;

                val = get_u32(pc);
                pc += 4;
                if (opcode == REOP_split_next_first) {
                    pc1 = pc + (int)val;
                } else {
                    pc1 = pc;
                    pc = pc + (int)val;
                }
                CHECK_STACK_SPACE(3);
                sp[0].ptr = (uint8_t *)pc1;
                sp[1].ptr = (uint8_t *)cptr;
                sp[2].bp.val = bp - s->stack_buf;
                sp[2].bp.type = RE_EXEC_STATE_SPLIT;
                sp += 3;
                bp = sp;
            }
            break;
        case REOP_lookahead:
        case REOP_negative_lookahead:
            val = get_u32(pc);
            pc += 4;
            CHECK_STACK_SPACE(3);
            sp[0].ptr = (uint8_t *)(pc + (int)val);
            sp[1].ptr = (uint8_t *)cptr;
            sp[2].bp.val = bp - s->stack_buf;
            sp[2].bp.type = RE_EXEC_STATE_LOOKAHEAD + opcode - REOP_lookahead;
            sp += 3;
            bp = sp;
            break;
        case REOP_goto:
            val = get_u32(pc);
            pc += 4 + (int)val;
            if (lre_poll_timeout(s))
                return LRE_RET_TIMEOUT;
            break;
        case REOP_line_start:
        case REOP_line_start_m:
            if (cptr == s->cbuf)
                break;
            if (opcode == REOP_line_start)
                goto no_match;
            PEEK_PREV_CHAR(c, cptr, s->cbuf, cbuf_type);
            if (!is_line_terminator(c))
                goto no_match;
            break;
        case REOP_line_end:
        case REOP_line_end_m:
            if (cptr == cbuf_end)
                break;
            if (opcode == REOP_line_end)
                goto no_match;
            PEEK_CHAR(c, cptr, cbuf_end, cbuf_type);
            if (!is_line_terminator(c))
                goto no_match;
            break;
        case REOP_dot:
            if (cptr == cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end, cbuf_type);
            if (is_line_terminator(c))
                goto no_match;
            break;
        case REOP_any:
            if (cptr == cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end, cbuf_type);
            break;
        case REOP_space:
            if (cptr == cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end, cbuf_type);
            if (!lre_is_space(c))
                goto no_match;
            break;
        case REOP_not_space:
            if (cptr == cbuf_end)
                goto no_match;
            GET_CHAR(c, cptr, cbuf_end, cbuf_type);
            if (lre_is_space(c))
                goto no_match;
            break;
        case REOP_save_start:
        case REOP_save_end:
            val = *pc++;
            assert(val < s->capture_count);
            idx = 2 * val + opcode - REOP_save_start;
            SAVE_CAPTURE(idx, (uint8_t *)cptr);
            break;
        case REOP_save_reset:
            {
                uint32_t val2;
                val = pc[0];
                val2 = pc[1];
                pc += 2;
                assert(val2 < s->capture_count);
                CHECK_STACK_SPACE(2 * (val2 - val + 1));
                while (val <= val2) {
                    idx = 2 * val;
                    SAVE_CAPTURE(idx, NULL);
                    idx = 2 * val + 1;
                    SAVE_CAPTURE(idx, NULL);
                    val++;
                }
            }
            break;
        case REOP_set_i32:
            idx = 2 * s->capture_count + pc[0];
            val = get_u32(pc + 1);
            pc += 5;
            SAVE_CAPTURE_CHECK(idx, (void *)(uintptr_t)val);
            break;
        case REOP_loop:
            {
                uint32_t val2;
                idx = 2 * s->capture_count + pc[0];
                val = get_u32(pc + 1);
                pc += 5;

                val2 = (uintptr_t)capture[idx] - 1;
                SAVE_CAPTURE_CHECK(idx, (void *)(uintptr_t)val2);
                if (val2 != 0) {
                    pc += (int)val;
                    if (lre_poll_timeout(s))
                        return LRE_RET_TIMEOUT;
                }
            }
            break;
        case REOP_loop_split_goto_first:
        case REOP_loop_split_next_first:
        case REOP_loop_check_adv_split_goto_first:
        case REOP_loop_check_adv_split_next_first:
            {
                const uint8_t *pc1;
                uint32_t val2, limit;
                idx = 2 * s->capture_count + pc[0];
                limit = get_u32(pc + 1);
                val = get_u32(pc + 5);
                pc += 9;

                /* decrement the counter */
                val2 = (uintptr_t)capture[idx] - 1;
                SAVE_CAPTURE_CHECK(idx, (void *)(uintptr_t)val2);

                if (val2 > limit) {
                    /* normal loop if counter > limit */
                    pc += (int)val;
                    if (lre_poll_timeout(s))
                        return LRE_RET_TIMEOUT;
                } else {
                    /* check advance */
                    if ((opcode == REOP_loop_check_adv_split_goto_first ||
                         opcode == REOP_loop_check_adv_split_next_first) &&
                        capture[idx + 1] == cptr &&
                        val2 != limit) {
                        goto no_match;
                    }
                    
                    /* otherwise conditional split */
                    if (val2 != 0) {
                        if (opcode == REOP_loop_split_next_first ||
                            opcode == REOP_loop_check_adv_split_next_first) {
                            pc1 = pc + (int)val;
                        } else {
                            pc1 = pc;
                            pc = pc + (int)val;
                        }
                        CHECK_STACK_SPACE(3);
                        sp[0].ptr = (uint8_t *)pc1;
                        sp[1].ptr = (uint8_t *)cptr;
                        sp[2].bp.val = bp - s->stack_buf;
                        sp[2].bp.type = RE_EXEC_STATE_SPLIT;
                        sp += 3;
                        bp = sp;
                    }
                }
            }
            break;
        case REOP_set_char_pos:
            idx = 2 * s->capture_count + pc[0];
            pc++;
            SAVE_CAPTURE_CHECK(idx, (uint8_t *)cptr);
            break;
        case REOP_check_advance:
            idx = 2 * s->capture_count + pc[0];
            pc++;
            if (capture[idx] == cptr)
                goto no_match;
            break;
        case REOP_word_boundary:
        case REOP_word_boundary_i:
        case REOP_not_word_boundary:
        case REOP_not_word_boundary_i:
            {
                BOOL v1, v2;
                int ignore_case = (opcode == REOP_word_boundary_i || opcode == REOP_not_word_boundary_i);
                BOOL is_boundary = (opcode == REOP_word_boundary || opcode == REOP_word_boundary_i);
                /* char before */
                if (cptr == s->cbuf) {
                    v1 = FALSE;
                } else {
                    PEEK_PREV_CHAR(c, cptr, s->cbuf, cbuf_type);
                    if (c < 256) {
                        v1 = (lre_is_word_byte(c) != 0);
                    } else {
                        v1 = ignore_case && (c == 0x017f || c == 0x212a);
                    }
                }
                /* current char */
                if (cptr >= cbuf_end) {
                    v2 = FALSE;
                } else {
                    PEEK_CHAR(c, cptr, cbuf_end, cbuf_type);
                    if (c < 256) {
                        v2 = (lre_is_word_byte(c) != 0);
                    } else {
                        v2 = ignore_case && (c == 0x017f || c == 0x212a);
                    }
                }
                if (v1 ^ v2 ^ is_boundary)
                    goto no_match;
            }
            break;
        case REOP_back_reference:
        case REOP_back_reference_i:
        case REOP_backward_back_reference:
        case REOP_backward_back_reference_i:
            {
                const uint8_t *cptr1, *cptr1_end, *cptr1_start;
                const uint8_t *pc1;
                uint32_t c1, c2;
                int i, n;

                n = *pc++;
                pc1 = pc;
                pc += n;

                for(i = 0; i < n; i++) {
                    val = pc1[i];
                    if (val >= s->capture_count)
                        goto no_match;
                    cptr1_start = capture[2 * val];
                    cptr1_end = capture[2 * val + 1];
                    /* test the first not empty capture */
                    if (cptr1_start && cptr1_end) {
                        if (opcode == REOP_back_reference ||
                            opcode == REOP_back_reference_i) {
                            cptr1 = cptr1_start;
                            while (cptr1 < cptr1_end) {
                                if (cptr >= cbuf_end)
                                    goto no_match;
                                GET_CHAR(c1, cptr1, cptr1_end, cbuf_type);
                                GET_CHAR(c2, cptr, cbuf_end, cbuf_type);
                                if (opcode == REOP_back_reference_i) {
                                    c1 = lre_canonicalize(c1, s->is_unicode);
                                    c2 = lre_canonicalize(c2, s->is_unicode);
                                }
                                if (c1 != c2)
                                    goto no_match;
                            }
                        } else {
                            cptr1 = cptr1_end;
                            while (cptr1 > cptr1_start) {
                                if (cptr == s->cbuf)
                                    goto no_match;
                                GET_PREV_CHAR(c1, cptr1, cptr1_start, cbuf_type);
                                GET_PREV_CHAR(c2, cptr, s->cbuf, cbuf_type);
                                if (opcode == REOP_backward_back_reference_i) {
                                    c1 = lre_canonicalize(c1, s->is_unicode);
                                    c2 = lre_canonicalize(c2, s->is_unicode);
                                }
                                if (c1 != c2)
                                    goto no_match;
                            }
                        }
                        break;
                    }
                }
            }
            break;
        case REOP_range:
        case REOP_range_i:
            {
                int n;
                uint32_t low, high, idx_min, idx_max, idx;

                n = get_u16(pc); /* n must be >= 1 */
                pc += 2;
                if (cptr >= cbuf_end)
                    goto no_match;
                GET_CHAR(c, cptr, cbuf_end, cbuf_type);
                if (opcode == REOP_range_i) {
                    c = lre_canonicalize(c, s->is_unicode);
                }
                idx_min = 0;
                low = get_u16(pc + 0 * 4);
                if (c < low)
                    goto no_match;
                idx_max = n - 1;
                high = get_u16(pc + idx_max * 4 + 2);
                /* 0xffff in for last value means +infinity */
                if (unlikely(c >= 0xffff) && high == 0xffff)
                    goto range_match;
                if (c > high)
                    goto no_match;
                while (idx_min <= idx_max) {
                    idx = (idx_min + idx_max) / 2;
                    low = get_u16(pc + idx * 4);
                    high = get_u16(pc + idx * 4 + 2);
                    if (c < low)
                        idx_max = idx - 1;
                    else if (c > high)
                        idx_min = idx + 1;
                    else
                        goto range_match;
                }
                goto no_match;
            range_match:
                pc += 4 * n;
            }
            break;
        case REOP_range32:
        case REOP_range32_i:
            {
                int n;
                uint32_t low, high, idx_min, idx_max, idx;

                n = get_u16(pc); /* n must be >= 1 */
                pc += 2;
                if (cptr >= cbuf_end)
                    goto no_match;
                GET_CHAR(c, cptr, cbuf_end, cbuf_type);
                if (opcode == REOP_range32_i) {
                    c = lre_canonicalize(c, s->is_unicode);
                }
                idx_min = 0;
                low = get_u32(pc + 0 * 8);
                if (c < low)
                    goto no_match;
                idx_max = n - 1;
                high = get_u32(pc + idx_max * 8 + 4);
                if (c > high)
                    goto no_match;
                while (idx_min <= idx_max) {
                    idx = (idx_min + idx_max) / 2;
                    low = get_u32(pc + idx * 8);
                    high = get_u32(pc + idx * 8 + 4);
                    if (c < low)
                        idx_max = idx - 1;
                    else if (c > high)
                        idx_min = idx + 1;
                    else
                        goto range32_match;
                }
                goto no_match;
            range32_match:
                pc += 8 * n;
            }
            break;
        case REOP_prev:
            /* go to the previous char */
            if (cptr == s->cbuf)
                goto no_match;
            PREV_CHAR(cptr, s->cbuf, cbuf_type);
            break;
        default:
#ifdef DUMP_EXEC
            printf("unknown opcode pc=%ld\n", pc - 1 - pc_start);
#endif            
            abort();
        }
    }
}

/* Return 1 if match, 0 if not match or < 0 if error (see LRE_RET_x). cindex is the
   starting position of the match and must be such as 0 <= cindex <=
   clen. */
int lre_exec(uint8_t **capture,
             const uint8_t *bc_buf, const uint8_t *cbuf, int cindex, int clen,
             int cbuf_type, void *opaque)
{
    REExecContext s_s, *s = &s_s;
    int re_flags, i, ret;
    const uint8_t *cptr;

    re_flags = lre_get_flags(bc_buf);
    s->is_unicode = (re_flags & (LRE_FLAG_UNICODE | LRE_FLAG_UNICODE_SETS)) != 0;
    s->capture_count = bc_buf[RE_HEADER_CAPTURE_COUNT];
    s->cbuf = cbuf;
    s->cbuf_end = cbuf + (clen << cbuf_type);
    s->cbuf_type = cbuf_type;
    if (s->cbuf_type == 1 && s->is_unicode)
        s->cbuf_type = 2;
    s->interrupt_counter = INTERRUPT_COUNTER_INIT;
    s->opaque = opaque;

    s->stack_buf = s->static_stack_buf;
    s->stack_size = countof(s->static_stack_buf);

    for(i = 0; i < s->capture_count * 2; i++)
        capture[i] = NULL;

    cptr = cbuf + (cindex << cbuf_type);
    if (0 < cindex && cindex < clen && s->cbuf_type == 2) {
        const uint16_t *p = (const uint16_t *)cptr;
        if (is_lo_surrogate(*p) && is_hi_surrogate(p[-1])) {
            cptr = (const uint8_t *)(p - 1);
        }
    }

    ret = lre_exec_backtrack(s, capture, bc_buf + RE_HEADER_LEN, cptr);

    if (s->stack_buf != s->static_stack_buf)
        lre_realloc(s->opaque, s->stack_buf, 0);
    return ret;
}

int lre_get_alloc_count(const uint8_t *bc_buf)
{
    return bc_buf[RE_HEADER_CAPTURE_COUNT] * 2 +
        bc_buf[RE_HEADER_REGISTER_COUNT];
}

int lre_get_capture_count(const uint8_t *bc_buf)
{
    return bc_buf[RE_HEADER_CAPTURE_COUNT];
}

int lre_get_flags(const uint8_t *bc_buf)
{
    return get_u16(bc_buf + RE_HEADER_FLAGS);
}

/* Return NULL if no group names. Otherwise, return a pointer to
   'capture_count - 1' zero terminated UTF-8 strings. */
const char *lre_get_groupnames(const uint8_t *bc_buf)
{
    uint32_t re_bytecode_len;
    if ((lre_get_flags(bc_buf) & LRE_FLAG_NAMED_GROUPS) == 0)
        return NULL;
    re_bytecode_len = get_u32(bc_buf + RE_HEADER_BYTECODE_LEN);
    return (const char *)(bc_buf + RE_HEADER_LEN + re_bytecode_len);
}
