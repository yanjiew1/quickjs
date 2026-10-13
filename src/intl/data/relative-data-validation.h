/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_RELATIVE_DATA_VALIDATION_H
#define QJS_INTL_RELATIVE_DATA_VALIDATION_H
#include "native-data-reader.h"
#include "relative-data-schema.h"
/* Called after generic directory, pool, section16 and plural validation.
 * All absent -> NOT_FOUND; partial pair or invalid dependency -> INVALID.
 * No allocator, renderer, decimal core, provider or JS dependency. */
QJSIntlDataStatus qjs_intl_data_validate_relative_extension(const QJSIntlDataView *);
#endif
