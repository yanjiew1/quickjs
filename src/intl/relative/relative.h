/* RelativeTimeFormat service, independent of JS coercion and locale matching.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_RELATIVE_SERVICE_H
#define QJS_INTL_RELATIVE_SERVICE_H
#include "format.h"

typedef enum QJSIntlRelativeNumeric {
    QJS_INTL_RELATIVE_ALWAYS,
    QJS_INTL_RELATIVE_AUTO
} QJSIntlRelativeNumeric;

typedef struct QJSIntlRelativeOptions {
    QJSIntlRelativeWidth width;
    QJSIntlRelativeNumeric numeric;
    size_t maximum_output_length;
} QJSIntlRelativeOptions;

typedef struct QJSIntlRelativeService QJSIntlRelativeService;
typedef struct QJSIntlRelativeFormatter QJSIntlRelativeFormatter;

/* Compile-time selected service. Copy allocator callbacks/opaque; malloc/free
   required, realloc unused. They outlive all corresponding handles/results.
   Validate selected profile and numeric/plural dependencies. Errors reset *out;
   NULL closes are harmless. No matching, JS callbacks or option coercion. */
QJSIntlStatus qjs_intl_relative_open(const QJSIntlAllocator *, QJSIntlRelativeService **);
void qjs_intl_relative_close(QJSIntlRelativeService *);
/* Borrow sorted, extension-free available tags, excluding root, until close.
   Queries allocate nothing. Non-NULL outputs reset on failure. */
QJSIntlStatus qjs_intl_relative_available(const QJSIntlRelativeService *, const QJSIntlBytes **,
                                          size_t *count);
QJSIntlStatus qjs_intl_relative_default_system(const QJSIntlRelativeService *, size_t locale,
                                               QJSIntlBytes *);
QJSIntlStatus qjs_intl_relative_system_supported(const QJSIntlRelativeService *, size_t locale,
                                                 QJSIntlBytes system, int *supported);
/* Global repertoire across admitted locales, without allocation or fallback.
   Unknown names are OK/false; errors clear non-NULL output. */
QJSIntlStatus qjs_intl_relative_system_available(const QJSIntlRelativeService *,
                                                 QJSIntlBytes system, int *supported);
/* Exact available index; empty system selects default. Unsupported explicit
   numbering systems do not fallback. Formatter owns its handles and may
   outlive the service. Copy options; retain no supplied string storage.
   The nonzero bound covers output and numeric intermediate coefficients. */
QJSIntlStatus qjs_intl_relative_formatter_open(const QJSIntlRelativeService *, size_t locale,
                                               QJSIntlBytes system, const QJSIntlRelativeOptions *,
                                               QJSIntlRelativeFormatter **);
void qjs_intl_relative_formatter_close(QJSIntlRelativeFormatter *);
/* Finite, already-coerced Number and singular unit. Auto literal lookup uses
   the original Number string before rounding (-0 maps to key0); tense retains
   the original sign including -0. Numeric magnitude uses the exact binary64
   real, shared with the internal ResolvePlural operation. Public PluralRules
   select has its own conversion boundary, which this internal call bypasses.
   Fresh disjoint output, same result ownership/clearing as format.h. */
QJSIntlStatus qjs_intl_relative_format(const QJSIntlRelativeFormatter *, double value,
                                       QJSIntlRelativeUnit, QJSIntlRelativeFormatted *);
#endif
