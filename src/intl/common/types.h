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
#ifndef QJS_INTL_COMMON_TYPES_H
#define QJS_INTL_COMMON_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Service constructors may borrow an unchanged view from successful native
   qjs_intl_data_open admission. NULL requests standalone compiled-data admission.
   Constructors copy the view; only its immutable bytes must outlive consumers.
   Typed sections and module capabilities are still checked by their owners.
   Minimal Segmenter does not use a data pack and ignores this optional view. */
struct QJSIntlData;

/* Plain C contracts shared by Intl services; no engine or backend state. */
typedef enum QJSIntlStatus {
    QJS_INTL_OK = 0,
    QJS_INTL_NO_MEMORY,
    QJS_INTL_INVALID_ARGUMENT,
    QJS_INTL_UNSUPPORTED,
    QJS_INTL_DATA_ERROR,
    QJS_INTL_OVERFLOW
} QJSIntlStatus;

/* Allocation callbacks have malloc/realloc/free semantics. Never request
   size zero. Clear functions use the same allocator that created a result. */
typedef struct QJSIntlAllocator {
    void *opaque;
    void *(*malloc)(void *opaque, size_t size);
    void *(*realloc)(void *opaque, void *ptr, size_t size);
    void (*free)(void *opaque, void *ptr);
} QJSIntlAllocator;

/* Borrowed byte slice. A particular operation specifies its text grammar. */
typedef struct QJSIntlBytes {
    const char *data;
    size_t length;
} QJSIntlBytes;

typedef enum QJSIntlMathematicalKind {
    QJS_INTL_FINITE = 0,
    QJS_INTL_NAN,
    QJS_INTL_POSITIVE_INFINITY,
    QJS_INTL_NEGATIVE_INFINITY,
    QJS_INTL_NEGATIVE_ZERO
} QJSIntlMathematicalKind;

/* Coercion belongs to the JS binding. Finite magnitudes remain exact decimal
   text, including BigInt values, rather than rounding through double. */
typedef struct QJSIntlMathematicalValue {
    QJSIntlMathematicalKind kind;
    QJSIntlBytes decimal;
} QJSIntlMathematicalValue;

#endif
