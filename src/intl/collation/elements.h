/* Raw CLDR root elements over immutable normalized input.
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
#ifndef QJS_INTL_COLLATION_ELEMENTS_H
#define QJS_INTL_COLLATION_ELEMENTS_H
#include "match.h"
#include "contexts.h"

typedef struct QJSIntlCollationElements {
    QJSIntlCollationMatch match;
    QJSIntlCollationContexts data;
    uint32_t pending_at;
    QJSIntlCollationEntry pending;
    QJSIntlCollationElement implicit[2];
    int use_implicit;
} QJSIntlCollationElements;

/* data must be an unchanged successfully opened contexts view, including a
   present empty context section when there are no prefix rules. Copy the view
   and allocator callbacks; only their pack bytes and allocator opaque owner
   must outlive this iterator. input is borrowed only during initialization.
   Initialize uninitialized/cleared storage; clear before reuse; do not copy an initialized iterator.
   Outputs/storage must be disjoint. Failed initialization is clearable. */
QJSIntlStatus qjs_intl_collation_elements_init(QJSIntlCollationElements *,
                                              const QJSIntlCollationContexts *,
                                              const uint16_t *, size_t,
                                              const QJSIntlAllocator *);
void qjs_intl_collation_elements_clear(QJSIntlCollationElements *);
/* A raw element can be completely ignorable. Success with present=0 means
   exhaustion; repeated exhausted calls remain valid. No comparison settings
   (case, numeric, alternate handling or locale tailoring) are applied here. */
QJSIntlStatus qjs_intl_collation_elements_next(QJSIntlCollationElements *,
                                              QJSIntlCollationElement *, int *present);

/* Numeric mode recognizes complete adjacent Nd runs in NFD input before
   looking up their initial digit. digits borrows the immutable normalized text
   until clear; leading zeros are removed once, retaining one digit for zero.
   A returned number has no ordinary CE. Do not mix modes within an iterator. */
QJSIntlStatus qjs_intl_collation_elements_next_numeric(QJSIntlCollationElements *,
                                                      QJSIntlCollationElement *,
                                                      QJSIntlCollationNumber *, int *present);
#endif
