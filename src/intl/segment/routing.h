/* Immutable script-family candidates for native dictionary refinement.
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
#ifndef QJS_INTL_SEGMENT_ROUTING_H
#define QJS_INTL_SEGMENT_ROUTING_H

#include "dictionary.h"

typedef struct QJSIntlSegmentRouting QJSIntlSegmentRouting;

/* Build Unicode Script and Script_Extensions interval caches once, using the
   supplied allocator throughout. The heap-stable cache owns callback opaque
   storage and all ranges; failure resets output and releases partial state.
   A shared service/configuration owner must keep it alive until its last
   segmenter releases that owner. The cache has no traversal state or separate
   reference count. Closing NULL is harmless. */
QJSIntlStatus qjs_intl_segment_routing_open(const QJSIntlAllocator *, QJSIntlSegmentRouting **out);
void qjs_intl_segment_routing_close(QJSIntlSegmentRouting *);

/* Return candidate bits (1u << dictionary family), without allocation. Select
   Script with extensions == 0 or Script_Extensions with extensions == 1.
   Invalid arguments/scalars and no membership return zero. Candidates alone
   do not make punctuation, digits or emoji dictionary cores: the refinement
   owner also checks category, surrounding run and protected base/grapheme
   decisions. No duplicate script database or exact Line_Break=SA claim. */
unsigned int qjs_intl_segment_routing_mask(const QJSIntlSegmentRouting *, uint32_t scalar,
                                            int extensions);

/* True only when Script_Extensions contains Inherited. A zero family mask
   also describes uncached foreign scripts and is not evidence of neutrality.
   The caller separately checks that the scalar is a Mark before attachment. */
int qjs_intl_segment_routing_inherited(const QJSIntlSegmentRouting *, uint32_t scalar);

#endif
