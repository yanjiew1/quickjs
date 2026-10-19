/* Native LDML digital-duration projection; not a backend service contract.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_NATIVE_DIGITAL_H
#define QJS_INTL_DURATION_NATIVE_DIGITAL_H
#include "../common/types.h"

typedef struct QJSIntlNativeDurationDigital {
    char *storage;
    QJSIntlBytes hour_minute, minute_second;
    unsigned int hour_width; /* Raw CLDR preference, not an option override. */
} QJSIntlNativeDurationDigital;

/* Project the authoritative scalar UTF-8 hms pattern's inter-field literals.
   Recognize ordered h/m/s runs of width 1 or 2 with LDML apostrophe quoting.
   Outer literals cannot be represented by DigitalFormat and are not emitted.
   Keep unequal separators and the raw hour width; do not substitute the
   unrelated numeric timeSeparator symbol. Malformed input is DATA_ERROR,
   not an implicit colon fallback. Require malloc/free; one owned buffer.
   Fresh disjoint output resets on failure and survives input storage. */
QJSIntlStatus qjs_intl_duration_native_digital(const QJSIntlAllocator *, QJSIntlBytes pattern,
                                             QJSIntlNativeDurationDigital *);
void qjs_intl_duration_native_digital_clear(const QJSIntlAllocator *, QJSIntlNativeDurationDigital *);
#endif
