/* UTF-16 composition around an already formatted numeric body.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_UNIT_FORMAT_H
#define QJS_INTL_UNIT_FORMAT_H
#include "pattern.h"
#include "../common/format.h"

typedef struct QJSIntlUnitAffix {
    const QJSIntlBytes *spans;
    size_t count;
} QJSIntlUnitAffix;

/* Join borrowed scalar UTF-8 fragments on each side of the numeric body.
   Each fragment must end at a scalar boundary. Classify the concatenated
   affix as one unit part, with only its outer White_Space emitted as literals;
   boundaries between fragments do not create extra parts. Empty lists are
   valid. Require a complete nonempty numeric body with valid partitioned parts.
   malloc/free required, realloc unused. Bound the complete UTF-16 result.
   The result owns both arrays; clear with the creating allocator.
   Inputs remain borrowed and unchanged; disjoint output resets on errors.
   This supports compound-unit composition without intermediate text buffers. */
QJSIntlStatus qjs_intl_unit_affixes_format(const QJSIntlAllocator *,
                                           const QJSIntlUnitAffix *prefix,
                                           const QJSIntlUnitAffix *suffix,
                                           const QJSIntlFormatted *body, size_t maximum_output_length,
                                           QJSIntlFormatted *);

/* Borrow parsed scalar UTF-8 affixes and a complete nonempty numeric result.
   Preserve all body text and parts. Emit nonspace unit affixes as UNIT parts;
   leading/trailing Unicode White_Space is literal, internal spaces stay UNIT.
   If neither affix has a nonspace name, retain one empty UNIT part at the end.
   This composes the unit body; the caller owns numeric sign placement.

   malloc/free required, realloc unused. Fresh disjoint output, reset on errors.
   Bound the complete UTF-16 result; output owns both arrays, independent of all
   input lifetimes. Clear with the creating allocator. No plural/unit selection. */
QJSIntlStatus qjs_intl_unit_pattern_format(const QJSIntlAllocator *, const QJSIntlUnitPattern *,
                                           const QJSIntlFormatted *body, size_t maximum_output_length,
                                           QJSIntlFormatted *);
void qjs_intl_unit_format_clear(const QJSIntlAllocator *, QJSIntlFormatted *);
#endif
