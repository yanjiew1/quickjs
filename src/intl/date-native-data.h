/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_NATIVE_DATA_H
#define QJS_INTL_DATE_NATIVE_DATA_H
#include "date-native.h"
#include "date-native-bank.h"
#include "data/native-data-reader.h"
#include "data/date-data-schema.h"
#include "calendar/calendar.h"
/* view must have passed the generic structural reader and the independent
 * date-data validator. Frontend resolves section16 locale and section17
 * numbering indices. Algorithmic numbering returns UNSUPPORTED. ISO8601
 * uses separate CLDR source rows when present, with Gregorian compatibility
 * only for old blobs that have no ISO8601 rows.
 * Open copies its entire data snapshot, then releases all borrowed slices.
 */
QJSIntlStatus qjs_intl_native_date_open_data(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index, uint32_t numbering_index,
    const QJSIntlDateOptions *, const QJSIntlDateEnvironment *, QJSIntlNativeDate **);
/* One decoded snapshot prepares all seven immutable slots atomically. */
QJSIntlStatus qjs_intl_native_date_bank_open_data(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index, uint32_t numbering_index,
    const QJSIntlDateBankOptions *, const QJSIntlDateEnvironment *,
    QJSIntlNativeDateBank **);
/* Checked presence for each stable QJSCalendarId bit: selected locale's
 * styles, range fallback and required name identities. Arithmetic support
 * and successful frontend bank probes remain separate gates.
 */
QJSIntlStatus qjs_intl_native_date_data_calendar_mask(const QJSIntlDataView *,
    uint32_t locale_index, uint32_t *calendar_mask);
#endif
