/* Resolve API calendar identities separately in each raw source inventory.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
