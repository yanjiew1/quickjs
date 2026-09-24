/*
 * Regular Expression Internal Interface
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
#ifndef REGEXP_INTERNAL_H
#define REGEXP_INTERNAL_H

#include <stdint.h>
#include "libregexp.h"

typedef enum {
#define DEF(id, size) REOP_ ## id,
#include "libregexp-opcode.h"
#undef DEF
    REOP_COUNT,
} REOPCodeEnum;

typedef struct {
#ifdef DUMP_REOP
    const char *name;
#endif
    uint8_t size;
} REOpCode;

#define RE_HEADER_FLAGS          0
#define RE_HEADER_CAPTURE_COUNT  2
#define RE_HEADER_REGISTER_COUNT 3
#define RE_HEADER_BYTECODE_LEN   4

#define RE_HEADER_LEN 8

extern const REOpCode reopcode_info[REOP_COUNT];

#endif
