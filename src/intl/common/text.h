/* Scalar text properties shared by Intl pattern consumers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_TEXT_H
#define QJS_INTL_COMMON_TEXT_H

/* Unicode White_Space, distinct from ECMAScript lexical WhiteSpace. */
static inline int qjs_intl_text_is_white_space(int c)
{
    return c == 0x20 || (c >= 0x9 && c <= 0xd) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
           c == 0x205f || c == 0x3000;
}
#endif
