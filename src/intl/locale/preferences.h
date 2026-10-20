/* Shared locale preference contracts and fixed selection rules.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_PREFERENCES_H
#define QJS_INTL_LOCALE_PREFERENCES_H
#include "locale.h"

typedef struct QJSIntlWeekInfo {
    unsigned int first_day;
    unsigned int minimum_days;
    unsigned int weekend; /* Bit zero is Monday; bit six is Sunday. */
} QJSIntlWeekInfo;

enum { QJS_INTL_DIRECTION_UNKNOWN, QJS_INTL_DIRECTION_LTR, QJS_INTL_DIRECTION_RTL };

typedef struct QJSIntlRegionPreference {
    char region[4];
    char override[4]; /* Empty when no valid rg keyword is present. */
} QJSIntlRegionPreference;

/* Backend data queries use exact canonical keys, without implicit fallback.
   UNSUPPORTED means the record is absent. Outputs are zeroed on error and
   slices borrow immutable service storage. No callback runs user code. */
QJSIntlStatus qjs_intl_locale_week_data(const QJSIntlLocaleService *service,
                                        QJSIntlBytes region, QJSIntlWeekInfo *out);
QJSIntlStatus qjs_intl_locale_hour_data(const QJSIntlLocaleService *service,
                                        QJSIntlBytes key, QJSIntlBytes *out,
                                        uint32_t *count);
QJSIntlStatus qjs_intl_locale_calendar_data(const QJSIntlLocaleService *service,
                                            QJSIntlBytes key, QJSIntlBytes *out,
                                            uint32_t *count);
QJSIntlStatus qjs_intl_locale_script_direction(const QJSIntlLocaleService *service,
                                               QJSIntlBytes script, unsigned int *out);

/* Fixed standard rules consume canonical locale tags from the identity
   service. Allocation and data errors are never treated as missing locale
   data. All output structures are zeroed on failure and must not alias input.
   Hour-cycle output borrows service storage or immutable fallback text. */
QJSIntlStatus qjs_intl_locale_region_preference(const QJSIntlLocaleService *service,
                                                QJSIntlBytes tag,
                                                QJSIntlRegionPreference *out);
QJSIntlStatus qjs_intl_locale_week_info(const QJSIntlLocaleService *service,
                                        QJSIntlBytes tag, QJSIntlBytes first_day,
                                        QJSIntlWeekInfo *out);
QJSIntlStatus qjs_intl_locale_hour_cycles(const QJSIntlLocaleService *service,
                                          QJSIntlBytes tag, QJSIntlBytes *out,
                                          uint32_t *count);
/* Select the first available language-region/region calendar preference list.
   Return raw source identifiers in preference order, borrowing service storage;
   no list is OK with empty output. The caller canonicalizes, deduplicates and
   filters by its formatting service's capabilities, then applies gregory fallback.
   Explicit Locale calendar slots bypass this query. */
QJSIntlStatus qjs_intl_locale_calendar_preferences(const QJSIntlLocaleService *service,
                                                   QJSIntlBytes tag, QJSIntlBytes *out,
                                                   uint32_t *count);
QJSIntlStatus qjs_intl_locale_text_direction(const QJSIntlLocaleService *service,
                                             QJSIntlBytes tag, unsigned int *out);
#endif
