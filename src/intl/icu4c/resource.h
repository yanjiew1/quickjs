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
#ifndef QJS_INTL_ICU4C_RESOURCE_H
#define QJS_INTL_ICU4C_RESOURCE_H
#include "common.h"
#include <unicode/ures.h>

/* Resolve a complete CLDR resource path through explicit %%Parent links and
   ordinary locale parents. Never take an unrelated process default locale.
   A found no-inheritance marker is returned, not skipped by parent fallback.
   ICU resolves aliases itself; this is not general CLDR lateral-alias rebasing.
   Consumers must still apply their width/target fallback from the request locale.
   The returned ICU handle must be closed; strings below use our allocator. */
QJSIntlStatus qjs_icu4c_resource_path(const QJSIntlAllocator *, const char *locale,
                                      const char *const *path, size_t count, UResourceBundle **out);
/* As above for an ICU data subtree, for example the unit-pattern package. */
QJSIntlStatus qjs_icu4c_resource_package_path(const QJSIntlAllocator *, const char *package,
                                              const char *locale, const char *const *path,
                                              size_t count, UResourceBundle **out);
typedef struct QJSICU4CResourcePath {
    const char *const *keys;
    size_t count;
} QJSICU4CResourcePath;

/* Try a fixed ordered list of at most 32 source variants at each locale before
   following its parent. A no-inheritance marker blocks only that variant;
   explicit empty text remains a found value. This is LDML variant fallback,
   not service matching or ICU lateral-alias rebasing. */
QJSIntlStatus qjs_icu4c_resource_variants(const QJSIntlAllocator *, const char *package,
                                          const char *locale, const QJSICU4CResourcePath *,
                                          size_t count, UResourceBundle **out);

QJSIntlStatus qjs_icu4c_resource_text(const QJSIntlAllocator *, const UResourceBundle *, char **out,
                                      int *blocked);
#endif
