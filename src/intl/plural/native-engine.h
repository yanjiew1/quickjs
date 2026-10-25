/* Native plural selection over validated immutable rule views.
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
#ifndef QJS_INTL_PLURAL_NATIVE_ENGINE_H
#define QJS_INTL_PLURAL_NATIVE_ENGINE_H
#include "data.h"

/* Rules must come from successful rules_at on unchanged open data. The caller
   owns numeric coercion, rounding and notation expansion. Calls allocate
   nothing and retain no input. Non-NULL output is OTHER on any error. */
QJSIntlStatus qjs_intl_native_plural_select(const QJSIntlPluralRuleSet *rules,
                                            const QJSIntlPluralOperands *number,
                                            QJSIntlPluralCategory *out);
/* Same selection with a virtual right shift of the visible mantissa. c/e
   remain number->exponent; see rule_evaluate_shifted's operand contract. */
QJSIntlStatus qjs_intl_native_plural_select_shifted(const QJSIntlPluralRuleSet *rules,
                                                    const QJSIntlPluralOperands *number,
                                                    uint32_t decimal_shift,
                                                    QJSIntlPluralCategory *out);
/* Select an explicit range result, or the end category when the pair is
   absent, as specified by pinned CLDR 49. The service handles equal formatted
   strings before calling this operation. */
QJSIntlStatus qjs_intl_native_plural_range(const QJSIntlPluralRuleSet *rules,
                                           QJSIntlPluralCategory start, QJSIntlPluralCategory end,
                                           QJSIntlPluralCategory *out);
#endif
