/* Private native Locale information. ECMA402 7ae78cf, reviewed 2026-10-09.
 * Plain C; no JS, ICU, global defaults or provider lifecycle. */
#ifndef QJS_INTL_NATIVE_LOCALE_INFO_H
#define QJS_INTL_NATIVE_LOCALE_INFO_H
#include "intl/native-locale-id.h"

typedef struct QJSIntlLocaleInfoCapabilities {
    /* Provider owner sets these only after the respective native engine and
     * complete data inventory are verified. Their corresponding optional
     * tables must be present. Zero does not assert an empty AvailableLocales;
     * it reports UNSUPPORTED rather than inventing spec no-match fallbacks.
     * Source metadata, identity/defaultContent and BCP47 key inventories do
     * not prove service coverage. This struct and the reader are borrowed. */
    unsigned int calendars_ready, collator_ready, number_format_ready;
    unsigned int time_zones_ready;
} QJSIntlLocaleInfoCapabilities;

typedef QJSIntlLocaleInfoResult QJSIntlNativeLocaleInfoResult;

/* Result storage must not overlap inputs. Output is zeroed before work and
 * remains zero on failure. Successfully opened immutable readers only; all
 * allocator callbacks required. Clear with the same allocator before reuse.
 * Accepts bounded nonterminated locale slices; parser rejects invalid syntax,
 * embedded NUL and non-ASCII. Canonicalizes through the native ID engine.
 * Schema1.1 supplies preferences/week data;1.3 adds optional24..26. Missing
 * required optional data is UNSUPPORTED, malformed data is DATA_ERROR.
 */
QJSIntlStatus qjs_intl_native_locale_info_get(const QJSIntlAllocator *,
    const QJSIntlDataView *, const QJSIntlLocaleInfoCapabilities *,
    const QJSIntlLocaleInfoRequest *, QJSIntlLocaleInfoField,
    QJSIntlNativeLocaleInfoResult *);
void qjs_intl_native_locale_info_result_clear(const QJSIntlAllocator *,
                                             QJSIntlNativeLocaleInfoResult *);
#endif
