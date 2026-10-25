/* Collation element weights and UCA implicit-weight construction.
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
#ifndef QJS_INTL_COLLATION_WEIGHTS_H
#define QJS_INTL_COLLATION_WEIGHTS_H
#include "../common/types.h"

typedef struct QJSIntlCollationElement {
    uint16_t primary, secondary, tertiary;
    unsigned int variable;
} QJSIntlCollationElement;

/* UCA 18 Table 16 fallback only: callers first normalize and prefer explicit
   mappings (including special noncharacter mappings). Lone UTF-16 surrogates
   use generic implicit weights. Reset both output elements on invalid input. */
QJSIntlStatus qjs_intl_collation_implicit(uint32_t code, QJSIntlCollationElement out[2]);
#endif
