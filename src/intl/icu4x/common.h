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
#ifndef QJS_INTL_ICU4X_COMMON_H
#define QJS_INTL_ICU4X_COMMON_H
#include "../common/types.h"
#include "../../icu4x/bridge.h"

typedef int QJSICU4XEmit(void *, const uint8_t *, size_t);
typedef int QJSICU4XEnumerate(void *, QJSICU4XEmit *);
typedef int QJSICU4XEnumerateContext(void *context, void *output, QJSICU4XEmit *);
typedef struct QJSICU4XInventory {
    QJSIntlAllocator allocator;
    QJSIntlBytes *items;
    char *storage;
    size_t count;
} QJSICU4XInventory;
QJSIntlStatus qjs_icu4x_status(int);
/* Enumerator supplies stable sorted unique nonempty identifiers. Its owner
   verifies domain/availability; these two bounded passes only capture one
   text arena and one item array. Callback spans borrow the enumeration call. */
QJSIntlStatus qjs_icu4x_inventory_open(const QJSIntlAllocator *, QJSICU4XEnumerate *,
                                       QJSICU4XInventory *);
QJSIntlStatus qjs_icu4x_inventory_open_context(const QJSIntlAllocator *, QJSICU4XEnumerateContext *,
                                               void *context, QJSICU4XInventory *);
void qjs_icu4x_inventory_clear(QJSICU4XInventory *);
#endif
