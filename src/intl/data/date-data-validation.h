/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_DATA_VALIDATION_H
#define QJS_INTL_DATE_DATA_VALIDATION_H
#include "native-data-reader.h"
#include "date-data-schema.h"
/* A reviewed reader derivative invokes this after structural/string checks.
 * No allocation and no formatter or timezone provider dependency. Missing
 * group is valid optional data; partial group/malformed rows are invalid.
 * The old sealed reader continues to reject100..107 until root adopts them.
 */
QJSIntlDataStatus qjs_intl_date_data_validate(const QJSIntlDataView *);
QJSIntlDataStatus qjs_intl_date_data_i64(const QJSIntlDataSection *, uint32_t,
                                       uint32_t, int64_t *);
#endif
