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
#ifndef QJS_ICU4X_LOCALE_H
#define QJS_ICU4X_LOCALE_H
#include "bridge.h"
#include "../intl/icu4x/locale-data.h"

typedef struct QJSICU4XLocale QJSICU4XLocale;
typedef struct QJSICU4XBytes {
    const uint8_t *data;
    size_t length;
} QJSICU4XBytes;
typedef struct QJSICU4XByteSink {
    void *opaque;
    int (*write)(void *, const uint8_t *, size_t);
} QJSICU4XByteSink;

/* Borrow immutable generated data until close. Failure preserves *out.
   Identity input has passed the core's complete Unicode-locale grammar check;
   operation is canonicalize=0, maximize=1 or minimize=2. Callback spans borrow
   storage only through the call; callbacks use bridge.h status codes. */
int qjs_icu4x_locale_open(const QJSICU4XLocaleAliases *, QJSICU4XLocale **);
void qjs_icu4x_locale_close(QJSICU4XLocale *);
int qjs_icu4x_locale_identity(const QJSICU4XLocale *, const uint8_t *, size_t, uint32_t operation,
                              QJSICU4XByteSink);
/* Canonical registered text borrows the immutable pool, not the input. */
int qjs_icu4x_locale_source_type(const QJSICU4XLocale *, const uint8_t *, size_t, const uint8_t *,
                                 size_t, QJSICU4XBytes *);
int qjs_icu4x_locale_script_direction(const uint8_t *, size_t, uint32_t *);
#endif
