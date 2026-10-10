/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_EXTRA_VALIDATION_H
#define QJS_INTL_NUMBER_EXTRA_VALIDATION_H
#include "number-data-validation.h"
#include "number-extra-schema.h"
/* Allocation-free owner gate. Includes original80..88 validation. */
QJSIntlDataStatus qjs_intl_number_extra_data_validate(const QJSIntlDataView *);
#endif
