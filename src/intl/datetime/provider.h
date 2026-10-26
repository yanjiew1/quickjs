/* Native DateTimeFormat ownership and selection over immutable source views.
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
#ifndef QJS_INTL_DATETIME_PROVIDER_H
#define QJS_INTL_DATETIME_PROVIDER_H
#include "../native/data.h"

/* Compile-time data binding for the shared date-program service. The returned
   pack borrows immutable storage; validators and formatter policy remain in
   the shared owner. A supplied native checked view follows the same contract. */
QJSIntlStatus qjs_intl_datetime_provider_pack_open(const QJSIntlAllocator *,
                                                 const QJSIntlData *checked_data, QJSIntlData *);
/* Native dependencies share the pack. Foreign providers use their own data
   and therefore receive NULL, never a companion presented as native payload. */
const QJSIntlData *qjs_intl_datetime_provider_dependency_data(const QJSIntlData *pack);
#endif
