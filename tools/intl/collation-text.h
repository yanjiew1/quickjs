/* Pinned Unicode text operations shared by host collation compilation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_COMPILER_TEXT_H
#define QJS_INTL_COMPILER_TEXT_H
#include "../../src/intl/common/types.h"

typedef struct QJSIntlCompilerText {
    uint32_t *codes;
    uint32_t count;
} QJSIntlCompilerText;

/* Output is fresh/cleared and disjoint from input; it owns its scalar array.
   The allocator remains alive through clear. No NUL-termination is implied. */
QJSIntlStatus qjs_intl_compiler_text_nfd(const QJSIntlAllocator *, const uint32_t *, uint32_t,
                                         QJSIntlCompilerText *);
void qjs_intl_compiler_text_clear(const QJSIntlAllocator *, QJSIntlCompilerText *);
/* Starred rules require both unchanged NFD and zero combining class. Check the
   original scalar: normalizing first would hide a forbidden decomposition. */
QJSIntlStatus qjs_intl_compiler_text_inert(const QJSIntlAllocator *, uint32_t, int *);
#endif
