/* Currency-name placement around an already formatted decimal number.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_CURRENCY_FORMAT_H
#define QJS_INTL_NUMBER_CURRENCY_FORMAT_H
#include "../common/format.h"

/* Exact scalar UTF-8 template containing {0} and {1} once each. Apostrophes
   are literal, as in PartitionPattern. {0} inserts the complete number and
   its parts, including numeric sign affixes; {1} emits one currency part.
   Currency-name text may be empty. Other braces and malformed UTF-8 fail.

   Borrow inputs unchanged; fresh disjoint output, reset on errors. Require
   malloc/free, never realloc. Bound complete UTF-16 output; clear with
   qjs_intl_number_body_clear and the creating allocator. */
QJSIntlStatus qjs_intl_number_currency_name_format(const QJSIntlAllocator *, QJSIntlBytes pattern,
                                                    QJSIntlBytes name, const QJSIntlFormatted *,
                                                    size_t maximum_output_length,
                                                    QJSIntlFormatted *);
#endif
