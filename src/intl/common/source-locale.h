/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_SOURCE_LOCALE_H
#define QJS_INTL_COMMON_SOURCE_LOCALE_H
#include "types.h"

/* Require malloc/free callbacks. Validate normalized CLDR source spelling,
   including root, casing and unique variants; extensions are not allowed.
   Duplicate-variant scratch is released before return. This does not resolve
   aliases or establish the locale's availability in a selected data profile. */
QJSIntlStatus qjs_intl_source_locale_validate(const QJSIntlAllocator *, QJSIntlBytes);
#endif
