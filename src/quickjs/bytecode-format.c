/*
 * QuickJS bytecode format helpers
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
#include "internal/base.h"
#include "internal/bytecode-format.h"
#include "internal/bytecode.h"
#include "internal/atom.h"

const JSOpCode opcode_info[OP_COUNT + (OP_TEMP_END - OP_TEMP_START)] = {
#define FMT(f)
#ifdef DUMP_BYTECODE
#define DEF(id, size, n_pop, n_push, f) { #id, size, n_pop, n_push, OP_FMT_ ## f },
#else
#define DEF(id, size, n_pop, n_push, f) { size, n_pop, n_push, OP_FMT_ ## f },
#endif
#include "quickjs-opcode.h"
#undef DEF
#undef FMT
};

void bc_byte_swap(uint8_t *bc_buf, int bc_len)
{
    int pos, len, op, fmt;

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        len = short_opcode_info(op).size;
        fmt = short_opcode_info(op).fmt;
        switch(fmt) {
        case OP_FMT_u16:
        case OP_FMT_i16:
        case OP_FMT_label16:
        case OP_FMT_npop:
        case OP_FMT_loc:
        case OP_FMT_arg:
        case OP_FMT_var_ref:
            put_u16(bc_buf + pos + 1,
                    bswap16(get_u16(bc_buf + pos + 1)));
            break;
        case OP_FMT_i32:
        case OP_FMT_u32:
        case OP_FMT_const:
        case OP_FMT_label:
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
            put_u32(bc_buf + pos + 1,
                    bswap32(get_u32(bc_buf + pos + 1)));
            break;
        case OP_FMT_atom_u16:
        case OP_FMT_label_u16:
            put_u32(bc_buf + pos + 1,
                    bswap32(get_u32(bc_buf + pos + 1)));
            put_u16(bc_buf + pos + 1 + 4,
                    bswap16(get_u16(bc_buf + pos + 1 + 4)));
            break;
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            put_u32(bc_buf + pos + 1,
                    bswap32(get_u32(bc_buf + pos + 1)));
            put_u32(bc_buf + pos + 1 + 4,
                    bswap32(get_u32(bc_buf + pos + 1 + 4)));
            if (fmt == OP_FMT_atom_label_u16) {
                put_u16(bc_buf + pos + 1 + 4 + 4,
                        bswap16(get_u16(bc_buf + pos + 1 + 4 + 4)));
            }
            break;
        case OP_FMT_npop_u16:
            put_u16(bc_buf + pos + 1,
                    bswap16(get_u16(bc_buf + pos + 1)));
            put_u16(bc_buf + pos + 1 + 2,
                    bswap16(get_u16(bc_buf + pos + 1 + 2)));
            break;
        default:
            break;
        }
        pos += len;
    }
}

void free_bytecode_atoms(JSRuntime *rt,
                         const uint8_t *bc_buf, int bc_len,
                         BOOL use_short_opcodes)
{
    int pos, len, op;
    JSAtom atom;
    const JSOpCode *oi;

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        if (use_short_opcodes)
            oi = &short_opcode_info(op);
        else
            oi = &opcode_info[op];

        len = oi->size;
        switch(oi->fmt) {
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
        case OP_FMT_atom_u16:
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            if ((pos + 1 + 4) > bc_len)
                break; /* may happen if there is not enough memory when emiting bytecode */
            atom = get_u32(bc_buf + pos + 1);
            JS_FreeAtomRT(rt, atom);
            break;
        default:
            break;
        }
        pos += len;
    }
}

void dbuf_put_leb128(DynBuf *s, uint32_t v)
{
    uint32_t a;
    for(;;) {
        a = v & 0x7f;
        v >>= 7;
        if (v != 0) {
            dbuf_putc(s, a | 0x80);
        } else {
            dbuf_putc(s, a);
            break;
        }
    }
}

void dbuf_put_sleb128(DynBuf *s, int32_t v1)
{
    uint32_t v = v1;
    dbuf_put_leb128(s, (2 * v) ^ -(v >> 31));
}

int get_leb128(uint32_t *pval, const uint8_t *buf,
               const uint8_t *buf_end)
{
    const uint8_t *ptr = buf;
    uint32_t v, a, i;
    v = 0;
    for(i = 0; i < 5; i++) {
        if (unlikely(ptr >= buf_end))
            break;
        a = *ptr++;
        v |= (a & 0x7f) << (i * 7);
        if (!(a & 0x80)) {
            *pval = v;
            return ptr - buf;
        }
    }
    *pval = 0;
    return -1;
}

int get_sleb128(int32_t *pval, const uint8_t *buf,
                const uint8_t *buf_end)
{
    int ret;
    uint32_t val;
    ret = get_leb128(&val, buf, buf_end);
    if (ret < 0) {
        *pval = 0;
        return -1;
    }
    *pval = (val >> 1) ^ -(val & 1);
    return ret;
}

/* use pc_value = -1 to get the position of the function definition */
int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                  uint32_t pc_value, int *pcol_num)
{
    const uint8_t *p_end, *p;
    int new_line_num, line_num, pc, v, ret, new_col_num, col_num;
    uint32_t val;
    unsigned int op;

    if (!b->has_debug || !b->debug.pc2line_buf)
        goto fail; /* function was stripped */

    p = b->debug.pc2line_buf;
    p_end = p + b->debug.pc2line_len;

    /* get the function line and column numbers */
    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    line_num = val + 1;

    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    col_num = val + 1;

    if (pc_value != -1) {
        pc = 0;
        while (p < p_end) {
            op = *p++;
            if (op == 0) {
                ret = get_leb128(&val, p, p_end);
                if (ret < 0)
                    goto fail;
                pc += val;
                p += ret;
                ret = get_sleb128(&v, p, p_end);
                if (ret < 0)
                    goto fail;
                p += ret;
                new_line_num = line_num + v;
            } else {
                op -= PC2LINE_OP_FIRST;
                pc += (op / PC2LINE_RANGE);
                new_line_num = line_num + (op % PC2LINE_RANGE) + PC2LINE_BASE;
            }
            ret = get_sleb128(&v, p, p_end);
            if (ret < 0)
                goto fail;
            p += ret;
            new_col_num = col_num + v;

            if (pc_value < pc)
                goto done;
            line_num = new_line_num;
            col_num = new_col_num;
        }
    }
 done:
    *pcol_num = col_num;
    return line_num;
 fail:
    *pcol_num = 0;
    return 0;
}
