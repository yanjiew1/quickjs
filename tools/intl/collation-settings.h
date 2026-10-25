/* Host interpretation of scalar rule settings, separate from weight mutation.
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
#ifndef QJS_INTL_COMPILER_SETTINGS_H
#define QJS_INTL_COMPILER_SETTINGS_H
#include "collation-ir.h"
#include "../../src/intl/collation/policy.h"

/* Root defaults. Reordering has a separate group owner. Recording normalization
   off permits an optimized FCD path; it does not justify breaking canonical
   equivalence. maxVariable applies after construction, not to logical resets. */
void qjs_intl_collation_settings_init(QJSIntlCollationPolicy *);
/* Apply a validated setting instruction to an initialized policy. No allocation; policy is unchanged on
   failure. Known scalar values are checked exactly. Reorder and unsupported
   legacy settings return UNSUPPORTED for their owning executor to handle or
   reject. Input slices and output policy are disjoint. */
QJSIntlStatus qjs_intl_collation_setting_apply(QJSIntlCollationPolicy *, const QJSIntlIRInstruction *);
#endif
