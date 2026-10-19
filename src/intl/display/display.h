/* DisplayNames service contract; no JS values or backend representation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_SERVICE_H
#define QJS_INTL_DISPLAY_SERVICE_H
#include "../common/types.h"

typedef enum QJSIntlDisplayType {
    QJS_INTL_DISPLAY_LANGUAGE,
    QJS_INTL_DISPLAY_REGION,
    QJS_INTL_DISPLAY_SCRIPT,
    QJS_INTL_DISPLAY_CURRENCY,
    QJS_INTL_DISPLAY_CALENDAR,
    QJS_INTL_DISPLAY_DATE_TIME_FIELD,
    QJS_INTL_DISPLAY_TYPE_COUNT
} QJSIntlDisplayType;

typedef enum QJSIntlNameStyle {
    QJS_INTL_NAME_LONG,
    QJS_INTL_NAME_SHORT,
    QJS_INTL_NAME_NARROW
} QJSIntlNameStyle;

typedef struct QJSIntlNameText {
    uint16_t *text; /* Owned UTF-16, explicit length, no terminator required. */
    size_t length;
    unsigned int found; /* Empty localized text is still present. */
} QJSIntlNameText;

typedef struct QJSIntlDisplayOptions {
    QJSIntlDisplayType type;
    QJSIntlNameStyle style;
    int dialect; /* Boolean; ignored for nonlanguage types. */
    size_t maximum_output_length;
} QJSIntlDisplayOptions;

typedef struct QJSIntlDisplayService QJSIntlDisplayService;
typedef struct QJSIntlDisplayFormatter QJSIntlDisplayFormatter;

/* Open the compile-time backend. Copy malloc/free callbacks and opaque state,
   which outlive all corresponding handles and results; realloc is unused.
   Failure resets *out. Closing NULL is harmless. No JS callbacks. */
QJSIntlStatus qjs_intl_display_open(const QJSIntlAllocator *, QJSIntlDisplayService **);
void qjs_intl_display_close(QJSIntlDisplayService *);
/* Borrow sorted, unique extension-free available locales until service close.
   Exclude root; availability does not promise a name for every valid code.
   Queries allocate nothing and reset non-NULL outputs on failure. */
QJSIntlStatus qjs_intl_display_available(const QJSIntlDisplayService *, const QJSIntlBytes **,
                                         size_t *count);
/* Exact available index and copied options, with a nonzero UTF-16 output bound.
   Formatter retains no service-owned pointers and may outlive the service. */
QJSIntlStatus qjs_intl_display_formatter_open(const QJSIntlDisplayService *, size_t locale,
                                              const QJSIntlDisplayOptions *, QJSIntlDisplayFormatter **);
void qjs_intl_display_formatter_close(QJSIntlDisplayFormatter *);
/* Already validated CanonicalCodeForDisplayNames result: canonical Language ID,
   uppercase region/currency, titlecase script, lowercase Unicode calendar type,
   or exact dateTimeField token. Core owns canonicalization and API fallback.
   Missing fields return OK/absent; empty and code-identical names are present.
   Result owns UTF-16 text and may outlive both handles. Require fresh disjoint
   output and unchanged input during this call; reset output on failure. */
QJSIntlStatus qjs_intl_display_format(const QJSIntlDisplayFormatter *, QJSIntlBytes canonical,
                                      QJSIntlNameText *);
void qjs_intl_name_text_clear(const QJSIntlAllocator *, QJSIntlNameText *);
#endif
