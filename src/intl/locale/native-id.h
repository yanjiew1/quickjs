/* Native Unicode locale operations over validated metadata.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_LOCALE_ID_H
#define QJS_INTL_NATIVE_LOCALE_ID_H
#include "native-data.h"

/* The allocator and successfully opened reader view are borrowed per call.
 * All allocator callbacks are required. Input slices may be nonterminated;
 * NUL/non-ASCII bytes are invalid. Output is zeroed before work. Successful
 * strings are separately allocated, NUL terminated, and freed by allocator.
 * Output pointer storage must not overlap input/view/allocator objects.
 * Locale ID operations do not infer installed service availability.
 */
QJSIntlStatus qjs_intl_native_locale_validate(const QJSIntlAllocator *,
                                              QJSIntlBytes, int *well_formed);
QJSIntlStatus qjs_intl_native_locale_canonicalize(const QJSIntlAllocator *,
                                                  const QJSIntlLocaleMetadata *,
                                                  QJSIntlBytes, char **out);
/* Key is a Unicode key (ASCII alnum + ASCII alpha). Value admits an empty
 * value or Unicode type subtags. Unknown well-formed types are preserved. */
QJSIntlStatus qjs_intl_native_locale_canonicalize_uvalue(
                                                         const QJSIntlAllocator *, const QJSIntlLocaleMetadata *, QJSIntlBytes key,
                                                         QJSIntlBytes value, char **out);
QJSIntlStatus qjs_intl_native_locale_maximize(const QJSIntlAllocator *,
                                              const QJSIntlLocaleMetadata *,
                                              QJSIntlBytes, char **out);
/* Remove Likely Subtags uses the Unicode favorRegion order. */
QJSIntlStatus qjs_intl_native_locale_minimize(const QJSIntlAllocator *,
                                              const QJSIntlLocaleMetadata *,
                                              QJSIntlBytes, char **out);
#endif
