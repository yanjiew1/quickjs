/* Owned decoded input and base-rule decisions, before native refinement.
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
#ifndef QJS_INTL_SEGMENT_SCAN_H
#define QJS_INTL_SEGMENT_SCAN_H

#include "program.h"

enum { QJS_INTL_SEGMENT_DEFAULT_RULE = UINT16_MAX };

typedef struct QJSIntlSegmentText {
    QJSIntlAllocator allocator;
    void *storage;
    uint32_t *scalars;
    size_t *offsets; /* count + 1 original UTF-16 boundaries, including EOF. */
    size_t count, length;
} QJSIntlSegmentText;

typedef struct QJSIntlSegmentScan {
    QJSIntlAllocator allocator;
    void *storage;
    uint16_t *classes; /* One class per scalar. */
    uint16_t *rules; /* count + 1 winning rule IDs, or DEFAULT_RULE. */
    uint8_t *breaks; /* count + 1 base decisions, including mandatory ends. */
    size_t count;
} QJSIntlSegmentScan;

/* Preparation is explicit and fallible. Borrow raw input only during decoding;
   retain paired scalars and lone surrogates with exact original UTF-16 offsets.
   The decoded owner can be shared across selected/grapheme program scans.
   Outputs must be fresh and disjoint from inputs. Publish only complete owners;
   failure resets outputs and frees partial allocations. Allocator opaque stays
   alive until clear. Owners must not be shallow-copied or cleared twice through
   different copies; clearing NULL/already-zero owners is harmless. */
QJSIntlStatus qjs_intl_segment_text_prepare(const QJSIntlAllocator *, const QJSIntlUTF16 *,
                                            QJSIntlSegmentText *out);
void qjs_intl_segment_text_clear(QJSIntlSegmentText *);

/* Require unchanged successful decoded/program views during this call. The
   result owns its arrays, not those views. Right machines run backward once
   from original EOF; left machines run forward once from original BOS. Keep
   the first matching rule at each scalar boundary. No arbitrary context cutoff
   or per-candidate rescanning. Scratch is released before publication.
   These are BASE decisions: dictionaries/suppressions/grapheme protection and
   native locale capability remain separate. Later refinement may edit breaks. */
QJSIntlStatus qjs_intl_segment_scan_prepare(const QJSIntlAllocator *, const QJSIntlSegmentProgram *,
                                            const QJSIntlSegmentText *, QJSIntlSegmentScan *out);
void qjs_intl_segment_scan_clear(QJSIntlSegmentScan *);

#endif
