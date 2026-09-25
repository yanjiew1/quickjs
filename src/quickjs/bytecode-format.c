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
