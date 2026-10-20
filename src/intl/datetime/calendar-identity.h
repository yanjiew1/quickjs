/* Resolve API calendar identities separately in each raw source inventory.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_CALENDAR_IDENTITY_H
#define QJS_INTL_DATETIME_CALENDAR_IDENTITY_H
#include "available-data.h"
#include "combination-data.h"
#include "interval-data.h"
#include "name-data.h"
#include "style-data.h"
#include "../locale/type-identity.h"

/* Resolve registered names/aliases against unchanged validated metadata and
   one typed owner's calendar catalog. Allocate nothing. Return that owner's
   raw index; indexes from different owners are unrelated. Missing identities
   return UNSUPPORTED; multiple distinct raw keys with the requested identity
   return DATA_ERROR. Keep ISO distinct from Gregorian, and generic source-only.
   Do not choose calendar fallback or establish arithmetic/rendering support.
   Reset non-NULL output on failure; index zero is a valid success. */
QJSIntlStatus qjs_intl_date_available_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                                   const QJSIntlDateAvailableData *data,
                                                   QJSIntlBytes calendar, uint32_t *out);
QJSIntlStatus qjs_intl_date_style_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                               const QJSIntlDateStyleData *data,
                                               QJSIntlBytes calendar, uint32_t *out);
QJSIntlStatus qjs_intl_date_combination_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                                     const QJSIntlDateCombinationData *data,
                                                     QJSIntlBytes calendar, uint32_t *out);
QJSIntlStatus qjs_intl_date_name_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                              const QJSIntlCalendarNameData *data,
                                              QJSIntlBytes calendar, uint32_t *out);
QJSIntlStatus qjs_intl_date_interval_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                                  const QJSIntlDateIntervalData *data,
                                                  QJSIntlBytes calendar, uint32_t *out);
#endif
