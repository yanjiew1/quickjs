/* Collation element weights and UCA implicit-weight construction.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_WEIGHTS_H
#define QJS_INTL_COLLATION_WEIGHTS_H
#include "../common/types.h"

typedef struct QJSIntlCollationElement {
    uint16_t primary, secondary, tertiary;
    unsigned int variable;
} QJSIntlCollationElement;

/* UCA 18 Table 16 fallback only: callers first normalize and prefer explicit
   mappings (including special noncharacter mappings). Lone UTF-16 surrogates
   use generic implicit weights. Reset both output elements on invalid input. */
QJSIntlStatus qjs_intl_collation_implicit(uint32_t code, QJSIntlCollationElement out[2]);
#endif
