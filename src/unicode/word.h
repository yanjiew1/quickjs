/* Default word boundaries, Unicode 18 / UAX #29 revision 49.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef UNICODE_WORD_H
#define UNICODE_WORD_H
#include <stddef.h>
#include <stdint.h>

/* Return the next original UTF-16 boundary. start must be zero or a previously
   returned boundary; length ends iteration. NULL text is valid only if empty.
   No allocation, normalization, locale tailoring or dictionary lookup. A full
   forward iteration is linear, including punctuation/ignored-character runs.
   These are boundary positions, not a classification of meaningful words. */
size_t unicode_word_next(const uint16_t *text, size_t length, size_t start);
#endif
