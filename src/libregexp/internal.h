/*
 * Regular Expression Engine private bytecode contract
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
#ifndef LIBREGEXP_INTERNAL_H
#define LIBREGEXP_INTERNAL_H

#include "../../cutils.h"

typedef enum {
#define DEF(id, size) REOP_ ## id,
#include "../../libregexp-opcode.h"
#undef DEF
    REOP_COUNT,
} REOPCodeEnum;

#define RE_HEADER_FLAGS          0
#define RE_HEADER_CAPTURE_COUNT  2
#define RE_HEADER_REGISTER_COUNT 3
#define RE_HEADER_BYTECODE_LEN   4
#define RE_HEADER_LEN            8

static inline int lre_bytecode_get_flags(const uint8_t *bc_buf)
{
    return get_u16(bc_buf + RE_HEADER_FLAGS);
}

#endif /* LIBREGEXP_INTERNAL_H */
