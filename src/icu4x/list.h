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
#ifndef QJS_ICU4X_LIST_H
#define QJS_ICU4X_LIST_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"
#include "../intl/common/format.h"

typedef struct QJSICU4XList QJSICU4XList;
typedef int QJSICU4XLocaleEmit(void *, const uint8_t *, size_t);
typedef struct QJSICU4XListSink {
    void *opaque;
    int (*write)(void *, const uint16_t *, size_t);
    int (*part)(void *, size_t, size_t, uint32_t); /* literal=0, element=1 */
} QJSICU4XListSink;

/* Private synchronous boundary. Sinks and handles must be valid; callbacks
   retain no borrowed spans. All statuses use bridge.h, including callbacks.
   Failed open preserves *out; C provider constructors clear their own output. */
int qjs_icu4x_list_locales(void *, QJSICU4XLocaleEmit *);
int qjs_icu4x_list_open(const uint8_t *, size_t, uint32_t, uint32_t, QJSICU4XList **);
void qjs_icu4x_list_close(QJSICU4XList *);
int qjs_icu4x_list_format(const QJSICU4XList *, const QJSIntlUTF16 *, size_t, QJSICU4XListSink);
#endif
