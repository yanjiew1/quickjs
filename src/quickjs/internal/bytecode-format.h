/*
 * QuickJS bytecode encoding and line number helpers
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
#ifndef QUICKJS_BYTECODE_FORMAT_H
#define QUICKJS_BYTECODE_FORMAT_H

#include "base.h"
#include "function.h"

void dbuf_put_leb128(DynBuf *s, uint32_t v);
void dbuf_put_sleb128(DynBuf *s, int32_t v1);
int get_leb128(uint32_t *pval, const uint8_t *buf,
               const uint8_t *buf_end);
int get_sleb128(int32_t *pval, const uint8_t *buf,
                const uint8_t *buf_end);

int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                  uint32_t pc_value, int *pcol_num);

#endif
