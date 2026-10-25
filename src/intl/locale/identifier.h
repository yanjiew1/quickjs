/* Fixed locale component operations, independent of backend data and JS.
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
#ifndef QJS_INTL_LOCALE_IDENTIFIER_H
#define QJS_INTL_LOCALE_IDENTIFIER_H
#include "../common/types.h"

typedef struct QJSIntlLocaleComponents {
    QJSIntlBytes base_name, language, script, region, variants;
    /* Includes its leading '-u'; excludes following singleton extensions. */
    QJSIntlBytes unicode_extension;
} QJSIntlLocaleComponents;

typedef struct QJSIntlLocaleBaseOptions {
    QJSIntlBytes language, script, region, variants;
} QJSIntlLocaleBaseOptions;

typedef struct QJSIntlLocaleKeyword {
    char key[2];
    QJSIntlBytes value;
} QJSIntlLocaleKeyword;

/* Input is a well-formed Unicode locale ID with lowercase extension singletons
   and keys, as produced by the identity service or base replacement below.
   These are component operations, not syntax validators. Results borrow input
   slices; absent components have NULL data. Output storage must not overlap input.
   No allocation, alias resolution or likely-subtag inference is performed. */
QJSIntlStatus qjs_intl_locale_components(QJSIntlBytes tag,
                                         QJSIntlLocaleComponents *out);
/* A present empty keyword has non-NULL data and zero length. Keys are two
   canonical ASCII bytes. Components must come from the operation above. */
QJSIntlBytes qjs_intl_locale_keyword(const QJSIntlLocaleComponents *components,
                                     const char key[2]);

/* Fixed UpdateLanguageId and Unicode-keyword composition. Base overrides are
   already validated, including duplicate variants; NULL data means unchanged.
   Keyword inputs have unique lowercase keys and Unicode type values (possibly
   empty). Preserve unrelated extensions, private use and Unicode attributes.
   Results are well-formed, not necessarily canonical; pass them to the locale
   service for data-dependent canonicalization. Copy keyword slots before that
   final canonicalization when implementing MakeLocaleRecord.
   All allocator callbacks are required. Outputs are separately owned strings,
   zeroed on failure and released with the same allocator. No input/output
   alias.
 */
QJSIntlStatus qjs_intl_locale_replace_base(
    const QJSIntlAllocator *allocator, QJSIntlBytes tag,
    const QJSIntlLocaleBaseOptions *options, char **out);
QJSIntlStatus qjs_intl_locale_replace_keywords(
    const QJSIntlAllocator *allocator, QJSIntlBytes tag,
    const QJSIntlLocaleKeyword *keywords, size_t count, char **out);
#endif
