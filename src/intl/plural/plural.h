/* Plural service contract, independent of engine and selected backend.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_PLURAL_SERVICE_H
#define QJS_INTL_PLURAL_SERVICE_H
#include "rules.h"
#include "../common/decimal.h"

typedef enum QJSIntlPluralNotation {
    QJS_INTL_PLURAL_STANDARD,
    QJS_INTL_PLURAL_SCIENTIFIC,
    QJS_INTL_PLURAL_ENGINEERING,
    QJS_INTL_PLURAL_COMPACT
} QJSIntlPluralNotation;

typedef struct QJSIntlPluralOptions {
    QJSIntlPluralType type;
    QJSIntlPluralNotation notation;
    unsigned int compact_long; /* Must be 0 or 1; consulted only for compact. */
    QJSIntlDecimalOptions digits;
} QJSIntlPluralOptions;

typedef struct QJSIntlPluralService QJSIntlPluralService;
typedef struct QJSIntlPluralFormatter QJSIntlPluralFormatter;

/* Compile-time selected implementation. Copy the allocator; its callbacks and
   opaque state must outlive all handles. Opening validates the selected pack.
   On error *out is NULL. Closing NULL is safe. No JS callbacks occur. */
QJSIntlStatus qjs_intl_plural_open(const QJSIntlAllocator *, QJSIntlPluralService **out);
void qjs_intl_plural_close(QJSIntlPluralService *);
/* Borrow sorted, canonical, extension-free available tags until service close.
   Root is excluded. Failure clears both outputs. No matching happens here. */
QJSIntlStatus qjs_intl_plural_available(const QJSIntlPluralService *, const QJSIntlBytes **,
                                        size_t *count);
/* Exact inventory index; options have already undergone JS coercion. Rounding
   uses the common numeric options contract. A formatter may outlive its service.
   Category mask bits follow QJSIntlPluralCategory in standard category order. */
QJSIntlStatus qjs_intl_plural_formatter_open(const QJSIntlPluralService *, size_t locale,
                                             const QJSIntlPluralOptions *,
                                             QJSIntlPluralFormatter **);
void qjs_intl_plural_formatter_close(QJSIntlPluralFormatter *);
QJSIntlStatus qjs_intl_plural_categories(const QJSIntlPluralFormatter *, unsigned int *mask);
/* Borrow mathematical values. Round the unscaled value once, preserving the
   unsigned plain formatted text for CLDR operands and range equality. Notation
   supplies exponent metadata without changing that text or rounding again.
   Nonfinite values select OTHER. NaN ranges are INVALID_ARGUMENT, not a category.
   Failure resets the output to OTHER. These operations can allocate rounding
   buffers using the formatter allocator and retain no input. */
QJSIntlStatus qjs_intl_plural_select(const QJSIntlPluralFormatter *,
                                     const QJSIntlMathematicalValue *, QJSIntlPluralCategory *);
QJSIntlStatus qjs_intl_plural_select_range(const QJSIntlPluralFormatter *,
                                           const QJSIntlMathematicalValue *,
                                           const QJSIntlMathematicalValue *,
                                           QJSIntlPluralCategory *);
#endif
