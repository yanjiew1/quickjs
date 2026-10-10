/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_COMPACT_H
#define QJS_INTL_NUMBER_COMPACT_H
#include "number-native.h"
/* Pure selection helpers. The owner validates/copies the sorted rows. */
const QJSIntlNumberCompact *qjs_intl_number_compact_for_magnitude(
    const QJSIntlNumberCompact *, size_t, int64_t);
int qjs_intl_number_compact_is_one(const QJSIntlDecimal *);
#endif
