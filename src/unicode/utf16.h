/* Bounded UTF-16 decoding shared by Unicode algorithms.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef UNICODE_UTF16_H
#define UNICODE_UTF16_H
#include <stddef.h>
#include <stdint.h>

/* Precondition: *position < length. Preserve a lone surrogate as its code
   point value; only a valid adjacent pair consumes two original code units. */
static inline uint32_t unicode_utf16_next(const uint16_t *input, size_t length,
                                         size_t *position)
{
    uint32_t c = input[(*position)++];
    if (c >= 0xd800 && c <= 0xdbff && *position < length &&
        input[*position] >= 0xdc00 && input[*position] <= 0xdfff) {
        c = 0x10000 + ((c - 0xd800) << 10) + input[(*position)++] - 0xdc00;
    }
    return c;
}
#endif
