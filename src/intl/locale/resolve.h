/* Fixed ECMA-402 ResolveLocale key negotiation after locale matching.
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
#ifndef QJS_INTL_LOCALE_RESOLVE_H
#define QJS_INTL_LOCALE_RESOLVE_H
#include "locale.h"

typedef QJSIntlStatus QJSIntlLocaleValueSupported(void *opaque, QJSIntlBytes value,
                                                 int *supported);
typedef struct QJSIntlLocaleResolveKey {
    char key[2];
    QJSIntlBytes default_value, option;
    unsigned int option_is_null;
    QJSIntlLocaleValueSupported *supported;
    void *opaque;
} QJSIntlLocaleResolveKey;

typedef struct QJSIntlResolvedLocale {
    QJSIntlAllocator allocator;
    char *locale;
    char **values; /* Owned strings in input key order; NULL denotes null. */
    size_t count;
} QJSIntlResolvedLocale;

/* Supply a canonical extension-free matched base and its canonical requested
   Unicode extension (or absent). Keys are unique lowercase Unicode keys, in
   RelevantExtensionKeys order. Defaults are already supported canonical types
   or null. Option.data == NULL means absent, unless option_is_null explicitly
   selects null, as hour12 requires for hc. Non-null options are validated
   Unicode types; canonicalize their aliases before comparison/membership.

   Callbacks implement the selected service/locale's membership without user
   code. NULL/0 is a null value, not an empty Boolean keyword; a present empty
   extension keyword requests true. A rejected option preserves an accepted
   extension. Compose retained keys once into the resulting locale.

   All allocator callbacks required. Identity may use a different allocator;
   no input/identity storage is retained. Results survive identity close and
   are released by clear. Inputs remain unchanged; callback state is borrowed.
   Outputs are fresh and disjoint; reset/unwind on every failure. No matching
   or option getters. */
QJSIntlStatus qjs_intl_resolve_locale_keys(const QJSIntlLocaleService *identity,
                                           const QJSIntlAllocator *allocator,
                                           QJSIntlBytes base, QJSIntlBytes extension,
                                           const QJSIntlLocaleResolveKey *keys, size_t count,
                                           QJSIntlResolvedLocale *out);
void qjs_intl_resolved_locale_clear(QJSIntlResolvedLocale *result);
#endif
