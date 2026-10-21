/* Default sentence boundaries, Unicode 18 / UAX #29 revision 49.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef UNICODE_SENTENCE_H
#define UNICODE_SENTENCE_H
#include <stddef.h>
#include <stdint.h>

/* Return the next original UTF-16 boundary. start must be zero or a previously
   returned boundary; length ends iteration. NULL text is valid only if empty.
   No allocation, normalization or locale abbreviation suppression. Forward
   iteration is linear even through long ambiguous punctuation/space runs. */
size_t unicode_sentence_next(const uint16_t *text, size_t length, size_t start);
#endif
