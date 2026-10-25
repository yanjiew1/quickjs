/* ListFormat templates and contexts over immutable shared locale data.
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
#ifndef QJS_INTL_LIST_DATA_H
#define QJS_INTL_LIST_DATA_H
#include "../native/data.h"

typedef struct QJSIntlListData {
    QJSIntlData data;
    QJSIntlSection lists, list_contexts;
} QJSIntlListData;

/* Require an unchanged validated shared view. Validate list templates and any
   contextual table, including exact CLDR-default admission checks. Contexts
   require base lists. Missing both returns UNSUPPORTED; base lists alone are
   permitted, without claiming a complete formatter. Temporary graph/string
   scratch is released on every path. Borrow immutable storage, not the input
   view's lifetime. Follow pack.h's disjoint-output/error-reset contract. */
QJSIntlStatus qjs_intl_list_data_open(const QJSIntlData *, const QJSIntlAllocator *, QJSIntlListData *);

/* Allocate nothing; borrow the unchanged owner. Variant follows the nine
   collector type/style combinations; part follows (2, start, middle, end).
   Outputs are scalar UTF-8. Reset disjoint non-NULL output on errors. */
QJSIntlStatus qjs_intl_list_data_template(const QJSIntlListData *, uint32_t locale,
                                         unsigned int variant, unsigned int part, QJSIntlBytes *);

/* Context kinds follow list/native-engine.h: none, Spanish conjunction,
   Spanish disjunction, Hebrew conjunction. A disabled context has empty
   pair/end outputs. Missing context data returns UNSUPPORTED, never a claim
   that base templates alone implement all contextual behavior. */
QJSIntlStatus qjs_intl_list_data_context(const QJSIntlListData *, uint32_t locale,
                                        unsigned int variant, unsigned int *kind,
                                        QJSIntlBytes *pair, QJSIntlBytes *end);
#endif
