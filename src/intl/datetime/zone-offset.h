/* Localized GMT offsets; independent of named-zone identity and rule lookup.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_ZONE_OFFSET_H
#define QJS_INTL_DATETIME_ZONE_OFFSET_H
#include "pattern.h"
#include "timezone-name-data.h"
#include "../common/unary-pattern.h"
#include "../number/number.h"

typedef struct QJSIntlDateOffsetPattern {
    QJSIntlDatePattern program;
    QJSIntlBytes prefix, separator, suffix;
} QJSIntlDateOffsetPattern;

typedef struct QJSIntlDateOffset {
    QJSIntlAllocator allocator;
    QJSIntlDateOffsetPattern positive, negative;
    QJSIntlUnaryPattern wrapper;
    QJSIntlBytes zero;
    void *storage;
} QJSIntlDateOffset;

/* Compile copied scalar UTF-8 source facts: one positive/negative hourFormat
   pair with H/HH then mm, literal unary gmtFormat and optional gmtZeroFormat.
   Apostrophe quoting applies to hourFormat only; it protects semicolons too.
   Empty hour/wrapper source is UNSUPPORTED. Malformed grammar/text is DATA_ERROR.
   Preserve source signs/bidi/separators; choose no name or numbering fallback.
   Require malloc/free, unchanged inputs and disjoint fresh output; reset errors.
   Own all data independently of input slices; clear with the retained allocator. */
QJSIntlStatus qjs_intl_date_offset_open(const QJSIntlAllocator *allocator,
                                       QJSIntlBytes hour, QJSIntlBytes wrapper, QJSIntlBytes zero,
                                       QJSIntlDateOffset *out);
/* Resolve exactly ordinary hourFormat/gmtFormat/gmtZeroFormat in the validated
   owner's FORMAT chain, respecting BLOCK/empty. Borrow only during open; the
   resulting owner survives the source pack. Absent/BLOCK/empty gmtZeroFormat
   uses numeric zero, not an invented standalone label. Catalog indexes are local. */
QJSIntlStatus qjs_intl_date_offset_from_data(const QJSIntlTimeZoneNameData *data, uint32_t locale,
                                            const QJSIntlAllocator *allocator, QJSIntlDateOffset *out);
void qjs_intl_date_offset_clear(QJSIntlDateOffset *format);

/* Render known integral offset seconds strictly between -86400 and +86400.
   long_format is 0/1. Long uses HH:mm, short unpadded H and optional minutes;
   nonzero seconds retain minutes and repeat the source H/m separator before ss.
   Zero uses a supplied gmtZeroFormat, otherwise the positive numeric template. Supply selected decimal formatters with grouping off,
   no fractions and minimum integer 1/2 respectively, created with this allocator.
   Collapse the entire result to one timeZoneName part. Require immutable live
   formatters, a nonzero UTF-16 bound and disjoint fresh output; reset errors.
   Result owns text/parts independently; clear with the owner's allocator. */
QJSIntlStatus qjs_intl_date_offset_format(const QJSIntlDateOffset *format, int32_t seconds,
                                         unsigned int long_format,
                                         const QJSIntlNumberFormatter *numeric,
                                         const QJSIntlNumberFormatter *two_digit,
                                         size_t maximum_output_length, QJSIntlFormatted *out);
#endif
