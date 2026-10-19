/* Sign-only projection for complete compact currency bodies.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_SIGN_AFFIX_H
#define QJS_INTL_NUMBER_SIGN_AFFIX_H
#include "pattern-data.h"

/* Require decoded, scalar-text pattern forms with exactly one NUMBER each.
   Subtract common positive prefix/suffix context on each side of NUMBER,
   comparing literal scalars and typed tokens. Preserve only the signed form's
   remaining context. This is a native composition policy, not an LDML rewrite.
   Reject residual currency or percent tokens, which cannot be a sign envelope.

   Rewrite selected in place, using at most its original count. Positive and
   selected arrays must be disjoint; literal slices borrow their original
   unchanged storage. No allocation. On failure *count is zero; array contents
   are unspecified. Output count must not alias either token array. */
QJSIntlStatus qjs_intl_number_sign_affix(const QJSIntlNumberPatternToken *positive,
                                        size_t positive_count,
                                        QJSIntlNumberPatternToken *selected, size_t selected_count,
                                        size_t *count);
#endif
