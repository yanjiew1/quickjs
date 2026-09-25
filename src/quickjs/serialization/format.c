/*
 * QuickJS binary object format
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
#include "../internal/bytecode.h"
#include "format.h"

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
