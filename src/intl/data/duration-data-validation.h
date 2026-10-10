/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_DATA_VALIDATION_H
#define QJS_INTL_DURATION_DATA_VALIDATION_H
#include "native-data-reader.h"
#include "duration-data-schema.h"
/* Reader and this file only: no formatter/decimal/plural/list dependency.
 * Central reader first validates envelopes/pool/version and admits width110.
 * Optional absent110 succeeds. Presence requires metadata16/17 and numerical
 * numbering17 flag0. Completeness and service activation are external gates.
 */
QJSIntlDataStatus qjs_intl_duration_data_validate(const QJSIntlDataView *);
#endif
