/* Native locale preference records; no engine or backend-selection policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_PREFERENCE_DATA_H
#define QJS_INTL_LOCALE_PREFERENCE_DATA_H
#include "../native/data.h"
#include "preferences.h"

typedef struct QJSIntlLocalePreferences {
    QJSIntlData data;
    QJSIntlSection weeks, calendars, hours, scripts;
} QJSIntlLocalePreferences;

/* Validate all four owner schemas against an unchanged successfully opened
   shared view. Borrow immutable data, release all allocation scratch before
   return, and follow pack.h's nonaliasing/zero-on-failure contract. Missing
   required tables are data errors; no service capability is inferred. */
QJSIntlStatus qjs_intl_locale_preferences_open(const QJSIntlData *data,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlLocalePreferences *out);

/* Exact key lookups, without world/likely-subtag fallback. UNSUPPORTED means
   no record. Outputs are zeroed on failure; borrowed slices share the view's
   lifetime. These calls allocate no memory and cannot invoke user code. */
QJSIntlStatus qjs_intl_preference_week(const QJSIntlLocalePreferences *data,
                                       QJSIntlBytes region, QJSIntlWeekInfo *out);
QJSIntlStatus qjs_intl_preference_list(const QJSIntlLocalePreferences *data, int hours,
                                       QJSIntlBytes key, QJSIntlBytes *out,
                                       uint32_t *count);
QJSIntlStatus qjs_intl_preference_direction(const QJSIntlLocalePreferences *data,
                                            QJSIntlBytes script, unsigned int *out);
#endif
