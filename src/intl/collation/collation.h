/* Compile-time collation service; no JS coercion or source compiler state.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QJS_INTL_COLLATION_SERVICE_H
#define QJS_INTL_COLLATION_SERVICE_H
#include "../common/types.h"
#include "policy.h"

typedef enum QJSIntlCollationUsage {
    QJS_INTL_COLLATION_SORT,
    QJS_INTL_COLLATION_SEARCH
} QJSIntlCollationUsage;

typedef struct QJSIntlCollationOptions {
    QJSIntlCollationUsage usage;
    QJSIntlCollationSensitivity sensitivity;
    QJSIntlCollationCaseFirst case_first;
    int numeric, ignore_punctuation; /* Resolved Boolean values, 0 or 1. */
    QJSIntlBytes collation; /* Empty is semantic null/default, not "standard". */
} QJSIntlCollationOptions;

typedef struct QJSIntlCollationService QJSIntlCollationService;
typedef struct QJSIntlCollator QJSIntlCollator;

/* Copy the allocator; all callbacks required and their opaque state must
   outlive handles. Open validates the selected immutable profile. Outputs are
   fresh/disjoint and reset on errors; close(NULL) is safe. No JS callbacks. */
QJSIntlStatus qjs_intl_collation_open(const QJSIntlAllocator *allocator,
                                      const struct QJSIntlData *checked_data,
                                      QJSIntlCollationService **out);
void qjs_intl_collation_close(QJSIntlCollationService *);
/* Borrow canonical sorted available locales, excluding source root, until
   service close. All locale indexes below refer to this array. No matching. */
QJSIntlStatus qjs_intl_collation_available(const QJSIntlCollationService *,
                                           const QJSIntlBytes **, size_t *count);
/* Locale/usage defaults are independent of a later named co selection. Sort
   sensitivity is variant; search projects source strength to API sensitivity,
   preserving case via variant when secondary+case cannot be expressed exactly.
   The returned options have semantic null co and remain valid after close. */
QJSIntlStatus qjs_intl_collation_defaults(const QJSIntlCollationService *, size_t locale,
                                          QJSIntlCollationUsage, QJSIntlCollationOptions *);
/* Exact named type membership; empty/unknown names give OK/false. No fallback
   or allocation. Enumeration excludes semantic null. Start cursor at zero;
   success/exhaustion advances it, errors preserve it. Exhaustion is UNSUPPORTED.
   Names borrow immutable profile storage; arrays/identity belong to service. */
QJSIntlStatus qjs_intl_collation_type_supported(const QJSIntlCollationService *, size_t locale,
                                                QJSIntlCollationUsage, QJSIntlBytes, int *);
QJSIntlStatus qjs_intl_collation_type_next(const QJSIntlCollationService *, size_t locale,
                                           QJSIntlCollationUsage, uint32_t *cursor, QJSIntlBytes *);
QJSIntlStatus qjs_intl_collation_available_type_next(const QJSIntlCollationService *,
                                                     uint32_t *cursor, QJSIntlBytes *);
/* Inputs are fully resolved; zero initialization is not a request for source
   defaults. Unsupported exact co returns UNSUPPORTED, not locale fallback.
   Copy program/policy/allocator, retain no service arrays or caller option
   strings. Handles can outlive service close; embedded bytes stay immutable. */
QJSIntlStatus qjs_intl_collator_open(const QJSIntlCollationService *, size_t locale,
                                     const QJSIntlCollationOptions *, QJSIntlCollator **);
void qjs_intl_collator_close(QJSIntlCollator *);
/* Counted UTF-16 preserves NUL, lone surrogates and canonical equivalence.
   Inputs/handle remain unchanged and disjoint from order. Reset order on error;
   otherwise return -1, 0 or 1. No input/scratch survives the call. */
QJSIntlStatus qjs_intl_collator_compare(const QJSIntlCollator *,
                                        const uint16_t *left, size_t left_length,
                                        const uint16_t *right, size_t right_length, int *order);
#endif
