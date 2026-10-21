/* Default extended grapheme clusters, Unicode 18 / UAX #29 revision 49.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef UNICODE_GRAPHEME_H
#define UNICODE_GRAPHEME_H
#include <stddef.h>
#include <stdint.h>

/* Return the next boundary in original UTF-16 code units. start must be zero
   or a boundary returned by this function; length ends iteration. text may be
   NULL only for an empty input. No allocation or normalization is performed.
   Repeated forward calls take linear time, including long RI/Extend runs.
   Locale-specific tailoring belongs to the segmentation service above this
   default algorithm. */
size_t unicode_grapheme_next(const uint16_t *text, size_t length, size_t start);
#endif
