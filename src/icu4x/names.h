/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_ICU4X_NAMES_H
#define QJS_ICU4X_NAMES_H
#include <stddef.h>
#include <stdint.h>
#include "text.h"

typedef struct QJSICU4XNames QJSICU4XNames;
/* Private public-API data foundation: formatting-context language/region/script
   names (kind 0/1/2); currency_names adds plain currency kind 3.
   The C Intl adapter supplies standalone/context and
   other missing CLDR data. Missing names return UNSUPPORTED before callbacks;
   empty/code-identical successful text is present. Style=long/short/narrow,
   dialect Boolean. Inputs are canonical codes; output spans borrow the call. */
int qjs_icu4x_names_open(const uint8_t *, size_t, uint32_t style, uint32_t dialect,
                         QJSICU4XNames **);
void qjs_icu4x_names_close(QJSICU4XNames *);
int qjs_icu4x_names_format(const QJSICU4XNames *, uint32_t kind, const uint8_t *, size_t,
                           QJSICU4XTextSink);
#endif
