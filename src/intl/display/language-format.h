/* Native locale-name composition, independent of JS values and API fallback.
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
#ifndef QJS_INTL_DISPLAY_LANGUAGE_FORMAT_H
#define QJS_INTL_DISPLAY_LANGUAGE_FORMAT_H
#include "language-match.h"

/* Input follows language-match.h's validated canonical Language ID contract.
   Compose unconsumed script/region/variants using source patterns and nested
   bracket replacements. Missing qualifiers use their canonical input subtags;
   a missing localized base returns OK/absent independently of API fallback.
   Require malloc/free and a nonzero UTF-16 limit. Allocate no intermediates
   proportional to rendered output; release all scratch on success or failure.
   Result owns its text with the supplied allocator and borrows nothing.
   Require disjoint output; reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_language_name_format(const QJSIntlNameData *, uint32_t locale,
                                          QJSIntlNameStyle, int dialect,
                                          QJSIntlBytes canonical,
                                          const QJSIntlAllocator *, size_t limit,
                                          QJSIntlNameText *);
#endif
