/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_PLURAL_DATA_VALIDATION_H
#define QJS_INTL_PLURAL_DATA_VALIDATION_H
#include "native-data-reader.h"
#include "plural-data-schema.h"
/* Optional additive validator. Call after the generic reader has checked
 * wire1.3 header/directory/pool/base metadata. All absent is valid. A partial
 * triple, invalid rule grammar, bad sorting/span/category/reserved byte is
 * DATA_INVALID. No allocation. Does not authenticate digest or set coverage.
 */
QJSIntlDataStatus qjs_intl_data_validate_plural_extension(const QJSIntlDataView *);
#endif
