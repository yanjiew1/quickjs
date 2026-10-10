/* Private native locale ID operations. No JS, ICU or provider lifecycle. */
#ifndef QJS_INTL_NATIVE_LOCALE_ID_H
#define QJS_INTL_NATIVE_LOCALE_ID_H
#include "intl/provider.h"
#include "intl/data/native-data-reader.h"

/* The allocator and successfully opened reader view are borrowed per call.
 * All allocator callbacks are required. Input slices may be nonterminated;
 * NUL/non-ASCII bytes are invalid. Output is zeroed before work. Successful
 * strings are separately allocated, NUL terminated, and freed by allocator.
 * Output pointer storage must not overlap input/view/allocator objects.
 * No API enumerates service availability: the M01-M04 blob has no coverage.
 */
QJSIntlStatus qjs_intl_native_locale_validate(const QJSIntlAllocator *,
                                             QJSIntlBytes, int *well_formed);
QJSIntlStatus qjs_intl_native_locale_canonicalize(const QJSIntlAllocator *,
                         const QJSIntlDataView *, QJSIntlBytes, char **out);
/* Key is a Unicode key (ASCII alnum + ASCII alpha). Value admits an empty
 * value or Unicode type subtags. Unknown well-formed types are preserved. */
QJSIntlStatus qjs_intl_native_locale_canonicalize_uvalue(const QJSIntlAllocator *,
                         const QJSIntlDataView *, QJSIntlBytes key,
                         QJSIntlBytes value, char **out);
QJSIntlStatus qjs_intl_native_locale_maximize(const QJSIntlAllocator *,
                         const QJSIntlDataView *, QJSIntlBytes, char **out);
/* Remove Likely Subtags uses the Unicode favorRegion order. */
QJSIntlStatus qjs_intl_native_locale_minimize(const QJSIntlAllocator *,
                         const QJSIntlDataView *, QJSIntlBytes, char **out);
#endif
