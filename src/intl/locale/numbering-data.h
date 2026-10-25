/* Native numbering metadata, independent of formatter capabilities and JS.
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
#ifndef QJS_INTL_LOCALE_NUMBERING_DATA_H
#define QJS_INTL_LOCALE_NUMBERING_DATA_H
#include "../native/data.h"

typedef struct QJSIntlNumberingData {
    QJSIntlData data;
    QJSIntlSection systems, preferences;
} QJSIntlNumberingData;

enum { QJS_INTL_NUMBERING_NUMERIC, QJS_INTL_NUMBERING_ALGORITHMIC };
#define QJS_INTL_NUMBERING_ABSENT UINT32_MAX
#define QJS_INTL_NUMBERING_BLOCKED (UINT32_MAX - 1)

typedef struct QJSIntlNumberingSystem {
    QJSIntlBytes name, value; /* UTF-8 digit repertoire or ASCII rule reference. */
    unsigned int kind;
} QJSIntlNumberingSystem;

typedef struct QJSIntlNumberingPreferences {
    uint32_t standard, native, traditional, finance; /* Definition indexes. */
} QJSIntlNumberingPreferences;

/* Validate both sections against an unchanged, successfully opened shared
   view. Borrow its storage; release all validation scratch before return.
   Missing/incomplete tables are data errors. No algorithmic rule is executed
   or advertised as supported by opening metadata. No input/output aliasing;
   non-NULL outputs are zeroed on failure. Allocator malloc/free are required. */
QJSIntlStatus qjs_intl_numbering_data_open(const QJSIntlData *data,
                                           const QJSIntlAllocator *allocator,
                                           QJSIntlNumberingData *out);
/* Accessors allocate nothing. Results borrow the view's unchanged storage.
   Exact key lookup returns UNSUPPORTED for missing data, with no fallback.
   Only optional traditional/finance fields may be ABSENT or BLOCKED. */
QJSIntlStatus qjs_intl_numbering_system_at(const QJSIntlNumberingData *data,
                                           uint32_t index, QJSIntlNumberingSystem *out);
QJSIntlStatus qjs_intl_numbering_system_find(const QJSIntlNumberingData *data,
                                             QJSIntlBytes name, uint32_t *out);
QJSIntlStatus qjs_intl_numbering_preferences(const QJSIntlNumberingData *data,
                                             QJSIntlBytes locale,
                                             QJSIntlNumberingPreferences *out);
#endif
