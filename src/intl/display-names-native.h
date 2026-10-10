/* Original native DisplayNames source proposal. No engine/ICU dependency.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, reviewed 2026-10-09,
 * sections 12.3.3 and 12.5.1. Unicode data attribution in packet evidence. */
#ifndef QJS_INTL_DISPLAY_NAMES_NATIVE_H
#define QJS_INTL_DISPLAY_NAMES_NATIVE_H
#include "intl/provider.h"
#include "intl/data/native-data-reader.h"
typedef enum QJSIntlDisplayNamesType {
    QJS_INTL_DN_LANGUAGE, QJS_INTL_DN_REGION, QJS_INTL_DN_SCRIPT,
    QJS_INTL_DN_CURRENCY, QJS_INTL_DN_CALENDAR, QJS_INTL_DN_DATE_TIME_FIELD
} QJSIntlDisplayNamesType;
typedef enum QJSIntlDisplayNamesStyle {
    QJS_INTL_DN_LONG, QJS_INTL_DN_SHORT, QJS_INTL_DN_NARROW
} QJSIntlDisplayNamesStyle;
typedef struct QJSIntlDisplayNamesOptions {
    QJSIntlDisplayNamesType type;
    QJSIntlDisplayNamesStyle style;
    int fallback_code;            /* 0 none; 1 code */
    int language_dialect;         /* 0 standard; 1 dialect */
} QJSIntlDisplayNamesOptions;
typedef struct QJSIntlDisplayNamesResult {
    uint16_t *text;               /* owned; explicit UTF16 length */
    size_t length;
    int present;                 /* 0 => ECMAScript undefined */
} QJSIntlDisplayNamesResult;
typedef struct QJSIntlNativeDisplayNames QJSIntlNativeDisplayNames;
/* Allocator callbacks and the opened reader descriptor are copied. The
 * allocator opaque and immutable blob must remain alive until close. Locale is
 * the already resolved section16 index, not an arbitrary fallback locale.
 * All allocator callbacks are required. The view must come from successful
 * qjs_intl_data_open using the combined schema1.3 reader, which owns global
 * validation of sections50/51, ordering, references, text and patterns. The
 * blob must remain unchanged; the caller may release the view descriptor.
 * Open checks section widths and local
 * required records before creating a handle, in logarithmic table lookups.
 * Absence is UNSUPPORTED; accessor/width failures are DATA_ERROR. Malformed
 * blobs reject at reader open, before a view can be passed here. No operation
 * enumerates service availability. */
QJSIntlStatus qjs_intl_native_display_names_open(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index,
    const QJSIntlDisplayNamesOptions *, QJSIntlNativeDisplayNames **out);
void qjs_intl_native_display_names_close(QJSIntlNativeDisplayNames *);
/* Accepts the UTF16 result of frontend ToString. Invalid syntax returns
 * INVALID_ARGUMENT regardless of fallback. An unknown valid code yields
 * present=0 or the canonical code. Output is zeroed before work. */
QJSIntlStatus qjs_intl_native_display_names_of(QJSIntlNativeDisplayNames *,
    QJSIntlUTF16 code, QJSIntlDisplayNamesResult *out);
void qjs_intl_native_display_names_result_clear(const QJSIntlAllocator *,
    QJSIntlDisplayNamesResult *);
#endif
