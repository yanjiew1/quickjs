/* Metadata equivalence, independent of locale syntax and service capability.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_TYPE_IDENTITY_H
#define QJS_INTL_LOCALE_TYPE_IDENTITY_H
#include "native-data.h"

/* Match an exact registered name or complete alias token within extension/key,
   then follow preferred names to a terminal nonvariable metadata row. Legacy
   source spellings need not be well-formed Unicode types. Do not normalize
   case/underscores or infer service support. Different matches may converge
   on one terminal; conflicting terminals return DATA_ERROR. Unknown/variable
   spellings return UNSUPPORTED. Allocate nothing, borrow unchanged validated
   metadata and reset non-NULL output on failure. Output is a metadata index,
   not a service catalog index; zero is a valid successful index. */
QJSIntlStatus qjs_intl_locale_type_identity(const QJSIntlLocaleMetadata *metadata,
                                           QJSIntlBytes extension, QJSIntlBytes key,
                                           QJSIntlBytes spelling, uint32_t *out);
#endif
