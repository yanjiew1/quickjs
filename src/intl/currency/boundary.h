/* Currency-symbol adjacency for CLDR alphaNextToNumber patterns.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_CURRENCY_BOUNDARY_H
#define QJS_INTL_CURRENCY_BOUNDARY_H
#include "../common/types.h"

/* Borrow a NUL-free scalar UTF-8 symbol (empty is allowed). Before a number,
   the symbol is alphabetic at the boundary if it ends in a letter followed by zero or
   more marks; a symbol after the number must begin with a letter. L and M are
   Unicode General_Category, not Alphabetic. Do not skip format characters.
   No allocation or retained input. Disjoint outputs reset on failure. */
QJSIntlStatus qjs_intl_currency_boundary(QJSIntlBytes symbol, unsigned int *before_number,
                                       unsigned int *after_number);
#endif
