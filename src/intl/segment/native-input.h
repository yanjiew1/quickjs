/* Owned final native boundary vectors, independent of service handles.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENT_NATIVE_INPUT_H
#define QJS_INTL_SEGMENT_NATIVE_INPUT_H

#include "suppression.h"

typedef struct QJSIntlSegmentNativeInput QJSIntlSegmentNativeInput;

/* Borrow validated selected/grapheme programs and a complete suppression list
   only during preparation. Sentence decisions are filtered and intersected
   with native grapheme boundaries before publishing final original-UTF16
   indexes. Grapheme mode needs only selected. Word refinement is not yet
   admitted by this private pipeline and returns UNSUPPORTED.
   Success retains no text, program, list or service pointer. The creating
   allocator's opaque owner must outlive the result. Fresh output is disjoint
   from inputs and reset on failure; all partial owners are released. */
QJSIntlStatus qjs_intl_segment_native_input_prepare(const QJSIntlAllocator *,
                                                     const QJSIntlSegmentProgram *selected,
                                                     const QJSIntlSegmentProgram *grapheme,
                                                     const QJSIntlSegmentLiteralGraph *suppressions,
                                                     const QJSIntlUTF16 *,
                                                     QJSIntlSegmentNativeInput **out);
void qjs_intl_segment_native_input_close(QJSIntlSegmentNativeInput *);

/* Query final vectors without allocation or shared cursor mutation. Follow
   segment.h's next/containing index contract. Outputs are disjoint and reset
   on failure. An input can outlive every handle used during preparation. */
QJSIntlStatus qjs_intl_segment_native_input_next(const QJSIntlSegmentNativeInput *, size_t start,
                                                 QJSIntlSegment *out);
QJSIntlStatus qjs_intl_segment_native_input_containing(const QJSIntlSegmentNativeInput *, size_t index,
                                                       QJSIntlSegment *out);

#endif
