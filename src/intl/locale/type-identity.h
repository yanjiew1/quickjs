/* Metadata equivalence, independent of locale syntax and service capability.
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
#ifndef QJS_INTL_LOCALE_TYPE_IDENTITY_H
#define QJS_INTL_LOCALE_TYPE_IDENTITY_H
#include "native-data.h"

/* Match an exact registered name or complete alias token within extension/key,
   then follow preferred names to a terminal nonvariable metadata row. Legacy
   source spellings need not be well-formed Unicode types. Do not normalize
   case/underscores or infer service support. Different matches may converge
   on one terminal; conflicting terminals return DATA_ERROR. Unknown/variable
   spellings return UNSUPPORTED. Allocate nothing, borrow unchanged validated
   metadata and reset non-NULL output on failure. Output is a metadata index,
   not a service catalog index; zero is a valid successful index. */
QJSIntlStatus qjs_intl_locale_type_identity(const QJSIntlLocaleMetadata *metadata,
                                           QJSIntlBytes extension, QJSIntlBytes key,
                                           QJSIntlBytes spelling, uint32_t *out);
#endif
